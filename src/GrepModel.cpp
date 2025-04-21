#include "GrepModel.hpp"
#include "GrepNode.hpp" // Include full definition

#include <QDebug>
#include <vector>
#include <stdexcept> // For exceptions if needed
#include <stdexcept> // For exceptions if needed
// Removed #include <functional>

// Removed helper function declaration

GrepModel::GrepModel(GrepNode* rootNode, QObject* parent)
    : QAbstractItemModel(parent), rootNode_(rootNode)
{
    if (!rootNode_) {
        qWarning("GrepModel created with null root node!");
        // Create a dummy root if necessary? Or handle error.
    }
    // TODO: Re-add signal connections if needed, but using reset model for now.
    // Original approach might have involved connecting rootNode_'s changed() signal here.
}

GrepModel::~GrepModel()
{
    // Model doesn't own the nodes
}

// --- Helper ---
GrepNode* GrepModel::getNode(const QModelIndex &index) const
{
    if (!index.isValid()) {
        // Invalid index usually refers to the invisible root item's parent
        // It should NOT return the actual data rootNode_ here.
        return nullptr;
    }
    // Retrieve the node stored when the index was created
    GrepNode* node = static_cast<GrepNode*>(index.internalPointer());
    // If internal pointer is null, it might be the top-level "Base" item's index
    // which should correspond to rootNode_. Check this logic carefully.
    // Let's assume createIndex always stores a valid node pointer for valid indices.
    return node;
}

// --- ReadOnly Model Implementation ---

QVariant GrepModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0)
        return tr("Filter");
    return QVariant();
}

QModelIndex GrepModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || !rootNode_) return QModelIndex(); // Only one column, need root

    GrepNode* parentNode = getNode(parent); // Get node associated with parent index

    if (!parent.isValid()) {
        // Parent is the invisible root item. We should return the index for the "Base" node.
        if (row == 0) {
            // Create index for the actual rootNode_ (Base)
            return createIndex(row, column, rootNode_);
        } else {
            return QModelIndex(); // Only one top-level item
        }
    } else {
        // Parent is a valid index (either "Base" or a filter node)
        if (!parentNode) return QModelIndex(); // Should have a node for a valid index

        const auto& children = parentNode->getChildren();
        if (row < 0 || static_cast<size_t>(row) >= children.size())
            return QModelIndex();

        GrepNode* childNode = children[row];
        if (childNode)
            // Create index for the child filter node
            return createIndex(row, column, childNode);
        else
            return QModelIndex();
    }
}

// --- Corrected parent() implementation ---
QModelIndex GrepModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    GrepNode* childNode = getNode(index);
    if (!childNode) return QModelIndex(); // Should not happen

    GrepNode* parentNode = childNode->getParent();

    // If the parent is the root node, return the index for the root node
    if (!parentNode || parentNode == rootNode_)
        // The parent is the "Base" node, which is the model's top-level item
        // Its index is (row 0, col 0, invalid parent)
        // Check if the child is actually the rootNode_ itself
         if (childNode == rootNode_) {
             return QModelIndex(); // The "Base" node has no parent in the view
         } else {
             // Create the index for the rootNode_ ("Base")
             return createIndex(0, 0, rootNode_);
         }


    // Parent is not the root node, find its row within the grandparent
    GrepNode* grandparentNode = parentNode->getParent();
    if (!grandparentNode) {
         // Should not happen if parentNode is not rootNode_
         qWarning("GrepModel::parent: Node has non-root parent but parent has no parent?");
         return QModelIndex();
    }

    const auto& siblings = grandparentNode->getChildren();
    int parentRow = -1;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i] == parentNode) {
            parentRow = static_cast<int>(i);
            break;
        }
    }

    if (parentRow == -1) {
        qWarning("GrepModel::parent: Could not find parent node in grandparent's children.");
        return QModelIndex();
    }

    // Create the index for the parent
    return createIndex(parentRow, 0, parentNode);
}


int GrepModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;
    if (!rootNode_) return 0;

    if (!parent.isValid()) {
        // Parent is invisible root, return 1 for the "Base" node
        return 1;
    } else {
        // Parent is a visible item, return number of its children
        GrepNode* parentNode = getNode(parent);
        return parentNode ? static_cast<int>(parentNode->getChildren().size()) : 0;
    }
}

