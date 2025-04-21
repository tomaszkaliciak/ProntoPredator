#ifndef GREP_NODE_HPP
#define GREP_NODE_HPP

#include <memory>
#include <string>
#include <vector>

#include <QObject>

namespace serializer { class GrepNode; }

class GrepNode : public QObject
{
Q_OBJECT
public:
    GrepNode(
        const std::string& value,
        const bool& is_regex = false,
        const bool& is_case_insensitive = false,
        const bool& is_inverted = false);

    GrepNode() = default;

    virtual ~GrepNode();

    std::string getPattern() const;

    bool isRegEx() const;

    bool isCaseInsensitive() const;

    bool isInverted() const;

    // Setters
    void setPattern(const std::string& pattern);
    void setIsRegEx(bool isRegEx);
    void setIsCaseInsensitive(bool isCaseInsensitive);
    void setIsInverted(bool isInverted);

    void addChild(GrepNode* node);

    void removeChild(GrepNode* node);

    std::vector<GrepNode*> getChildren() const;
    GrepNode* getParent() const; // Added getter for parent

protected:
    GrepNode* parent_ = nullptr; // Added parent pointer
    std::vector<GrepNode*> children_{};
    std::string pattern_{};
    bool is_regex_{};
    bool is_case_insensitive_{};
    bool is_inverted_{};

    friend class serializer::GrepNode;

// Removed private slots section as child_changed is no longer used

signals:
    // Signal emitted when this node's data changes (or children change)
    void changed();
};

#endif // GREP_NODE_HPP
