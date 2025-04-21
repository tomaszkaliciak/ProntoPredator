#include "MainWindow.hpp"
#include "ui_MainWindow.h"

#include <memory>
#include <string> // Add missing include for std::string

// TODO: cleanup this includes after some mockups creation and proper class segregation
#include <QAction>
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QMimeData>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextEdit>

#include "Bookmark.hpp"
#include "BookmarksModel.hpp"
#include "GrepDialogWindow.hpp"
#include "GrepNode.hpp"
#include "Logfile.hpp"
#include "ProjectModel.hpp"
#include "LogViewer.hpp"
#include "TextRenderer.hpp"
#include "FileViewer.hpp"
#include "loader/Project.hpp"
#include "serializer/SerializerProjectModel.hpp"
#include "Version.hpp"
#include "ProjectUiManager.hpp"
#include "LogfileModel.hpp" // Include for Column enum
#include "GrepDialogWindow.hpp" // Added back
#include <QTableView> // Added for model() access
#include <QTreeView> // Needed for grep tree access
#include <QStandardItemModel> // Needed for grep tree model access
#include <QItemSelectionModel> // Needed for grep tree selection access
#include "GrepModel.hpp" // Added include

// Define the role used in FileViewer again (or move to a shared header)
const int GrepNodeRole = Qt::UserRole + 1;

void MainWindow::closeFileTab(const int index)
{
    QTabWidget* tabWidget = ui->fileView;
    QWidget* tabContents = tabWidget->widget(index);
    tabWidget->removeTab(index);
    if (tabContents != nullptr) delete(tabContents);
}
void MainWindow::connect_signals()
{
    connect(ui->fileView, &QTabWidget::tabCloseRequested, this, &MainWindow::closeFileTab);
}

