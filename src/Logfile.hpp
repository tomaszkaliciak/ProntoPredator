#ifndef LOGFILE_HPP
#define LOGFILE_HPP

#include <memory>

#include <QFile>
#include <QMessageBox>
#include <QFile>
#include <QVector>
#include <QObject>
#include <QCache> // Added for line caching
#include <QtConcurrent/QtConcurrent> // Added for background tasks
#include <QFutureWatcher> // Added to monitor background tasks

#include "BookmarksModel.hpp"
#include "GrepNode.hpp"

// Forward declarations
namespace serializer { class Logfile; }
class QProgressDialog;

struct Line
{
    qint64 number; // Use qint64 for potentially huge line counts
    QString text;
};

// No longer storing all lines in memory
// using Lines = QVector<Line>;

class Logfile : public QObject
{
    Q_OBJECT
public:
    // Constructor now takes parent, filename is set via initialize
    explicit Logfile(QObject* parent = nullptr);
    ~Logfile(); // Destructor might need to wait for pending operations

    // Asynchronous initialization
    void initialize(const QString& filename);
    bool isInitialized() const; // Check if indexing is complete

    // Accessors
    const QString& getFileName() const;
    qint64 getLineCount() const; // Returns current count, might be 0 during indexing
    Line getLine(qint64 line_number) const; // Line numbers typically 1-based
    QVector<qint64> getLineIndexCopy() const; // Added getter for line index

    // Models
    BookmarksModel* getBookmarksModel();
    GrepNode* getGrepHierarchy(); // Return raw pointer if ownership stays here

    // Public members (consider making private with accessors if needed)
    std::unique_ptr<GrepNode> grep_hierarchy_;
    std::unique_ptr<BookmarksModel> bookmarks_model_;

private:
    QString filename_;
    QFile file_; // Keep the file open
    QVector<qint64> line_index_; // Stores start position of each line
    mutable QCache<qint64, QString> line_cache_; // Added cache (line number -> line text)
    bool initialized_ = false; // Flag to track completion
    QFutureWatcher<bool> index_watcher_; // To monitor the background indexing task
    QFutureWatcher<void> cache_watcher_; // To monitor background cache population tasks

    // bool initialize(); // Original private helper removed
    bool buildIndexInternal(); // Renamed internal blocking index builder
    void connect_events();
    void populateCacheInBackground(qint64 startLine, int count); // Background cache population task

    friend class serializer::Logfile;

public slots: // Make this public so LogViewer/CustomLogView can trigger it
    // Slot to request background population of the cache around a center line
    void requestCachePopulation(qint64 centerLine, int contextLines);

private slots:
    void handleIndexFinished(); // Slot to react when background indexing is done
    // Optional: Add a slot to handle cache watcher finished if needed

protected slots:
    void grep_hierarchy_changed();
    void bookmarks_model_changed();

signals:
    void changed();
    void indexingProgress(int percent); // Signal for progress updates
    void indexingFinished(bool success); // Signal when indexing is complete (success/failure)
    void initializedChanged(); // Signal when initialization state changes
};

#endif // LOGFILE_HPP
