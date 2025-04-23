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
    // Connection to filterWatcher_ removed as completion is handled by QTimer/atomic counter
    // connect(&filterWatcher_, &QFutureWatcher<QBitArray>::finished,
    //         this, &EfficientLogFilterProxyModel::handleFilterFinished);
}


// --- FilterChunkTask Implementation ---
void FilterChunkTask::run()
{
    // Check for null pointers passed to constructor (basic safety)
    // rowsToProcessChunk_ and lineIndex_ are now value members
    if (!filename_ || !filterChainParams_ || !outputBitArray_ || !outputMutex_ || !tasksRemaining_) {
        qWarning("FilterChunkTask %d: Invalid pointers provided.", taskId_);
        if (tasksRemaining_) tasksRemaining_->fetch_sub(1); // Decrement counter if possible
        return;
    }

    // Each thread needs its own QFile object
    QFile threadLocalFile(*filename_);
    if (!threadLocalFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("FilterChunkTask %d: Failed to open file %s", taskId_, qPrintable(*filename_));
        tasksRemaining_->fetch_sub(1); // Decrement counter
        return;
    }

    // Process each row index in the assigned chunk (use the member variable)
    for (int sourceRowIndex : rowsToProcessChunk_) {
        // --- Cancellation Check (Optional but recommended) ---
        // TODO: Implement a proper cancellation mechanism if needed (e.g., shared atomic bool)
        // if (g_cancelFilteringFlag.load()) { threadLocalFile.close(); tasksRemaining_->fetch_sub(1); return; }

        // Assume the line matches the full chain until proven otherwise
        bool lineMatchesChain = true;

        // Read the line text only if there's an actual filter to apply
        QString lineText;
        bool lineRead = false;

        // Apply each filter step in the chain sequentially
        for (const FilterParams& params : *filterChainParams_) { // Dereference pointer to list
            // If the pattern for this step is empty, skip it
            if (params.pattern.isEmpty()) continue;
            // Regex validity already checked before starting tasks

            // Read the line text if we haven't already for this row
            if (!lineRead) {
                // Check index bounds (important!) using the value member lineIndex_
                if (sourceRowIndex < 0 || sourceRowIndex >= lineIndex_.size()) {
                     qWarning("FilterChunkTask %d: Invalid sourceRowIndex %d", taskId_, sourceRowIndex);
                     lineMatchesChain = false;
                     break;
                }
                qint64 start_pos = lineIndex_.at(sourceRowIndex); // Use value member
                if (!threadLocalFile.seek(start_pos)) {
                    qWarning("FilterChunkTask %d: Failed to seek to pos %lld for line %d", taskId_, start_pos, sourceRowIndex + 1);
                    lineMatchesChain = false; // Treat as non-match on error
                    break; // Stop processing this line
                }
                QByteArray lineData = threadLocalFile.readLine();
                lineText = QString::fromUtf8(lineData).trimmed();
                lineRead = true;
            }

            // Apply the *current step's* filter logic
            bool stepMatchFound = false;
            if (params.isRegex) {
                 stepMatchFound = params.regex.match(lineText).hasMatch();
            } else {
                 stepMatchFound = lineText.contains(params.pattern, params.cs);
            }

            // Check if the line passes this specific step (considering inversion)
            bool passesThisStep = (params.inverted ? !stepMatchFound : stepMatchFound);

            if (!passesThisStep) {
                lineMatchesChain = false; // Line failed one step, it doesn't match the full chain
                break; // No need to check further filter steps for this line
            }
        } // End of filter chain loop for one line

        // If it's a match for the whole chain, update the shared output QBitArray safely
        if (lineMatchesChain) {
            QMutexLocker locker(outputMutex_); // Lock the mutex before accessing shared data
            // Check bounds again before writing
            if (sourceRowIndex >= 0 && sourceRowIndex < outputBitArray_->size()) {
                 outputBitArray_->setBit(sourceRowIndex, true);
            } else {
                 qWarning("FilterChunkTask %d: Invalid sourceRowIndex %d for outputBitArray", taskId_, sourceRowIndex);
            }
            // Mutex is automatically unlocked when locker goes out of scope
        }
    } // End of row processing loop

    threadLocalFile.close();
    // qDebug("FilterChunkTask %d finished.", taskId_);

    // Decrement the counter. The main thread will check if it reached zero.
    tasksRemaining_->fetch_sub(1);

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
        // Connect the crucial modelReset signal
        connect(sourceModel_, &QAbstractItemModel::modelReset, this, &EfficientLogFilterProxyModel::sourceModelReset);
        // Other connections remain commented out for now unless needed
        // connect(sourceModel_, &QAbstractItemModel::rowsInserted, this, &EfficientLogFilterProxyModel::sourceRowsInserted);
        // connect(sourceModel_, &QAbstractItemModel::rowsRemoved, this, &EfficientLogFilterProxyModel::sourceRowsRemoved);
        // connect(sourceModel_, &QAbstractItemModel::dataChanged, this, &EfficientLogFilterProxyModel::sourceDataChanged);
        // connect(sourceModel_, &QAbstractItemModel::layoutChanged, this, &EfficientLogFilterProxyModel::sourceLayoutChanged);

        // Initial population of mapping (assuming no filter initially)
        if (sourceModel_->rowCount() > 0) {
             currentSourceMatches_.resize(sourceModel_->rowCount());
              currentSourceMatches_.fill(true); // Initially, all rows match (no filter)
              updateMapping(currentSourceMatches_); // Update view based on initial state
         } else {
              currentSourceMatches_.clear();
              // proxyToSourceMap_.clear(); // REMOVED
         }

     } else {
         // Clear mapping if source model is removed
         currentSourceMatches_.clear();
         // proxyToSourceMap_.clear(); // REMOVED
         // sourceToProxyMap_.clear(); // REMOVED
         // Update mapping with empty results
         updateMapping(currentSourceMatches_);
     }

     endResetModel(); // Signal completion of the model change
}


