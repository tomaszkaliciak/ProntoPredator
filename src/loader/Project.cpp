#include "Project.hpp"

#include <memory>

#include "ui_MainWindow.h"

#include "../ProjectModel.hpp"

#include "LoaderLogFile.hpp"

namespace loader
{

void Project::load(Ui::MainWindow *ui, ::ProjectModel* pm, std::function<void(::FileViewer*)> connect_slots_to_manager)
{
    for (const auto& logfile : pm->get_log_files())
    {
            ::Logfile* raw_logfile = logfile.get();
            // Use the renamed class LogfileLoader
            LogfileLoader::load(ui->fileView, raw_logfile, connect_slots_to_manager);
            // Note: The initialize call was previously in ProjectUiManager::load_log_file
            // and also in serializer::ProjectModel::deserialize.
            // Ensure initialize is called appropriately after loading a project.
            // It seems loader::Project::load might be called *after* deserialization,
            // so initialize should have already been called by the deserializer.
            // If this loader is used elsewhere, ensure initialize is called.
        }
}
}  // namespace loader
