#include "ProjectUiManager.hpp"

#include "FileViewer.hpp"
#include "Logfile.hpp" // Needed for Logfile type
#include "loader/LoaderLogFile.hpp" // Needed for loader::Logfile
#include "serializer/SerializerProjectModel.hpp" // Needed for serializer::ProjectModel
#include "ProjectModel.hpp" // Needed for ProjectModel
#include "ui_MainWindow.h" // Needed for Ui::MainWindow

// Add missing Qt Includes
#include <QDebug>
#include <QString>
#include <QFileDialog>
#include <QFile>
#include <QIODevice>
#include <QJsonObject>
#include <QJsonDocument>
#include <QObject> // Needed for QObject base class and tr() context (implicitly)
#include <QTabWidget> // Needed for ui_->fileView type


ProjectUiManager::ProjectUiManager(Ui::MainWindow* ui)
: ui_(ui)
{
    pm_ = std::make_unique<ProjectModel>();
    pm_->changed_ = false;
}

void ProjectUiManager::create_new()
{
    pm_ = std::make_unique<ProjectModel>();
    pm_->changed_ = false;
}

void ProjectUiManager::load_log_file(QString file_path)
{
    // Ensure ui_ and ui_->fileView are valid before proceeding
    if (!ui_ || !ui_->fileView) {
        qWarning("ProjectUiManager::load_log_file: UI or fileView is not initialized.");
        return;
    }

    // 1. Create Logfile instance (pass ProjectModel as parent)
    // Note: Logfile constructor no longer takes filename
    auto new_logfile_ptr = std::make_unique<Logfile>(pm_.get());

    // 2. Add to project model (get raw pointer back)
    Logfile* lf = pm_->add_to_project(std::move(new_logfile_ptr));
    if (!lf) {
        qWarning("Failed to add logfile to project model.");
        return; // Or handle error appropriately
    }

    // 3. Call the loader function (assuming it's adapted for async loading)
    //    It will connect signals to the Logfile object *before* initialize is called.
    loader::LogfileLoader::load( // Renamed class
        ui_->fileView, // Pass the QTabWidget*
        lf,            // Pass the Logfile pointer (not yet initialized)
        [&](FileViewer* fileviewer) // Connection lambda remains the same
        {
            connect_logviewer_signal(fileviewer);
        });

    // 4. Start asynchronous initialization *after* the loader has connected signals.
    lf->initialize(file_path);
}

void ProjectUiManager::on_logfile_wiget_close(Logfile* lf)
{
    qDebug() << "Clean some projet resources here";
    pm_->remove_file_from_project(lf);
}

void ProjectUiManager::connect_update_notif(std::function<void(void)> notif_callback)
{
    update_client_notif_ = notif_callback;
}

bool ProjectUiManager::is_empty()
{
    return pm_->is_empty();
}
bool ProjectUiManager::has_changed()
{
    return pm_->changed_;
}

const QString& ProjectUiManager::project_name()
{
    return pm_->projectName_;
}

void ProjectUiManager::save_project()
{
    QString file_path = project_name();

    if (file_path.isEmpty())
    {
        file_path = QFileDialog::getSaveFileName(nullptr,
            tr("Save project"), "",
            tr("Project file (*.json)"));
        qDebug() << "FP: " << file_path;
    }

    if (file_path.isEmpty())
        return;

    QFile saveFile(file_path);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("Couldn't open save file!");
        return;
    }
    pm_->projectName_ = saveFile.fileName();

    QJsonObject object;
    serializer::ProjectModel::serialize(*pm_, object);
    QJsonDocument document(object);
    saveFile.write(document.toJson(QJsonDocument::Indented));

    pm_->changed_ = false;
    update_client_notif_();
}

void ProjectUiManager::open_project()
{
    QString file_path = QFileDialog::getOpenFileName(nullptr,
                                                     tr("Open project"), "",
                                                     tr("Project file (*.json)"));
    if (file_path.isEmpty())
        return;

    while(ui_->fileView->count())
    {
        ui_->fileView->removeTab(0);
    }
    ui_->fileView->clear();

    pm_ = std::make_unique<ProjectModel>();

    QFile loadFile(file_path);
    if (!loadFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open save file!");
        return;
    }
    QJsonDocument document = QJsonDocument::fromJson(loadFile.readAll());
    QJsonObject object = document.object();

    serializer::ProjectModel::deserialize(*pm_, object);
    QObject::connect(pm_.get(), &ProjectModel::changed, this, &ProjectUiManager::project_changed);
    loader::Project::load(
        ui_,
        pm_.get(),
        [&](FileViewer* fileviewer)
        {
            connect_logviewer_signal(fileviewer);
        });
    pm_->changed_ = false;
}

void ProjectUiManager::connect_logviewer_signal(FileViewer* fileviewer)
{
    connect(fileviewer, &FileViewer::destroyed, this, &ProjectUiManager::file_viewer_closed);
}

void ProjectUiManager::project_changed()
{
    update_client_notif_();
}

void ProjectUiManager::file_viewer_closed(Logfile* lf)
{
    qDebug() << "Closed viewer: " << lf;
    pm_->remove_file_from_project(lf);
}
