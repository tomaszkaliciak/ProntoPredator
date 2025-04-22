#include "FileViewer.hpp"

#include <QHBoxLayout>
#include <QListView>
#include <QSizePolicy>
#include <QSplitter>
#include <QDebug>
#include <QModelIndex>
#include <QMessageBox>
#include <QTreeView>
// #include <QStandardItemModel> // Removed
// #include <QStandardItem> // Removed
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QPoint>

#include "LogViewer.hpp"
#include "ProjectModel.hpp"
#include "GrepNode.hpp"
#include "BookmarksModel.hpp"
#include "Logfile.hpp"
#include "LogFilterProxyModel.hpp" // Keep for casting check? Maybe remove later.
#include "GrepModel.hpp" // Added include
#include "CustomLogView.hpp" // Added include for custom view
#include <QSortFilterProxyModel> // Needed for casting check

// Removed GrepNodeRole definition as internalPointer is used by GrepModel

FileViewer::FileViewer(QWidget* parent, Logfile* logfile)
    : QWidget(parent)
{
    logfile_ = logfile;
    layout_ = new QHBoxLayout(this);

    // --- Create Grep Tree View ---
    grep_tree_view_ = new QTreeView();
    // Create the custom GrepModel
    grep_model_ = new GrepModel(logfile_->getGrepHierarchy(), this); // Pass root node
    grep_tree_view_->setModel(grep_model_);
    grep_tree_view_->setHeaderHidden(true);
    // grep_tree_model_->setHorizontalHeaderLabels({"Filter"}); // Model provides header data

    // Remove manual item creation - Model handles the root
    // QStandardItem* rootItem = new QStandardItem("Base");
    // rootItem->setEditable(false);
    // rootItem->setData(QVariant::fromValue(static_cast<void*>(logfile_->getGrepHierarchy())), GrepNodeRole);
    // grep_tree_model_->appendRow(rootItem);

    // Connect selection changes
    connect(grep_tree_view_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &FileViewer::grepTreeSelectionChanged);
    // Connect context menu request signal
    connect(grep_tree_view_, &QTreeView::customContextMenuRequested,
            this, &FileViewer::showGrepContextMenu);


    // --- Create Bookmarks View ---
    bookmarks_widget_ = new QListView();
    bookmarks_widget_->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    bookmarks_widget_->setModel(logfile_->getBookmarksModel());
    bookmarks_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bookmarks_widget_, &QListView::doubleClicked, this, &FileViewer::bookmarksItemDoubleClicked);

    // --- Create Log Viewer ---
    logViewer_ = new LogViewer(this, logfile_);
    logViewer_->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    // --- Create Splitter ---
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(grep_tree_view_);
    splitter->addWidget(bookmarks_widget_);
    splitter->addWidget(logViewer_);
    splitter->setSizes({150, 150, 1000});

    // Add splitter to layout
    layout_->addWidget(splitter);
    layout_->setContentsMargins(0, 0, 0, 0);

    // Select the root item initially
    QModelIndex rootIndex = grep_model_->index(0, 0, QModelIndex());
    if (rootIndex.isValid()) {
        grep_tree_view_->selectionModel()->select(rootIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        // Expand the root item if desired
        // grep_tree_view_->expand(rootIndex);
    }
}

FileViewer::~FileViewer()
{
    emit destroyed(logfile_);
    qDebug() << "FileViewer closed for:" << (logfile_ ? logfile_->getFileName() : "null");
}

LogViewer* FileViewer::getLogViewer() const
{
    return logViewer_;
}

// Method to get the currently selected GrepNode from the tree
GrepNode* FileViewer::getSelectedGrepNode() const
{
     if (!grep_tree_view_ || !grep_model_) return nullptr;

     QModelIndexList selectedIndexes = grep_tree_view_->selectionModel()->selectedIndexes();
     if (selectedIndexes.isEmpty()) {
         // If nothing selected, return the root node?
         return grep_model_->getNode(QModelIndex());
     }
     return grep_model_->getNode(selectedIndexes.first());
}


// Removed helper function findItemForGrepNode
// QStandardItem* findItemForGrepNode(...) { ... }

// Removed addGrepNodeToTree method - Model handles this now
// void FileViewer::addGrepNodeToTree(...) { ... }


