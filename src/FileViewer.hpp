#ifndef PROJECT_VIEWER_HPP
#define PROJECT_VIEWER_HPP

#include <QWidget>

// Forward declarations
class QHBoxLayout;
class Logfile;
class LogViewer;
class QModelIndex;
class QListView;
class QTreeView;
// class QStandardItemModel; // Removed
class QItemSelection;
class GrepNode;
class GrepModel; // Added

class FileViewer: public QWidget
{
    Q_OBJECT
public:
    FileViewer(QWidget* parent, Logfile* logfile);
    ~FileViewer();

    LogViewer* getLogViewer() const;
    Logfile* logfile_; // Keep public for now

    // Method to add a new grep node to the tree (Now handled by GrepModel)
    // void addGrepNodeToTree(GrepNode* parentNode, GrepNode* newNode); // Removed

    // Method to get the currently selected GrepNode from the tree
    GrepNode* getSelectedGrepNode() const;


signals:
    void destroyed(Logfile* logfile);

protected:
    QHBoxLayout* layout_;
    LogViewer* logViewer_;
    QListView* bookmarks_widget_;
    QTreeView* grep_tree_view_;
    // QStandardItemModel* grep_tree_model_; // Removed
    GrepModel* grep_model_; // Added

private slots:
    void bookmarksItemDoubleClicked(const QModelIndex& idx);
    // Slot to handle selection changes in the grep tree
    void grepTreeSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    // Slot to show context menu for the grep tree
    void showGrepContextMenu(const QPoint& pos);
    // Slot to handle removing the selected filter
    void removeSelectedGrepFilter();
};

#endif // PROJECT_VIEWER_HPP
