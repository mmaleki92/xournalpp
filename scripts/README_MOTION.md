# Motion Recording Video Rendering Scripts

This directory contains helper scripts for rendering Xournal++ motion recording data to video.

## render_motion_video.py

High-quality motion recording renderer with Cairo graphics, direct video encoding, and optional cursor overlay.

### Requirements

```bash
pip install pycairo
```

For video encoding (optional):
```bash
# Ubuntu/Debian
sudo apt-get install ffmpeg

# macOS
brew install ffmpeg

# Windows
# Download from https://ffmpeg.org/
```

### Quick Start

1. Export motion data from Xournal++:
   - Open a document with motion recording data
   - Go to **Tools > Export Motion Recording** (or press Shift+Alt+M)
   - Choose an output directory

2. Render and create video in one command:
   ```bash
   python scripts/render_motion_video.py motion_metadata.json output/ --video demo.mp4
   ```

### Usage Examples

```bash
# Render frames only (for manual processing)
python scripts/render_motion_video.py motion_metadata.json output/

# Render at 60 FPS and encode to video
python scripts/render_motion_video.py motion_metadata.json output/ --fps 60 --video demo.mp4

# High-quality video with cursor overlay
python scripts/render_motion_video.py motion_metadata.json output/ --video demo.mp4 --quality high --cursor

# Create GIF with cursor and cleanup frames
python scripts/render_motion_video.py motion_metadata.json output/ --video demo.gif --quality gif --cursor --cleanup

# Show all options
python scripts/render_motion_video.py --help
```

### Features

**Rendering Quality:**
- **Cairo graphics**: Professional anti-aliased rendering with smooth curves
- **Pressure-sensitive strokes**: Variable width based on stylus pressure
- **High-resolution output**: Crisp, smooth lines without pixelation

**Drawing Features:**
- **Multi-page documents**: Automatically switches between pages based on timestamps
- **Page backgrounds**: Renders proper background colors and dimensions
- **Stroke styling**: Correct colors, widths, and tool types (pen/highlighter/eraser)
- **Progressive drawing**: Shows strokes being drawn over time frame-by-frame
- **Eraser handling**: Eraser strokes rendered in background color to simulate erasing

**New Features:**
- **Direct video encoding**: Built-in FFmpeg integration (no manual commands needed)
- **Cursor overlay**: Optional pencil/eraser cursor showing current drawing position
- **Quality presets**: High, medium, low, and GIF output options
- **Auto-cleanup**: Optional frame deletion after video encoding

### Command-Line Options

| Option | Description |
|--------|-------------|
| `metadata` | Path to motion_metadata.json file (required) |
| `output_dir` | Directory to save rendered frames (required) |
| `--fps N` | Override frame rate (default: use metadata value) |
| `--video FILE` | Encode frames to video file (requires FFmpeg) |
| `--quality PRESET` | Video quality: high, medium, low, or gif (default: high) |
| `--cursor` | Show pencil/eraser cursor at current drawing position |
| `--cleanup` | Delete frame images after encoding video |
| `--help` | Show help message |

### FPS Behavior

FPS represents the **sampling rate** - how many frames to generate per second of recording time:
- 30 FPS on 2-second recording = 60 frames (2 × 30)
- 60 FPS on 2-second recording = 120 frames (2 × 60)

Higher FPS creates smoother animations but generates more frames.

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

### Example Workflows

**Simple video creation:**
```bash
# Export from Xournal++ (Tools > Export Motion Recording)
# Then render and encode in one command:
python scripts/render_motion_video.py motion_metadata.json frames/ --video my_drawing.mp4
```

**High-quality tutorial video:**
```bash
# Render at 60 FPS with cursor showing what's being drawn
python scripts/render_motion_video.py motion_metadata.json frames/ \
  --fps 60 --video tutorial.mp4 --quality high --cursor --cleanup
```

**Create shareable GIF:**
```bash
# Lower framerate for smaller file size
python scripts/render_motion_video.py motion_metadata.json frames/ \
  --video animation.gif --quality gif --cursor --cleanup
```

**Advanced: Keep frames for editing:**
```bash
# Render frames, encode video, but keep frames for further editing
python scripts/render_motion_video.py motion_metadata.json frames/ \
  --fps 60 --video draft.mp4 --cursor
# Frames remain in frames/ directory for manual editing
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
