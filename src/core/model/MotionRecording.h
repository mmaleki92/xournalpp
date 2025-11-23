/*
 * Xournal++
 *
 * Motion recording data for strokes
 * Records timestamped positions for pen and eraser movements
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <vector>   // for vector

#include "Point.h"  // for Point

class ObjectInputStream;
class ObjectOutputStream;

/**
 * @brief Represents a single recorded motion point with timestamp
 */
struct MotionPoint {
    Point point;           // Position and pressure (if applicable)
    size_t timestamp;      // Timestamp in milliseconds
    bool isEraser;         // True if this was recorded during erasing

    MotionPoint() = default;
    MotionPoint(const Point& p, size_t ts, bool eraser): point(p), timestamp(ts), isEraser(eraser) {}
};

/**
 * @brief Records the full motion of drawing/erasing a stroke
 * 
 * This class stores timestamped position data that captures the complete
 * motion of the pen or eraser while creating a stroke. This data can be
 * used to recreate the drawing motion for video export or animation.
 */
class MotionRecording {
public:
    MotionRecording() = default;
    ~MotionRecording() = default;

    MotionRecording(const MotionRecording&) = default;
    MotionRecording(MotionRecording&&) = default;
    MotionRecording& operator=(const MotionRecording&) = default;
    MotionRecording& operator=(MotionRecording&&) = default;

    /**
     * @brief Add a motion point to the recording
     * @param point The position and pressure data
     * @param timestamp Timestamp in milliseconds
     * @param isEraser True if recording eraser motion
     */
    void addMotionPoint(const Point& point, size_t timestamp, bool isEraser = false);

    /**
     * @brief Get all recorded motion points
     */
    const std::vector<MotionPoint>& getMotionPoints() const;

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

    /**
     * @brief Serialize the motion recording
     */
    void serialize(ObjectOutputStream& out) const;

    /**
     * @brief Deserialize the motion recording
     */
    void readSerialized(ObjectInputStream& in);

private:
    std::vector<MotionPoint> motionPoints{};
};