QModelIndex EfficientLogFilterProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    // Use the restored map
    if (!sourceModel_ || !proxyIndex.isValid() || proxyIndex.row() >= proxyToSourceMap_.size()) {
        return QModelIndex();
    }
    int sourceRow = proxyToSourceMap_.at(proxyIndex.row());
    return sourceModel_->index(sourceRow, proxyIndex.column());
}

QModelIndex EfficientLogFilterProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    // Use the restored map
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
    qDebug() << "EfficientLogFilterProxyModel::applyFilterChain: Received chain of size:" << chain.size();
    if (!sourceModel_ || !sourceLogfile_) {
         qWarning("EfficientLogFilterProxyModel: Cannot apply filter chain, source model or logfile not set.");
         return;
    }

    QList<FilterParams> newParamsList;
    qDebug() << "  Building FilterParams list..."; // Log param building
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
                 // Show user-facing error and stop processing this filter chain
                 QMessageBox::warning(nullptr, // No easy parent access here
                                      tr("Invalid Filter"),
                                      tr("Invalid regular expression pattern:\n'%1'\n\nError: %2")
                                      .arg(params.pattern)
                                      .arg(params.regex.errorString()));
                 qWarning() << "Invalid regex pattern in filter chain:" << params.pattern << "Error:" << params.regex.errorString();
                 return; // Do not proceed with applying this invalid filter chain
             }
        }
        newParamsList.append(params);
        // Log details of each param
        qDebug() << "    Param:" << params.pattern << "Regex:" << params.isRegex << "CS:" << params.cs << "Inv:" << params.inverted;
    }
    qDebug() << "  FilterParams list built. Size:" << newParamsList.size();

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
        // Store the new chain to be picked up later if needed, or handle differently
        currentFilterChainParams_ = newParamsList; // Store the *intended* filter
        // Maybe queue the request? For now, just update intended and let current finish.
        return;
    }

    if (chainsAreEqual) { // Re-enable this check
        qDebug() << "Filter chain hasn't changed, skipping redundant filtering.";
        return;
    }

    currentFilterChainParams_ = newParamsList;
    qDebug() << "EfficientLogFilterProxyModel::applyFilterChain: Starting async filtering...";
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
    if (sourceModel_->rowCount() == 0) {
        qDebug() << "Source model is empty, skipping filtering.";
        // Directly update with empty results
        updateMapping(QBitArray(0));
        lastAppliedFilterChainParams_ = currentFilterChainParams_;
        emit filteringFinished(0);
        return;
    }

    isFiltering_ = true;
    emit filteringStarted();

    // Prepare data for tasks (use pointers/references where safe)
    const QString* filenamePtr = &sourceLogfile_->getFileName(); // Pointer is fine, Logfile lifetime > task
    // Store the copy locally first, then take its address
    QVector<qint64> lineIndexCopy = sourceLogfile_->getLineIndexCopy();
    const QVector<qint64>* lineIndexPtr = &lineIndexCopy;
    const QList<FilterParams>* filterChainParamsPtr = &currentFilterChainParams_; // Pointer is fine

    int sourceRowCount = sourceModel_->rowCount();
    parallelFilterResult_.resize(sourceRowCount); // Resize shared result array
    parallelFilterResult_.fill(false); // Initialize to false (only set true on match)

    int numThreads = QThread::idealThreadCount();
    // Clamp threads to a reasonable number, e.g., max 8, min 1
    numThreads = qBound(1, numThreads, 8);
    int chunkSize = (sourceRowCount + numThreads - 1) / numThreads; // Ceiling division

    qDebug() << "Starting parallel filtering with" << numThreads << "threads, chunk size" << chunkSize;

    tasksRemaining_.store(numThreads); // Initialize atomic counter

    // Create and start tasks
    QVector<QVector<int>> rowChunks(numThreads);
    for (int i = 0; i < sourceRowCount; ++i) {
        rowChunks[i / chunkSize].append(i); // Distribute source row indices into chunks
    }

    for (int i = 0; i < numThreads; ++i) {
        if (rowChunks[i].isEmpty()) {
             tasksRemaining_.fetch_sub(1); // Decrement counter if a chunk is empty
             continue;
        }
        // Pass vectors by const reference (constructor will copy them)
        // Pass other data by pointer
        FilterChunkTask* task = new FilterChunkTask(
            i,
            rowChunks[i],       // Pass vector by const reference
            filenamePtr,
            lineIndexCopy,      // Pass vector by const reference
            filterChainParamsPtr,
            &parallelFilterResult_, // Pointer to shared result array
            &resultMutex_,         // Pointer to shared mutex
            &tasksRemaining_       // Pointer to atomic counter
        );
        QThreadPool::globalInstance()->start(task);
    }

    // We no longer use QFutureWatcher directly here.
    // Completion is handled when tasksRemaining_ hits zero.
    // We need a mechanism to check this counter and call handleFilterFinished.
    // Using a QTimer is one way, or connecting a signal from the task (more complex).

    // Let's try checking periodically with a QTimer (simplest for now)
    // This timer should be stopped in handleFilterFinished or cancelFiltering
    QTimer* checkTimer = new QTimer(this);
    connect(checkTimer, &QTimer::timeout, this, [this, checkTimer]() {
        if (tasksRemaining_.load() == 0) {
            checkTimer->stop();
            checkTimer->deleteLater();
            // Check if cancellation was requested *during* the tasks
            // TODO: Implement proper cancellation check for parallel tasks if needed
            bool wasCancelled = false; // Placeholder
            handleParallelFilterCompletion(wasCancelled);
        }
    });
    checkTimer->start(50); // Check every 50ms
}

