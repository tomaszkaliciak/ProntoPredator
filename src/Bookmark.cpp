#include "Bookmark.hpp"
#include <cstdint> // Added for uint32_t

// Use member initializer list
Bookmark::Bookmark(uint32_t line_number, const QString &text, const QString &icon)
    : line_number_(line_number)
    , text_(text)
    , icon_(icon)
{
    // Constructor body is now empty
}

// Rename parameter 'b' to 'other'
bool Bookmark::operator<(const Bookmark& other) const
{
    // Access private member via getter or directly if needed (friendship allows direct access if desired)
    // Using direct access here for simplicity as it's the class's own operator.
    return line_number_ < other.line_number_;
}
