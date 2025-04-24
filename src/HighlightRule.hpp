#ifndef HIGHLIGHT_RULE_HPP
#define HIGHLIGHT_RULE_HPP

#include <QString>
#include <QColor>

struct HighlightRule {
    QString substring;
    QColor color;
    bool isEnabled = true; // Added for potential future use

    // Default constructor for container compatibility
    HighlightRule() = default;

    // Constructor for convenience
    HighlightRule(const QString &sub, const QColor &col, bool enabled = true)
        : substring(sub), color(col), isEnabled(enabled) {}

    // Equality operator for comparisons (e.g., in lists)
    bool operator==(const HighlightRule &other) const {
        return substring == other.substring && color == other.color && isEnabled == other.isEnabled;
    }
};

#endif // HIGHLIGHT_RULE_HPP