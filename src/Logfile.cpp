#include "Logfile.hpp"
#include <memory>

#include <QFile>
#include <QMessageBox>
#include <QVector>
#include <QObject>

#include "BookmarksModel.hpp"
#include <QTextStream>
#include <QProgressDialog> // Added
#include <QApplication> // Added for qApp

#include "GrepNode.hpp"

Logfile::Logfile(const QString& filename, QObject* parent)
    : QObject(parent), // Pass parent to base class
      filename_(filename),
      file_(filename) // Initialize QFile member
{
    // Set cache size (e.g., max 10000 lines)
    line_cache_.setMaxCost(10000);

    // Initialization logic moved to initialize()
    if (!initialize())
    {
        // Handle initialization failure, maybe throw an exception
        // or set an error state that can be checked.
        // For now, just print a warning.
        qWarning("Failed to initialize Logfile: %s", qPrintable(filename_));
        // Ensure file is closed if initialization failed partially
        if (file_.isOpen()) {
            file_.close();
        }
    }

    // Initialize models after file is successfully opened and indexed
    if (file_.isOpen()) {
        // Use default constructor for root node (empty pattern = no filter)
        grep_hierarchy_ = std::make_unique<GrepNode>();
        // Optionally set a specific display name if needed, but pattern must be empty
        // grep_hierarchy_->setDisplayName("Base"); // Requires adding such a method to GrepNode if desired

        bookmarks_model_ = std::make_unique<BookmarksModel>(this); // Pass parent
        connect_events();
    } else {
        // Ensure models are null if init failed
        grep_hierarchy_.reset();
        bookmarks_model_.reset();
    }
}

// Destructor: QFile handles closing automatically when it goes out of scope.
Logfile::~Logfile() = default;


bool Logfile::initialize()
{
    if (!file_.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox msg;
        msg.setText("Unable to open file: " + file_.errorString());
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Critical);
        msg.exec();
        return false;
    }

    // Show progress during indexing
    QProgressDialog progress("Indexing file...", "Cancel", 0, 100, nullptr); // Use a generic parent or nullptr
    progress.setWindowModality(Qt::WindowModal);
    progress.setValue(0);
    progress.show();
    qApp->processEvents(); // Ensure dialog is shown

    if (!buildIndex(progress)) { // Pass progress dialog
        progress.cancel();
        return false; // Indexing failed or was cancelled
    }
    progress.setValue(100); // Mark as complete
    return true;
}


// Overloaded buildIndex to accept QProgressDialog
bool Logfile::buildIndex(QProgressDialog& progress)
{
    line_index_.clear();
    if (!file_.seek(0)) {
        qWarning("Failed to seek to beginning of file for indexing.");
        return false;
    }

    line_index_.append(0); // First line always starts at 0

    const qint64 bufferSize = 1024 * 1024; // 1MB buffer
    QByteArray buffer;
    qint64 currentPos = 0;
    qint64 fileSize = file_.size(); // Get total size for progress calculation

    while (!file_.atEnd()) {
        buffer = file_.read(bufferSize);
        if (buffer.isEmpty()) {
            // Error reading or EOF reached unexpectedly
            break;
        }

        const char* data = buffer.constData();
        int len = buffer.size();
        for (int i = 0; i < len; ++i) {
            if (data[i] == '\n') {
                // Found a newline, the next line starts after this character
                qint64 nextLinePos = currentPos + i + 1;
                // Avoid adding index if it's exactly at EOF (e.g., file ends with \n)
                if (nextLinePos < fileSize) {
                     line_index_.append(nextLinePos);
                }
            }
        }
        currentPos += len;

        // Update progress
        int percent = (fileSize > 0) ? static_cast<int>((currentPos * 100) / fileSize) : 100;
        progress.setValue(percent);
        qApp->processEvents(); // Keep UI responsive
        if (progress.wasCanceled()) {
            qInfo("File indexing cancelled by user.");
            line_index_.clear(); // Clear partial index
            return false;
        }
    }

    qInfo("Indexed %lld lines for file %s", line_index_.size(), qPrintable(filename_));
    return true; // Indexing completed successfully
}



void Logfile::connect_events()
{
    // Ensure models are valid before connecting
    if (bookmarks_model_) {
        QObject::connect(bookmarks_model_.get(), &BookmarksModel::changed,
                         this, &Logfile::bookmarks_model_changed);
    }
    if (grep_hierarchy_) {
        QObject::connect(grep_hierarchy_.get(), &GrepNode::changed,
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
    // Return the number of lines found during indexing
    return line_index_.size();
}

// Added getter implementation
QVector<qint64> Logfile::getLineIndexCopy() const
{
    return line_index_; // Return by value creates a copy
}

Line Logfile::getLine(qint64 line_number) const
{
    // Line numbers are 1-based for users, index is 0-based
    if (line_number < 1 || line_number > line_index_.size() || !file_.isOpen()) {
        // Return an empty line or handle error appropriately
        return {line_number, QString()};
    }

    // Check cache first
    if (QString* cachedLine = line_cache_.object(line_number)) {
        // Cache hit!
        return {line_number, *cachedLine};
    }

    // Cache miss, proceed to read from file
    qint64 start_pos = line_index_[line_number - 1];

    // Create a mutable copy of the file handle for seeking,
    // or manage seeking carefully if the main handle is used.
    // For simplicity here, assume seeking on the main handle is okay
    // if reading is sequential or infrequent. For high-performance UI,
    // might need a separate handle or more complex caching.
    // NOTE: Seeking is slow! This is the core of lazy loading.
    if (!const_cast<QFile&>(file_).seek(start_pos)) {
         qWarning("Failed to seek to position %lld for line %lld", start_pos, line_number);
         return {line_number, QString()}; // Seek failed
    }

    // Read the line
    QByteArray line_data = const_cast<QFile&>(file_).readLine();

    // Convert to QString, trim whitespace (as original code did)
    QString line_text = QString::fromUtf8(line_data).trimmed();

    // Add to cache before returning (cost is 1 per line)
    // Create a copy for the cache
    line_cache_.insert(line_number, new QString(line_text), 1);

    return {line_number, line_text};
}


// --- Model Accessors ---

BookmarksModel* Logfile::getBookmarksModel()
{
    return bookmarks_model_.get();
}

GrepNode* Logfile::getGrepHierarchy()
{
    return grep_hierarchy_.get();
}


// --- Slots ---

void Logfile::grep_hierarchy_changed()
{
    emit changed();
}
void Logfile::bookmarks_model_changed()
{
    emit changed();
}
