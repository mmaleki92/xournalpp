# Motion Export Feature - User Guide

## Overview

The Motion Export feature allows you to export motion recording data from Xournal++ strokes. This data captures the complete drawing motion of both pen and eraser tools, with timestamps and pressure information, enabling you to recreate the drawing process as a video or animation.

## Setup

### 1. Configure Motion Export Settings

1. Open Xournal++
2. Go to **Edit > Preferences** (or press the preferences shortcut)
3. Navigate to the **Audio Recording** tab
4. Scroll down to the **Motion Export Settings** section
5. Configure the following:
   - **Export Folder**: Choose where motion exports will be saved
   - **Frame Rate (fps)**: Set the desired frame rate (1-120 fps, default: 30)
   - **Enable Motion Export Feature**: Make sure this checkbox is checked

6. Click **OK** to save your settings

## Using Motion Export

### Prerequisites

- Your strokes must have motion recording data
- Motion recording is automatically captured when you draw (in supported versions)
- Older strokes without motion data cannot be exported

### Exporting Motion Data

#### Method 1: Using the Menu

1. Open a document containing strokes with motion data
2. Go to **Tools > Export Motion Recording**
3. The export will start automatically
4. A success message will appear when the export is complete

#### Method 2: Using Keyboard Shortcut

1. Open a document containing strokes with motion data
2. Press **Shift + Alt + M**
3. The export will start automatically
4. A success message will appear when the export is complete

### What Gets Exported

Each export creates a timestamped subfolder in your configured export directory with the following files:

1. **motion_metadata.json** - Contains all motion data in JSON format:
   ```json
   {
     "frameRate": 30,
     "totalFrames": 150,
     "totalMotionPoints": 500,
     "minTimestamp": 1000,
     "maxTimestamp": 6000,
     "pages": [
       {
         "pageIndex": 0,
         "strokes": [
           {
             "motionPoints": [
               {
                 "t": 1000,
                 "x": 100.5,
                 "y": 200.3,
                 "p": 0.8,
                 "isEraser": false
               },
               ...
             ]
           }
         ]
       }
     ]
   }
   ```

2. **README.txt** - Instructions and examples for video rendering

### Motion Data Format

Each motion point in the JSON file contains:

- **t** (timestamp): Time in milliseconds since recording started
- **x**: X-coordinate of the pen/eraser position
- **y**: Y-coordinate of the pen/eraser position
- **p** (pressure): Stylus pressure (0.0 to 1.0), or -1.0 if not available
- **isEraser**: Boolean flag (true for eraser, false for pen)

## Creating Videos from Motion Data

### Using FFmpeg (for rendered frames)

If you create frame images from the motion data:

```bash
# Basic video
ffmpeg -framerate 30 -pattern_type glob -i 'frame_*.png' \
       -c:v libx264 -pix_fmt yuv420p output.mp4

# High quality
ffmpeg -framerate 30 -pattern_type glob -i 'frame_*.png' \
       -c:v libx264 -crf 18 -preset slow output.mp4

# Create GIF
ffmpeg -framerate 10 -pattern_type glob -i 'frame_*.png' \
       -vf "scale=800:-1:flags=lanczos" output.gif
```

### Using Custom Rendering Scripts

You can write your own scripts to read the `motion_metadata.json` file and create custom visualizations:

**Python Example:**
```python
import json
import matplotlib.pyplot as plt

# Load motion data
with open('motion_metadata.json', 'r') as f:
    data = json.load(f)

# Extract motion points from first stroke
points = data['pages'][0]['strokes'][0]['motionPoints']

# Visualize the path
x_coords = [p['x'] for p in points]
y_coords = [p['y'] for p in points]
plt.plot(x_coords, y_coords)
plt.savefig('motion_path.png')
```

**JavaScript Example:**
```javascript
const fs = require('fs');

// Load motion data
const data = JSON.parse(fs.readFileSync('motion_metadata.json', 'utf8'));

// Process motion points
data.pages.forEach((page, pageIdx) => {
  page.strokes.forEach((stroke, strokeIdx) => {
    stroke.motionPoints.forEach(point => {
      console.log(`Time: ${point.t}ms, Position: (${point.x}, ${point.y})`);
    });
  });
});
```

## Troubleshooting

### "No motion recording data found in document"

**Problem**: The document doesn't contain any strokes with motion recording data.

**Solutions**:
- Ensure you're using a version of Xournal++ that supports motion recording
- Draw new strokes in the document (motion data is captured automatically)
- Check if the strokes were created in a version that supports motion recording

### "Motion export folder not set or invalid"

**Problem**: The export folder path in preferences is not configured or doesn't exist.

**Solutions**:
- Open Preferences > Audio Recording tab
- Set a valid folder path in the "Motion Export Settings" section
- Ensure you have write permissions to the folder

### "Motion export failed"

**Problem**: The export process encountered an error.

**Solutions**:
- Check that the export folder exists and is writable
- Ensure you have enough disk space
- Verify that the document is saved (unsaved documents may have issues)
- Try exporting a different document to isolate the issue

### Export folder fills up quickly

**Problem**: Each export creates a new timestamped folder, consuming disk space.

**Solutions**:
- Regularly clean up old export folders
- Use a dedicated drive or partition with ample space
- Adjust the frame rate to reduce the number of calculated frames

## Advanced Usage

### Adjusting Frame Rate

The frame rate setting determines how many frames per second are calculated for the export:

- **Low (10-15 fps)**: Smaller file size, choppier motion
- **Medium (24-30 fps)**: Standard video frame rate, smooth motion
- **High (60+ fps)**: Very smooth motion, larger file size

Choose based on your needs:
- For slow, detailed drawings: 24-30 fps is sufficient
- For fast sketching or presentations: 30-60 fps is recommended
- For ultra-smooth playback: 60-120 fps (larger files)

### Batch Processing

To export motion data from multiple documents:

1. Write a script to open each document
2. Trigger the export action programmatically
3. Or manually export each document using Shift+Alt+M

### Integration with Video Editors

You can import the motion metadata into video editing software:

1. Export motion data from Xournal++
2. Create frame images using a custom script
3. Import frame sequence into video editor (Adobe Premiere, DaVinci Resolve, etc.)
4. Add effects, transitions, or overlays
5. Export final video

## Best Practices

1. **Regular Exports**: Export important work regularly to preserve motion data
2. **Organize Folders**: Keep export folders organized by project or date
3. **Test Settings**: Test export settings on a small document first
4. **Backup Data**: Back up both .xopp files and exported motion data
5. **Document Metadata**: Keep notes about what each export contains

## Tips

- Motion data is relatively small compared to video files
- Higher frame rates don't increase file size significantly (metadata-only export)
- The real file size comes from rendered frame images
- You can re-export the same document multiple times with different frame rates
- Export before making major edits to preserve intermediate states

## Support

For issues, questions, or feature requests:
- GitHub Issues: https://github.com/xournalpp/xournalpp/issues
- Documentation: https://github.com/xournalpp/xournalpp
- Motion Recording Documentation: See MOTION_RECORDING.md

## License

This feature is part of Xournal++ and licensed under GPL v2 or later.
