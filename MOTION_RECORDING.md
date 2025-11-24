# Motion Recording Feature

## Overview

Xournal++ now supports recording the complete motion of pen and eraser strokes. This feature captures timestamped position data during drawing, allowing you to recreate the drawing process as a video or animation.

## Key Features

- **Records both pen and eraser movements** with timestamps
- **Eraser motion tracking** - Records where and when erasing occurs (for default eraser mode)
- **Pressure-sensitive data** captured (when available from tablet)
- **Backward compatible** - files with motion data work in older versions (motion data is simply ignored)
- **Minimal storage overhead** - motion data stored efficiently in the file format
- **Plugin-friendly** - easy access to motion data via Lua plugins

## File Format

Motion recording data is stored as an optional attribute in the .xopp XML format:

```xml
<stroke tool="pen" color="#000000ff" width="2.0" 
        motion="1000,100.5,200.3,0.8,0 1050,101.2,201.1,0.9,0">
  100.5 200.3 101.2 201.1
</stroke>

<stroke tool="eraser" color="#000000ff" width="10.0"
        motion="2000,150.0,180.0,-1.0,1 2050,151.5,181.2,-1.0,1">
  150.0 180.0 151.5 181.2
</stroke>
```

### Motion Data Format

The `motion` attribute contains space-separated tuples:
```
timestamp,x,y,pressure,isEraser timestamp,x,y,pressure,isEraser ...
```

Where:
- **timestamp**: Time in milliseconds (relative to recording start)
- **x, y**: Position coordinates
- **pressure**: Stylus pressure (0.0 to 1.0), or -1.0 if not available
- **isEraser**: 1 for eraser, 0 for pen/highlighter

## Backward Compatibility

The motion recording feature is designed to be fully backward compatible:

1. **Old files in new version**: Files without motion data load normally
2. **New files in old version**: The `motion` attribute is ignored, strokes display correctly
3. **Storage format**: Motion data is optional - strokes without motion work as before

## Usage

### Recording Motion

Motion recording is automatically enabled during drawing and erasing:

**For Pen/Highlighter Strokes:**
1. Start drawing a stroke
2. Position and pressure data is recorded at high frequency
3. When stroke is complete, motion data is attached
4. File save includes motion data

**For Eraser (Default Mode):**
1. When using the default eraser to erase strokes
2. The eraser position and movement is recorded with timestamps
3. An eraser stroke (tool type: eraser) is created to store this motion
4. The eraser stroke shows where and when erasing occurred
5. Both the modified strokes AND eraser strokes are saved/exported

**Note about Eraser Types:**
- **Default Eraser**: Modifies strokes by splitting/removing parts. Motion is now recorded in separate eraser strokes.
- **Whiteout Eraser**: Creates white strokes that cover content. Motion is recorded in those white strokes (as before).
- **Delete Stroke Eraser**: Removes entire strokes. No motion recording (stroke is deleted).

### Exporting Motion Data

Motion export is now integrated into Xournal++ preferences:

1. **Configure Export Settings**:
   - Open Preferences (Edit > Preferences)
   - Navigate to "Audio Recording" tab
   - Scroll down to "Motion Export Settings" section
   - Set the export folder path
   - Configure frame rate (default: 30 fps)
   - Ensure "Enable Motion Export Feature" is checked

2. **Export Motion Data**:
   - Create or open a document with strokes containing motion data
   - Menu: Tools > "Export Motion Recording" (or press Shift+Alt+M)
   - Motion data will be exported to a timestamped subfolder
   - Output includes:
     - `motion_metadata.json` - Complete motion data
     - `README.txt` - Instructions for video rendering

3. **Create Video** (using external tools):
   ```bash
   # If you have generated frame images (custom script):
   ffmpeg -framerate 30 -pattern_type glob -i 'frame_*.png' \
          -c:v libx264 -pix_fmt yuv420p output.mp4
   
   # Use the JSON metadata for custom rendering
   python render_motion.py motion_metadata.json
   ```

**Note**: The old MotionPlayback plugin has been replaced with this integrated feature.

## API Reference

### C++ API

```cpp
#include "model/MotionRecording.h"
#include "model/Stroke.h"

// Create motion recording
auto motionRec = std::make_unique<MotionRecording>();

// Add motion points
motionRec->addMotionPoint(Point(100.0, 200.0, 0.8), 1000, false); // pen
motionRec->addMotionPoint(Point(150.0, 180.0, -1.0), 2000, true);  // eraser

// Attach to stroke
stroke->setMotionRecording(std::move(motionRec));

// Query motion data
if (stroke->hasMotionRecording()) {
    auto* motion = stroke->getMotionRecording();
    for (const auto& mp : motion->getMotionPoints()) {
        // Process motion point
        size_t timestamp = mp.timestamp;
        double x = mp.point.x;
        double y = mp.point.y;
        double pressure = mp.point.z;
        bool isEraser = mp.isEraser;
    }
}
```

