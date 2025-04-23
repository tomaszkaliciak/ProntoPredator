#ifndef BOOKMARKS_MODEL_HPP
#define BOOKMARKS_MODEL_HPP

#include <QString>
#include <QAbstractListModel>

#include "Bookmark.hpp"
class QJsonObject;

namespace serializer { class BookmarksModel; }

class BookmarksModel : public QAbstractListModel
{
Q_OBJECT
public:
    BookmarksModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void add_bookmark(const uint32_t line, const QString& icon, const QString& text);
    // Return const reference, make method const. Caller must ensure index validity.
    const Bookmark& get_bookmark(uint32_t index) const;

protected:
    QVector<Bookmark> bookmarks_;

    friend class serializer::BookmarksModel;

signals:
    void changed();
};
#endif // BOOKMARKS_MODEL_HPP
