#include "EfficientLogFilterProxyModel.hpp"
#include "Logfile.hpp" // Needed for Logfile methods
#include "LogfileModel.hpp" // Needed to cast sourceModel()
#include "GrepNode.hpp" // Needed for applyFilterChain

#include <QDebug>
#include <QApplication>
#include <QRegularExpression>
#include <QFile>
#include <QVector>
#include <QBitArray>
#include <QList>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrent> // For QtConcurrent::run
#include <stdexcept> // For std::runtime_error in mapFromSource
#include <utility> // For std::pair
#include <QMutexLocker> // For QMutexLocker

// FilterParams and its operator== are now defined in FilterParams.hpp

EfficientLogFilterProxyModel::EfficientLogFilterProxyModel(QObject* parent)
    : QAbstractProxyModel(parent)
{
    connect(&filterWatcher_, &QFutureWatcher<QBitArray>::finished,
            this, &EfficientLogFilterProxyModel::handleFilterFinished);
}


// --- FilterChunkTask Implementation ---
void FilterChunkTask::run()
{
    // Check for null pointers passed to constructor (basic safety)
    if (!rowsToProcessChunk_ || !filename_ || !lineIndex_ || !filterParams_ || !outputBitArray_ || !outputMutex_) {
        qWarning("FilterChunkTask %d: Invalid pointers provided.", taskId_);
        return;
    }

    // Each thread needs its own QFile object
    QFile threadLocalFile(*filename_);
    if (!threadLocalFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("FilterChunkTask %d: Failed to open file %s", taskId_, qPrintable(*filename_));
        return;
    }

    // Process each row index in the assigned chunk
    for (int sourceRowIndex : *rowsToProcessChunk_) {
        // --- Cancellation Check (Optional but recommended) ---
        // Note: QThreadPool doesn't have a direct per-task cancel like QFuture.
        // Cancellation needs to be managed externally (e.g., via an atomic flag
        // checked here, or relying on the main thread cancelling the overall operation).
        // For now, we omit the check here for simplicity, relying on the main QFutureWatcher.
        // if (isCancelledAtomicFlag.load()) return;

        qint64 start_pos = lineIndex_->at(sourceRowIndex);
        if (!threadLocalFile.seek(start_pos)) {
             qWarning("FilterChunkTask %d: Failed to seek to pos %lld for line %d", taskId_, start_pos, sourceRowIndex + 1);
             continue; // Skip this row on error
        }

        QByteArray lineData = threadLocalFile.readLine();
        QString lineText = QString::fromUtf8(lineData).trimmed();

        // Apply the filter logic (same as before)
        bool matchFound = false;
        if (filterParams_->isRegex) {
             matchFound = filterParams_->regex.match(lineText).hasMatch();
        } else {
             matchFound = lineText.contains(filterParams_->pattern, filterParams_->cs);
        }

        bool stepResult = (filterParams_->inverted ? !matchFound : matchFound);

        // If it's a match, update the shared output QBitArray safely
        if (stepResult) {
            QMutexLocker locker(outputMutex_); // Lock the mutex before accessing shared data
            outputBitArray_->setBit(sourceRowIndex, true);
            // Mutex is automatically unlocked when locker goes out of scope
        }
    }

    threadLocalFile.close();
    // qDebug("FilterChunkTask %d finished.", taskId_);
}
// --- End FilterChunkTask Implementation ---


// --- QAbstractProxyModel overrides ---

void EfficientLogFilterProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    if (sourceModel_ == sourceModel) {
        return;
    }

    beginResetModel(); // Signal that the entire model structure is changing

    if (sourceModel_) {
        disconnect(sourceModel_, nullptr, this, nullptr); // Disconnect old signals if any
    }

    sourceModel_ = sourceModel;

    if (sourceModel_) {
        // Connect signals if needed, e.g., for source model resets or data changes
        // connect(sourceModel_, &QAbstractItemModel::modelReset, this, &EfficientLogFilterProxyModel::sourceModelReset);
        // connect(sourceModel_, &QAbstractItemModel::rowsInserted, this, &EfficientLogFilterProxyModel::sourceRowsInserted);
        // connect(sourceModel_, &QAbstractItemModel::rowsRemoved, this, &EfficientLogFilterProxyModel::sourceRowsRemoved);
        // connect(sourceModel_, &QAbstractItemModel::dataChanged, this, &EfficientLogFilterProxyModel::sourceDataChanged);
        // connect(sourceModel_, &QAbstractItemModel::layoutChanged, this, &EfficientLogFilterProxyModel::sourceLayoutChanged);

        // Initial population of mapping (assuming no filter initially)
        if (sourceModel_->rowCount() > 0) {
             currentSourceMatches_.resize(sourceModel_->rowCount());
             currentSourceMatches_.fill(true); // Initially, all rows match (no filter)
             updateMapping(currentSourceMatches_); // Populate proxyToSourceMap_
        } else {
             currentSourceMatches_.clear();
             proxyToSourceMap_.clear();
        }

    } else {
        // Clear mapping if source model is removed
        currentSourceMatches_.clear();
        proxyToSourceMap_.clear();
        sourceToProxyMap_.clear(); // Clear reverse map too
    }

    endResetModel(); // Signal completion of the model change
}


