#ifndef BOOKMARK_HPP
#define BOOKMARK_HPP

#include <cstdint> // For uint32_t
#include <QString>
// Removed QPixmap include as it's not used here

namespace serializer { class Bookmark; } // Forward declaration needed for friend

class Bookmark
{
public:
    // Constructor now initializes private members
    Bookmark(uint32_t line_number, const QString &text, const QString &icon);
    ~Bookmark() = default; // Add default destructor

    // Provide public getters for private members
    uint32_t getLineNumber() const { return line_number_; }
    const QString& getText() const { return text_; }
    const QString& getIcon() const { return icon_; }

    // Comparison operator
    bool operator<(const Bookmark& other) const; // Renamed parameter 'b' to 'other'

private:
    // Grant friendship for serialization
    friend class serializer::Bookmark;

    uint32_t line_number_; // Made private
    QString text_;         // Made private, removed redundant initializer
    QString icon_;         // Made private, removed redundant initializer
};

#endif // BOOKMARK_HPP
