#ifndef LOGFILTERPROXYMODEL_HPP
#define LOGFILTERPROXYMODEL_HPP

#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QBitArray>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QList> // For filter chain

class LogfileModel;
class Logfile;
class GrepNode; // Forward declaration

// Structure to hold filter parameters for thread safety
struct FilterParams {
    QString pattern;
    bool isRegex = false;
    Qt::CaseSensitivity cs = Qt::CaseSensitive;
    bool inverted = false;
    QRegularExpression regex; // Pre-compiled regex if applicable
};


class LogFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit LogFilterProxyModel(QObject* parent = nullptr);

    // Removed individual setters
    // void setFilterPattern(const QString& pattern);
    // ...

    // New method to apply the entire filter chain
    void applyFilterChain(const QList<GrepNode*>& chain);

    // Override the core filtering method (checks internal bit array)
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

    void setSourceLogfile(Logfile* logfile);
    bool isFiltering() const;
    void cancelFiltering();

signals:
    void filteringStarted();
    void filteringFinished(int matchCount);

private slots:
    void handleFilterFinished();

private:
    // Store the current and last applied chain parameters
    QList<FilterParams> currentFilterChainParams_;
    QList<FilterParams> lastAppliedFilterChainParams_; // Added to avoid redundant filtering

    // Internal state
    Logfile* sourceLogfile_ = nullptr;
    QBitArray matchingSourceRows_;
    QFutureWatcher<QBitArray> filterWatcher_;
    bool isFiltering_ = false;

    // Helper methods
    void startAsyncFiltering(); // Triggers the background filtering task
    // Static method for the background task (updated signature)
    static QBitArray performFilteringTask(QString filename, QVector<qint64> lineIndex, QList<FilterParams> filterChainParams);
};

#endif // LOGFILTERPROXYMODEL_HPP
