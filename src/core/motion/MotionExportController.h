/*
 * Xournal++
 *
 * Motion Export Controller
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr

#include "filesystem.h"  // for path

class Control;
class Document;
class MotionExporter;
class Settings;

class MotionExportController final {
public:
    MotionExportController(Settings* settings, Control* control, Document* document);
    ~MotionExportController();

    /**
     * @brief Start motion export
     * @return true if export started successfully
     */
    bool startExport();

    /**
     * @brief Stop motion export
     * @return true if export stopped successfully
     */
    bool stopExport();

    /**
     * @brief Check if currently exporting
     */
    bool isExporting();

    /**
     * @brief Get the motion export folder path
     */
    fs::path getMotionExportFolder() const;

    /**
     * @brief Get export progress (0.0 to 1.0)
     */
    double getProgress() const;

private:
    Settings& settings;
    Control& control;
    Document* document;

    std::unique_ptr<MotionExporter> motionExporter;
};
