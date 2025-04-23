#include "SerializerBookmarksModel.hpp"

#include "../BookmarksModel.hpp"
#include "SerializerBookmark.hpp"

#include <QJsonArray>
#include <QJsonObject>

namespace serializer
{
void BookmarksModel::serialize(const ::BookmarksModel& bmodel, QJsonObject &json)
{
    QJsonArray array;
    for (const auto& bookmark : bmodel.bookmarks_)
    {
        QJsonObject jsonBookmark;
        Bookmark::serialize(bookmark, jsonBookmark);
        array.append(jsonBookmark);
    }
    json["bookmarks"] = array;
}

void BookmarksModel::deserialize(::BookmarksModel& bmodel, const QJsonObject &json)
{
    QJsonArray bookmarks = json["bookmarks"].toArray();

    // Clear existing bookmarks before deserializing
    bmodel.bookmarks_.clear();
    // bmodel.bookmarks_.reserve(bookmarks.size()); // Reserve might require default constructor

    for (const QJsonValue child : bookmarks)
    {
        // We need to extract data first, then construct the Bookmark
        QJsonObject jsonObj = child.toObject();
        uint32_t line = static_cast<uint32_t>(jsonObj["line"].toInt()); // Add necessary includes if needed
        QString text = jsonObj["text"].toString();
        QString icon = jsonObj["icon"].toString();

        // Construct the bookmark directly in append (C++11 onwards supports emplacement)
        bmodel.bookmarks_.append(::Bookmark(line, text, icon));
    }
     // TODO: The BookmarksModel should probably emit layoutChanged or modelReset here
     // after modifying the underlying data directly. Consider modelReset for simplicity.
     // bmodel.beginResetModel(); bmodel.endResetModel();
}

}  // namespace serializer

