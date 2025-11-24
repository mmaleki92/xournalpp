#!/usr/bin/env python3
"""
Render Xournal++ motion recording data to video frames.

This script reads the motion_metadata.json file exported from Xournal++
and renders it frame-by-frame, creating images that can be converted to video.

Features:
    - Pressure-sensitive rendering: stroke width varies based on stylus pressure
    - Multi-page support: handles documents with multiple pages
    - Progressive drawing: shows strokes being drawn over time
    - Eraser support: renders eraser strokes correctly
    - High-quality rendering with Cairo for smooth, anti-aliased output

Requirements:
    pip install pycairo
    
    For video encoding (optional):
    - FFmpeg (https://ffmpeg.org/)

Usage:
    # Render frames only
    python render_motion_video.py motion_metadata.json output_frames/
    
    # Render and encode to video directly
    python render_motion_video.py motion_metadata.json output_frames/ --video output.mp4
    
    # Render at custom FPS and encode high-quality video
    python render_motion_video.py motion_metadata.json output_frames/ --fps 60 --video output.mp4 --quality high
    
    # Encode to GIF and cleanup frames
    python render_motion_video.py motion_metadata.json output_frames/ --video output.gif --quality gif --cleanup
"""

import json
import os
import sys
import subprocess
import shutil
import cairo


def get_normalized_pressure(point):
    """
    Get normalized pressure from a motion point.
    
    Args:
        point: Dictionary containing motion point data with optional 'p' field
    
    Returns:
        float: Pressure value between 0.0 and 1.0
               - Returns 1.0 if 'p' field is missing (old format, no pressure data)
               - Returns 1.0 if 'p' is -1.0 (pressure sensor not available)
               - Returns the actual pressure value if 'p' is in range [0.0, 1.0]
    """
    pressure = point.get('p', -1.0)
    # Treat missing field (default -1.0) or any negative pressure value as full pressure (1.0)
    # This handles: no pressure sensor (p=-1.0), old format (no 'p' field), or invalid values
    return 1.0 if pressure < 0.0 else pressure


def encode_video(frames_dir, output_video, frame_rate, quality='high', cleanup_frames=False):
    """
    Encode rendered frames into a video file using FFmpeg.
    
    Args:
        frames_dir: Directory containing the frame images
        output_video: Path to output video file
        frame_rate: Video frame rate
        quality: Video quality - 'high', 'medium', 'low', or 'gif'
        cleanup_frames: If True, delete frame images after encoding
    
    Returns:
        bool: True if encoding succeeded, False otherwise
    """
    # Check if FFmpeg is available
    if not shutil.which('ffmpeg'):
        print("Error: FFmpeg not found. Please install FFmpeg to encode videos.")
        print("  Ubuntu/Debian: sudo apt-get install ffmpeg")
        print("  macOS: brew install ffmpeg")
        print("  Windows: Download from https://ffmpeg.org/")
        return False
    
    print(f"\nEncoding video to {output_video}...")
    
    # Build FFmpeg command based on quality setting
    if quality == 'gif':
        # Create animated GIF
        # Limit framerate to 20 FPS for reasonable file size (GIFs don't benefit from higher FPS)
        # Scale to 800px width to reduce file size while maintaining readability
        cmd = [
            'ffmpeg', '-y',
            '-framerate', str(min(frame_rate, 20)),
            '-i', os.path.join(frames_dir, 'frame_%06d.png'),
            '-vf', 'scale=800:-1:flags=lanczos',
            output_video
        ]
    elif quality == 'high':
        # High quality H.264
        cmd = [
            'ffmpeg', '-y',
            '-framerate', str(frame_rate),
            '-i', os.path.join(frames_dir, 'frame_%06d.png'),
            '-c:v', 'libx264',
            '-crf', '18',
            '-preset', 'slow',
            '-pix_fmt', 'yuv420p',
            output_video
        ]
    elif quality == 'medium':
        # Medium quality H.264
        cmd = [
            'ffmpeg', '-y',
            '-framerate', str(frame_rate),
            '-i', os.path.join(frames_dir, 'frame_%06d.png'),
            '-c:v', 'libx264',
            '-crf', '23',
            '-preset', 'medium',
            '-pix_fmt', 'yuv420p',
            output_video
        ]
    else:  # low
        # Low quality/fast encode
        cmd = [
            'ffmpeg', '-y',
            '-framerate', str(frame_rate),
            '-i', os.path.join(frames_dir, 'frame_%06d.png'),
            '-c:v', 'libx264',
            '-crf', '28',
            '-preset', 'fast',
            '-pix_fmt', 'yuv420p',
            output_video
        ]
    
    try:
        # Run FFmpeg
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"✓ Video encoded successfully: {output_video}")
            
            # Cleanup frames if requested
            if cleanup_frames:
                print(f"Cleaning up frame images from {frames_dir}...")
                for file in os.listdir(frames_dir):
                    if file.startswith('frame_') and file.endswith('.png'):
                        os.remove(os.path.join(frames_dir, file))
                print("✓ Frame cleanup complete")
            
            return True
        else:
            print(f"✗ FFmpeg encoding failed:")
            print(result.stderr)
            return False
            
    except Exception as e:
        print(f"✗ Error during video encoding: {e}")
        return False