QModelIndex EfficientLogFilterProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!sourceModel_ || !proxyIndex.isValid() || proxyIndex.row() >= proxyToSourceMap_.size()) {
        return QModelIndex();
    }
    int sourceRow = proxyToSourceMap_.at(proxyIndex.row());
    return sourceModel_->index(sourceRow, proxyIndex.column());
}

QModelIndex EfficientLogFilterProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceModel_ || !sourceIndex.isValid()) {
        return QModelIndex();
    }

    // Use the hash map for efficient lookup O(1) average
    int sourceRow = sourceIndex.row();
    int proxyRow = sourceToProxyMap_.value(sourceRow, -1); // Returns -1 if not found

    if (proxyRow != -1) {
        return createIndex(proxyRow, sourceIndex.column());
    } else {
        // Source row is not currently visible in the proxy
        return QModelIndex();
    }
}

QModelIndex EfficientLogFilterProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    // This model is flat (no hierarchy), so parent should always be invalid.
    if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 || column >= columnCount()) {
        return QModelIndex();
    }
    return createIndex(row, column);
}

QModelIndex EfficientLogFilterProxyModel::parent(const QModelIndex& /*child*/) const
{
    // No hierarchy
    return QModelIndex();
}

int EfficientLogFilterProxyModel::rowCount(const QModelIndex& parent) const
{
    // No hierarchy, return 0 if parent is valid.
    // Otherwise, return the number of mapped rows.
    return (!parent.isValid() && sourceModel_) ? proxyToSourceMap_.size() : 0;
}

int EfficientLogFilterProxyModel::columnCount(const QModelIndex& parent) const
{
    // No hierarchy, return 0 if parent is valid.
    // Otherwise, return the column count from the source model.
    return (!parent.isValid() && sourceModel_) ? sourceModel_->columnCount() : 0;
}

QVariant EfficientLogFilterProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
    if (!sourceModel_ || !proxyIndex.isValid()) {
        return QVariant();
    }
    // Map to source and retrieve data
    QModelIndex sourceIndex = mapToSource(proxyIndex);
    return sourceModel_->data(sourceIndex, role);
}

// --- Filtering Logic ---

void EfficientLogFilterProxyModel::setSourceLogfile(Logfile* logfile)
{
    // This is needed for the background task to access file data
    sourceLogfile_ = logfile;
    // Reset filter state when logfile changes
    lastAppliedFilterChainParams_.clear();
    currentFilterChainParams_.clear();
    if (sourceModel_) {
        beginResetModel();
        currentSourceMatches_.resize(sourceModel_->rowCount());
        currentSourceMatches_.fill(true); // All rows match initially
        updateMapping(currentSourceMatches_); // Rebuild map
        endResetModel();
    }
}

bool EfficientLogFilterProxyModel::isFiltering() const
{
    return isFiltering_;
}

void EfficientLogFilterProxyModel::cancelFiltering()
{
    if (isFiltering_) {
        qDebug() << "Attempting to cancel filtering (Efficient)...";
        filterWatcher_.cancel();
        // Note: The task might not stop immediately. handleFilterFinished will check isCanceled().
    }
}

