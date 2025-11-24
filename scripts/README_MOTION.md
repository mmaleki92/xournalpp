# Motion Recording Video Rendering Scripts

This directory contains helper scripts for rendering Xournal++ motion recording data to video.

## render_motion_video.py

Renders motion_metadata.json to video frames that can be converted to video using FFmpeg.

### Requirements

```bash
pip install pillow
```

### Usage

1. Export motion data from Xournal++:
   - Open a document with motion recording data
   - Go to **Tools > Export Motion Recording** (or press Shift+Alt+M)
   - Choose an output directory

2. Render frames:
   ```bash
   python scripts/render_motion_video.py path/to/motion_metadata.json output_frames/
   ```

3. Create video with FFmpeg:
   ```bash
   # Standard quality video
   ffmpeg -framerate 30 -i output_frames/frame_%06d.png -c:v libx264 -pix_fmt yuv420p output.mp4
   
   # High quality video
   ffmpeg -framerate 30 -i output_frames/frame_%06d.png -c:v libx264 -crf 18 -preset slow output.mp4
   
   # Create animated GIF
   ffmpeg -framerate 10 -i output_frames/frame_%06d.png -vf "scale=800:-1:flags=lanczos" output.gif
   ```

### Features

The script correctly handles:
- **Pressure-sensitive rendering**: Stroke width varies based on stylus pressure at each point
- **Multi-page documents**: Automatically switches between pages based on timestamps
- **Page backgrounds**: Renders proper background colors and dimensions for each page
- **Stroke styling**: Applies correct colors, widths, and tool types (pen/highlighter/eraser)
- **Progressive drawing**: Shows strokes being drawn over time frame-by-frame
- **Eraser handling**: Eraser strokes are rendered in the background color to simulate erasing

### Options

```bash
# Use default frame rate from metadata
python scripts/render_motion_video.py motion_metadata.json output_frames/

# Override frame rate (e.g., 60 fps for smoother animation)
python scripts/render_motion_video.py motion_metadata.json output_frames/ 60
```

### Output

The script generates sequentially numbered PNG frames:
```
output_frames/
  frame_000000.png
  frame_000001.png
  frame_000002.png
  ...
```

These frames can then be combined into a video using FFmpeg or any video editing software.

### Example Workflow

```bash
# 1. Export motion data from Xournal++ (via Tools > Export Motion Recording)
#    This creates a directory with motion_metadata.json

# 2. Render to frames
python scripts/render_motion_video.py export_20231124_120000/motion_metadata.json frames/

# 3. Create video
ffmpeg -framerate 30 -i frames/frame_%06d.png -c:v libx264 -pix_fmt yuv420p drawing.mp4

# 4. (Optional) Clean up frames
rm -rf frames/
```

### Troubleshooting

**"No module named 'PIL'"**
- Install Pillow: `pip install pillow`

**"File not found" error**
- Check that the path to motion_metadata.json is correct
- Ensure you've exported motion data from Xournal++ first

**Video playback issues**
- Use `-pix_fmt yuv420p` flag with FFmpeg for better compatibility
- Try different video codecs if needed (e.g., `-c:v libx265` for H.265)

### Performance

Rendering time depends on:
- Number of frames (document duration × frame rate)
- Page dimensions (larger pages take longer)
- Number of strokes and motion points

Typical performance:
- ~100-200 frames per second on modern hardware
- A 1-minute recording at 30 fps (~1800 frames) takes ~10-20 seconds to render

## See Also

- [MOTION_EXPORT_GUIDE.md](../MOTION_EXPORT_GUIDE.md) - Complete guide to motion export feature
- [MOTION_RECORDING.md](../MOTION_RECORDING.md) - Technical documentation for motion recording
