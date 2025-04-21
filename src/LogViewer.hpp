#ifndef LOG_VIEWER_HPP
#define LOG_VIEWER_HPP

#include <QWidget>
#include <QtCore/Qt> // For Qt::CaseSensitivity

// Forward declarations
class Logfile;
class GrepNode;
class QTableView;
class LogfileModel;
class QAbstractItemModel;
class LogFilterProxyModel;
class QLabel; // For status overlay
class QProgressDialog; // Added for progress dialog

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
    QTableView* getTableView() const;

private slots:
    // Slots to handle filtering state changes from the proxy model
    void onFilteringStarted();
    void onFilteringFinished(int matchCount);

protected:
    Logfile* logfile_;
    QTableView* view_;
    LogfileModel* baseSourceModel_;
    LogFilterProxyModel* proxyModel_;
    QLabel* statusOverlay_; // Simple label to show "Filtering..."
    QProgressDialog* filterProgressDialog_ = nullptr; // Added progress dialog pointer
};

#endif // LOG_VIEWER_HPP
