#include "Logfile.hpp"
#include <memory>
#include <atomic> // For cancellation flag

#include <QFile>
#include <QMessageBox>
#include <QVector>
#include <QObject>
#include <QTextStream> // Keep for potential future use, but not needed for indexing
// #include <QProgressDialog> // Removed
// #include <QApplication> // Removed (use signals instead of processEvents)

#include "BookmarksModel.hpp"
#include "GrepNode.hpp"

// Constructor: Initialize members, connect watcher
Logfile::Logfile(QObject* parent)
    : QObject(parent)
{
    // Set cache size (e.g., max 10000 lines)
    line_cache_.setMaxCost(10000);

    // Connect the watcher's finished signal to our handler slot
    connect(&index_watcher_, &QFutureWatcher<bool>::finished,
            this, &Logfile::handleIndexFinished);
    // Optional: Connect progress signals if needed
    // connect(&index_watcher_, &QFutureWatcher<bool>::progressValueChanged, ...);
}

// Destructor: Ensure file is closed. QFutureWatcher cleanup is automatic.
Logfile::~Logfile()
{
    // Request cancellation if indexing is still running
    // index_watcher_.cancel(); // Might be needed depending on ownership/threading model
    // index_watcher_.waitForFinished(); // Ensure thread completes before destruction if necessary

    if (file_.isOpen()) {
        file_.close();
    }
}

// Asynchronous initialization function
void Logfile::initialize(const QString& filename)
{
    if (index_watcher_.isRunning()) {
        qWarning("Initialization already in progress.");
        return;
    }

    filename_ = filename;
    initialized_ = false; // Reset initialization state
    line_index_.clear(); // Clear previous index
    line_cache_.clear(); // Clear cache
    emit initializedChanged(); // Notify state change

    file_.setFileName(filename_);
    if (!file_.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // Show user-facing error message
        QMessageBox::warning(nullptr, // No parent window available here easily
                             tr("Error Opening File"),
                             tr("Unable to open file '%1':\n%2").arg(filename_).arg(file_.errorString()));
        qWarning("Unable to open file '%s': %s", qPrintable(filename_), qPrintable(file_.errorString())); // Keep log warning
        emit indexingFinished(false); // Signal failure immediately
        emit initializedChanged(); // Ensure state is updated
        return;
    }

    // Run buildIndexInternal in a separate thread
    // Correct order: (pointerToObject, &ClassName::memberFunction)
    QFuture<bool> future = QtConcurrent::run(this, &Logfile::buildIndexInternal);
    index_watcher_.setFuture(future);
}

bool Logfile::isInitialized() const
{
    return initialized_;
}


// Internal blocking function to build the index (runs in background thread)
bool Logfile::buildIndexInternal()
{
    // Note: This function runs in a background thread.
    // Do NOT interact with GUI elements directly. Use signals to communicate.
    // QFile operations should be safe if the QFile object is not accessed
    // concurrently from other threads (which it shouldn't be here).

    line_index_.clear(); // Ensure it's clear before starting
    if (!file_.seek(0)) {
        qWarning("Failed to seek to beginning of file for indexing.");
        return false; // Return failure
    }

    line_index_.append(0); // First line always starts at 0

    const qint64 bufferSize = 1024 * 1024; // 1MB buffer
    QByteArray buffer;
    qint64 currentPos = 0;
    qint64 fileSize = file_.size(); // Get total size for progress calculation
    int lastPercent = -1; // Track last emitted percentage

    while (!file_.atEnd()) {
        // Optional: Check for cancellation request if needed
        // if (index_watcher_.isCanceled()) { // Check if cancellation was requested
        //     qInfo("File indexing cancelled.");
        //     line_index_.clear(); // Clear partial index
        //     return false; // Indicate failure due to cancellation
        // }

        buffer = file_.read(bufferSize);
        if (buffer.isEmpty() && !file_.atEnd()) {
            // Error reading or EOF reached unexpectedly
             qWarning("Error reading file during indexing.");
             line_index_.clear();
             return false;
        }
        if (buffer.isEmpty()) break; // Normal EOF

        const char* data = buffer.constData();
        int len = buffer.size();
        for (int i = 0; i < len; ++i) {
            if (data[i] == '\n') {
                qint64 nextLinePos = currentPos + i + 1;
                if (nextLinePos < fileSize) {
                     line_index_.append(nextLinePos);
                }
            }
        }
        currentPos += len;

        // Update progress (emit signal only when percentage changes)
        int percent = (fileSize > 0) ? static_cast<int>((currentPos * 100) / fileSize) : 100;
        if (percent != lastPercent) {
            emit indexingProgress(percent); // Signal progress
            lastPercent = percent;
        }
    }

    // Ensure final progress is 100% if loop finished normally
    if (lastPercent != 100) {
        emit indexingProgress(100);
    }

    qInfo("Indexed %lld lines for file %s", line_index_.size(), qPrintable(filename_));
    return true; // Indexing completed successfully
}

