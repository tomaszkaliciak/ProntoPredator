#include "LoaderLogFile.hpp"

// Use qualified includes
#include <QtCore/QRegularExpression>
#include <QtWidgets/QTabWidget> // Assuming QTabWidget is in QtWidgets
#include <QtCore/QString>
#include <QtCore/QDebug> // For qWarning

// #include "../BookmarksModel.hpp" // Unused include
#include "../GrepNode.hpp"
#include "../Logfile.hpp" // Add include for full Logfile definition

#include "../LogViewer.hpp"
#include "../FileViewer.hpp"
// #include "ui_MainWindow.h" // Removed

namespace loader
{

// Update function signature to match header
void Logfile::load(QTabWidget *target_tab_widget, ::Logfile* logfile_data, std::function<void(FileViewer*)> connect_slots_to_manager)
{
    // Ensure logfile_data is valid
    if (!logfile_data) {
        qWarning("LoaderLogFile::load called with null Logfile pointer.");
        return;
    }
    // Ensure target_tab_widget is valid
    if (!target_tab_widget) {
         qWarning("LoaderLogFile::load called with null target_tab_widget pointer.");
         return;
    }

    // Pass target_tab_widget as parent for memory management
    FileViewer* viewer = new FileViewer(target_tab_widget, logfile_data);
    connect_slots_to_manager(viewer); // Assuming this connects signals/slots correctly

    // Use public accessor getFileName()
    QString filename = logfile_data->getFileName();
    // Use QRegularExpression with qualified namespace if needed, or assume it's found via includes
    int tab_index = target_tab_widget->addTab(viewer, filename.split(QRegularExpression("[\\/\\\\]")).last()); // Use public addTab, handle both / and \
    target_tab_widget->setTabToolTip(tab_index, filename); // Use public setTabToolTip

    // Use public accessors getDeepestActiveTab() and getGrepHierarchy()
    // spawnViews(viewer->getDeepestActiveTab(), logfile_data->getGrepHierarchy()); // REMOVED - Relies on old structure and disabled grep
}

/* // REMOVED - Relies on old structure and disabled grep
void Logfile::spawnViews(LogViewer* parent_tab, const GrepNode* node)
{
    if (node == nullptr || parent_tab == nullptr) return; // Add check for parent_tab

    // Use public accessor getChildren()
    for (GrepNode* child : node->getChildren())
    {
        // Use public method grep()
        LogViewer* spawned_viewer = parent_tab->grep(child);
        // Recurse only if a new viewer was successfully created (grep might return nullptr)
        if (spawned_viewer) {
            spawnViews(spawned_viewer, child);
        }
    }
}
*/

}  // namespace loader
