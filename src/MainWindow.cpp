#include "MainWindow.hpp"
#include "ui_MainWindow.h"

#include <memory>
#include <string> // Add missing include for std::string

// TODO: Includes might need cleanup after further refactoring
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
#include "HighlightDialog.hpp" // Added for custom highlighting dialog

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
    // Use dynamic_cast and return nullptr if cast fails or widget is null
    FileViewer* viewerWidget = dynamic_cast<FileViewer*>(ui->fileView->widget(tab_index));
    // No need to throw, callers should handle nullptr
    return viewerWidget;
}

void MainWindow::grepCurrentView()
{
    // Get the active FileViewer widget
    FileViewer* viewerWidget = get_active_viewer_widget();
    if (!viewerWidget) {
        // Optionally show a message if no file tab is active
        // QMessageBox::information(this, "Grep", "Please open a file first.");
        return;
    }

    // Show the grep dialog
    GrepDialogWindow grepDialog(this); // Set parent
    if (grepDialog.exec() != QDialog::Accepted) {
        return; // User cancelled
    }

    // Get the results from the dialog
    auto result = grepDialog.getResult();

    // Delegate the actual filter addition to the FileViewer
    viewerWidget->addGrepFilter(result);
}

#include <QItemSelectionModel> // Needed for selection model
#include <QTableView>        // Needed for casting view_
#include "LogFilterProxyModel.hpp" // Needed for proxy model access (though maybe via LogViewer?)

// Forward declare QTableView if not included via LogViewer.hpp indirectly
// class QTableView;

void MainWindow::bookmark_current_line()
{
    // Get the active FileViewer widget
    FileViewer* viewerWidget = get_active_viewer_widget();
    if (!viewerWidget) {
        // Optionally show a message if no file tab is active
        // QMessageBox::information(this, "Bookmark", "Please open a file first.");
        return;
    }

    // Delegate the bookmarking action to the FileViewer
    viewerWidget->bookmarkSelectedLine();
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

// Helper function to prompt the user to save changes if the project is modified.
// Returns true if the operation should proceed (Save/Discard), false if cancelled.
bool MainWindow::promptSaveChanges()
{
    if (!pm_->has_changed()) {
        return true; // No changes, proceed
    }

    QMessageBox msgBox(this); // Set parent
    msgBox.setText("The document has been modified.");
    msgBox.setInformativeText("Do you want to save changes you made in the current project?\nAll changes will be lost if you don't save them.");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setIcon(QMessageBox::Warning); // Add an icon
    int ret = msgBox.exec();

    if (ret == QMessageBox::Save) {
        saveProject(); // Save the project
        return true; // Proceed after saving
    } else if (ret == QMessageBox::Discard) {
        return true; // Proceed without saving
    } else { // QMessageBox::Cancel
        return false; // Cancel the operation
    }
}

void MainWindow::on_exit_app_triggered()
{
    // Prompt to save if needed, return if cancelled
    if (!promptSaveChanges()) {
        return;
    }

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
    // Prompt to save if needed, return if cancelled
    if (!promptSaveChanges()) {
        return;
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
void MainWindow::on_actionCustomHighlighting_triggered()
{
    HighlightDialog dialog(this);

    // Pass current rules to the dialog
    dialog.setHighlightRules(m_highlightRules);

    if (dialog.exec() == QDialog::Accepted) {
        // Get updated rules from the dialog and store them
        m_highlightRules = dialog.getHighlightRules();

        m_highlightRules = dialog.getHighlightRules();

        // Trigger update in all open FileViewers
        for (int i = 0; i < ui->fileView->count(); ++i) {
            FileViewer* viewer = dynamic_cast<FileViewer*>(ui->fileView->widget(i));
            if (viewer) {
                viewer->updateHighlightRules(m_highlightRules);
            }
        }
        qDebug() << "Highlight rules updated and applied to open views.";
    }
}