// Slot implementation for bookmark navigation
void FileViewer::bookmarksItemDoubleClicked(const QModelIndex& idx)
{
    if (!logfile_ || !logViewer_) return;

    Bookmark bookmark = logfile_->getBookmarksModel()->get_bookmark(static_cast<uint32_t>(idx.row()));
    qint64 target_line_number = bookmark.line_number_;

    CustomLogView* customView = logViewer_->getCustomView(); // Get the custom view
    QAbstractItemModel* currentViewModel = customView ? customView->model() : nullptr;
    // Cast to the specific proxy model type if needed, or keep generic QSortFilterProxyModel
    QSortFilterProxyModel* proxyModel = qobject_cast<QSortFilterProxyModel*>(currentViewModel);
    QAbstractItemModel* sourceModel = proxyModel ? proxyModel->sourceModel() : currentViewModel;

    if (!customView || !sourceModel) { // Check customView instead of tableView
        qWarning("Bookmark navigation failed: Could not find view/models.");
        return;
    }

    int source_row = static_cast<int>(target_line_number - 1);

    if (source_row < 0 || source_row >= sourceModel->rowCount()) {
        qWarning("Bookmark navigation failed: Target line number %lld out of source model range.", target_line_number);
        return;
    }

    QModelIndex sourceIndex = sourceModel->index(source_row, 0);
    QModelIndex viewIndex = proxyModel ? proxyModel->mapFromSource(sourceIndex) : sourceIndex;

    if (viewIndex.isValid()) {
        // Use the custom view's method to scroll
        customView->ensureIndexVisible(viewIndex);
        // Selection is handled internally by CustomLogView's paint/mouse events,
        // so we don't need to manually select rows here.
        // customView->selectionModel()->clearSelection(); // No selection model exposed
        // customView->selectionModel()->select(viewIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows); // No selection model exposed
    } else {
        QMessageBox::information(this, "Bookmark Navigation",
            QString("The bookmarked line (%1) is currently filtered out.").arg(target_line_number));
    }
}

// Slot to handle selection changes in the grep tree
void FileViewer::grepTreeSelectionChanged(const QItemSelection& selected, const QItemSelection& /*deselected*/)
{
    if (!logViewer_ || selected.indexes().isEmpty()) {
        return;
    }

    QModelIndex selectedIndex = selected.indexes().first();
    // Use the model's helper function to get the node
    GrepNode* selectedNode = grep_model_->getNode(selectedIndex);
    if (!selectedNode) return;

    QString pattern = QString::fromStdString(selectedNode->getPattern());
    bool isRegex = selectedNode->isRegEx();
    Qt::CaseSensitivity cs = selectedNode->isCaseInsensitive() ? Qt::CaseInsensitive : Qt::CaseSensitive;
    // Build the filter chain from root to selected node
    QList<GrepNode*> filterChain;
    GrepNode* currentNode = selectedNode;
    while (currentNode != nullptr) {
        // Don't include the root node itself if it has an empty pattern (represents "Base")
        if (!currentNode->getParent() && currentNode->getPattern().empty()) {
             // Skip root node if it's the base/empty filter
        } else {
            filterChain.prepend(currentNode); // Add to front to build root-to-leaf order
        }
        currentNode = currentNode->getParent();
    }

    // Apply the filter chain to the LogViewer's proxy model
    logViewer_->applyFilterChain(filterChain);

    // The old applyFilter call is replaced by applyFilterChain
    // logViewer_->applyFilter(pattern, isRegex, cs, inverted);
}

// Slot to show context menu for the grep tree
void FileViewer::showGrepContextMenu(const QPoint& pos)
{
    QModelIndex index = grep_tree_view_->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    // Use model's helper to get the node
    GrepNode* node = grep_model_->getNode(index);
    if (!node) return;

    // Check if the clicked item is the root node
    bool isBaseItem = (node == logfile_->getGrepHierarchy());

    QMenu contextMenu(this);
    QAction* removeAction = contextMenu.addAction("Remove Filter");
    removeAction->setEnabled(!isBaseItem);

    connect(removeAction, &QAction::triggered, this, &FileViewer::removeSelectedGrepFilter);

    contextMenu.exec(grep_tree_view_->viewport()->mapToGlobal(pos));
}

// Slot to handle removing the selected filter
void FileViewer::removeSelectedGrepFilter()
{
    QModelIndexList selectedIndexes = grep_tree_view_->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) {
        return;
    }

    QModelIndex selectedIndex = selectedIndexes.first();
    GrepNode* nodeToRemove = grep_model_->getNode(selectedIndex);

    // Check if it's the root node
    if (!nodeToRemove || nodeToRemove == logfile_->getGrepHierarchy()) {
        qWarning("Cannot remove the Base filter node.");
        return;
    }

    // Let the model handle the removal (it needs parent info internally)
    grep_model_->removeGrepNode(nodeToRemove);

    // Optional: Select the parent item after removal?
    // The model should emit signals that cause the view to update.
    // We might need to manually select the parent after removal if desired.
    // QModelIndex parentIndex = selectedIndex.parent(); // Get parent before removal
    // if(parentIndex.isValid()) {
    //     grep_tree_view_->selectionModel()->select(parentIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    // }
}
