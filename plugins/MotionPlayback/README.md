# Motion Playback Plugin

This plugin demonstrates how to access and export motion recording data from Xournal++ strokes. The motion recording feature captures the complete drawing motion of both pen and eraser tools, allowing you to recreate the drawing process as a video.

## Features

- Export motion recording data from Xournal++ documents
- Support for both pen and eraser movements
- Timestamped motion points for accurate playback
- Pressure-sensitive data (when available)
- Ready for video rendering with external tools

## Usage

1. **Create a document with motion recording**:
   - Draw strokes in Xournal++ (future versions will automatically record motion)
   - Both pen and eraser movements will be recorded

2. **Export motion recording**:
   - Save your document first
   - Go to menu: "Export Motion Recording"
   - Or press `Shift+Alt+M`
   - Choose output location for frames/metadata

3. **Render as video**:
   - Use FFmpeg or similar tools to create video from frames
   - Example: `ffmpeg -framerate 30 -i frame_%04d.png -c:v libx264 output.mp4`

## Motion Data Format

Each motion point contains:
- **timestamp**: Time in milliseconds since recording started
- **x, y**: Position coordinates
- **z**: Pressure value (if available, -1 otherwise)
- **isEraser**: Boolean flag (true for eraser, false for pen)

## Video Rendering

### Using FFmpeg

```bash
# Basic video creation from frames
ffmpeg -framerate 30 -pattern_type glob -i 'frame_*.png' \
       -c:v libx264 -pix_fmt yuv420p output.mp4

# With higher quality
ffmpeg -framerate 30 -pattern_type glob -i 'frame_*.png' \
       -c:v libx264 -crf 18 -preset slow output.mp4

# Create GIF animation
ffmpeg -framerate 10 -pattern_type glob -i 'frame_*.png' \
       -vf "scale=800:-1:flags=lanczos" output.gif
```

## Customization

### Custom Pen/Eraser Icons

Place custom images in the output directory:
- `pen_icon.png` - Icon shown for pen movements
- `eraser_icon.png` - Icon shown for eraser movements

The plugin will overlay these icons at the recorded positions during rendering.

### Playback Speed

Adjust the `-framerate` parameter in FFmpeg:
- Slower: `-framerate 15` (15 fps)
- Normal: `-framerate 30` (30 fps)
- Faster: `-framerate 60` (60 fps)

## File Format

Motion recording data is stored in the .xopp file format as an optional attribute on stroke elements:

```xml
<stroke tool="pen" color="#000000ff" width="2.0" motion="1000,100.5,200.3,0.8,0 1050,101.2,201.1,0.9,0 ...">
  100.5 200.3 101.2 201.1 ...
</stroke>
```

The `motion` attribute contains space-separated tuples of:
`timestamp,x,y,pressure,isEraser`

This format is fully backward compatible - older versions of Xournal++ simply ignore the motion attribute.

## Requirements

- Xournal++ with motion recording support
- FFmpeg (for video rendering)
- Optional: Image editing software for custom icons

## Future Enhancements

- Real-time preview of motion playback
- Built-in video export (without FFmpeg)
- Configurable playback speed in the plugin
- Audio synchronization (if audio recording is also used)
- Multiple stroke animation simultaneously

## Technical Details

The motion recording feature:
- Records position updates at high frequency during drawing
- Minimal storage overhead (compressed in file format)
- Backward compatible with older Xournal++ versions
- Supports pressure-sensitive tablets
- Distinguishes between pen and eraser tools

## License

GPL v2 or later (same as Xournal++)