// Slot called when the background indexing task finishes
void Logfile::handleIndexFinished()
{
    bool success = index_watcher_.result(); // Get the return value from buildIndexInternal

    if (success) {
        qInfo("Background indexing finished successfully for %s.", qPrintable(filename_));
        // Initialize models now that indexing is complete
        grep_hierarchy_ = std::make_unique<GrepNode>();
        bookmarks_model_ = std::make_unique<BookmarksModel>(this); // Pass parent
        connect_events(); // Connect signals from models
    } else {
        qWarning("Background indexing failed or was cancelled for %s.", qPrintable(filename_));
        // Ensure models are null if indexing failed
        grep_hierarchy_.reset();
        bookmarks_model_.reset();
        // Ensure file is closed if it was opened but indexing failed
        if (file_.isOpen()) {
            file_.close();
        }
    }

    initialized_ = success; // Update initialization state
    emit indexingFinished(success); // Signal completion status
    emit initializedChanged(); // Signal state change
}


void Logfile::connect_events()
{
    // Ensure models are valid before connecting (should be if called from handleIndexFinished on success)
    if (bookmarks_model_) {
        connect(bookmarks_model_.get(), &BookmarksModel::changed,
                this, &Logfile::bookmarks_model_changed);
    }
    if (grep_hierarchy_) {
        connect(grep_hierarchy_.get(), &GrepNode::changed,
                this, &Logfile::grep_hierarchy_changed);
    }
}

// --- Accessors ---

const QString& Logfile::getFileName() const
{
    return filename_;
}

qint64 Logfile::getLineCount() const
{
    // Return the number of lines found during indexing.
    // This will be 0 until indexing is complete.
    return initialized_ ? line_index_.size() : 0;
}

// Added getter implementation
QVector<qint64> Logfile::getLineIndexCopy() const
{
    // Return copy only if initialized
    return initialized_ ? line_index_ : QVector<qint64>();
}

Line Logfile::getLine(qint64 line_number) const
{
    // Ensure initialized and line number is valid
    if (!initialized_ || line_number < 1 || line_number > line_index_.size() || !file_.isOpen()) {
        return {line_number, QString()}; // Return empty line
    }

    // Check cache first (thread-safe access to QCache is handled internally by Qt)
    if (QString* cachedLine = line_cache_.object(line_number)) {
        return {line_number, *cachedLine}; // Cache hit!
    }

    // Cache miss, proceed to read from file
    // IMPORTANT: File I/O (seek, readLine) should ideally happen in a separate
    // thread if this getLine() can be called frequently from the GUI thread
    // causing stutters. For now, we keep it here but acknowledge this limitation.
    // The const_cast is still needed as QFile::seek/readLine are not const.
    qint64 start_pos = line_index_[line_number - 1];

    // --- Read-Ahead Caching ---
    const int READ_AHEAD_COUNT = 50; // How many lines to read ahead
    qint64 current_read_pos = start_pos;
    if (!const_cast<QFile&>(file_).seek(current_read_pos)) {
         qWarning("Failed to seek to position %lld for line %lld", current_read_pos, line_number);
         return {line_number, QString()}; // Seek failed
    }

    QString line_text_to_return; // Store the specifically requested line

    for (int i = 0; i < READ_AHEAD_COUNT; ++i) {
        qint64 current_line_num = line_number + i;
        // Stop if we go past the known number of lines
        if (current_line_num > line_index_.size()) {
            break;
        }
        // Stop if we are trying to read a line already in cache (implies we caught up)
        if (i > 0 && line_cache_.contains(current_line_num)) {
             break;
        }

        QByteArray line_data = const_cast<QFile&>(file_).readLine();
        if (line_data.isNull()) { // Check for read error or EOF
             break;
        }

        QString line_text = QString::fromUtf8(line_data).trimmed();

        // Store the requested line separately
        if (i == 0) {
            line_text_to_return = line_text;
        }

        // Add to cache only if not already there (QCache::insert replaces)
        // Use cost 1 per line. QCache takes ownership of the new QString.
        if (!line_cache_.contains(current_line_num)) {
             line_cache_.insert(current_line_num, new QString(line_text), 1);
        }

        // Update position for next readLine (readLine advances the file pointer)
        // current_read_pos = const_cast<QFile&>(file_).pos(); // Not needed as readLine advances pos
    }

    // Ensure the originally requested line was actually read and cached
    // (It should have been if i == 0 loop ran)
    if (line_text_to_return.isNull()) {
         // This might happen if the file ends exactly at the start_pos
         // or if readLine immediately returned null. Re-check cache just in case.
         if (QString* cachedLine = line_cache_.object(line_number)) {
             return {line_number, *cachedLine};
         } else {
              qWarning("Failed to read line %lld even after attempt.", line_number);
              return {line_number, QString()}; // Read failed
         }
    }


    return {line_number, line_text_to_return};
}


// --- Model Accessors ---

BookmarksModel* Logfile::getBookmarksModel()
{
    // Return model only if initialized
    return initialized_ ? bookmarks_model_.get() : nullptr;
}

GrepNode* Logfile::getGrepHierarchy()
{
    // Return hierarchy only if initialized
    return initialized_ ? grep_hierarchy_.get() : nullptr;
}


// --- Slots ---

void Logfile::grep_hierarchy_changed()
{
    if (initialized_) { // Only emit if ready
        emit changed();
    }
}
void Logfile::bookmarks_model_changed()
{
    if (initialized_) { // Only emit if ready
        emit changed();
    }
}