int GrepModel::columnCount(const QModelIndex &/*parent*/) const
{
    return 1;
}

QVariant GrepModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0)
        return QVariant();

    GrepNode* node = getNode(index);
    if (!node) return QVariant();

    if (role == Qt::DisplayRole) {
        // Special case for the root node displayed as "Base"
        if (node == rootNode_) return QString("Base");

        // Generate display name for filter nodes
        QString displayName = QString::fromStdString(node->getPattern());
        if (displayName.isEmpty()) displayName = "[Empty Filter]";
        else if (displayName.length() > 25) displayName = displayName.left(22) + "...";

        displayName += " (";
        displayName += node->isRegEx() ? "R" : "r";
        displayName += node->isCaseInsensitive() ? "C" : "c";
        displayName += node->isInverted() ? "I" : "i";
        displayName += ")";
        return displayName;
    }
    else if (role == Qt::UserRole) {
         // Store the GrepNode pointer for easy access
         return QVariant::fromValue(static_cast<void*>(node));
    }

    return QVariant();
}

// --- Add/Remove ---

// Helper to find the QModelIndex for a given node (including root)
// Optimized: Avoids recursive calls for parent index.
QModelIndex GrepModel::findIndexForNode(GrepNode* node) const
{
    if (!node) {
        return QModelIndex();
    }

    // Handle the root node ("Base")
    if (node == rootNode_) {
        // The root node is the first (and only) item under the invisible root.
        return createIndex(0, 0, node);
    }

    // Handle non-root nodes
    GrepNode* parentNode = node->getParent();
    if (!parentNode) {
        // Non-root node must have a parent.
        qWarning("GrepModel::findIndexForNode: Non-root node has no parent.");
        return QModelIndex();
    }

    // Find the row of the node within its parent's children
    const auto& siblings = parentNode->getChildren();
    int row = -1;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i] == node) {
            row = static_cast<int>(i);
            break;
        }
    }

    if (row == -1) {
        qWarning("GrepModel::findIndexForNode: Could not find node in parent's children.");
        return QModelIndex();
    }

    // Create the index directly using the row, column 0, and the node pointer.
    // The parent's QModelIndex is not needed here, only the node's pointer.
    return createIndex(row, 0, node);
}


void GrepModel::addGrepNode(GrepNode* parentNode, GrepNode* newNode)
{
    if (!parentNode || !newNode) return;

    // Get the correct parent index using the helper
    QModelIndex parentIndex = findIndexForNode(parentNode);
    // Note: parentIndex might be invalid if parentNode is root, which is correct for beginInsertRows

    int newRow = static_cast<int>(parentNode->getChildren().size()); // Size *before* adding

    beginInsertRows(parentIndex, newRow, newRow);
    parentNode->addChild(newNode); // Add to the actual data structure
    // TODO: Connect newNode's changed() signal if needed for reset model approach
    endInsertRows();
}

void GrepModel::removeGrepNode(GrepNode* nodeToRemove)
{
     if (!nodeToRemove || nodeToRemove == rootNode_) return;

     GrepNode* parentNode = nodeToRemove->getParent();
     if (!parentNode) {
         qWarning("GrepModel::removeGrepNode: Node has no parent.");
         return;
     }

     // Find the row of the node *before* removing it
     const auto& siblings = parentNode->getChildren();
     int row = -1;
     for (size_t i = 0; i < siblings.size(); ++i) {
         if (siblings[i] == nodeToRemove) {
             row = static_cast<int>(i);
             break;
         }
     }

     if (row < 0) {
         qWarning("GrepModel::removeGrepNode: Could not find node in parent's children.");
         return;
     }

     // Get the correct parent index
     QModelIndex parentIndex = findIndexForNode(parentNode);

     beginRemoveRows(parentIndex, row, row);
     parentNode->removeChild(nodeToRemove);
     endRemoveRows();
}


// --- Slot ---
// Reverted to original reset model implementation
void GrepModel::onGrepNodeChanged()
{
    // TODO: Connect this slot to the GrepNode::changed() signal appropriately
    // if this reset approach is kept.
    qDebug() << "GrepModel::onGrepNodeChanged: Resetting model.";
    beginResetModel();
    endResetModel();
}

// Removed helper function implementation