// Adapted from LogFilterProxyModel::applyFilterChain
void EfficientLogFilterProxyModel::applyFilterChain(const QList<GrepNode*>& chain)
{
    if (!sourceModel_ || !sourceLogfile_) {
         qWarning("EfficientLogFilterProxyModel: Cannot apply filter chain, source model or logfile not set.");
         return;
    }

    QList<FilterParams> newParamsList;
    for (const GrepNode* node : chain) {
        if (!node) continue;
        FilterParams params;
        params.pattern = QString::fromStdString(node->getPattern());
        params.isRegex = node->isRegEx();
        params.cs = node->isCaseInsensitive() ? Qt::CaseInsensitive : Qt::CaseSensitive;
        params.inverted = node->isInverted();
        if (params.isRegex) {
            QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
            if (params.cs == Qt::CaseInsensitive) {
                options |= QRegularExpression::CaseInsensitiveOption;
            }
            params.regex.setPattern(params.pattern);
            params.regex.setPatternOptions(options);
             if (!params.regex.isValid()) {
                 qWarning() << "Invalid regex pattern in filter chain:" << params.pattern;
                 // Optionally: Clear the chain or handle error appropriately
                 // For now, we let the background task handle invalid regex
             }
        }
        newParamsList.append(params);
    }

    // Check if the new chain is the same as the last applied one
    bool chainsAreEqual = (newParamsList.size() == lastAppliedFilterChainParams_.size());
    if (chainsAreEqual) {
        for (int i = 0; i < newParamsList.size(); ++i) {
            // Use the operator== defined in FilterParams.hpp
            if (!(newParamsList.at(i) == lastAppliedFilterChainParams_.at(i))) {
                chainsAreEqual = false;
                break;
            }
        }
    }

    // Skip if already filtering OR if the chain hasn't changed
    if (isFiltering_) {
        qDebug() << "Filter chain changed while filtering, will apply new filter after current one finishes.";
        // Store the new chain to be picked up later if needed, or handle differently
        currentFilterChainParams_ = newParamsList; // Store the *intended* filter
        // Maybe queue the request? For now, just update intended and let current finish.
        return;
    }

    if (chainsAreEqual) {
        qDebug() << "Filter chain hasn't changed, skipping redundant filtering.";
        return;
    }

    currentFilterChainParams_ = newParamsList;
    startAsyncFiltering();
}

// Adapted from LogFilterProxyModel::startAsyncFiltering
void EfficientLogFilterProxyModel::startAsyncFiltering()
{
    if (isFiltering_) { // Double check
        qDebug() << "Filtering already in progress.";
        return;
    }
    if (!sourceLogfile_ || !sourceModel_) {
        qWarning("Cannot filter: source Logfile or sourceModel not set.");
        return;
    }

    isFiltering_ = true;
    emit filteringStarted();

    // Prepare data copies for the background task
    QString filenameCopy = sourceLogfile_->getFileName();
    QVector<qint64> lineIndexCopy = sourceLogfile_->getLineIndexCopy();
    QList<FilterParams> filterChainParamsCopy = currentFilterChainParams_;

    // Run the filtering task in a separate thread
    auto filterLambda = [=]() -> QBitArray {
        return EfficientLogFilterProxyModel::performFilteringTask(
            filenameCopy, lineIndexCopy, filterChainParamsCopy
        );
    };
    QFuture<QBitArray> future = QtConcurrent::run(filterLambda);
    filterWatcher_.setFuture(future);
}

// Adapted from LogFilterProxyModel::handleFilterFinished
void EfficientLogFilterProxyModel::handleFilterFinished()
{
    bool wasCancelled = filterWatcher_.isCanceled();
    int matchCount = 0;
    QBitArray newMatches; // Holds the results

    if (!wasCancelled) {
        newMatches = filterWatcher_.result();
        if (newMatches.isEmpty() && sourceModel_ && sourceModel_->rowCount() > 0) { // Added sourceModel check
            // Handle potential cancellation within performFilteringTask returning empty
             qDebug() << "Filtering task returned empty result, likely cancelled internally.";
             wasCancelled = true; // Treat as cancelled
             matchCount = currentSourceMatches_.count(true); // Use old count
        } else {
            lastAppliedFilterChainParams_ = currentFilterChainParams_; // Store the successfully applied filter
            matchCount = newMatches.count(true);
            qDebug() << "Filtering finished. Matches found:" << matchCount;
        }
    } else {
        qDebug() << "Filtering was cancelled externally.";
        matchCount = currentSourceMatches_.count(true); // Use count from *before* this filter started
    }

    isFiltering_ = false; // Mark filtering as done *before* emitting signal/updating map

    // Emit signal *before* potentially blocking map update
    emit filteringFinished(matchCount);

    // Update the model mapping only if the filter wasn't cancelled
    if (!wasCancelled) {
        updateMapping(newMatches); // This is where the magic happens!
    }

    // TODO: Check if currentFilterChainParams_ changed while filtering was running
    // and potentially restart filtering with the latest parameters.
}