def draw_pencil_cursor(ctx, x, y, color, size=20):
    """
    Draw a pencil icon at the specified position.
    
    Args:
        ctx: Cairo context
        x, y: Position to draw the pencil tip
        color: RGB color tuple (r, g, b) normalized 0-1
        size: Size of the pencil icon
    """
    # Save context state
    ctx.save()
    
    # Pencil body (wood part)
    ctx.set_source_rgb(0.8, 0.6, 0.2)  # Wood color
    ctx.move_to(x, y)
    ctx.line_to(x - size * 0.3, y - size * 0.8)
    ctx.line_to(x + size * 0.3, y - size * 0.8)
    ctx.close_path()
    ctx.fill()
    
    # Pencil tip (graphite)
    ctx.set_source_rgb(color[0], color[1], color[2])
    ctx.move_to(x, y)
    ctx.line_to(x - size * 0.2, y - size * 0.3)
    ctx.line_to(x + size * 0.2, y - size * 0.3)
    ctx.close_path()
    ctx.fill()
    
    # Pencil outline
    ctx.set_source_rgb(0.2, 0.2, 0.2)
    ctx.set_line_width(1.5)
    ctx.move_to(x, y)
    ctx.line_to(x - size * 0.3, y - size * 0.8)
    ctx.line_to(x + size * 0.3, y - size * 0.8)
    ctx.close_path()
    ctx.stroke()
    
    # Restore context state
    ctx.restore()


def draw_eraser_cursor(ctx, x, y, size=25):
    """
    Draw an eraser icon at the specified position.
    
    Args:
        ctx: Cairo context
        x, y: Position to draw the eraser
        size: Size of the eraser icon
    """
    # Save context state
    ctx.save()
    
    # Eraser body (pink/white)
    ctx.set_source_rgb(0.95, 0.7, 0.8)  # Pink eraser color
    ctx.rectangle(x - size * 0.4, y - size * 0.6, size * 0.8, size * 0.5)
    ctx.fill()
    
    # Eraser bottom (darker)
    ctx.set_source_rgb(0.7, 0.5, 0.6)
    ctx.rectangle(x - size * 0.4, y - size * 0.1, size * 0.8, size * 0.2)
    ctx.fill()
    
    # Eraser outline
    ctx.set_source_rgb(0.2, 0.2, 0.2)
    ctx.set_line_width(1.5)
    ctx.rectangle(x - size * 0.4, y - size * 0.6, size * 0.8, size * 0.7)
    ctx.stroke()
    
    # Restore context state
    ctx.restore()


