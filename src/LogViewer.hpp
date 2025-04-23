#ifndef LOG_VIEWER_HPP
#define LOG_VIEWER_HPP

#include <QWidget>
#include <QtCore/Qt> // For Qt::CaseSensitivity

// Forward declarations
class Logfile;
class GrepNode;
class CustomLogView; // Changed from QTableView
class LogfileModel;
class QAbstractItemModel;
class EfficientLogFilterProxyModel; // Changed from LogFilterProxyModel
class QLabel; // For status label

class LogViewer : public QWidget
{
    Q_OBJECT
public:
    LogViewer(QWidget* parent, Logfile* logfile);

    // Method to apply filtering criteria using a chain of nodes
    void applyFilterChain(const QList<GrepNode*>& chain);

    // Accessors
    Logfile* getLogfile();
    LogfileModel* getBaseSourceModel() const;
    CustomLogView* getCustomView() const; // Changed accessor name and type

private slots:
    // Slots to handle filtering state changes from the proxy model
    void onFilteringStarted();
    void onFilteringFinished(int matchCount);
    // Slot for copying selected text
    void copySelectionToClipboard();

protected:
    Logfile* logfile_;
    CustomLogView* view_; // Changed type from QTableView
    LogfileModel* baseSourceModel_;
    EfficientLogFilterProxyModel* proxyModel_; // Changed type
    // QLabel* statusOverlay_; // Removed old overlay label
    QLabel* statusLabel_ = nullptr; // Label to show filtering status
};

#endif // LOG_VIEWER_HPP
