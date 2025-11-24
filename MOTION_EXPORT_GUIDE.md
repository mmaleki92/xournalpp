# Motion Export Feature - User Guide

## Overview

The Motion Export feature allows you to export motion recording data from Xournal++ strokes. This data captures the complete drawing motion of both pen and eraser tools, with timestamps and pressure information, enabling you to recreate the drawing process as a video or animation.

The exported `motion_metadata.json` includes complete page styling (dimensions, background type/color) and stroke properties (color, width, tool type, line style), allowing for accurate video reproduction of your drawings.

**Quick Start**: Use the included Python script to render videos:
```bash
python scripts/render_motion_video.py motion_metadata.json output_frames/
ffmpeg -framerate 30 -i output_frames/frame_%06d.png -c:v libx264 -pix_fmt yuv420p output.mp4
```
See [scripts/README_MOTION.md](scripts/README_MOTION.md) for detailed instructions.

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
     "totalDurationMs": 5000,
     "pages": [
       {
         "pageIndex": 0,
         "width": 612.0,
         "height": 792.0,
         "background": {
           "type": "lined",
           "config": "",
           "color": {
             "r": 255,
             "g": 255,
             "b": 255,
             "a": 255
           }
         },
         "strokes": [
           {
             "tool": "pen",
             "width": 2.0,
             "color": {
               "r": 0,
               "g": 0,
               "b": 0,
               "a": 255
             },
             "fill": -1,
             "lineStyle": {
               "hasDashes": false
             },
             "motionPoints": [
               {
                 "t": 0,
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
   
   **Note**: Timestamps in `motionPoints` are normalized per-stroke (starting from 0 for each stroke), which excludes idle time between strokes. The `totalDurationMs` field represents the sum of all stroke durations without idle time.

2. **README.txt** - Instructions and examples for video rendering

### Motion Data Format

Each page in the JSON file contains:

- **pageIndex**: Zero-based page number
- **width**: Page width in points
- **height**: Page height in points
- **background**: Page background information
  - **type**: Background type (plain, ruled, lined, staves, graph, dotted, isodotted, isograph, pdf, image)
  - **config**: Additional background configuration string
  - **color**: Background color in RGBA format
    - **r**: Red component (0-255)
    - **g**: Green component (0-255)
    - **b**: Blue component (0-255)
    - **a**: Alpha component (0-255)

Each stroke contains:

- **tool**: Tool type used (pen, highlighter, eraser)
- **width**: Stroke width in points
- **color**: Stroke color in RGBA format
  - **r**: Red component (0-255)
  - **g**: Green component (0-255)
  - **b**: Blue component (0-255)
  - **a**: Alpha component (0-255)
- **fill**: Fill opacity as integer
  - **-1**: No fill (shape is not filled)
  - **1-255**: Fill opacity (1 = nearly transparent, 255 = fully opaque)
- **lineStyle**: Line style information
  - **hasDashes**: Whether the stroke has a dashed pattern
  - **dashes**: Array of dash lengths in points (only present if hasDashes is true)

**Note about Eraser Strokes:**
When using the default eraser to erase content, eraser motion is recorded in separate strokes with `"tool": "eraser"`. These strokes capture where and when erasing occurred. When rendering videos:
- Draw pen/highlighter strokes normally
- Use eraser strokes to show where erasing happened
- Progressively remove or fade out portions of strokes that were erased
- The `isEraser` flag in motion points indicates eraser movement

Each motion point in a stroke contains:

- **t** (timestamp): Time in milliseconds relative to the stroke's start (always starts from 0 for each stroke)
- **x**: X-coordinate of the pen/eraser position
- **y**: Y-coordinate of the pen/eraser position
- **p** (pressure): Stylus pressure (0.0 to 1.0), or -1.0 if not available
- **isEraser**: Boolean flag (true for eraser motion points, false for pen/highlighter)

**Important**: Timestamps are normalized per-stroke, meaning each stroke's timestamps start from 0. This removes idle time between strokes, making the exported data more compact and video rendering more efficient. To render a complete timeline, accumulate stroke durations sequentially.

## Creating Videos from Motion Data

The enhanced motion_metadata.json now includes complete page styling (dimensions, background type and color) and stroke properties (color, width, tool type, line style). This enables you to create accurate video reproductions of the drawing process.

### Video Rendering Strategy

The metadata supports proper multi-page rendering:
1. **Page transitions**: When a new page starts, clear the canvas and render the new page's background
2. **Background rendering**: Use the page's background type and color to set up the canvas
3. **Stroke rendering**: Apply each stroke's color, width, and line style when drawing
4. **Timeline sequencing**: Use timestamps to create smooth animations

### Using FFmpeg (for rendered frames)

After creating frame images from the motion data with a custom script:

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
from PIL import Image, ImageDraw

# Load motion data
with open('motion_metadata.json', 'r') as f:
    data = json.load(f)

# Process each page
for page in data['pages']:
    # Create canvas with page dimensions and background color
    width = int(page['width'])
    height = int(page['height'])
    bg_color = (page['background']['color']['r'],
                page['background']['color']['g'],
                page['background']['color']['b'],
                page['background']['color']['a'])
    
    img = Image.new('RGBA', (width, height), bg_color)
    draw = ImageDraw.Draw(img)
    
    # Draw each stroke with its motion
    for stroke in page['strokes']:
        stroke_color = (stroke['color']['r'],
                       stroke['color']['g'],
                       stroke['color']['b'],
                       stroke['color']['a'])
        stroke_width = int(stroke['width'])
        
        points = stroke['motionPoints']
        # Animate the drawing motion frame by frame
        for i in range(len(points)):
            if i > 0:
                prev_point = points[i-1]
                curr_point = points[i]
                draw.line([(prev_point['x'], prev_point['y']),
                          (curr_point['x'], curr_point['y'])],
                         fill=stroke_color, width=stroke_width)
    
    img.save(f"page_{page['pageIndex']}.png")
```

**JavaScript/Node.js Example with Canvas:**
```javascript
const fs = require('fs');
const { createCanvas } = require('canvas');

// Load motion data
const data = JSON.parse(fs.readFileSync('motion_metadata.json', 'utf8'));

// Process each page
data.pages.forEach((page, pageIdx) => {
  // Create canvas with page dimensions
  const canvas = createCanvas(page.width, page.height);
  const ctx = canvas.getContext('2d');
  
  // Set background color
  const bg = page.background.color;
  ctx.fillStyle = `rgba(${bg.r}, ${bg.g}, ${bg.b}, ${bg.a / 255})`;
  ctx.fillRect(0, 0, page.width, page.height);
  
  // Draw each stroke with its properties
  page.strokes.forEach((stroke) => {
    const color = stroke.color;
    ctx.strokeStyle = `rgba(${color.r}, ${color.g}, ${color.b}, ${color.a / 255})`;
    ctx.lineWidth = stroke.width;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    
    // Apply dash pattern if present
    if (stroke.lineStyle.hasDashes) {
      ctx.setLineDash(stroke.lineStyle.dashes);
    } else {
      ctx.setLineDash([]);
    }
    
    // Draw motion path
    ctx.beginPath();
    stroke.motionPoints.forEach((point, idx) => {
      if (idx === 0) {
        ctx.moveTo(point.x, point.y);
      } else {
        ctx.lineTo(point.x, point.y);
      }
    });
    ctx.stroke();
  });
  
  // Save frame
  const buffer = canvas.toBuffer('image/png');
  fs.writeFileSync(`page_${pageIdx}.png`, buffer);
});
```

### Complete Video Rendering Example (Python)

Here's a complete example that renders video with proper multi-page handling, showing strokes being drawn over time:

```python
import json
from PIL import Image, ImageDraw
import os

def render_motion_video(metadata_path, output_dir, fps=30):
    """
    Render motion recording to video frames.
    Each frame shows the progressive drawing up to that timestamp.
    When a new page appears, the canvas is cleared and redrawn with the new page's background.
    """
    # Load motion data
    with open(metadata_path, 'r') as f:
        data = json.load(f)
    
    frame_rate = data['frameRate']
    min_time = data['minTimestamp']
    max_time = data['maxTimestamp']
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Calculate frame times
    duration_ms = max_time - min_time
    num_frames = int((duration_ms * frame_rate) / 1000) + 1
    frame_interval_ms = 1000 / frame_rate
    
    frame_number = 0
    
    # Render each frame
    for frame_idx in range(num_frames):
        current_time = min_time + (frame_idx * frame_interval_ms)
        
        # Find which page we're on based on timestamp
        current_page = None
        for page in data['pages']:
            # Check if any stroke in this page has motion at current_time
            for stroke in page['strokes']:
                if stroke['motionPoints']:
                    first_time = stroke['motionPoints'][0]['t']
                    last_time = stroke['motionPoints'][-1]['t']
                    if first_time <= current_time <= last_time:
                        current_page = page
                        break
            if current_page:
                break
        
        # If no page found, use the first page
        if not current_page:
            current_page = data['pages'][0]
        
        # Create canvas with page background
        width = int(current_page['width'])
        height = int(current_page['height'])
        bg = current_page['background']['color']
        bg_color = (bg['r'], bg['g'], bg['b'], bg['a'])
        
        img = Image.new('RGBA', (width, height), bg_color)
        draw = ImageDraw.Draw(img)
        
        # Draw all strokes up to current_time
        for stroke in current_page['strokes']:
            stroke_color = (stroke['color']['r'],
                          stroke['color']['g'],
                          stroke['color']['b'],
                          stroke['color']['a'])
            stroke_width = int(stroke['width'])
            
            # Collect points up to current_time
            visible_points = []
            for point in stroke['motionPoints']:
                if point['t'] <= current_time:
                    visible_points.append(point)
                else:
                    break
            
            # Draw the stroke progressively
            if len(visible_points) > 1:
                for i in range(1, len(visible_points)):
                    prev = visible_points[i-1]
                    curr = visible_points[i]
                    
                    # Handle eraser: draw in background color
                    if curr['isEraser']:
                        draw_color = bg_color
                    else:
                        draw_color = stroke_color
                    
                    draw.line([(prev['x'], prev['y']), 
                              (curr['x'], curr['y'])],
                             fill=draw_color, width=stroke_width)
        
        # Save frame
        frame_path = os.path.join(output_dir, f'frame_{frame_number:06d}.png')
        img.save(frame_path)
        frame_number += 1
        
        if frame_number % 30 == 0:
            print(f"Rendered {frame_number}/{num_frames} frames...")
    
    print(f"Rendering complete! {frame_number} frames saved to {output_dir}")
    print(f"\nCreate video with:")
    print(f"ffmpeg -framerate {frame_rate} -i {output_dir}/frame_%06d.png -c:v libx264 -pix_fmt yuv420p output.mp4")

# Usage
render_motion_video('motion_metadata.json', 'output_frames', fps=30)
```

This script:
- Handles multi-page documents by detecting page transitions
- Renders proper background colors for each page
- Shows progressive drawing of strokes over time
- Handles eraser strokes by drawing in background color (simple approach)
- Applies correct stroke colors and widths
- Generates frames ready for FFmpeg conversion

**Note about Eraser Rendering:**
The simple approach above treats eraser strokes (tool="eraser") as drawing in the background color. For more sophisticated rendering that shows the actual erasing process:

1. **Track which strokes exist at each timestamp**
2. **When encountering an eraser stroke**, determine which drawn strokes it overlaps
3. **Progressively remove or fade** the portions of those strokes that intersect with the eraser path
4. **Use the eraser's width** (from the stroke's width property) to determine the eraser size

Example advanced eraser handling:
```python
def apply_eraser_to_strokes(eraser_stroke, drawn_strokes, current_time):
    """
    Apply eraser motion to existing drawn strokes.
    Returns modified versions of strokes with erased portions removed.
    """
    eraser_points = [p for p in eraser_stroke['motionPoints'] if p['t'] <= current_time]
    eraser_width = eraser_stroke['width']
    
    modified_strokes = []
    for drawn_stroke in drawn_strokes:
        # Check if eraser path intersects with this stroke
        # For each eraser position, remove nearby portions of drawn_stroke
        # This requires geometric intersection calculations
        # (Implementation details depend on your rendering library)
        pass
    
    return modified_strokes
```

This advanced approach provides a more accurate recreation of the erasing process, showing strokes being progressively removed as the eraser moves over them.

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
