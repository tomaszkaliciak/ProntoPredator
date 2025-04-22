#include "LogFilterProxyModel.hpp"
#include "Logfile.hpp"
#include "GrepNode.hpp"

#include <QDebug>
#include <QApplication>
#include <QRegularExpression>
#include <QFile>
#include <QVector>
#include <QBitArray>
#include <QList>
#include <QThread> // Added for cancellation check
#include <QTimer>  // Added for delayed invalidateFilter
#include <numeric> // For std::popcount (requires C++20) or use alternative

// FilterParams and its operator== are now defined in FilterParams.hpp

LogFilterProxyModel::LogFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    connect(&filterWatcher_, &QFutureWatcher<QBitArray>::finished,
            this, &LogFilterProxyModel::handleFilterFinished);
}

void LogFilterProxyModel::setSourceLogfile(Logfile* logfile)
{
    sourceLogfile_ = logfile;
    lastAppliedFilterChainParams_.clear(); // Clear last applied on source change
    applyFilterChain({});
}

bool LogFilterProxyModel::isFiltering() const
{
    return isFiltering_;
}

void LogFilterProxyModel::cancelFiltering()
{
    if (isFiltering_) {
        qDebug() << "Attempting to cancel filtering...";
        filterWatcher_.cancel();
    }
}

// New method to apply filter chain
void LogFilterProxyModel::applyFilterChain(const QList<GrepNode*>& chain)
{
    // Build the new parameter list
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
                 newParamsList.clear();
                 break;
             }
        }
        newParamsList.append(params);
    } // End of for loop building newParamsList

    // Check if the new chain is the same as the last applied one,
    // but *always* proceed if the new chain is empty (the "Base" filter).
    bool chainsAreEqual = (newParamsList.size() == lastAppliedFilterChainParams_.size());
    if (chainsAreEqual && !newParamsList.isEmpty()) { // Add check for non-empty list
        for (int i = 0; i < newParamsList.size(); ++i) {
            if (!(newParamsList.at(i) == lastAppliedFilterChainParams_.at(i))) {
                chainsAreEqual = false;
                break;
            }
        }
    }

    // Skip filtering only if filtering is not already running AND
    // the new chain is identical to the last applied one AND
    // the new chain is not empty (we always want to run the 'Base' filter initially).
    if (!isFiltering_ && chainsAreEqual && !newParamsList.isEmpty()) {
        qDebug() << "Filter chain hasn't changed, skipping redundant filtering.";
        return;
    }
    // If chainsAreEqual is true but newParamsList IS empty, we proceed to run the filter.

    // Store the new chain as the current one and start filtering
    currentFilterChainParams_ = newParamsList;
    startAsyncFiltering();
} // End of applyFilterChain


// Updated filterAcceptsRow
bool LogFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex& /*source_parent*/) const
{
    if (source_row < 0 || source_row >= matchingSourceRows_.size()) {
        return false;
    }
    // Accept row if filter chain is empty OR if the corresponding bit is set
    return lastAppliedFilterChainParams_.isEmpty() || matchingSourceRows_.testBit(source_row);
}

void LogFilterProxyModel::startAsyncFiltering()
{
    if (isFiltering_) {
        qDebug() << "Filtering already in progress.";
        return;
    }
    if (!sourceLogfile_ || !sourceModel()) {
        qWarning("Cannot filter: source Logfile or sourceModel not set.");
        return;
    }

    isFiltering_ = true;
    // Don't clear matchingSourceRows_ here, keep old results visible until new ones are ready
    emit filteringStarted();
    // Don't invalidate here, wait for results

    // --- Prepare data copies for the background task ---
    QString filenameCopy = sourceLogfile_->getFileName();
    QVector<qint64> lineIndexCopy = sourceLogfile_->getLineIndexCopy();
    QList<FilterParams> filterChainParamsCopy = currentFilterChainParams_; // Use current chain

    // --- Run the filtering task in a separate thread using a lambda ---
    auto filterLambda = [=]() -> QBitArray {
        return LogFilterProxyModel::performFilteringTask(
            filenameCopy, lineIndexCopy, filterChainParamsCopy
        );
    };
    QFuture<QBitArray> future = QtConcurrent::run(filterLambda);
    filterWatcher_.setFuture(future);
}

