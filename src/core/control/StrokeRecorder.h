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

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "model/PageRef.h"
#include "model/Point.h"
#include "undo/UndoRedoHandler.h"
#include "util/Color.h"

#include "filesystem.h"

class Control;
class Stroke;
class Image;
class Element;

/**
 * @brief Event types that can be recorded
 */
enum class RecordEventType {
    STROKE_START,
    STROKE_POINT,
    STROKE_END,
    ERASE_START,
    ERASE_POINT,
    ERASE_END,
    IMAGE_ADD,
    IMAGE_MOVE,
    UNDO,
    REDO,
    PAGE_CHANGE,
    BACKGROUND_COLOR_CHANGE
};

/**
 * @brief Represents a single recorded event
 */
struct RecordEvent {
    RecordEventType type;
    int64_t timestamp;  // Milliseconds since recording started
    size_t pageNumber;

    // Stroke data
    double x;
    double y;
    double pressure;
    Color color;
    double width;
    int strokeId;  // Unique ID for each stroke

    // Eraser data
    double eraserSize;
    std::vector<int> affectedStrokeIds;  // Strokes being erased

    // Image data
    std::string imageId;
    double imageX;
    double imageY;
    double imageWidth;
    double imageHeight;

    // Background color
    Color backgroundColor;
};

/**
 * @brief Represents a complete stroke for replay
 */
struct RecordedStroke {
    int id;
    Color color;
    double width;
    std::vector<Point> points;
    std::vector<int64_t> pointTimestamps;
    bool isHighlighter;
    int fill;  // Fill value (-1 for no fill)
};

/**
 * @brief Represents an image for replay
 */
struct RecordedImage {
    std::string id;
    std::string filename;  // Relative path to stored image
    double x;
    double y;
    double width;
    double height;
    int64_t addedTimestamp;
    std::vector<uint8_t> imageData;  // Raw image data (PNG)
};

/**
 * @brief StrokeRecorder records all drawing actions for later video/GIF export
 */
class StrokeRecorder: public UndoRedoListener {
public:
    explicit StrokeRecorder(Control* control);
    ~StrokeRecorder() override;

    /**
     * @brief Start recording
     */
    void startRecording();

    /**
     * @brief Stop recording
     */
    void stopRecording();

    /**
     * @brief Check if currently recording
     */
    bool isRecording() const;

    /**
     * @brief Record a stroke starting
     */
    void recordStrokeStart(const Stroke* stroke, size_t pageNumber);

    /**
     * @brief Record a point being added to current stroke
     */
    void recordStrokePoint(double x, double y, double pressure);

    /**
     * @brief Record stroke completion
     */
    void recordStrokeEnd();

    /**
     * @brief Record eraser start
     */
    void recordEraseStart(double x, double y, double eraserSize, size_t pageNumber);

    /**
     * @brief Record eraser movement
     */
    void recordErasePoint(double x, double y, double eraserSize, const std::vector<int>& affectedStrokeIds);

    /**
     * @brief Record eraser end
     */
    void recordEraseEnd();

    /**
     * @brief Record image addition
     */
    void recordImageAdd(const Image* image, size_t pageNumber);

    /**
     * @brief Record image movement
     */
    void recordImageMove(const std::string& imageId, double newX, double newY);

    /**
     * @brief Record background color change
     */
    void recordBackgroundColorChange(Color color, size_t pageNumber);

    /**
     * @brief Record page change
     */
    void recordPageChange(size_t pageNumber);

    /**
     * @brief Record undo action
     */
    void recordUndo(size_t pageNumber);

    /**
     * @brief Record redo action
     */
    void recordRedo(size_t pageNumber);

    /**
     * @brief Export recording to JSON file
     * @param filepath Path to save the JSON file
     * @param imageDir Directory to save extracted images
     * @return true on success
     */
    bool exportToJson(const fs::path& filepath, const fs::path& imageDir) const;

    /**
     * @brief Get the current background color
     */
    Color getBackgroundColor() const;

    /**
     * @brief Get total recording duration in milliseconds
     */
    int64_t getRecordingDuration() const;

    // UndoRedoListener interface
    void undoRedoChanged() override;
    void undoRedoPageChanged(PageRef page) override;

private:
    /**
     * @brief Get current timestamp relative to recording start
     */
    int64_t getCurrentTimestamp() const;

    /**
     * @brief Generate unique ID for a new stroke
     */
    int generateStrokeId();

    /**
     * @brief Generate unique ID for a new image
     */
    std::string generateImageId();

    /**
     * @brief Compress idle time in the recording
     */
    void compressIdleTime();

    /**
     * @brief Store image data to file
     */
    bool storeImage(const Image* image, const fs::path& imageDir, const std::string& imageId) const;

    Control* control;
    bool recording;
    int64_t recordingStartTime;
    int64_t lastEventTime;
    int nextStrokeId;
    int nextImageId;
    int currentStrokeId;
    size_t currentPage;

    Color backgroundColor;

    std::vector<RecordEvent> events;
    std::vector<RecordedStroke> strokes;
    std::vector<RecordedImage> images;

    // Maximum idle time to keep in the recording (in ms)
    static constexpr int64_t MAX_IDLE_TIME_MS = 500;
};