def render_motion_video(metadata_path, output_dir, fps=None, encode_to_video=None, video_quality='high', cleanup_frames=False, show_cursor=False):
    """
    Render motion recording to video frames and optionally encode to video.
    
    Args:
        metadata_path: Path to motion_metadata.json file
        output_dir: Directory to save rendered frames
        fps: Override frame rate (uses metadata value if None)
        encode_to_video: Path to output video file (None = frames only)
        video_quality: Video quality - 'high', 'medium', 'low', or 'gif'
        cleanup_frames: If True, delete frames after encoding video
        show_cursor: If True, show pencil/eraser cursor at current drawing position
    """
    # Load motion data
    print(f"Loading motion data from {metadata_path}...")
    with open(metadata_path, 'r') as f:
        data = json.load(f)
    
    frame_rate = fps if fps is not None else data['frameRate']
    
    # New format: timestamps are normalized per-stroke, use totalDurationMs
    # Old format compatibility: fall back to minTimestamp/maxTimestamp if present
    if 'totalDurationMs' in data:
        # New format: idle time between strokes is excluded
        duration_ms = data['totalDurationMs']
    else:
        # Old format compatibility
        min_time = data.get('minTimestamp', 0)
        max_time = data.get('maxTimestamp', 0)
        duration_ms = max_time - min_time
    
    duration_sec = duration_ms / 1000.0
    total_motion_points = data['totalMotionPoints']
    
    # Check for pressure data in the motion points
    # Pressure values: -1.0 = no sensor/missing, 0.0-1.0 = valid pressure data
    # If any point has valid pressure (>= 0.0), we consider the data to have pressure info
    has_pressure = any(
        point.get('p', -1.0) >= 0.0
        for page in data['pages']
        for stroke in page.get('strokes', [])
        for point in stroke.get('motionPoints', [])
    )
    
    print(f"Motion data info:")
    print(f"  Frame rate: {frame_rate} fps")
    print(f"  Total duration: {duration_ms}ms ({duration_sec:.2f}s, excluding idle time)")
    print(f"  Total motion points: {total_motion_points}")
    print(f"  Pages: {len(data['pages'])}")
    print(f"  Pressure data: {'Yes' if has_pressure else 'No (using fixed width)'}")
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Calculate frame times based on FPS as sampling rate
    # FPS represents how many frames to generate per second of recording time
    if duration_sec > 0:
        # Number of frames = duration in seconds × frames per second
        # This gives us a consistent sampling rate regardless of motion point density
        num_frames = int(duration_sec * frame_rate) + 1
        frame_interval_ms = duration_ms / max(num_frames - 1, 1)
    else:
        num_frames = 1
        frame_interval_ms = 1.0
    
    print(f"Rendering {num_frames} frames ({duration_sec:.2f}s at {frame_rate} fps, {total_motion_points} motion points)...")
    
    frame_number = 0
    
    # Pre-process: build a cumulative timeline for all strokes across all pages
    # Since timestamps are now normalized per-stroke (starting from 0), we need to
    # calculate absolute times by accumulating stroke durations
    stroke_timeline = []
    cumulative_time = 0
    
    for page in data['pages']:
        for stroke in page.get('strokes', []):
            motion_points = stroke.get('motionPoints', [])
            # Skip strokes with no motion points or only a single point
            if len(motion_points) >= 2:
                # Calculate duration using max/min to handle potentially unsorted timestamps
                timestamps = [p['t'] for p in motion_points]
                stroke_duration = max(timestamps) - min(timestamps)
                stroke_timeline.append({
                    'page': page,
                    'stroke': stroke,
                    'startTime': cumulative_time,
                    'endTime': cumulative_time + stroke_duration,
                    'normalizedPoints': motion_points  # Points with timestamps starting from 0
                })
                cumulative_time += stroke_duration
    
    # Render each frame
    for frame_idx in range(num_frames):
        current_time = frame_idx * frame_interval_ms
        
        # Find which page we're on based on the current time
        current_page = data['pages'][0]  # Default to first page
        for stroke_info in stroke_timeline:
            if stroke_info['startTime'] <= current_time <= stroke_info['endTime']:
                current_page = stroke_info['page']
                break
        
        # Create canvas with page background using Cairo for high-quality rendering
        width = int(current_page['width'])
        height = int(current_page['height'])
        bg = current_page['background']['color']
        
        # Create Cairo surface and context
        surface = cairo.ImageSurface(cairo.FORMAT_RGB24, width, height)
        ctx = cairo.Context(surface)
        
        # Fill background
        ctx.set_source_rgb(bg['r'] / 255.0, bg['g'] / 255.0, bg['b'] / 255.0)
        ctx.paint()
        
        # Set line rendering quality
        ctx.set_line_cap(cairo.LINE_CAP_ROUND)
        ctx.set_line_join(cairo.LINE_JOIN_ROUND)
        ctx.set_antialias(cairo.ANTIALIAS_BEST)
        
        # Track the most recent drawing position and tool for cursor display
        cursor_x, cursor_y = None, None
        cursor_is_eraser = False
        cursor_color = (0, 0, 0)  # Default black
        
        # Draw all strokes up to current_time using the cumulative timeline
        for stroke_info in stroke_timeline:
            # Only draw strokes on the current page
            if stroke_info['page']['pageIndex'] != current_page['pageIndex']:
                continue
            
            stroke = stroke_info['stroke']
            stroke_start_time = stroke_info['startTime']
            stroke_end_time = stroke_info['endTime']
            
            # Skip strokes that haven't started yet
            if current_time < stroke_start_time:
                continue
            
            # Get stroke properties
            stroke_color = stroke.get('color', {})
            base_width = stroke.get('width', 2.0)
            stroke_tool = stroke.get('tool', 'pen')
            
            # Collect points up to current_time within this stroke
            # Convert current_time to stroke-relative time
            stroke_relative_time = current_time - stroke_start_time
            
            # Safety check: ensure stroke_relative_time is non-negative
            if stroke_relative_time < 0:
                continue
            
            visible_points = []
            for point in stroke_info['normalizedPoints']:
                if point['t'] <= stroke_relative_time:
                    visible_points.append(point)
                else:
                    break
            
            # Draw the stroke progressively
            if len(visible_points) > 1:
                for i in range(1, len(visible_points)):
                    prev = visible_points[i-1]
                    curr = visible_points[i]
                    
                    # Determine if this is an eraser stroke
                    point_is_eraser = curr.get('isEraser', False)
                    is_eraser_stroke = (stroke_tool == 'eraser') or point_is_eraser
                    
                    # Set color based on tool type
                    # Eraser strokes use background color to simulate erasing
                    # Pen/highlighter strokes use their designated stroke color
                    if is_eraser_stroke:
                        # Eraser: draw in background color to simulate erasing
                        ctx.set_source_rgb(bg['r'] / 255.0, bg['g'] / 255.0, bg['b'] / 255.0)
                    else:
                        # Pen/Highlighter: draw in stroke color
                        ctx.set_source_rgb(
                            stroke_color.get('r', 0) / 255.0,
                            stroke_color.get('g', 0) / 255.0,
                            stroke_color.get('b', 0) / 255.0
                        )
                    
                    # Calculate width based on pressure
                    pressure = get_normalized_pressure(curr)
                    stroke_width = max(0.5, base_width * pressure)
                    ctx.set_line_width(stroke_width)
                    
                    # Draw line segment
                    ctx.move_to(prev['x'], prev['y'])
                    ctx.line_to(curr['x'], curr['y'])
                    ctx.stroke()
                    
                    # Update cursor position to the most recent point
                    if show_cursor and current_time >= stroke_start_time and current_time <= stroke_end_time:
                        cursor_x = curr['x']
                        cursor_y = curr['y']
                        cursor_is_eraser = is_eraser_stroke
                        if not is_eraser_stroke:
                            cursor_color = (
                                stroke_color.get('r', 0) / 255.0,
                                stroke_color.get('g', 0) / 255.0,
                                stroke_color.get('b', 0) / 255.0
                            )
        
        # Draw cursor (pencil or eraser) at the current drawing position
        if show_cursor and cursor_x is not None and cursor_y is not None:
            if cursor_is_eraser:
                draw_eraser_cursor(ctx, cursor_x, cursor_y)
            else:
                draw_pencil_cursor(ctx, cursor_x, cursor_y, cursor_color)
        
        # Save frame
        frame_path = os.path.join(output_dir, f'frame_{frame_number:06d}.png')
        surface.write_to_png(frame_path)
        frame_number += 1
        
        # Progress indicator
        if frame_number % 30 == 0 or frame_number == num_frames:
            progress = (frame_number / num_frames) * 100
            print(f"  Progress: {frame_number}/{num_frames} frames ({progress:.1f}%)")
    
    print(f"\n✓ Rendering complete! {frame_number} frames saved to {output_dir}/")
    
    # Encode to video if requested
    if encode_to_video:
        encode_video(output_dir, encode_to_video, frame_rate, video_quality, cleanup_frames)
    else:
        # Show manual FFmpeg commands
        print(f"\nCreate video with FFmpeg:")
        print(f"  ffmpeg -framerate {frame_rate} -i {output_dir}/frame_%06d.png -c:v libx264 -pix_fmt yuv420p output.mp4")
        print(f"\nCreate high-quality video:")
        print(f"  ffmpeg -framerate {frame_rate} -i {output_dir}/frame_%06d.png -c:v libx264 -crf 18 -preset slow output.mp4")
        print(f"\nCreate GIF:")
        print(f"  ffmpeg -framerate 10 -i {output_dir}/frame_%06d.png -vf \"scale=800:-1:flags=lanczos\" output.gif")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Render Xournal++ motion recording data to video frames or video file.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Render frames only
  python render_motion_video.py motion_metadata.json output_frames/
  
  # Render frames at 60 FPS
  python render_motion_video.py motion_metadata.json output_frames/ --fps 60
  
  # Render and encode to video
  python render_motion_video.py motion_metadata.json output_frames/ --video output.mp4
  
  # Render and encode high-quality video, then cleanup frames
  python render_motion_video.py motion_metadata.json output_frames/ --video output.mp4 --quality high --cleanup
  
  # Render and encode to GIF
  python render_motion_video.py motion_metadata.json output_frames/ --video output.gif --quality gif
        """
    )
    
    parser.add_argument('metadata', help='Path to motion_metadata.json file')
    parser.add_argument('output_dir', help='Directory to save rendered frames')
    parser.add_argument('--fps', type=int, default=None, 
                        help='Override frame rate (uses metadata value if not specified)')
    parser.add_argument('--video', '-v', default=None,
                        help='Encode frames to video file (requires FFmpeg)')
    parser.add_argument('--quality', '-q', choices=['high', 'medium', 'low', 'gif'], default='high',
                        help='Video encoding quality (default: high)')
    parser.add_argument('--cleanup', '-c', action='store_true',
                        help='Delete frame images after encoding video')
    parser.add_argument('--cursor', action='store_true',
                        help='Show pencil/eraser cursor at current drawing position')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.metadata):
        print(f"Error: File not found: {args.metadata}")
        sys.exit(1)
    
    try:
        render_motion_video(
            args.metadata, 
            args.output_dir, 
            fps=args.fps,
            encode_to_video=args.video,
            video_quality=args.quality,
            cleanup_frames=args.cleanup,
            show_cursor=args.cursor
        )
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
