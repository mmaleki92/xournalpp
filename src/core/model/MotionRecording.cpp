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

#include "MotionRecording.h"

#include "util/serializing/ObjectInputStream.h"   // for ObjectInputStream
#include "util/serializing/ObjectOutputStream.h"  // for ObjectOutputStream

void MotionRecording::addMotionPoint(const Point& point, size_t timestamp, bool isEraser) {
    motionPoints.emplace_back(point, timestamp, isEraser);
}

auto MotionRecording::getMotionPoints() const -> const std::vector<MotionPoint>& { return motionPoints; }

auto MotionRecording::hasMotionData() const -> bool { return !motionPoints.empty(); }

auto MotionRecording::getMotionPointCount() const -> size_t { return motionPoints.size(); }

void MotionRecording::clear() { motionPoints.clear(); }

auto MotionRecording::getStartTimestamp() const -> size_t {
    if (motionPoints.empty()) {
        return 0;
    }
    return motionPoints.front().timestamp;
}

auto MotionRecording::getEndTimestamp() const -> size_t {
    if (motionPoints.empty()) {
        return 0;
    }
    return motionPoints.back().timestamp;
}

void MotionRecording::serialize(ObjectOutputStream& out) const {
    out.writeUInt(motionPoints.size());
    for (const auto& mp : motionPoints) {
        out.writeDouble(mp.point.x);
        out.writeDouble(mp.point.y);
        out.writeDouble(mp.point.z);
        out.writeUInt(mp.timestamp);
        out.writeBool(mp.isEraser);
    }
}

void MotionRecording::readSerialized(ObjectInputStream& in) {
    motionPoints.clear();
    size_t count = 0;
    in.readUInt(count);
    motionPoints.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        Point point;
        in.readDouble(point.x);
        in.readDouble(point.y);
        in.readDouble(point.z);
        size_t timestamp = 0;
        in.readUInt(timestamp);
        bool isEraser = false;
        in.readBool(isEraser);
        motionPoints.emplace_back(point, timestamp, isEraser);
    }
}