void MainWindow::newProject()
{
    pm_->create_new();
}

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    setAcceptDrops(true);
    ui->setupUi(this);
    ui->fileView->setTabsClosable(true);
    statusBar()->showMessage(tr("Use load from file menu or drop files in this window to begin."));
    connect_signals();
    pm_ = std::make_unique<ProjectUiManager>(ui);
    pm_->connect_update_notif([this](){updateUi();});
    newProject();
    updateUi();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    event->acceptProposedAction();
    const QMimeData* mimeData = event->mimeData();
    if (!mimeData->hasUrls())
    {
        qDebug() << "Non URL mime data type";
        return;
    }

    QList<QUrl> urlList = mimeData->urls();
    for (const auto& fileList : urlList)
    {
        load_log_file(fileList.toLocalFile());
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
  // if some actions should not be usable, like move, this code must be adopted
  event->acceptProposedAction();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::load_log_file(QString file_path)
{
   pm_->load_log_file(file_path);
}

FileViewer* MainWindow::get_active_viewer_widget()
{
    const int tab_index = ui->fileView->currentIndex();
    if(tab_index == -1) return nullptr;
    FileViewer* viewerWidget = dynamic_cast<FileViewer*>(ui->fileView->widget(tab_index));
    if (!viewerWidget) throw std::string("Could not find active ViewerWidget");
    return viewerWidget;
}

void MainWindow::grepCurrentView()
{
    //TODO make grep and bookmark active only when file is loaded
    FileViewer* viewerWidget = get_active_viewer_widget();
    if (!viewerWidget) return; // can display here some message

    // Get the LogViewer from the FileViewer
    LogViewer* logViewer = viewerWidget->getLogViewer();
    if (!logViewer) {
        QMessageBox::warning(this, "Grep Error", "Could not find the log viewer component.");
        return;
    }

    GrepDialogWindow grepDialog;
    if (grepDialog.exec() != QDialog::Accepted) return;

    auto result = grepDialog.getResult();

    // --- Get the currently selected GrepNode from the FileViewer's tree ---
    GrepNode* parentNode = viewerWidget->getSelectedGrepNode();
    if (!parentNode) {
         QMessageBox::warning(this, "Grep Error", "Could not determine parent grep node.");
         return;
    }

    // --- Create the new GrepNode data structure ---
    GrepNode* newNode = new GrepNode(result.pattern.toStdString(),
                                     result.is_regex,
                                     result.is_case_insensitive,
                                     result.is_inverted);
    // Add the new node via the GrepModel (which handles data and view updates)
    // Need access to the GrepModel from FileViewer (make public/add getter)
    // Hacky alternative:
    QTreeView* grepTreeView = viewerWidget->findChild<QTreeView*>();
    GrepModel* grepModel = qobject_cast<GrepModel*>(grepTreeView ? grepTreeView->model() : nullptr);
    if (grepModel) {
        grepModel->addGrepNode(parentNode, newNode);
    } else {
         QMessageBox::warning(this, "Grep Error", "Could not access grep model to add node.");
         // Clean up the created node if we can't add it?
         delete newNode; // Or handle ownership differently
    }
    // viewerWidget->addGrepNodeToTree(parentNode, newNode); // Removed - Model handles UI update
}

#include <QItemSelectionModel> // Needed for selection model
#include <QTableView>        // Needed for casting view_
#include "LogFilterProxyModel.hpp" // Needed for proxy model access (though maybe via LogViewer?)

// Forward declare QTableView if not included via LogViewer.hpp indirectly
// class QTableView;

void MainWindow::bookmark_current_line()
{
    FileViewer* viewerWidget = get_active_viewer_widget();
    if (!viewerWidget) return;

    LogViewer* logViewer = viewerWidget->getLogViewer();
    if (!logViewer || !viewerWidget->logfile_) {
         QMessageBox::warning(this, "Bookmark Error", "Could not find viewer components.");
         return;
    }

    // Access the internal QTableView (Need to make view_ accessible or add getter in LogViewer)
    // For now, let's assume we add a getter `getTableView()` to LogViewer
    // QTableView* tableView = logViewer->getTableView(); // Assumed getter
    // Hacky alternative for now (breaks encapsulation):
    QTableView* tableView = logViewer->findChild<QTableView*>(); // Find the view_ child
    if (!tableView) {
        QMessageBox::warning(this, "Bookmark Error", "Could not find table view.");
        return;
    }

    QItemSelectionModel* selectionModel = tableView->selectionModel();
    if (!selectionModel || !selectionModel->hasSelection()) {
        QMessageBox::information(this, "Bookmark", "Please select a line to bookmark.");
        return;
    }

    // Get the first selected index in the view
    QModelIndex viewIndex = selectionModel->selectedIndexes().first();

    // Map the view index to the proxy model index (if filtering is active)
    // QSortFilterProxyModel* proxyModel = qobject_cast<QSortFilterProxyModel*>(tableView->model()); // Get proxy model
    // QModelIndex proxyIndex = proxyModel ? proxyModel->mapToSource(viewIndex) : viewIndex; // Map if proxy exists

    // Map the view index directly to the source model index
    QSortFilterProxyModel* proxyModel = qobject_cast<QSortFilterProxyModel*>(tableView->model());
    if (!proxyModel) {
         QMessageBox::warning(this, "Bookmark Error", "Could not access filter model.");
         return;
    }
    QModelIndex sourceIndex = proxyModel->mapToSource(viewIndex);

    if (!sourceIndex.isValid()) {
         QMessageBox::warning(this, "Bookmark Error", "Could not map selected line to source.");
         return;
    }

    // Get the original line number (1-based) from the source model data (column 0)
    qint64 absolute_line_index = sourceIndex.model()->data(sourceIndex.model()->index(sourceIndex.row(), LogfileModel::Column::LineNumberColumn)).toLongLong();

    if (absolute_line_index < 1) {
         QMessageBox::warning(this, "Bookmark Error", "Invalid line number obtained.");
         return;
    }

    // Get the line text from the Logfile for the dialog default
    QString current_line_text = viewerWidget->logfile_->getLine(absolute_line_index).text;

    // Simple QInputDialog will be extended later for something more fancy
    bool ok = false;
    QString bookmark_name = QInputDialog::getText(this, tr("Bookmark creation"),
        tr("Name:"), QLineEdit::Normal, current_line_text, &ok); // Use fetched text

    if (!ok || bookmark_name.isEmpty()) return; // Also check if name is empty

    // Add the bookmark using the absolute line number
    viewerWidget->logfile_->getBookmarksModel()->add_bookmark(static_cast<uint32_t>(absolute_line_index),
        QString(":/icon/Gnome-Bookmark-New-32.png"),
        bookmark_name);

    // Note: Navigation to bookmarks still needs implementation.

    // Remove the old placeholder message:
    // QMessageBox::information(this, "Bookmark", "Bookmark functionality is temporarily disabled due to refactoring for large file support.\nNavigation requires reimplementation with the new view.");
}

void MainWindow::on_actionLoad_from_file_triggered()
{
    QString file_path = QFileDialog::getOpenFileName(this,
        tr("Open log file"), "",
        tr("All Files (*)"));
    if (file_path.isEmpty())
        return;

    load_log_file(file_path);
}

void MainWindow::on_actionGrep_current_view_triggered()
{
    grepCurrentView();
}
void MainWindow::on_actionBookmark_current_line_triggered()
{
    bookmark_current_line();
}

void MainWindow::on_exit_app_triggered()
{
    if (pm_->has_changed())
    {
        QMessageBox msgBox;
        msgBox.setText("The document has been modified.");
        msgBox.setInformativeText("Do you want to save changes you made in current project? All changes will be lost if you don't save them.");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Save) saveProject();
    }   //duplicate code (move it to separate function later)

    QApplication::exit();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, QString("About application"),
        QString("LogView: " + QString(APP_VERSION) +
            "\nBuild: " + __DATE__ + " " + __TIME__ +
            "\n\nRadX64 © 2019\nReleased under\nGNU GENERAL PUBLIC LICENSE"));
}

void MainWindow::on_actionSave_project_as_triggered()
{
    if (pm_->is_empty())
    {
      QMessageBox::warning(this,"Warning!","Nothing to save.\nLoad file or project first.");
      return;
    }

    saveProject();
    updateUi();
}

void MainWindow::on_actionLoad_project_triggered()
{
    if (pm_->has_changed())
    {
        QMessageBox msgBox;
        msgBox.setText("The document has been modified.");
        msgBox.setInformativeText("Do you want to save changes you made in current project? All changes will be lost if you don't save them.");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Save) saveProject();
    }

    openProject();
    updateUi();
}

void MainWindow::setWindowTitle(const QString& title)
{
    QMainWindow::setWindowTitle("LogView " + QString(APP_VERSION) +"\t" + title);
}

void MainWindow::refreshWindowTitle()
{
    setWindowTitle(pm_->project_name().isEmpty()?"":"  -  " +
                   pm_->project_name() +
                   QString(pm_->has_changed()?" *":""));
}

void MainWindow::project_changed()
{
    updateUi();
}

void MainWindow::on_actionSave_project_triggered()
{
    pm_->save_project();
}

void MainWindow::updateMenus()
{
    ui->actionSave_project->setEnabled(!pm_->project_name().isEmpty() && pm_->has_changed());
}

void MainWindow::updateUi()
{
    refreshWindowTitle();
    updateMenus();
}

void MainWindow::saveProject()
{
    pm_->save_project();
}
void MainWindow::openProject()
{
    pm_->open_project();

}
