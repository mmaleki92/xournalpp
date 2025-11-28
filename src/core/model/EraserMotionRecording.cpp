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

#include "EraserMotionRecording.h"

void EraserMotionRecording::addMotionPoint(const Point& point, size_t timestamp, double eraserSize, size_t pageIndex) {
    motionPoints.emplace_back(point, timestamp, eraserSize, pageIndex);
}

void EraserMotionRecording::addAffectedStrokeToLast(size_t strokeIndex) {
    if (!motionPoints.empty()) {
        motionPoints.back().addAffectedStroke(strokeIndex);
    }
}

auto EraserMotionRecording::getMotionPoints() const -> const std::vector<EraserMotionPoint>& { 
    return motionPoints; 
}

auto EraserMotionRecording::hasMotionData() const -> bool { 
    return !motionPoints.empty(); 
}

auto EraserMotionRecording::getMotionPointCount() const -> size_t { 
    return motionPoints.size(); 
}

void EraserMotionRecording::clear() { 
    motionPoints.clear(); 
}

auto EraserMotionRecording::getStartTimestamp() const -> size_t {
    if (motionPoints.empty()) {
        return 0;
    }
    return motionPoints.front().timestamp;
}

auto EraserMotionRecording::getEndTimestamp() const -> size_t {
    if (motionPoints.empty()) {
        return 0;
    }
    return motionPoints.back().timestamp;
}
