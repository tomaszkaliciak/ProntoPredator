#include "SerializerProjectModel.hpp"

#include <QJsonObject>
#include <QJsonArray>

#include "../ProjectModel.hpp"
#include "SerializerLogfile.hpp"
#include "../Logfile.hpp"

namespace serializer
{

void ProjectModel::serialize(const ::ProjectModel &pm, QJsonObject &json)
{
    QJsonArray array;

    for (const auto& logfile : pm.logfiles_)
    {
        QJsonObject jsonLogfile;
        Logfile::serialize(*logfile, jsonLogfile);
        array.append(jsonLogfile);
    }

    json["logfiles"] = array;
    json["projectName"] = pm.projectName_;
}
void ProjectModel::deserialize(::ProjectModel &pm, const QJsonObject &json)
{
    QJsonArray logfiles = json["logfiles"].toArray();
    pm.projectName_ = json["projectName"].toString();

    for (const QJsonValue child_val : logfiles)
    {
         QJsonObject child_obj = child_val.toObject();
         QString filepath = child_obj["filepath"].toString(); // Extract filepath first
         if (filepath.isEmpty()) {
             qWarning("Skipping logfile entry with empty filepath during deserialization.");
             continue;
         }

         // Create Logfile instance (pass ProjectModel* as parent)
         auto logfile_ptr = std::make_unique<::Logfile>(&pm); // Pass parent

         // Deserialize other properties (bookmarks, grep nodes) BEFORE initializing
         // This assumes deserialize doesn't rely on the file being indexed yet
         ::serializer::Logfile::deserialize(*logfile_ptr, child_obj);

         // Add the logfile to the project model *before* initializing it
         // This ensures the model owns the object while it initializes
         ::Logfile* raw_logfile_ptr = pm.add_to_project(std::move(logfile_ptr));

         // Start asynchronous initialization *after* adding to model and deserializing properties
         if (raw_logfile_ptr) {
             raw_logfile_ptr->initialize(filepath);
         } else {
             qWarning("Failed to add deserialized logfile to project: %s", qPrintable(filepath));
         }
    }
    pm.changed_ = false; // Reset changed flag after loading
}

}  // namespace serializer