// --- Static Filtering Task (Single-Threaded - Reverted) ---
QBitArray EfficientLogFilterProxyModel::performFilteringTask(
    QString filename, QVector<qint64> lineIndex, QList<FilterParams> filterChainParams)
{
    qint64 lineCount = lineIndex.size();
    if (lineCount == 0) return QBitArray();

    QBitArray currentMatches(lineCount, true); // Start with all lines matching
    QFile localFile(filename); // Open file once

    if (!localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Background task: Failed to open file %s", qPrintable(filename));
        return QBitArray(lineCount, false); // Return empty matches on error
    }

    // Apply each filter step sequentially
    for (const FilterParams& params : filterChainParams) {
        // If the pattern for this step is empty, skip it
        if (params.pattern.isEmpty()) continue;

        // Check regex validity once per step
        if (params.isRegex && !params.regex.isValid()) {
            qWarning("Background task: Invalid regex '%s' encountered in chain.", qPrintable(params.pattern));
            return QBitArray(); // Signal error by returning empty array
        }

        QBitArray stepMatches(lineCount, false); // Results for *this* step

        // Iterate only through lines that matched the *previous* step
        for (qint64 i = 0; i < lineCount; ++i) {
            int rowIndex = static_cast<int>(i);
            if (!currentMatches.testBit(rowIndex)) {
                continue; // Skip if didn't pass previous filters
            }

            // --- Cancellation Check ---
            if (QThread::currentThread()->isInterruptionRequested()) {
                qDebug() << "Background filtering task interrupted.";
                localFile.close(); // Ensure file is closed
                return QBitArray(); // Return empty array on cancellation
            }
            // --- End Cancellation Check ---

            qint64 start_pos = lineIndex.at(i);
            if (!localFile.seek(start_pos)) {
                 qWarning("Background task: Failed to seek to pos %lld for line %lld", start_pos, i + 1);
                 continue; // Skip line on seek error
            }
            QByteArray lineData = localFile.readLine();
            QString lineText = QString::fromUtf8(lineData).trimmed();

            // Apply the *current step's* filter
            bool matchFound = false;
            if (params.isRegex) {
                 matchFound = params.regex.match(lineText).hasMatch();
            } else {
                 matchFound = lineText.contains(params.pattern, params.cs);
            }

            // If the line passes this step (considering inversion), mark it in stepMatches
            if (params.inverted ? !matchFound : matchFound) {
                stepMatches.setBit(rowIndex, true);
            }
        }
        // The results of this step become the input for the next step
        currentMatches = stepMatches;

        // Optimization: if no lines match at any step, stop early
        if (currentMatches.count(true) == 0) {
            qDebug() << "Filter chain resulted in 0 matches at step:" << params.pattern;
            break;
        }
    } // End of filter chain loop

    localFile.close(); // Close the file
    return currentMatches; // Return the final set of matching lines
} // End of performFilteringTask function


// --- The Core Update Logic ---
void EfficientLogFilterProxyModel::updateMapping(const QBitArray& newMatches)
{
    if (!sourceModel_) return;

    qDebug() << "Updating mapping...";
    QBitArray oldMatches = currentSourceMatches_; // Keep a copy of the old state
    currentSourceMatches_ = newMatches; // Store the new state

    // Ensure sizes match (should happen if source model hasn't changed drastically)
    if (oldMatches.size() != currentSourceMatches_.size()) {
         qWarning("EfficientLogFilterProxyModel::updateMapping: Mismatch between old and new match array sizes. Resetting model.");
         beginResetModel();
         proxyToSourceMap_.clear();
         for (int sourceRow = 0; sourceRow < currentSourceMatches_.size(); ++sourceRow) {
             if (currentSourceMatches_.testBit(sourceRow)) {
                 proxyToSourceMap_.append(sourceRow);
             }
         }
         endResetModel();
         return;
    }

    // --- Always use Reset Model based on perf results ---
    // The batching logic (beginInsert/RemoveRows) triggers extremely expensive
    // QHeaderView::isSectionHidden calls when many rows change.
    // Resetting the model is faster overall, despite losing view state.
    qDebug() << "Using beginResetModel/endResetModel for update.";
    beginResetModel();
    proxyToSourceMap_.clear();
    sourceToProxyMap_.clear();
    int currentProxyRow = 0;
    for (int sourceRow = 0; sourceRow < currentSourceMatches_.size(); ++sourceRow) {
        if (currentSourceMatches_.testBit(sourceRow)) {
            proxyToSourceMap_.append(sourceRow);
            sourceToProxyMap_.insert(sourceRow, currentProxyRow++);
        }
    }
    endResetModel();
    // --- End Reset Model Logic ---

    qDebug() << "Mapping update finished. New proxy row count:" << proxyToSourceMap_.size();
}