### Lua Plugin API (Planned)

```lua
-- Access motion recording data
local docStructure = app.getDocumentStructure()
for pageNum, page in ipairs(docStructure.pages) do
    for layerNum, layer in ipairs(page.layers) do
        for elemNum, element in ipairs(layer.elements) do
            if element.type == "stroke" and element.hasMotionRecording then
                for i, mp in ipairs(element.motionPoints) do
                    -- Process motion point
                    local timestamp = mp.timestamp
                    local x, y = mp.x, mp.y
                    local pressure = mp.pressure
                    local isEraser = mp.isEraser
                end
            end
        end
    end
end
```

## Implementation Details

### Data Structures

**MotionPoint** (`src/core/model/MotionRecording.h`):
```cpp
struct MotionPoint {
    Point point;           // x, y, pressure
    size_t timestamp;      // milliseconds
    bool isEraser;         // tool type flag
};
```

**MotionRecording** class:
- Stores vector of MotionPoint objects
- Provides serialization/deserialization
- Memory efficient (shared_ptr in Stroke)

### File Format Details

**XML Serialization** (`src/core/control/xojfile/SaveHandler.cpp`):
- Motion data written as `motion` attribute
- Only present if stroke has motion recording
- Space-separated for readability and parsing

**XML Deserialization** (`src/core/control/xojfile/LoadHandler.cpp`):
- Optional attribute parsing
- Gracefully handles missing motion data
- Validates data format during parsing

**Binary Serialization** (`src/core/model/Stroke.cpp`):
- Used for clipboard operations
- Includes motion data in binary format
- Backward compatible flag system

## Performance Considerations

- **Recording overhead**: Minimal - only captures position updates
- **File size**: ~20-30 bytes per motion point (compressed in XML)
- **Memory usage**: Motion data only loaded when stroke is accessed
- **Playback**: Efficient iteration through timestamped points

## Future Enhancements

1. **Real-time preview**: Preview motion playback in the app
2. **Recording controls**: Enable/disable motion recording per document
3. **Compression**: Further optimize motion data storage
4. **Interpolation**: Smooth motion between recorded points
5. **Audio sync**: Synchronize motion with audio recordings
6. **Multi-stroke animation**: Render multiple strokes simultaneously

## Examples

### Simple Motion Recording
```cpp
// Create a stroke with motion
auto stroke = std::make_unique<Stroke>();
auto motion = std::make_unique<MotionRecording>();

// Record a simple line motion
size_t startTime = getCurrentTimeMs();
for (int i = 0; i <= 100; i++) {
    double x = i * 2.0;
    double y = 50.0;
    size_t timestamp = startTime + i * 10; // 10ms intervals
    motion->addMotionPoint(Point(x, y, 0.5), timestamp, false);
}

stroke->setMotionRecording(std::move(motion));
```

### Export Motion as JSON
```lua
-- Plugin example: export motion as JSON
function exportMotionAsJSON(stroke)
    local motion = stroke.motionPoints
    local json = "{\n  \"motion\": [\n"
    for i, mp in ipairs(motion) do
        json = json .. string.format(
            "    {\"t\":%d,\"x\":%.2f,\"y\":%.2f,\"p\":%.2f,\"e\":%s}",
            mp.timestamp, mp.x, mp.y, mp.pressure,
            mp.isEraser and "true" or "false"
        )
        if i < #motion then json = json .. ",\n" end
    end
    json = json .. "\n  ]\n}"
    return json
end
```

## Troubleshooting

**Q: Motion data not saving?**
A: Check that strokes have motion recording attached before saving.

**Q: File size increased significantly?**
A: Motion data adds ~30 bytes per motion point. Adjust recording frequency if needed.

**Q: Old version can't open file?**
A: Motion recording is backward compatible. If issues occur, ensure file version is correct.

**Q: Motion playback is choppy?**
A: Increase recording frequency or use interpolation during playback.

## Contributing

To extend the motion recording feature:

1. **Add new motion attributes**: Extend `MotionPoint` structure
2. **Custom plugins**: Use Lua API to process motion data
3. **Rendering engines**: Implement custom video exporters
4. **Testing**: Add unit tests in `test/unit_tests/model/`

## License

This feature is part of Xournal++ and licensed under GPL v2 or later.