// New function to handle completion of parallel tasks
void EfficientLogFilterProxyModel::handleParallelFilterCompletion(bool wasCancelled)
{
     // This function is called when tasksRemaining_ hits 0
     qDebug() << "All parallel filter tasks finished.";

     int matchCount = 0;

     if (!wasCancelled) {
         // The result is already in parallelFilterResult_
         lastAppliedFilterChainParams_ = currentFilterChainParams_; // Store the successfully applied filter
         matchCount = parallelFilterResult_.count(true);
         qDebug() << "Parallel filtering finished. Matches found:" << matchCount;
     } else {
         qDebug() << "Parallel filtering was cancelled.";
         // If cancelled, revert to the state before this filter started
         parallelFilterResult_ = currentSourceMatches_; // Use old matches
         matchCount = parallelFilterResult_.count(true);
     }

     isFiltering_ = false; // Mark filtering as done

     // Emit signal *before* potentially blocking map update
     emit filteringFinished(matchCount);

     // Update the model mapping using the combined result
     if (!wasCancelled) {
         updateMapping(parallelFilterResult_);
     }
     // If cancelled, mapping remains unchanged (using old currentSourceMatches_)
}


// Adapted from LogFilterProxyModel::handleFilterFinished - REMOVED as redundant
/*
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
*/

