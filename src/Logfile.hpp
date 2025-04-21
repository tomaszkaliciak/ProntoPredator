#ifndef LOGFILE_HPP
#define LOGFILE_HPP

#include <memory>

#include <QFile>
#include <QMessageBox>
#include <QFile>
#include <QVector>
#include <QObject>

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
    // Constructor might need error handling indication (e.g., return bool or throw)
    Logfile(const QString& filename, QObject* parent = nullptr);
    ~Logfile(); // Need destructor to close file? QFile handles it if member.

    // Accessors
    const QString& getFileName() const;
    qint64 getLineCount() const;
    Line getLine(qint64 line_number) const; // Line numbers typically 1-based
    QVector<qint64> getLineIndexCopy() const; // Added getter for line index

    // Models (assuming they can be adapted)
    BookmarksModel* getBookmarksModel();
    GrepNode* getGrepHierarchy(); // Return raw pointer if ownership stays here

    // Public members (consider making private with accessors if needed)
    std::unique_ptr<GrepNode> grep_hierarchy_;
    std::unique_ptr<BookmarksModel> bookmarks_model_;

private:
    QString filename_;
    QFile file_; // Keep the file open
    QVector<qint64> line_index_; // Stores start position of each line

    bool initialize(); // Private helper for constructor logic
    // void buildIndex(); // Original declaration removed
    bool buildIndex(QProgressDialog& progress); // Added declaration for new version
    void connect_events();

    friend class serializer::Logfile;

protected slots:
    void grep_hierarchy_changed();
    void bookmarks_model_changed();

signals:
    void changed();
};

#endif // LOGFILE_HPP
