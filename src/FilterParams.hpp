#ifndef FILTERPARAMS_HPP
#define FILTERPARAMS_HPP

#include <QString>
#include <QRegularExpression>
#include <QtCore/Qt> // For Qt::CaseSensitivity

// Structure to hold filter parameters
struct FilterParams {
    QString pattern;
    bool isRegex = false;
    Qt::CaseSensitivity cs = Qt::CaseSensitive;
    bool inverted = false;
    QRegularExpression regex; // Pre-compiled regex if isRegex is true

    // Need an equality operator for comparing chains
    friend bool operator==(const FilterParams& lhs, const FilterParams& rhs);
};

// Define the equality operator implementation here or in a .cpp file
// For simplicity, let's define it here as it's small.
inline bool operator==(const FilterParams& lhs, const FilterParams& rhs) {
    return lhs.pattern == rhs.pattern &&
           lhs.isRegex == rhs.isRegex &&
           lhs.cs == rhs.cs &&
           lhs.inverted == rhs.inverted &&
           // Explicitly compare relevant QRegularExpression properties if needed
           (!lhs.isRegex || (lhs.regex.pattern() == rhs.regex.pattern() && lhs.regex.patternOptions() == rhs.regex.patternOptions()));
}


#endif // FILTERPARAMS_HPP
