/*
 * Xournal++
 *
 * Class to export motion recording data as video frames
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>   // for unique_ptr
#include <string>   // for string
#include <vector>   // for vector

#include "filesystem.h"  // for path

class Document;
class MotionRecording;
class Settings;
class XojPage;

class MotionExporter final {
public:
    explicit MotionExporter(Settings& settings, Document* document);
    MotionExporter(MotionExporter const&) = delete;
    MotionExporter(MotionExporter&&) = delete;
    auto operator=(MotionExporter const&) -> MotionExporter& = delete;
    auto operator=(MotionExporter&&) -> MotionExporter& = delete;
    ~MotionExporter();

    /**
     * @brief Start exporting motion recording to frames
     * @param outputPath Directory where frames will be saved
     * @param frameRate Frames per second for the export (default: 30)
     * @return true if export started successfully
     */
    bool startExport(fs::path const& outputPath, int frameRate = 30);

    /**
     * @brief Stop the current export
     */
    void stop();

    /**
     * @brief Check if currently exporting
     */
    [[nodiscard]] bool isExporting() const;

    /**
     * @brief Get export progress (0.0 to 1.0)
     */
    [[nodiscard]] double getProgress() const;

    /**
     * @brief Get number of frames exported
     */
    [[nodiscard]] size_t getFrameCount() const;

private:
    /**
     * @brief Export motion data from a page
     */
    bool exportPageMotion(XojPage* page, size_t pageIndex);

    /**
     * @brief Render a single frame at given timestamp
     */
    bool renderFrame(size_t frameIndex, size_t timestamp);

    Settings& settings;
    Document* document;
    fs::path outputPath;
    int frameRate;
    bool exporting;
    double progress;
    size_t frameCount;
    size_t totalFrames;
};
