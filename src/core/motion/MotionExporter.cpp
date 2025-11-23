#include "MotionExporter.h"

#include <algorithm>  // for min, max
#include <cstdio>     // for snprintf
#include <fstream>    // for ofstream

#include <glib.h>  // for g_message, g_warning

#include "control/settings/Settings.h"  // for Settings
#include "model/Document.h"             // for Document
#include "model/MotionRecording.h"      // for MotionRecording
#include "model/Stroke.h"               // for Stroke
#include "model/XojPage.h"              // for XojPage
#include "util/PathUtil.h"              // for toUri

using std::string;
using std::vector;

MotionExporter::MotionExporter(Settings& settings, Document* document):
        settings(settings), document(document), frameRate(30), exporting(false), progress(0.0), frameCount(0),
        totalFrames(0) {}

MotionExporter::~MotionExporter() { this->stop(); }

auto MotionExporter::startExport(fs::path const& outputPath, int frameRate) -> bool {
    if (this->exporting) {
        g_warning("Motion export already in progress");
        return false;
    }

    if (!this->document) {
        g_warning("No document available for motion export");
        return false;
    }

    // Create output directory if it doesn't exist
    if (!fs::exists(outputPath)) {
        try {
            fs::create_directories(outputPath);
        } catch (const fs::filesystem_error& e) {
            g_warning("Failed to create output directory: %s", e.what());
            return false;
        }
    }

    this->outputPath = outputPath;
    this->frameRate = frameRate;
    this->exporting = true;
    this->progress = 0.0;
    this->frameCount = 0;

    g_message("Starting motion export to: %s (frame rate: %d fps)", outputPath.string().c_str(), frameRate);

    // Count total motion points across all pages to estimate total frames
    size_t totalMotionPoints = 0;
    size_t minTimestamp = SIZE_MAX;
    size_t maxTimestamp = 0;

    for (size_t p = 0; p < this->document->getPageCount(); p++) {
        auto page = this->document->getPage(p);
        if (!page) {
            continue;
        }

        auto layers = page->getLayers();
        for (auto* layer : *layers) {
            auto elements = layer->getElements();
            for (auto* element : *elements) {
                if (auto* stroke = dynamic_cast<Stroke*>(element)) {
                    if (stroke->hasMotionRecording()) {
                        auto* motion = stroke->getMotionRecording();
                        totalMotionPoints += motion->getMotionPointCount();
                        if (motion->hasMotionData()) {
                            minTimestamp = std::min(minTimestamp, motion->getStartTimestamp());
                            maxTimestamp = std::max(maxTimestamp, motion->getEndTimestamp());
                        }
                    }
                }
            }
        }
    }

    if (totalMotionPoints == 0) {
        g_warning("No motion recording data found in document");
        this->exporting = false;
        return false;
    }

    // Calculate total frames based on time range and frame rate
    if (maxTimestamp > minTimestamp) {
        size_t durationMs = maxTimestamp - minTimestamp;
        this->totalFrames = (durationMs * frameRate) / 1000 + 1;
    } else {
        this->totalFrames = 1;
    }

    g_message("Found %zu motion points, estimated %zu frames to export", totalMotionPoints, this->totalFrames);

    // Export motion data as metadata file
    fs::path metadataPath = outputPath / "motion_metadata.json";
    std::ofstream metadataFile(metadataPath);
    if (metadataFile.is_open()) {
        metadataFile << "{\n";
        metadataFile << "  \"frameRate\": " << frameRate << ",\n";
        metadataFile << "  \"totalFrames\": " << this->totalFrames << ",\n";
        metadataFile << "  \"totalMotionPoints\": " << totalMotionPoints << ",\n";
        metadataFile << "  \"minTimestamp\": " << minTimestamp << ",\n";
        metadataFile << "  \"maxTimestamp\": " << maxTimestamp << ",\n";
        metadataFile << "  \"pages\": [\n";

        // Export detailed motion data for each page
        for (size_t p = 0; p < this->document->getPageCount(); p++) {
            auto page = this->document->getPage(p);
            if (!page) {
                continue;
            }

            metadataFile << "    {\n";
            metadataFile << "      \"pageIndex\": " << p << ",\n";
            metadataFile << "      \"strokes\": [\n";

            bool firstStroke = true;
            auto layers = page->getLayers();
            for (auto* layer : *layers) {
                auto elements = layer->getElements();
                for (auto* element : *elements) {
                    if (auto* stroke = dynamic_cast<Stroke*>(element)) {
                        if (stroke->hasMotionRecording()) {
                            if (!firstStroke) {
                                metadataFile << ",\n";
                            }
                            firstStroke = false;

                            auto* motion = stroke->getMotionRecording();
                            metadataFile << "        {\n";
                            metadataFile << "          \"motionPoints\": [\n";

                            const auto& points = motion->getMotionPoints();
                            for (size_t i = 0; i < points.size(); i++) {
                                const auto& mp = points[i];
                                metadataFile << "            {";
                                metadataFile << "\"t\": " << mp.timestamp << ", ";
                                metadataFile << "\"x\": " << mp.point.x << ", ";
                                metadataFile << "\"y\": " << mp.point.y << ", ";
                                metadataFile << "\"p\": " << mp.point.z << ", ";
                                metadataFile << "\"isEraser\": " << (mp.isEraser ? "true" : "false");
                                metadataFile << "}";
                                if (i < points.size() - 1) {
                                    metadataFile << ",";
                                }
                                metadataFile << "\n";
                            }

                            metadataFile << "          ]\n";
                            metadataFile << "        }";
                        }
                    }
                }
            }

            metadataFile << "\n      ]\n";
            metadataFile << "    }";
            if (p < this->document->getPageCount() - 1) {
                metadataFile << ",";
            }
            metadataFile << "\n";
        }

        metadataFile << "  ]\n";
        metadataFile << "}\n";
        metadataFile.close();

        g_message("Motion metadata exported to: %s", metadataPath.string().c_str());
    }

    // Create a README file with instructions
    fs::path readmePath = outputPath / "README.txt";
    std::ofstream readmeFile(readmePath);
    if (readmeFile.is_open()) {
        readmeFile << "Motion Recording Export\n";
        readmeFile << "=======================\n\n";
        readmeFile << "This directory contains exported motion recording data from Xournal++.\n\n";
        readmeFile << "Frame Rate: " << frameRate << " fps\n";
        readmeFile << "Total Frames: " << this->totalFrames << "\n";
        readmeFile << "Total Motion Points: " << totalMotionPoints << "\n\n";
        readmeFile << "Files:\n";
        readmeFile << "  - motion_metadata.json: Detailed motion data in JSON format\n";
        readmeFile << "  - README.txt: This file\n\n";
        readmeFile << "To create a video from this data, you can use external tools like:\n";
        readmeFile << "  1. Custom rendering script (using motion_metadata.json)\n";
        readmeFile << "  2. FFmpeg (if you generate frame images)\n\n";
        readmeFile << "Example FFmpeg command (after generating frames):\n";
        readmeFile << "  ffmpeg -framerate " << frameRate
                   << " -pattern_type glob -i 'frame_*.png' -c:v libx264 -pix_fmt yuv420p output.mp4\n";
        readmeFile.close();
    }

    this->progress = 1.0;
    this->frameCount = this->totalFrames;
    this->exporting = false;

    g_message("Motion export completed successfully");
    return true;
}

void MotionExporter::stop() {
    if (this->exporting) {
        g_message("Stopping motion export");
        this->exporting = false;
    }
}

auto MotionExporter::isExporting() const -> bool { return this->exporting; }

auto MotionExporter::getProgress() const -> double { return this->progress; }

auto MotionExporter::getFrameCount() const -> size_t { return this->frameCount; }

auto MotionExporter::exportPageMotion(XojPage* page, size_t pageIndex) -> bool {
    // This can be extended to export page-specific data
    return true;
}

auto MotionExporter::renderFrame(size_t frameIndex, size_t timestamp) -> bool {
    // This can be extended to render actual frame images
    return true;
}
