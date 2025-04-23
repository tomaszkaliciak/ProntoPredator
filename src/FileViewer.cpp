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
    grep_model_ = nullptr; // Initialize grep_model_ to nullptr

    // --- Create Grep Tree View (without model initially) ---
    grep_tree_view_ = new QTreeView();
    // grep_model_ = new GrepModel(logfile_->getGrepHierarchy(), this); // DEFER MODEL CREATION
    // grep_tree_view_->setModel(grep_model_); // DEFER SETTING MODEL
    grep_tree_view_->setHeaderHidden(true);
    grep_tree_view_->setContextMenuPolicy(Qt::CustomContextMenu); // Enable context menu

    // Remove manual item creation - Model handles the root (when created)
    // QStandardItem* rootItem = new QStandardItem("Base");
    // rootItem->setEditable(false);
    // rootItem->setData(QVariant::fromValue(static_cast<void*>(logfile_->getGrepHierarchy())), GrepNodeRole);
    // grep_tree_model_->appendRow(rootItem);

    // Connect selection changes - MOVED TO handleLogfileInitialized
    // connect(grep_tree_view_->selectionModel(), &QItemSelectionModel::selectionChanged,
    //         this, &FileViewer::grepTreeSelectionChanged);
    // Connect context menu request signal (This one is likely okay here)
    connect(grep_tree_view_, &QTreeView::customContextMenuRequested,
            this, &FileViewer::showGrepContextMenu);


    // --- Create Bookmarks View ---
    bookmarks_widget_ = new QListView();
    bookmarks_widget_->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    // bookmarks_widget_->setModel(logfile_->getBookmarksModel()); // DEFER SETTING MODEL
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

    // --- Connect Logfile Initialization Signal ---
    // Connect the indexingFinished signal to our new slot
    connect(logfile_, &Logfile::indexingFinished, this, &FileViewer::handleLogfileInitialized);

    // If the logfile is *already* initialized when the viewer is created
    // (e.g., loading a project where indexing finished before UI was shown),
    // call the handler immediately.
    if (logfile_->isInitialized()) {
        handleLogfileInitialized(true); // Assume success if already initialized
    }

    // Select the root item initially - MOVED TO handleLogfileInitialized
    // QModelIndex rootIndex = grep_model_->index(0, 0, QModelIndex());
    // if (rootIndex.isValid()) {
    //     grep_tree_view_->selectionModel()->select(rootIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    //     // Expand the root item if desired
    //     // grep_tree_view_->expand(rootIndex);
    // }
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
    // Check if model exists and selection is valid
    if (!logViewer_ || !grep_model_ || selected.indexes().isEmpty()) {
        return;
    }

    QModelIndex selectedIndex = selected.indexes().first();
    // Use the model's helper function to get the node
    GrepNode* selectedNode = grep_model_->getNode(selectedIndex);
    if (!selectedNode) {
        qWarning() << "grepTreeSelectionChanged: Could not get node for index" << selectedIndex;
        return;
    }
    qDebug() << "grepTreeSelectionChanged: Selected node pattern:" << QString::fromStdString(selectedNode->getPattern());

    // Build the filter chain from selected node up to the root
    QList<GrepNode*> filterChain;
    GrepNode* currentNode = selectedNode;
    while (currentNode != nullptr) {
        // Always include the node in the chain.
        // The filtering logic will skip steps with empty patterns.
        filterChain.prepend(currentNode); // Add to front to build root-to-leaf order
        currentNode = currentNode->getParent();
    }
    qDebug() << "grepTreeSelectionChanged: Built filter chain of size:" << filterChain.size();

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

// Slot to handle logfile initialization completion
void FileViewer::handleLogfileInitialized(bool success)
{
    if (success && !grep_model_) // Only proceed if successful and model doesn't exist yet
    {
        qDebug() << "Logfile initialized, creating GrepModel for:" << logfile_->getFileName();
        // Create the custom GrepModel now that getGrepHierarchy() is valid
        grep_model_ = new GrepModel(logfile_->getGrepHierarchy(), this); // Pass root node and parent
        grep_tree_view_->setModel(grep_model_);

        // Connect selection changes *after* setting the model
        connect(grep_tree_view_->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &FileViewer::grepTreeSelectionChanged);

        // Set bookmarks model now that logfile_->getBookmarksModel() is valid
        bookmarks_widget_->setModel(logfile_->getBookmarksModel());

        // Select the root item now that the model is set
        QModelIndex rootIndex = grep_model_->index(0, 0, QModelIndex());
        if (rootIndex.isValid()) {
            grep_tree_view_->selectionModel()->select(rootIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            // Optionally expand the root item
            // grep_tree_view_->expand(rootIndex);
        } else {
            qWarning("Could not get root index after setting GrepModel.");
        }
    } else if (!success) {
        qWarning("Logfile initialization failed for %s. Grep tree will not be populated.",
                 qPrintable(logfile_ ? logfile_->getFileName() : "unknown file"));
        // Optionally display an error message in the tree view area
    }
}
