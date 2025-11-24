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

Requirements:
    pip install pillow

Usage:
    python render_motion_video.py motion_metadata.json output_frames/
    
    # Then create video with FFmpeg:
    ffmpeg -framerate 30 -i output_frames/frame_%06d.png -c:v libx264 -pix_fmt yuv420p output.mp4
"""

import json
import os
import sys
from PIL import Image, ImageDraw


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


def render_motion_video(metadata_path, output_dir, fps=None):
    """
    Render motion recording to video frames.
    
    Args:
        metadata_path: Path to motion_metadata.json file
        output_dir: Directory to save rendered frames
        fps: Override frame rate (uses metadata value if None)
    """
    # Load motion data
    print(f"Loading motion data from {metadata_path}...")
    with open(metadata_path, 'r') as f:
        data = json.load(f)
    
    frame_rate = fps if fps is not None else data['frameRate']
    min_time = data['minTimestamp']
    max_time = data['maxTimestamp']
    
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
    print(f"  Time range: {min_time}ms - {max_time}ms ({(max_time - min_time) / 1000:.2f}s)")
    print(f"  Total motion points: {data['totalMotionPoints']}")
    print(f"  Pages: {len(data['pages'])}")
    print(f"  Pressure data: {'Yes' if has_pressure else 'No (using fixed width)'}")
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Calculate frame times based on motion data points
    # FPS controls sampling density: how many motion states to capture per second
    # Instead of duration × FPS, we base frame count on actual motion points available
    duration_ms = max_time - min_time
    total_motion_points = data['totalMotionPoints']
    
    # Calculate number of frames based on motion point density and FPS
    # Strategy: Use total motion points as base, scale by FPS as a sampling factor
    # Higher FPS = more frequent sampling of the motion data
    if total_motion_points > 0 and duration_ms > 0:
        # Calculate average motion points per second in the recording
        motion_density = total_motion_points / (duration_ms / 1000.0)
        
        # Use FPS as a multiplier for sampling: higher FPS = more frames
        # But cap it to avoid generating too many frames
        num_frames = max(int(total_motion_points * (frame_rate / motion_density)), total_motion_points)
        frame_interval_ms = duration_ms / max(num_frames - 1, 1)
    else:
        num_frames = 1
        frame_interval_ms = 1.0
    
    print(f"Rendering {num_frames} frames (motion density: {motion_density:.1f} points/sec, sampling at {frame_rate} fps)...")
    
    frame_number = 0
    current_page_idx = 0
    
    # Pre-process: create a timeline of which page is active at each timestamp
    page_timeline = []
    for page in data['pages']:
        if page['strokes']:
            # Find min and max timestamps for this page
            page_min = float('inf')
            page_max = float('-inf')
            for stroke in page['strokes']:
                if stroke['motionPoints']:
                    page_min = min(page_min, stroke['motionPoints'][0]['t'])
                    page_max = max(page_max, stroke['motionPoints'][-1]['t'])
            
            if page_min != float('inf'):
                page_timeline.append({
                    'pageIndex': page['pageIndex'],
                    'minTime': page_min,
                    'maxTime': page_max,
                    'page': page
                })
    
    # Sort by start time
    page_timeline.sort(key=lambda p: p['minTime'])
    
    # Render each frame
    for frame_idx in range(num_frames):
        current_time = min_time + (frame_idx * frame_interval_ms)
        
        # Find which page we're on
        current_page = data['pages'][0]  # Default to first page
        for page_info in page_timeline:
            if page_info['minTime'] <= current_time <= page_info['maxTime']:
                current_page = page_info['page']
                break
        
        # Create canvas with page background
        width = int(current_page['width'])
        height = int(current_page['height'])
        bg = current_page['background']['color']
        # Use RGB mode for opaque output (convert RGBA to RGB)
        bg_color_rgb = (bg['r'], bg['g'], bg['b'])
        
        img = Image.new('RGB', (width, height), bg_color_rgb)
        draw = ImageDraw.Draw(img)
        
        # Draw all strokes up to current_time on this page
        for stroke in current_page['strokes']:
            # Get stroke color - ensure all components are valid
            stroke_color = stroke.get('color', {})
            stroke_color_rgb = (
                stroke_color.get('r', 0),
                stroke_color.get('g', 0),
                stroke_color.get('b', 0)
            )
            base_width = stroke.get('width', 2.0)  # Default width if missing
            stroke_tool = stroke.get('tool', 'pen')  # Default to pen if missing
            
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
                    
                    # Determine if this is an eraser stroke
                    # A stroke is an eraser if:
                    #   1. The stroke tool is 'eraser', OR
                    #   2. The individual motion point is marked as eraser
                    point_is_eraser = curr.get('isEraser', False)
                    is_eraser_stroke = (stroke_tool == 'eraser') or point_is_eraser
                    
                    # DEBUG: Log first point of first stroke to help diagnose issues
                    if frame_idx == 0 and i == 1:
                        print(f"  DEBUG: First stroke - tool='{stroke_tool}', point_isEraser={point_is_eraser}, "
                              f"is_eraser_stroke={is_eraser_stroke}, "
                              f"stroke_color={stroke_color_rgb}, bg_color={bg_color_rgb}")
                    
                    # Choose drawing color:
                    # - For pen/highlighter: use stroke color (visible drawing)
                    # - For eraser: use background color (simulate erasing)
                    if not is_eraser_stroke:
                        # This is a pen/highlighter stroke - draw in stroke color
                        draw_color = stroke_color_rgb
                    else:
                        # This is an eraser stroke - draw in background color to erase
                        draw_color = bg_color_rgb
                    
                    # Calculate width based on pressure if available
                    pressure = get_normalized_pressure(curr)
                    
                    # Apply pressure to base width (ensure minimum 1px)
                    stroke_width = int(max(1, base_width * pressure))
                    
                    # Draw line segment with pressure-sensitive width
                    draw.line([(prev['x'], prev['y']), 
                              (curr['x'], curr['y'])],
                             fill=draw_color, width=stroke_width)
        
        # Save frame
        frame_path = os.path.join(output_dir, f'frame_{frame_number:06d}.png')
        img.save(frame_path)
        frame_number += 1
        
        # Progress indicator
        if frame_number % 30 == 0 or frame_number == num_frames:
            progress = (frame_number / num_frames) * 100
            print(f"  Progress: {frame_number}/{num_frames} frames ({progress:.1f}%)")
    
    print(f"\n✓ Rendering complete! {frame_number} frames saved to {output_dir}/")
    print(f"\nCreate video with FFmpeg:")
    print(f"  ffmpeg -framerate {frame_rate} -i {output_dir}/frame_%06d.png -c:v libx264 -pix_fmt yuv420p output.mp4")
    print(f"\nCreate high-quality video:")
    print(f"  ffmpeg -framerate {frame_rate} -i {output_dir}/frame_%06d.png -c:v libx264 -crf 18 -preset slow output.mp4")
    print(f"\nCreate GIF:")
    print(f"  ffmpeg -framerate 10 -i {output_dir}/frame_%06d.png -vf \"scale=800:-1:flags=lanczos\" output.gif")


def main():
    if len(sys.argv) < 3:
        print("Usage: python render_motion_video.py <motion_metadata.json> <output_dir> [fps]")
        print("\nExample:")
        print("  python render_motion_video.py motion_metadata.json output_frames/")
        print("  python render_motion_video.py motion_metadata.json output_frames/ 60")
        sys.exit(1)
    
    metadata_path = sys.argv[1]
    output_dir = sys.argv[2]
    fps = int(sys.argv[3]) if len(sys.argv) > 3 else None
    
    if not os.path.exists(metadata_path):
        print(f"Error: File not found: {metadata_path}")
        sys.exit(1)
    
    try:
        render_motion_video(metadata_path, output_dir, fps)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
