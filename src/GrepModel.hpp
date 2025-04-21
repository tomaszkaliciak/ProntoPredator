#ifndef GREPMODEL_HPP
#define GREPMODEL_HPP

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

class GrepNode; // Forward declaration

class GrepModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit GrepModel(GrepNode* rootNode, QObject* parent = nullptr);
    ~GrepModel() override;

    // Header:
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // Basic functionality:
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    // Data display:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Add/Remove functionality (to be called when GrepNode hierarchy changes)
    // These will emit the appropriate signals (beginInsertRows, etc.)
    void addGrepNode(GrepNode* parentNode, GrepNode* newNode);
    void removeGrepNode(GrepNode* nodeToRemove);

    // Helper to get GrepNode* from QModelIndex
    GrepNode* getNode(const QModelIndex &index) const;

private slots:
    // Slot to react to changes in GrepNode (currently resets model)
    void onGrepNodeChanged();

private:
    GrepNode* rootNode_; // The root of the GrepNode data structure

    // Helper to find the parent GrepNode and row index for a given GrepNode
    // std::pair<GrepNode*, int> findNodeParentAndRow(GrepNode* node) const; // Removed - No longer needed
    // Helper to find the QModelIndex for a given GrepNode
    QModelIndex findIndexForNode(GrepNode* node) const;
};

#endif // GREPMODEL_HPP