void LogFilterProxyModel::handleFilterFinished()
{
     bool wasCancelled = filterWatcher_.isCanceled();
     int matchCount = 0;
     if (!wasCancelled) {
         matchingSourceRows_ = filterWatcher_.result(); // Update results
         lastAppliedFilterChainParams_ = currentFilterChainParams_; // Store the successfully applied filter
         matchCount = matchingSourceRows_.count(true); // Use QBitArray::count()
         qDebug() << "Filtering finished. Matches found:" << matchCount;
     } else {
         qDebug() << "Filtering was cancelled.";
         // Use count on the existing (stale) matchingSourceRows_ if cancelled
         matchCount = matchingSourceRows_.count(true);
     }

    isFiltering_ = false;
    emit filteringFinished(matchCount); // Emit signal first to update UI (e.g., close dialog)

    // Delay the potentially blocking invalidateFilter call slightly
    // Note: This helps ensure the filteringFinished signal is processed, but the
    // invalidateFilter call itself can still block the main thread for a noticeable
    // time if the number of rows changing visibility is very large.
    QTimer::singleShot(0, this, [this]() {
        invalidateFilter(); // Update the view with new (or old if cancelled) results
    });
}


// --- Static method executed in the background thread ---
// Reworked logic to perform *chained* filtering correctly
QBitArray LogFilterProxyModel::performFilteringTask(
    QString filename, QVector<qint64> lineIndex, QList<FilterParams> filterChainParams)
{
    qint64 lineCount = lineIndex.size();
    // Start with all lines matching (indices 0 to lineCount-1)
    QBitArray currentMatches(lineCount, true);
    QFile localFile(filename);

    if (!localFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Background task: Failed to open file %s", qPrintable(filename));
        return QBitArray(lineCount, false); // Return empty matches on error
    }

    // Apply each filter step sequentially
    for (const FilterParams& params : filterChainParams) {
        // If the pattern for this step is empty, skip it (this represents the "Base" level)
        if (params.pattern.isEmpty()) continue;

        QBitArray stepMatches(lineCount, false); // Results for *this* step
        bool stepRegexIsValid = !params.isRegex || params.regex.isValid();

        if (params.isRegex && !stepRegexIsValid) {
             qWarning("Background task: Invalid regex '%s' encountered in chain.", qPrintable(params.pattern));
             currentMatches.fill(false); // Invalidate all previous matches
             break; // Stop processing chain
        }

        // Iterate only through lines that matched the *previous* step
        for (qint64 i = 0; i < lineCount; ++i) {
            // Use int for bit array index
            int rowIndex = static_cast<int>(i);
            if (!currentMatches.testBit(rowIndex)) {
                continue; // Skip if didn't pass previous filters
            }

            // --- Cancellation Check ---
            if (QThread::currentThread()->isInterruptionRequested()) {
                qDebug() << "Background filtering task interrupted.";
                return QBitArray(); // Return empty array on cancellation
            }
            // --- End Cancellation Check ---

            // Get line data
            qint64 start_pos = lineIndex[i];
            if (!localFile.seek(start_pos)) {
                 qWarning("Background task: Failed to seek to pos %lld for line %lld", start_pos, i + 1);
                 continue; // Skip line on seek error
            }
            QByteArray lineData = localFile.readLine();
            QString lineText = QString::fromUtf8(lineData).trimmed();

            // Apply the *current step's* filter
            bool matchFound = false;
            if (params.isRegex) { // Regex validity checked above
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

    return currentMatches; // Return the final set of matching lines
}
