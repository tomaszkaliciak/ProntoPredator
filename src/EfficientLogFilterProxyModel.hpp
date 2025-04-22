#ifndef EFFICIENTLOGFILTERPROXYMODEL_HPP
#define EFFICIENTLOGFILTERPROXYMODEL_HPP

#include <QAbstractProxyModel>
#include <QFutureWatcher>
#include <QBitArray>
#include <QVector>
#include <QRunnable> // For QRunnable
#include <QThreadPool> // For QThreadPool
#include <QMutex> // For potential result synchronization (if needed)
#include <QList>
#include <QRegularExpression>
#include <QString>
#include "FilterParams.hpp" // Added include

// Forward declarations
class Logfile;
class GrepNode;

// FilterParams struct is now defined in FilterParams.hpp


// --- Helper Runnable for Parallel Filtering ---
// Note: Making this internal to the .cpp might be cleaner, but for simplicity let's put it here.
class FilterChunkTask : public QRunnable
{
public:
    // Constructor takes necessary data (pointers or copies)
    FilterChunkTask(
        int taskId, // For debugging/identification
        const QVector<int>* rowsToProcessChunk, // Pointer to the chunk of source indices
        const QString* filename,
        const QVector<qint64>* lineIndex,
        const FilterParams* filterParams,
        QBitArray* outputBitArray, // Pointer to the shared output array
        QMutex* outputMutex // Mutex to protect access to outputBitArray
    ) : QRunnable(),
        taskId_(taskId),
        rowsToProcessChunk_(rowsToProcessChunk),
        filename_(filename),
        lineIndex_(lineIndex),
        filterParams_(filterParams),
        outputBitArray_(outputBitArray),
        outputMutex_(outputMutex)
    {
        setAutoDelete(true); // Auto-delete after run() finishes
    }

    void run() override; // Implementation will be in the .cpp

private:
    int taskId_;
    const QVector<int>* rowsToProcessChunk_;
    const QString* filename_;
    const QVector<qint64>* lineIndex_;
    const FilterParams* filterParams_;
    QBitArray* outputBitArray_;
    QMutex* outputMutex_;
};
// --- End Helper Runnable ---


class EfficientLogFilterProxyModel : public QAbstractProxyModel
{
    Q_OBJECT

public:
    explicit EfficientLogFilterProxyModel(QObject* parent = nullptr);
    ~EfficientLogFilterProxyModel() override = default;

    // --- QAbstractProxyModel overrides ---
    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    void setSourceModel(QAbstractItemModel* sourceModel) override;
    // Optional: headerData if needed

    // --- Filtering Logic ---
    void setSourceLogfile(Logfile* logfile); // Needed for background task
    void applyFilterChain(const QList<GrepNode*>& chain);
    bool isFiltering() const;
    void cancelFiltering();

signals:
    void filteringStarted();
    void filteringFinished(int matchCount); // Signal with the number of matches

private slots:
    void handleFilterFinished();

private:
    // --- Filtering Implementation ---
    void startAsyncFiltering();
    static QBitArray performFilteringTask(QString filename, QVector<qint64> lineIndex, QList<FilterParams> filterChainParams);
    void updateMapping(const QBitArray& newMatches); // The core logic for smart updates

    // --- Member Variables ---
    Logfile* sourceLogfile_ = nullptr; // Pointer to the source logfile data
    QAbstractItemModel* sourceModel_ = nullptr; // Pointer to the source LogfileModel

    QFutureWatcher<QBitArray> filterWatcher_;
    bool isFiltering_ = false;

    QList<FilterParams> currentFilterChainParams_; // Parameters for the filter currently running or queued
    QList<FilterParams> lastAppliedFilterChainParams_; // Parameters for the filter whose results are currently displayed

    QBitArray currentSourceMatches_; // Bitmask representing matches in the *source* model for the *last applied* filter
    QVector<int> proxyToSourceMap_; // Maps proxy row index -> source row index
    QHash<int, int> sourceToProxyMap_; // Maps source row index -> proxy row index (for faster mapFromSource)

};

#endif // EFFICIENTLOGFILTERPROXYMODEL_HPP
