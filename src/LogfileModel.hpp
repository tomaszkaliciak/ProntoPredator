#ifndef LOGFILEMODEL_HPP
#define LOGFILEMODEL_HPP

#include <QAbstractListModel> // Change to QAbstractTableModel later if needed, ListModel works for simple tables too
#include <QObject> // Include QObject for parent parameter
#include "Logfile.hpp" // Include Logfile definition

// Note: While QAbstractListModel can sometimes work with QTableView for simple cases,
// QAbstractTableModel is technically more correct for multi-column data.
// We'll stick with QAbstractListModel for now and see if it suffices,
// but might need to change to QAbstractTableModel if issues arise.
class LogfileModel : public QAbstractListModel
{
    Q_OBJECT

public:
    // Constructor takes the Logfile data source and a parent object
    explicit LogfileModel(Logfile* logfile, QObject* parent = nullptr);

    // --- QAbstractListModel overrides ---
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    // --- Add TableView specific overrides ---
    int columnCount(const QModelIndex &parent = QModelIndex()) const override; // Added
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override; // Modified logic needed in .cpp
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override; // Added
    Qt::ItemFlags flags(const QModelIndex &index) const override; // Added declaration

    // Enum for columns
    enum Column {
        LineNumberColumn = 0,
        MessageColumn = 1
    };


    // Public method to trigger a full model reset
    void resetModel();

private:
    Logfile* logfile_; // Pointer to the actual log file data (non-owning)
};

#endif // LOGFILEMODEL_HPP