// --- Static Filtering Task ---
QBitArray EfficientLogFilterProxyModel::performFilteringTask(
    QString filename, QVector<qint64> lineIndex, QList<FilterParams> filterChainParams)
{
    qint64 lineCount = lineIndex.size();
    if (lineCount == 0) return QBitArray();

    // Start with all lines potentially matching
    QBitArray finalMatches(lineCount, true); // Assume match until proven otherwise

    QFile localFile(filename);
    if (!localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Background task: Failed to open file %s", qPrintable(filename));
        return QBitArray(); // Return empty on error
    }

    // Iterate through all lines ONCE
    for (qint64 i = 0; i < lineCount; ++i) {
        int rowIndex = static_cast<int>(i);

        // --- Cancellation Check ---
        if (QThread::currentThread()->isInterruptionRequested()) {
            qDebug() << "Background filtering task interrupted.";
            localFile.close();
            return QBitArray(); // Return empty on cancellation
        }
        // --- End Cancellation Check ---

        // Assume the line matches the full chain until proven otherwise
        bool lineMatchesChain = true;

        // Read the line text only if there's an actual filter to apply
        QString lineText;
        bool lineRead = false;

        // Apply each filter step in the chain sequentially
        for (const FilterParams& params : filterChainParams) {
            // If the pattern for this step is empty, skip it (it doesn't filter anything out)
            if (params.pattern.isEmpty()) continue;

            // Check regex validity (already done in applyFilterChain, but safe to re-check)
            if (params.isRegex && !params.regex.isValid()) {
                qWarning("Background task: Invalid regex '%s' encountered.", qPrintable(params.pattern));
                localFile.close();
                return QBitArray(); // Signal error
            }

            // Read the line text if we haven't already for this row
            if (!lineRead) {
                qint64 start_pos = lineIndex.at(i);
                if (!localFile.seek(start_pos)) {
                    qWarning("Background task: Failed to seek to pos %lld for line %lld", start_pos, i + 1);
                    lineMatchesChain = false; // Treat as non-match on error
                    break; // Stop processing this line
                }
                QByteArray lineData = localFile.readLine();
                lineText = QString::fromUtf8(lineData).trimmed();
                lineRead = true;
            }

            // Apply the *current step's* filter logic
            bool stepMatchFound = false;
            if (params.isRegex) {
                 stepMatchFound = params.regex.match(lineText).hasMatch();
            } else {
                 stepMatchFound = lineText.contains(params.pattern, params.cs);
            }

            // Check if the line passes this specific step (considering inversion)
            bool passesThisStep = (params.inverted ? !stepMatchFound : stepMatchFound);

            if (!passesThisStep) {
                lineMatchesChain = false; // Line failed one step, it doesn't match the full chain
                break; // No need to check further filter steps for this line
            }
        } // End of filter chain loop for one line

        // Update the final result bit for this line based on whether it passed all steps
        finalMatches.setBit(rowIndex, lineMatchesChain);

    } // End of line iteration loop

    localFile.close();
    return finalMatches; // Return the final set of matching lines
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
    // Iterate based on the size of the new results
    for (int sourceRow = 0; sourceRow < newMatches.size(); ++sourceRow) {
        // Check the bit in the *new* results array passed to the function
        if (newMatches.testBit(sourceRow)) {
            proxyToSourceMap_.append(sourceRow);
            sourceToProxyMap_.insert(sourceRow, currentProxyRow++);
        }
    }
    endResetModel();
    // --- End Reset Model Logic ---

    qDebug() << "Mapping update finished. New proxy row count:" << proxyToSourceMap_.size();
}

// Slot implementation for source model reset
void EfficientLogFilterProxyModel::sourceModelReset()
{
    qDebug() << "Source model reset detected. Re-evaluating filter mapping.";
    if (!sourceModel_) return;

    // Rebuild the mapping based on the current filter results (currentSourceMatches_)
    // which should correspond to the last applied filter.
    // If the source model size changed drastically, currentSourceMatches_ might be invalid.
    // A safer approach is to assume no filter is active initially after a reset,
    // or re-trigger the filtering process if a filter *was* active.

    // For simplicity, let's reset to show all rows initially, matching setSourceModel logic.
    // A more complex approach could re-apply the last filter.

    // Prepare the state (show all rows) and let updateMapping handle the reset signals
    if (sourceModel_->rowCount() > 0) {
         currentSourceMatches_.resize(sourceModel_->rowCount());
         currentSourceMatches_.fill(true); // Assume all rows match initially
    } else {
         currentSourceMatches_.clear();
    }
    // Call updateMapping to rebuild the map and emit reset signals
    updateMapping(currentSourceMatches_);

    // Optional: If a filter was previously applied (lastAppliedFilterChainParams_ is not empty),
    // you might want to automatically re-apply it here by calling applyFilterChain again.
    // if (!lastAppliedFilterChainParams_.isEmpty()) {
    //     qDebug() << "Re-applying previous filter after source model reset.";
    //     // Need to copy params as applyFilterChain might modify them
    //     QList<FilterParams> paramsToReapply = lastAppliedFilterChainParams_;
    //     // applyFilterChain(paramsToReapply); // Be careful about triggering loops
    // }
}
