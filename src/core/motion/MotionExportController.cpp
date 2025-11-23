#include "MotionExportController.h"

#include <array>   // for array
#include <cstdio>  // for snprintf
#include <ctime>   // for tm, localtime, time
#include <string>  // for string

#include <glib.h>  // for g_get_monotonic_time

#include "control/Control.h"                     // for Control
#include "control/actions/ActionDatabase.h"      // for ActionDatabase
#include "control/settings/Settings.h"           // for Settings
#include "gui/MainWindow.h"                      // for MainWindow
#include "gui/toolbarMenubar/ToolMenuHandler.h"  // for ToolMenuHandler
#include "model/Document.h"                      // for Document
#include "motion/MotionExporter.h"               // for MotionExporter
#include "util/XojMsgBox.h"                      // for XojMsgBox
#include "util/i18n.h"                           // for _

using std::string;

MotionExportController::MotionExportController(Settings* settings, Control* control, Document* document):
        settings(*settings),
        control(*control),
        document(document),
        motionExporter(std::make_unique<MotionExporter>(*settings, document)) {}

MotionExportController::~MotionExportController() = default;

auto MotionExportController::startExport() -> bool {
    if (!this->isExporting()) {
        auto exportFolder = getMotionExportFolder();
        if (exportFolder.empty()) {
            return false;
        }

        // Create timestamped subfolder for this export
        std::array<char, 50> buffer{};
        time_t secs = time(nullptr);
        tm* t = localtime(&secs);
        snprintf(buffer.data(), buffer.size(), "motion_export_%04d-%02d-%02d_%02d-%02d-%02d", t->tm_year + 1900,
                 t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
        string folderName(buffer.data());

        auto outputPath = exportFolder / folderName;

        g_message("Starting motion export to: %s", outputPath.string().c_str());

        // Get frame rate from settings (default to 30)
        int frameRate = this->settings.getMotionExportFrameRate();

        bool success = this->motionExporter->startExport(outputPath, frameRate);

        if (!success) {
            g_message("Failed to start motion export");
            return false;
        }

        g_message("Motion export completed");
        return true;
    }
    return false;
}

auto MotionExportController::stopExport() -> bool {
    if (this->motionExporter->isExporting()) {
        g_message("Stopping motion export");
        this->motionExporter->stop();
    }
    return true;
}

auto MotionExportController::isExporting() -> bool { return this->motionExporter->isExporting(); }

auto MotionExportController::getMotionExportFolder() const -> fs::path {
    auto const& mf = this->settings.getMotionExportFolder();

    if (!fs::is_directory(mf)) {
        string msg = _("Motion export folder not set or invalid! Export won't work!\nPlease set the "
                       "export folder under \"Preferences > Motion Export\"");
        XojMsgBox::showErrorToUser(this->control.getGtkWindow(), msg);
        return fs::path{};
    }
    return mf;
}

auto MotionExportController::getProgress() const -> double { return this->motionExporter->getProgress(); }
