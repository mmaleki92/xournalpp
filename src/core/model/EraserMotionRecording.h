/*
 * Xournal++
 *
 * Eraser motion recording data
 * Records timestamped positions and affected strokes during erasing
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <string>   // for string
#include <vector>   // for vector

#include "Point.h"  // for Point

/**
 * @brief Represents a single eraser motion point with timestamp and affected stroke info
 */
struct EraserMotionPoint {
    Point point;           // Position of eraser (x, y, pressure if available)
    size_t timestamp;      // Timestamp in milliseconds
    double eraserSize;     // Size of eraser at this point
    
    // Information about which strokes were affected at this point
    std::vector<size_t> affectedStrokeIndices;  // Indices of strokes affected
    
    EraserMotionPoint() = default;
    EraserMotionPoint(const Point& p, size_t ts, double size): 
        point(p), timestamp(ts), eraserSize(size) {}
    
    void addAffectedStroke(size_t strokeIndex) {
        affectedStrokeIndices.push_back(strokeIndex);
    }
};

/**
 * @brief Records the full motion of erasing strokes
 * 
 * This class stores timestamped position data and information about which
 * strokes were affected during erasing. This data can be used to recreate
 * the erasing motion for video export or animation.
 */
class EraserMotionRecording {
public:
    EraserMotionRecording() = default;
    ~EraserMotionRecording() = default;

    EraserMotionRecording(const EraserMotionRecording&) = default;
    EraserMotionRecording(EraserMotionRecording&&) = default;
    EraserMotionRecording& operator=(const EraserMotionRecording&) = default;
    EraserMotionRecording& operator=(EraserMotionRecording&&) = default;

    /**
     * @brief Add an eraser motion point to the recording
     * @param point The position data
     * @param timestamp Timestamp in milliseconds
     * @param eraserSize Size of the eraser
     */
    void addMotionPoint(const Point& point, size_t timestamp, double eraserSize);

    /**
     * @brief Add information about an affected stroke to the last motion point
     * @param strokeIndex Index of the affected stroke
     */
    void addAffectedStrokeToLast(size_t strokeIndex);

    /**
     * @brief Get all recorded eraser motion points
     */
    const std::vector<EraserMotionPoint>& getMotionPoints() const;

    /**
     * @brief Check if this recording has any motion data
     */
    bool hasMotionData() const;

    /**
     * @brief Get the number of recorded motion points
     */
    size_t getMotionPointCount() const;

    /**
     * @brief Clear all recorded motion data
     */
    void clear();

    /**
     * @brief Get the start timestamp of the recording
     */
    size_t getStartTimestamp() const;

    /**
     * @brief Get the end timestamp of the recording
     */
    size_t getEndTimestamp() const;

private:
    std::vector<EraserMotionPoint> motionPoints{};
};
