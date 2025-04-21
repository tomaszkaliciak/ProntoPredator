#include "GrepNode.hpp"

#include <memory>
#include <string>
#include <vector>
#include <algorithm> // Required for std::find

#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

GrepNode::GrepNode(
    const std::string& value,
    const bool& is_regex,
    const bool& is_case_insensitive,
    const bool& is_inverted)
: pattern_{value},
    is_regex_{is_regex},
    is_case_insensitive_{is_case_insensitive},
    is_inverted_{is_inverted}
{}

GrepNode::~GrepNode()
{
    // Disconnect signals and delete children to prevent memory leaks
    // Note: This assumes GrepNode owns its children.
    for (auto child : children_)
    {
        if (child) {
            // Disconnect manually before deleting to be safe, though Qt might handle it
            QObject::disconnect(child, &GrepNode::changed,
                             nullptr, nullptr); // Disconnect all slots from child
            delete child;
        }
    }
    children_.clear(); // Clear the vector after deleting pointers
}

std::string GrepNode::getPattern() const
{
    return pattern_;
}

bool GrepNode::isRegEx() const
{
    return is_regex_;
}

bool GrepNode::isCaseInsensitive() const
{
    return is_case_insensitive_;
}

bool GrepNode::isInverted() const
{
    return is_inverted_;
}

// --- Setters ---
void GrepNode::setPattern(const std::string& pattern) {
    if (pattern_ != pattern) {
        pattern_ = pattern;
        emit changed(); // Reverted
    }
}

void GrepNode::setIsRegEx(bool isRegEx) {
    if (is_regex_ != isRegEx) {
        is_regex_ = isRegEx;
        emit changed(); // Reverted
    }
}

void GrepNode::setIsCaseInsensitive(bool isCaseInsensitive) {
    if (is_case_insensitive_ != isCaseInsensitive) {
        is_case_insensitive_ = isCaseInsensitive;
        emit changed(); // Reverted
    }
}

void GrepNode::setIsInverted(bool isInverted) {
    if (is_inverted_ != isInverted) {
        is_inverted_ = isInverted;
        emit changed(); // Reverted
    }
}

// Corrected addChild
void GrepNode::addChild(GrepNode* node)
{
    if (!node) return;
    node->parent_ = this; // Set parent pointer
    children_.push_back(node);
    // Re-added connect for child_changed (though slot is currently empty)
    // QObject::connect(node, &GrepNode::changed, this, &GrepNode::child_changed);
    emit changed(); // Reverted: Signal that this node's children have changed
}

// Corrected removeChild
void GrepNode::removeChild(GrepNode* node)
{
    if (!node) return;
    auto it = std::find(children_.begin(), children_.end(), node);
    if (it == children_.end()) return;

    // Disconnect before deleting
    // QObject::disconnect(node, &GrepNode::changed, this, &GrepNode::child_changed);
    children_.erase(it); // Remove pointer from vector
    // node->parent_ = nullptr; // Clear parent pointer before deleting
    delete node; // Delete the child node object
    emit changed(); // Reverted: Signal that this node's children have changed
}

std::vector<GrepNode*> GrepNode::getChildren() const
{
    // Return copy or const reference depending on usage needs
    // Returning a copy for now to match previous behavior
    std::vector<GrepNode*> result;
    result.reserve(children_.size());
    for (const auto& child : children_) {
        result.push_back(child);
    }
    return result;
}

GrepNode* GrepNode::getParent() const
{
    return parent_;
}

// Re-added child_changed slot implementation (currently does nothing)
// void GrepNode::child_changed()
// {
//   // Propagate the changed signal upwards if needed, but model reset handles it now
//   // emit changed();
// }
