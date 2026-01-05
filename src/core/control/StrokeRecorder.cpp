/*
 * Xournal++
 *
 * Records drawing actions (strokes, erasing, images, undo/redo) for video export
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#include "StrokeRecorder.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

#include "control/Control.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"

StrokeRecorder::StrokeRecorder(Control* control):
        control(control),
        recording(false),
        recordingStartTime(0),
        lastEventTime(0),
        nextStrokeId(1),
        nextImageId(1),
        currentStrokeId(-1),
        currentPage(0),
        backgroundColor(Colors::white),
        nextZOrder(1) {

    // Register as undo/redo listener
    control->getUndoRedoHandler()->addUndoRedoListener(this);
}

StrokeRecorder::~StrokeRecorder() = default;

void StrokeRecorder::startRecording() {
    if (recording) {
        return;
    }

    recording = true;
    events.clear();
    strokes.clear();
    images.clear();
    nextStrokeId = 1;
    nextImageId = 1;
    currentStrokeId = -1;
    nextZOrder = 1;

    // Record start time
    auto now = std::chrono::steady_clock::now();
    recordingStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    lastEventTime = 0;

    // Record initial state
    currentPage = control->getCurrentPageNo();

    // Get background color from current page
    PageRef page = control->getCurrentPage();
    if (page) {
        backgroundColor = page->getBackgroundColor();
    }
}

void StrokeRecorder::stopRecording() {
    if (!recording) {
        return;
    }

    recording = false;
    compressIdleTime();
}

bool StrokeRecorder::isRecording() const { return recording; }

int64_t StrokeRecorder::getCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() - recordingStartTime;
}

int StrokeRecorder::generateStrokeId() { return nextStrokeId++; }

std::string StrokeRecorder::generateImageId() {
    std::stringstream ss;
    ss << "img_" << nextImageId++;
    return ss.str();
}

void StrokeRecorder::recordStrokeStart(const Stroke* stroke, size_t pageNumber) {
    if (!recording || !stroke) {
        return;
    }

    currentStrokeId = generateStrokeId();
    currentPage = pageNumber;

    RecordEvent event;
    event.type = RecordEventType::STROKE_START;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;
    event.strokeId = currentStrokeId;
    event.color = stroke->getColor();
    event.width = stroke->getWidth();
    event.x = 0;
    event.y = 0;
    event.pressure = 0;
    event.zOrder = nextZOrder;

    events.push_back(event);
    lastEventTime = event.timestamp;

    // Create a recorded stroke entry
    RecordedStroke recordedStroke;
    recordedStroke.id = currentStrokeId;
    recordedStroke.color = stroke->getColor();
    recordedStroke.width = stroke->getWidth();
    recordedStroke.isHighlighter = (stroke->getToolType() == StrokeTool::HIGHLIGHTER);
    recordedStroke.fill = stroke->getFill();
    recordedStroke.zOrder = nextZOrder++;

    strokes.push_back(recordedStroke);
}

void StrokeRecorder::recordStrokePoint(double x, double y, double pressure) {
    if (!recording || currentStrokeId < 0) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::STROKE_POINT;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = currentPage;
    event.strokeId = currentStrokeId;
    event.x = x;
    event.y = y;
    event.pressure = pressure;

    events.push_back(event);
    lastEventTime = event.timestamp;

    // Add point to the current stroke
    if (!strokes.empty() && strokes.back().id == currentStrokeId) {
        strokes.back().points.emplace_back(x, y, pressure);
        strokes.back().pointTimestamps.push_back(event.timestamp);
    }
}

void StrokeRecorder::recordStrokeEnd() {
    if (!recording || currentStrokeId < 0) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::STROKE_END;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = currentPage;
    event.strokeId = currentStrokeId;

    events.push_back(event);
    lastEventTime = event.timestamp;

    currentStrokeId = -1;
}

void StrokeRecorder::recordEraseStart(double x, double y, double eraserSize, size_t pageNumber) {
    if (!recording) {
        return;
    }

    currentPage = pageNumber;

    RecordEvent event;
    event.type = RecordEventType::ERASE_START;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;
    event.x = x;
    event.y = y;
    event.eraserSize = eraserSize;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

void StrokeRecorder::recordErasePoint(double x, double y, double eraserSize, const std::vector<int>& affectedStrokeIds) {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::ERASE_POINT;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = currentPage;
    event.x = x;
    event.y = y;
    event.eraserSize = eraserSize;
    event.affectedStrokeIds = affectedStrokeIds;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

void StrokeRecorder::recordEraseEnd() {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::ERASE_END;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = currentPage;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

void StrokeRecorder::recordImageAdd(const Image* image, size_t pageNumber) {
    if (!recording || !image) {
        return;
    }

    std::string imageId = generateImageId();

    RecordEvent event;
    event.type = RecordEventType::IMAGE_ADD;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;
    event.imageId = imageId;
    event.imageX = image->getX();
    event.imageY = image->getY();
    event.imageWidth = image->getElementWidth();
    event.imageHeight = image->getElementHeight();
    event.imageRotation = 0.0;  // Initial rotation is 0
    event.zOrder = nextZOrder;

    events.push_back(event);
    lastEventTime = event.timestamp;

    // Store image info with raw data
    RecordedImage recordedImage;
    recordedImage.id = imageId;
    recordedImage.filename = imageId + ".png";
    recordedImage.x = image->getX();
    recordedImage.y = image->getY();
    recordedImage.width = image->getElementWidth();
    recordedImage.height = image->getElementHeight();
    recordedImage.rotation = 0.0;
    recordedImage.addedTimestamp = event.timestamp;
    recordedImage.zOrder = nextZOrder++;
    
    // Copy raw image data for later export
    if (image->hasData()) {
        const uint8_t* data = image->getRawData();
        size_t len = image->getRawDataLength();
        recordedImage.imageData.assign(data, data + len);
    }

    images.push_back(std::move(recordedImage));
}

void StrokeRecorder::recordImageMove(const std::string& imageId, double newX, double newY) {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::IMAGE_MOVE;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = currentPage;
    event.imageId = imageId;
    event.imageX = newX;
    event.imageY = newY;

    events.push_back(event);
    lastEventTime = event.timestamp;
    
    // Update the image position in our records
    for (auto& img : images) {
        if (img.id == imageId) {
            img.x = newX;
            img.y = newY;
            break;
        }
    }
}

void StrokeRecorder::recordImageResize(const std::string& imageId, double newX, double newY, double newWidth, double newHeight) {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::IMAGE_RESIZE;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = currentPage;
    event.imageId = imageId;
    event.imageX = newX;
    event.imageY = newY;
    event.imageWidth = newWidth;
    event.imageHeight = newHeight;

    events.push_back(event);
    lastEventTime = event.timestamp;
    
    // Update the image size in our records
    for (auto& img : images) {
        if (img.id == imageId) {
            img.x = newX;
            img.y = newY;
            img.width = newWidth;
            img.height = newHeight;
            break;
        }
    }
}

void StrokeRecorder::recordImageRotate(const std::string& imageId, double rotation) {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::IMAGE_ROTATE;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = currentPage;
    event.imageId = imageId;
    event.imageRotation = rotation;

    events.push_back(event);
    lastEventTime = event.timestamp;
    
    // Update the image rotation in our records
    for (auto& img : images) {
        if (img.id == imageId) {
            img.rotation = rotation;
            break;
        }
    }
}

void StrokeRecorder::recordImageCopy(const Image* image, const std::string& sourceImageId, size_t pageNumber) {
    if (!recording || !image) {
        return;
    }

    std::string newImageId = generateImageId();

    RecordEvent event;
    event.type = RecordEventType::IMAGE_COPY;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;
    event.imageId = newImageId;
    event.imageX = image->getX();
    event.imageY = image->getY();
    event.imageWidth = image->getElementWidth();
    event.imageHeight = image->getElementHeight();
    event.imageRotation = 0.0;
    event.zOrder = nextZOrder;

    events.push_back(event);
    lastEventTime = event.timestamp;

    // Find source image and copy its data
    RecordedImage recordedImage;
    recordedImage.id = newImageId;
    recordedImage.filename = newImageId + ".png";
    recordedImage.x = image->getX();
    recordedImage.y = image->getY();
    recordedImage.width = image->getElementWidth();
    recordedImage.height = image->getElementHeight();
    recordedImage.rotation = 0.0;
    recordedImage.addedTimestamp = event.timestamp;
    recordedImage.zOrder = nextZOrder++;
    
    // Copy from source image or from the Image object
    bool foundSource = false;
    for (const auto& srcImg : images) {
        if (srcImg.id == sourceImageId) {
            recordedImage.imageData = srcImg.imageData;
            foundSource = true;
            break;
        }
    }
    
    // If source not found, copy from the Image object directly
    if (!foundSource && image->hasData()) {
        const uint8_t* data = image->getRawData();
        size_t len = image->getRawDataLength();
        recordedImage.imageData.assign(data, data + len);
    }

    images.push_back(std::move(recordedImage));
}

void StrokeRecorder::recordZOrderChange(const std::string& elementId, int newZOrder, size_t pageNumber) {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::ELEMENT_Z_CHANGE;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;
    event.imageId = elementId;
    event.zOrder = newZOrder;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

std::string StrokeRecorder::findImageIdAtPosition(double x, double y, double tolerance) const {
    // Search from most recent to oldest (higher z-order first)
    for (auto it = images.rbegin(); it != images.rend(); ++it) {
        const auto& img = *it;
        if (x >= img.x - tolerance && x <= img.x + img.width + tolerance &&
            y >= img.y - tolerance && y <= img.y + img.height + tolerance) {
            return img.id;
        }
    }
    return "";
}

void StrokeRecorder::recordBackgroundColorChange(Color color, size_t pageNumber) {
    if (!recording) {
        return;
    }

    backgroundColor = color;

    RecordEvent event;
    event.type = RecordEventType::BACKGROUND_COLOR_CHANGE;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;
    event.backgroundColor = color;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

void StrokeRecorder::recordPageChange(size_t pageNumber) {
    if (!recording) {
        return;
    }

    currentPage = pageNumber;

    RecordEvent event;
    event.type = RecordEventType::PAGE_CHANGE;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

void StrokeRecorder::recordUndo(size_t pageNumber) {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::UNDO;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

void StrokeRecorder::recordRedo(size_t pageNumber) {
    if (!recording) {
        return;
    }

    RecordEvent event;
    event.type = RecordEventType::REDO;
    event.timestamp = getCurrentTimestamp();
    event.pageNumber = pageNumber;

    events.push_back(event);
    lastEventTime = event.timestamp;
}

void StrokeRecorder::undoRedoChanged() {
    if (!recording) {
        return;
    }

    // This is called after an undo or redo operation
    // We need to track which one happened - we'll use a simple heuristic:
    // The Control class will explicitly call our methods for undo/redo
}

void StrokeRecorder::undoRedoPageChanged(PageRef page) {
    // Not needed for our purposes
}

Color StrokeRecorder::getBackgroundColor() const { return backgroundColor; }

int64_t StrokeRecorder::getRecordingDuration() const {
    if (events.empty()) {
        return 0;
    }
    return events.back().timestamp;
}

void StrokeRecorder::compressIdleTime() {
    if (events.size() < 2) {
        return;
    }

    int64_t totalCompression = 0;

    for (size_t i = 1; i < events.size(); ++i) {
        events[i].timestamp -= totalCompression;

        int64_t idleTime = events[i].timestamp - events[i - 1].timestamp;
        if (idleTime > MAX_IDLE_TIME_MS) {
            int64_t compression = idleTime - MAX_IDLE_TIME_MS;
            totalCompression += compression;
            events[i].timestamp -= compression;
        }
    }

    // Also compress timestamps in recorded strokes
    // Apply the same compression ratio to stroke point timestamps
    if (totalCompression > 0 && !events.empty()) {
        int64_t originalDuration = events.back().timestamp + totalCompression;
        int64_t compressedDuration = events.back().timestamp;
        
        if (originalDuration > 0) {
            double compressionRatio = static_cast<double>(compressedDuration) / static_cast<double>(originalDuration);
            
            for (auto& stroke: strokes) {
                for (auto& ts: stroke.pointTimestamps) {
                    ts = static_cast<int64_t>(ts * compressionRatio);
                }
            }
        }
    }
}

bool StrokeRecorder::storeImage(const Image* image, const fs::path& imageDir, const std::string& imageId) const {
    if (!image || !image->hasData()) {
        return false;
    }

    fs::path imagePath = imageDir / (imageId + ".png");

    try {
        std::ofstream file(imagePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        file.write(reinterpret_cast<const char*>(image->getRawData()), static_cast<std::streamsize>(image->getRawDataLength()));
        return file.good();
    } catch (...) {
        return false;
    }
}

bool StrokeRecorder::exportToJson(const fs::path& filepath, const fs::path& imageDir) const {
    // Allow empty recordings - they will just have no events
    // This is not an error condition

    // Create image directory if it doesn't exist and we have images to save
    if (!images.empty() && !fs::exists(imageDir)) {
        try {
            fs::create_directories(imageDir);
        } catch (...) {
            return false;
        }
    }
    
    // Save all recorded images to disk
    for (const auto& img: images) {
        if (!img.imageData.empty()) {
            fs::path imagePath = imageDir / img.filename;
            try {
                std::ofstream imgFile(imagePath, std::ios::binary);
                if (imgFile.is_open()) {
                    imgFile.write(reinterpret_cast<const char*>(img.imageData.data()), 
                                  static_cast<std::streamsize>(img.imageData.size()));
                }
            } catch (...) {
                // Continue even if image save fails
            }
        }
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    // Use "C" locale to ensure consistent number formatting (no thousand separators)
    file.imbue(std::locale::classic());

    // Get page dimensions
    double pageWidth = 595.0;   // Default A4 width in points
    double pageHeight = 842.0;  // Default A4 height in points
    PageRef page = control->getCurrentPage();
    if (page) {
        pageWidth = page->getWidth();
        pageHeight = page->getHeight();
    }

    // Write JSON manually to avoid external dependencies
    file << "{\n";
    file << "  \"version\": \"1.0\",\n";
    file << "  \"duration_ms\": " << getRecordingDuration() << ",\n";
    file << "  \"page_width\": " << pageWidth << ",\n";
    file << "  \"page_height\": " << pageHeight << ",\n";
    file << std::hex << std::setfill('0');
    file << "  \"background_color\": \"" << std::setw(8) << static_cast<uint32_t>(backgroundColor) << "\",\n";
    file << std::dec;

    // Write strokes
    file << "  \"strokes\": [\n";
    for (size_t i = 0; i < strokes.size(); ++i) {
        const auto& s = strokes[i];
        file << "    {\n";
        file << "      \"id\": " << s.id << ",\n";
        file << "      \"color\": \"" << std::hex << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(s.color) << std::dec << "\",\n";
        file << "      \"width\": " << s.width << ",\n";
        file << "      \"is_highlighter\": " << (s.isHighlighter ? "true" : "false") << ",\n";
        file << "      \"fill\": " << s.fill << ",\n";
        file << "      \"z_order\": " << s.zOrder << ",\n";
        file << "      \"points\": [\n";
        for (size_t j = 0; j < s.points.size(); ++j) {
            const auto& p = s.points[j];
            file << "        {\"x\": " << p.x << ", \"y\": " << p.y << ", \"z\": " << p.z;
            if (j < s.pointTimestamps.size()) {
                file << ", \"t\": " << s.pointTimestamps[j];
            }
            file << "}";
            if (j < s.points.size() - 1)
                file << ",";
            file << "\n";
        }
        file << "      ]\n";
        file << "    }";
        if (i < strokes.size() - 1)
            file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Write images
    file << "  \"images\": [\n";
    for (size_t i = 0; i < images.size(); ++i) {
        const auto& img = images[i];
        file << "    {\n";
        file << "      \"id\": \"" << img.id << "\",\n";
        file << "      \"filename\": \"" << img.filename << "\",\n";
        file << "      \"x\": " << img.x << ",\n";
        file << "      \"y\": " << img.y << ",\n";
        file << "      \"width\": " << img.width << ",\n";
        file << "      \"height\": " << img.height << ",\n";
        file << "      \"rotation\": " << img.rotation << ",\n";
        file << "      \"z_order\": " << img.zOrder << ",\n";
        file << "      \"timestamp\": " << img.addedTimestamp << "\n";
        file << "    }";
        if (i < images.size() - 1)
            file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Write events
    file << "  \"events\": [\n";
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        file << "    {\n";
        file << "      \"type\": \"";
        switch (e.type) {
            case RecordEventType::STROKE_START:
                file << "stroke_start";
                break;
            case RecordEventType::STROKE_POINT:
                file << "stroke_point";
                break;
            case RecordEventType::STROKE_END:
                file << "stroke_end";
                break;
            case RecordEventType::ERASE_START:
                file << "erase_start";
                break;
            case RecordEventType::ERASE_POINT:
                file << "erase_point";
                break;
            case RecordEventType::ERASE_END:
                file << "erase_end";
                break;
            case RecordEventType::IMAGE_ADD:
                file << "image_add";
                break;
            case RecordEventType::IMAGE_MOVE:
                file << "image_move";
                break;
            case RecordEventType::IMAGE_RESIZE:
                file << "image_resize";
                break;
            case RecordEventType::IMAGE_ROTATE:
                file << "image_rotate";
                break;
            case RecordEventType::IMAGE_COPY:
                file << "image_copy";
                break;
            case RecordEventType::ELEMENT_Z_CHANGE:
                file << "element_z_change";
                break;
            case RecordEventType::UNDO:
                file << "undo";
                break;
            case RecordEventType::REDO:
                file << "redo";
                break;
            case RecordEventType::PAGE_CHANGE:
                file << "page_change";
                break;
            case RecordEventType::BACKGROUND_COLOR_CHANGE:
                file << "background_color_change";
                break;
        }
        file << "\",\n";
        file << "      \"timestamp\": " << e.timestamp << ",\n";
        file << "      \"page\": " << e.pageNumber;

        // Type-specific data
        if (e.type == RecordEventType::STROKE_START || e.type == RecordEventType::STROKE_POINT ||
            e.type == RecordEventType::STROKE_END) {
            file << ",\n      \"stroke_id\": " << e.strokeId;
            if (e.type == RecordEventType::STROKE_POINT) {
                file << ",\n      \"x\": " << e.x;
                file << ",\n      \"y\": " << e.y;
                file << ",\n      \"pressure\": " << e.pressure;
            }
            if (e.type == RecordEventType::STROKE_START) {
                file << ",\n      \"color\": \"" << std::hex << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(e.color) << std::dec << "\"";
                file << ",\n      \"width\": " << e.width;
                file << ",\n      \"z_order\": " << e.zOrder;
            }
        } else if (e.type == RecordEventType::ERASE_START || e.type == RecordEventType::ERASE_POINT ||
                   e.type == RecordEventType::ERASE_END) {
            file << ",\n      \"x\": " << e.x;
            file << ",\n      \"y\": " << e.y;
            if (e.type == RecordEventType::ERASE_START || e.type == RecordEventType::ERASE_POINT) {
                file << ",\n      \"eraser_size\": " << e.eraserSize;
            }
            if (!e.affectedStrokeIds.empty()) {
                file << ",\n      \"affected_strokes\": [";
                for (size_t j = 0; j < e.affectedStrokeIds.size(); ++j) {
                    file << e.affectedStrokeIds[j];
                    if (j < e.affectedStrokeIds.size() - 1)
                        file << ", ";
                }
                file << "]";
            }
        } else if (e.type == RecordEventType::IMAGE_ADD || e.type == RecordEventType::IMAGE_MOVE ||
                   e.type == RecordEventType::IMAGE_RESIZE || e.type == RecordEventType::IMAGE_ROTATE ||
                   e.type == RecordEventType::IMAGE_COPY) {
            file << ",\n      \"image_id\": \"" << e.imageId << "\"";
            file << ",\n      \"x\": " << e.imageX;
            file << ",\n      \"y\": " << e.imageY;
            if (e.type == RecordEventType::IMAGE_ADD || e.type == RecordEventType::IMAGE_RESIZE ||
                e.type == RecordEventType::IMAGE_COPY) {
                file << ",\n      \"width\": " << e.imageWidth;
                file << ",\n      \"height\": " << e.imageHeight;
            }
            if (e.type == RecordEventType::IMAGE_ROTATE) {
                file << ",\n      \"rotation\": " << e.imageRotation;
            }
            if (e.type == RecordEventType::IMAGE_ADD || e.type == RecordEventType::IMAGE_COPY) {
                file << ",\n      \"z_order\": " << e.zOrder;
            }
        } else if (e.type == RecordEventType::ELEMENT_Z_CHANGE) {
            file << ",\n      \"element_id\": \"" << e.imageId << "\"";
            file << ",\n      \"z_order\": " << e.zOrder;
        } else if (e.type == RecordEventType::BACKGROUND_COLOR_CHANGE) {
            file << ",\n      \"color\": \"" << std::hex << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(e.backgroundColor) << std::dec
                 << "\"";
        }

        file << "\n    }";
        if (i < events.size() - 1)
            file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // Write SVG cursors for pen and eraser
    file << "  \"cursors\": {\n";
    file << "    \"pen\": \"<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24'><path "
            "d='M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 "
            "0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z'/></svg>\",\n";
    file << "    \"eraser\": \"<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 "
            "24'><path d='M15.14 3c-.51 0-1.02.2-1.41.59L2.59 14.73c-.78.77-.78 2.04 0 2.83L5.03 "
            "20H11c.55 0 1-.45 1-1s-.45-1-1-1H6.41L5.03 16.59 16 5.62 18.38 8l-5.94 5.94c-.39.39-.39 1.02 0 "
            "1.41.39.39 1.02.39 1.41 0l6.94-6.93c.78-.78.78-2.05 0-2.83l-3.75-3.75c-.39-.38-.9-.59-1.41-.59z'/></svg>\"\n";
    file << "  }\n";

    file << "}\n";

    return file.good();
}
