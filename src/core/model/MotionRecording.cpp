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
    out.writeUInt(static_cast<uint32_t>(motionPoints.size()));
    for (const auto& mp : motionPoints) {
        out.writeDouble(mp.point.x);
        out.writeDouble(mp.point.y);
        out.writeDouble(mp.point.z);
        out.writeUInt(static_cast<uint32_t>(mp.timestamp));
        out.writeBool(mp.isEraser);
    }
}

void MotionRecording::readSerialized(ObjectInputStream& in) {
    motionPoints.clear();
    uint32_t count = in.readUInt();
    motionPoints.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        Point point;
        point.x = in.readDouble();
        point.y = in.readDouble();
        point.z = in.readDouble();
        size_t timestamp = static_cast<size_t>(in.readUInt());
        bool isEraser = in.readBool();
        motionPoints.emplace_back(point, timestamp, isEraser);
    }
}
