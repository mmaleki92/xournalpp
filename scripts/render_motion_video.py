#!/usr/bin/env python3
"""
Render Xournal++ motion recording to High-Res video/GIF with Realistic Cursors.
Supports normalized per-stroke timing (skips idle time automatically).
Supports audio generation (requires pydub).
Supports custom background patterns (dotted, graph, ruled).
"""

import json
import os
import sys
import subprocess
import shutil
import cairo
import math

# Try to import pydub for audio processing
try:
    from pydub import AudioSegment
    PYDUB_AVAILABLE = True
except ImportError:
    PYDUB_AVAILABLE = False

def get_normalized_pressure(point):
    """Get normalized pressure from a motion point."""
    pressure = point.get('p', -1.0)
    # Treat missing field or negative pressure as full pressure
    return 1.0 if pressure < 0.0 else pressure

def is_background_dark(rgb_dict):
    """Check if background color is dark to adjust contrast."""
    r = rgb_dict.get('r', 255)
    g = rgb_dict.get('g', 255)
    b = rgb_dict.get('b', 255)
    # Calculate luminance (standard formula)
    lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0
    return lum < 0.5

def draw_background(ctx, width, height, bg_info):
    """Draws the page background (color + pattern)."""
    # 1. Fill Base Color
    color = bg_info.get('color', {'r': 255, 'g': 255, 'b': 255})
    r, g, b = color.get('r', 255)/255.0, color.get('g', 255)/255.0, color.get('b', 255)/255.0
    ctx.set_source_rgb(r, g, b)
    ctx.paint()

    bg_type = bg_info.get('type', 'plain')
    
    # 2. Determine Pattern Color (Contrast)
    # If bg is dark, use light grey (0.8). If bg is light, use dark grey (0.6)
    is_dark = is_background_dark(color)
    if is_dark:
        ctx.set_source_rgba(0.8, 0.8, 0.9, 0.4) # Light whitish-blue, semi-transparent
    else:
        ctx.set_source_rgba(0.4, 0.4, 0.5, 0.4) # Dark grey-blue, semi-transparent

    # 3. Draw Pattern
    # Xournal++ standard spacing is often around 20-30 points.
    spacing = 24 
    
    if bg_type == 'dotted':
        for y in range(0, height, spacing):
            for x in range(0, width, spacing):
                ctx.arc(x, y, 1.0, 0, 2*math.pi) # Dot radius 1.0
                ctx.fill()
                
    elif bg_type == 'graph':
        ctx.set_line_width(0.5)
        # Vertical lines
        for x in range(0, width, spacing):
            ctx.move_to(x, 0)
            ctx.line_to(x, height)
        # Horizontal lines
        for y in range(0, height, spacing):
            ctx.move_to(0, y)
            ctx.line_to(width, y)
        ctx.stroke()
        
    elif bg_type == 'ruled':
        ctx.set_line_width(0.5)
        # Horizontal lines only
        for y in range(spacing, height, spacing):
            ctx.move_to(0, y)
            ctx.line_to(width, y)
        ctx.stroke()
        
        # Vertical margin line (standard notebook style)
        margin_x = 72 # approx 1 inch
        if is_dark:
             ctx.set_source_rgba(0.9, 0.5, 0.5, 0.5) # Light pinkish on dark
        else:
             ctx.set_source_rgba(1.0, 0.3, 0.3, 0.5) # Red on light
        ctx.move_to(margin_x, 0)
        ctx.line_to(margin_x, height)
        ctx.stroke()

def encode_video(frames_dir, output_video, frame_rate, audio_file=None, quality='high', cleanup_frames=False):
    """Encode rendered frames into a video file using FFmpeg."""
    if not shutil.which('ffmpeg'):
        print("Error: FFmpeg not found. Please install FFmpeg.")
        return False
    
    print(f"\nEncoding video to {output_video}...")

    pad_filter = 'pad=ceil(iw/2)*2:ceil(ih/2)*2'
    
    input_args = ['-framerate', str(frame_rate), '-i', os.path.join(frames_dir, 'frame_%06d.png')]
    
    if audio_file and os.path.exists(audio_file):
        input_args.extend(['-i', audio_file])
        map_args = ['-map', '0:v', '-map', '1:a', '-c:a', 'aac', '-shortest']
    else:
        map_args = []

    if quality == 'gif':
        filter_graph = (
            f"[0:v] fps={min(frame_rate, 20)}, "
            f"{pad_filter}, "
            f"split [a][b];[a] palettegen [p];[b][p] paletteuse"
        )
        cmd = ['ffmpeg', '-y'] + input_args + ['-filter_complex', filter_graph, output_video]
    else:
        crf = '17' if quality == 'high' else '23'
        preset = 'veryslow' if quality == 'high' else 'fast'
        
        cmd = (
            ['ffmpeg', '-y'] + 
            input_args + 
            ['-c:v', 'libx264', '-vf', pad_filter, '-crf', crf, '-preset', preset, '-pix_fmt', 'yuv420p'] +
            map_args +
            [output_video]
        )
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"✓ Video encoded successfully: {output_video}")
            if cleanup_frames:
                print("Cleaning up frames...")
                for file in os.listdir(frames_dir):
                    if file.startswith('frame_') and file.endswith('.png'):
                        os.remove(os.path.join(frames_dir, file))
            return True
        else:
            print(f"✗ FFmpeg encoding failed:\n{result.stderr}")
            return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def generate_audio_track(stroke_timeline, total_duration_ms, tap_file, scratch_file, output_path):
    """Generates a WAV audio track synchronized with the strokes."""
    if not PYDUB_AVAILABLE:
        print("Warning: 'pydub' library not found. Skipping audio generation.")
        return None

    if not tap_file or not scratch_file:
        return None
        
    print(f"Generating audio track from {tap_file} and {scratch_file}...")
    
    try:
        tap_sound = AudioSegment.from_file(tap_file)
        scratch_sound = AudioSegment.from_file(scratch_file)
    except Exception as e:
        print(f"Error loading audio files: {e}")
        return None

    base_track = AudioSegment.silent(duration=total_duration_ms + 1000)
    
    count_tap = 0
    count_scratch = 0

    for info in stroke_timeline:
        start_ms = int(info['startTime'])
        end_ms = int(info['endTime'])
        duration = end_ms - start_ms
        
        if duration <= 0: continue
        
        if duration < 300:
            base_track = base_track.overlay(tap_sound, position=start_ms)
            count_tap += 1
        else:
            count_scratch += 1
            needed_audio = scratch_sound
            while len(needed_audio) < duration:
                needed_audio += scratch_sound
            raw_stroke_audio = needed_audio[:duration]
            
            chunk_ms = 10
            points = info['normalizedPoints']
            processed_chunks = []
            
            p_idx = 0
            num_points = len(points)
            if num_points > 0:
                last_x, last_y = points[0]['x'], points[0]['y']
            else:
                last_x, last_y = 0, 0

            for t_start in range(0, duration, chunk_ms):
                t_end = min(t_start + chunk_ms, duration)
                curr_chunk_dur = t_end - t_start
                if curr_chunk_dur <= 0: break
                
                chunk_audio = raw_stroke_audio[t_start:t_end]
                
                dist = 0.0
                pressure_sum = 0.0
                pressure_count = 0
                
                while p_idx < num_points and points[p_idx]['t'] < t_start:
                    last_x = points[p_idx]['x']
                    last_y = points[p_idx]['y']
                    p_idx += 1
                
                curr_p_idx = p_idx
                while curr_p_idx < num_points and points[curr_p_idx]['t'] < t_end:
                    p = points[curr_p_idx]
                    d = math.hypot(p['x'] - last_x, p['y'] - last_y)
                    dist += d
                    last_x = p['x']
                    last_y = p['y']
                    pr = get_normalized_pressure(p)
                    pressure_sum += pr
                    pressure_count += 1
                    curr_p_idx += 1
                
                speed = dist / curr_chunk_dur
                avg_pressure = (pressure_sum / pressure_count) if pressure_count > 0 else 0.5
                
                target_gain_db = -100.0
                if speed > 0.02: 
                    speed_factor = min(speed / 0.5, 1.0)
                    pressure_factor = 0.2 + (avg_pressure * 0.8)
                    combined = speed_factor * pressure_factor
                    if combined > 0.001:
                        target_gain_db = 20 * math.log10(combined)
                
                chunk_audio = chunk_audio + target_gain_db
                processed_chunks.append(chunk_audio)
                
            if processed_chunks:
                full_stroke_audio = sum(processed_chunks)
                fade_len = min(50, duration // 4)
                full_stroke_audio = full_stroke_audio.fade_in(fade_len).fade_out(fade_len)
                base_track = base_track.overlay(full_stroke_audio, position=start_ms)
            
    print(f"  Audio generated: {count_tap} taps, {count_scratch} strokes.")
    base_track.export(output_path, format="wav")
    return output_path

def draw_realistic_pencil(ctx, x, y, tip_color, scale=1.0, cursor_scale=1.0, is_dark_bg=False):
    """
    Draws a realistic, faceted (hexagonal) pencil with WOOD TEXTURE.
    Adjusts shadow intensity based on background brightness.
    """
    ctx.save()
    ctx.translate(x, y)
    
    main_angle_deg = 135
    s = scale * cursor_scale * 2.5
    ctx.scale(s, s)
    
    width = 14
    half_w = width / 2
    length = 200 
    cone_h = 35 
    lead_h = 10 
    
    # --- 1. DRAW SHADOW ---
    ctx.save()
    ctx.rotate(math.radians(main_angle_deg - 3))
    shadow_len = length * 0.25
    
    grad_shadow = cairo.LinearGradient(0, 0, 0, -shadow_len)
    
    # Shadow Color Adjustment for Visibility
    if is_dark_bg:
        # On dark background, shadow must be dense black to be seen against dark blue
        grad_shadow.add_color_stop_rgba(0.0, 0, 0, 0, 0.95) # Nearly opaque black start
        grad_shadow.add_color_stop_rgba(1.0, 0, 0, 0, 0.0)
    else:
        # Normal shadow for white/light paper
        grad_shadow.add_color_stop_rgba(0.0, 0, 0, 0, 0.6)
        grad_shadow.add_color_stop_rgba(1.0, 0, 0, 0, 0.0)
        
    ctx.set_source(grad_shadow)
    ctx.move_to(0, 0)
    ctx.line_to(half_w * 0.35, -lead_h) 
    ctx.line_to(half_w, -cone_h) 
    ctx.line_to(half_w, -shadow_len) 
    ctx.line_to(-half_w, -shadow_len) 
    ctx.line_to(-half_w, -cone_h) 
    ctx.line_to(-half_w * 0.35, -lead_h) 
    ctx.close_path()
    ctx.fill()
    ctx.restore()

    # --- 2. DRAW PENCIL ---
    ctx.rotate(math.radians(main_angle_deg))

    # A. LEAD TIP
    blunt_w = 1.5 
    ctx.move_to(-blunt_w/2, 0)
    ctx.line_to(blunt_w/2, 0)
    ctx.line_to(half_w * 0.35, -lead_h)
    ctx.line_to(-half_w * 0.35, -lead_h)
    ctx.close_path()
    ctx.set_source_rgb(0.15, 0.15, 0.18) 
    ctx.fill()
        
    # B. WOOD CONE WITH TEXTURE
    ctx.save()
    ctx.move_to(-half_w * 0.35, -lead_h)
    ctx.line_to(half_w * 0.35, -lead_h)
    ctx.line_to(half_w, -cone_h)
    ctx.line_to(-half_w, -cone_h)
    ctx.close_path()
    
    pat_wood = cairo.LinearGradient(0, -lead_h, 0, -cone_h)
    pat_wood.add_color_stop_rgb(0.0, 0.85, 0.70, 0.55) 
    pat_wood.add_color_stop_rgb(0.3, 0.95, 0.85, 0.70) 
    pat_wood.add_color_stop_rgb(1.0, 0.90, 0.80, 0.65) 
    ctx.set_source(pat_wood)
    ctx.fill_preserve() 
    
    ctx.clip() 
    ctx.set_source_rgba(0.65, 0.5, 0.35, 0.6) 
    ctx.set_line_width(0.4)
    
    for i in range(4):
        offset_x = (i - 1.5) * 3 
        ctx.move_to(offset_x, -lead_h)
        ctx.curve_to(offset_x + 1, -lead_h - 10, offset_x - 2, -cone_h + 10, offset_x * 0.5, -cone_h)
        ctx.stroke()
        
    ctx.restore()

    # C. FACETED BODY
    face_w = width / 3.0 
    
    # Left Face
    ctx.move_to(-half_w, -cone_h)
    ctx.curve_to(-half_w, -cone_h + 5, -half_w + face_w, -cone_h + 5, -half_w + face_w, -cone_h)
    ctx.line_to(-half_w + face_w, -length)
    ctx.line_to(-half_w, -length)
    ctx.close_path()
    ctx.set_source_rgb(0.0, 0.1, 0.4) 
    ctx.fill()

    # Center Face
    ctx.move_to(-half_w + face_w, -cone_h)
    ctx.curve_to(-half_w + face_w, -cone_h + 5, half_w - face_w, -cone_h + 5, half_w - face_w, -cone_h)
    ctx.line_to(half_w - face_w, -length)
    ctx.line_to(-half_w + face_w, -length)
    ctx.close_path()
    ctx.set_source_rgb(0.1, 0.3, 0.8) 
    ctx.fill()
    
    ctx.save()
    ctx.rectangle(-half_w + face_w + 1, -length, 2, length - cone_h)
    ctx.set_source_rgba(1, 1, 1, 0.2)
    ctx.fill()
    ctx.restore()

    # Right Face
    ctx.move_to(half_w - face_w, -cone_h)
    ctx.curve_to(half_w - face_w, -cone_h + 5, half_w, -cone_h + 5, half_w, -cone_h)
    ctx.line_to(half_w, -length)
    ctx.line_to(half_w - face_w, -length)
    ctx.close_path()
    ctx.set_source_rgb(0.05, 0.2, 0.6) 
    ctx.fill()

    # D. END
    ctx.move_to(-half_w, -length)
    ctx.line_to(half_w, -length)
    ctx.save()
    ctx.translate(0, -length)
    ctx.scale(1, 0.3) 
    ctx.arc(0, 0, half_w, 0, 2 * math.pi)
    ctx.restore()
    ctx.set_source_rgb(0.7, 0.6, 0.5) 
    ctx.fill()
    
    ctx.restore()

def draw_realistic_eraser(ctx, x, y, scale=1.0, cursor_scale=1.0, is_dark_bg=False):
    """Draws a realistic rectangular block eraser with shadow."""
    ctx.save()
    ctx.translate(x, y)
    ctx.rotate(math.radians(-15))
    s = scale * cursor_scale * 2.0
    ctx.scale(s, s)
    ctx.translate(-20, -45)
    
    w = 40
    h = 60
    depth = 15

    def draw_eraser_shape(is_shadow=False):
        if is_shadow:
            if is_dark_bg:
                 ctx.set_source_rgba(0, 0, 0, 0.8) # Dense shadow for dark bg
            else:
                 ctx.set_source_rgba(0, 0, 0, 0.2) # Light shadow for light bg
            
            ctx.move_to(0, h)
            ctx.line_to(w, h)
            ctx.line_to(w + depth, h - 5)
            ctx.line_to(w + depth, -5)
            ctx.line_to(depth, -5)
            ctx.line_to(0, 0)
            ctx.close_path()
            ctx.fill()
            return

        ctx.rectangle(0, 0, w, h)
        pat = cairo.LinearGradient(0, 0, w, h)
        pat.add_color_stop_rgb(0, 0.95, 0.95, 0.95)
        pat.add_color_stop_rgb(1, 0.85, 0.85, 0.85)
        ctx.set_source(pat)
        ctx.fill()
        ctx.move_to(w, 0)
        ctx.line_to(w + depth, -5)
        ctx.line_to(w + depth, h - 5)
        ctx.line_to(w, h)
        ctx.close_path()
        ctx.set_source_rgb(0.8, 0.8, 0.8)
        ctx.fill()
        ctx.move_to(0, 0)
        ctx.line_to(depth, -5)
        ctx.line_to(w + depth, -5)
        ctx.line_to(w, 0)
        ctx.close_path()
        ctx.set_source_rgb(0.9, 0.9, 0.9)
        ctx.fill()
        sleeve_h = h * 0.5
        sleeve_y = h * 0.3
        ctx.rectangle(0, sleeve_y, w, sleeve_h)
        ctx.set_source_rgb(0.2, 0.3, 0.8) 
        ctx.fill()
        ctx.move_to(w, sleeve_y)
        ctx.line_to(w + depth, sleeve_y - 2)
        ctx.line_to(w + depth, sleeve_y + sleeve_h - 2)
        ctx.line_to(w, sleeve_y + sleeve_h)
        ctx.close_path()
        ctx.set_source_rgb(0.15, 0.25, 0.6) 
        ctx.fill()
        ctx.set_source_rgba(1, 1, 1, 0.6)
        ctx.move_to(5, sleeve_y + 10)
        ctx.line_to(w - 5, sleeve_y + 10)
        ctx.set_line_width(2)
        ctx.stroke()

    ctx.save()
    ctx.translate(6, 6)
    draw_eraser_shape(is_shadow=True)
    ctx.restore()
    draw_eraser_shape(is_shadow=False)
    ctx.restore()

def render_motion_video(metadata_path, output_dir, fps=None, encode_to_video=None, video_quality='high', cleanup_frames=False, show_cursor=False, scale_factor=2.0, cursor_scale=1.0, audio_tap=None, audio_scratch=None):
    
    print(f"Loading data from {metadata_path}...")
    with open(metadata_path, 'r') as f: data = json.load(f)
    
    frame_rate = fps if fps is not None else data['frameRate']
    
    stroke_timeline = []
    cumulative_time = 0
    
    for page in data['pages']:
        for stroke in page.get('strokes', []):
            motion_points = stroke.get('motionPoints', [])
            if len(motion_points) >= 2:
                try:
                    min_t = min(p['t'] for p in motion_points)
                    max_t = max(p['t'] for p in motion_points)
                    stroke_duration = max_t - min_t
                    stroke_timeline.append({
                        'page': page,
                        'stroke': stroke,
                        'startTime': cumulative_time,
                        'endTime': cumulative_time + stroke_duration,
                        'normalizedPoints': motion_points 
                    })
                    cumulative_time += stroke_duration
                except (KeyError, ValueError):
                    continue

    duration_ms = cumulative_time
    duration_sec = duration_ms / 1000.0
    
    num_frames = int(duration_sec * frame_rate) + 1
    if num_frames < 1: num_frames = 1
    frame_interval_ms = duration_ms / max(num_frames - 1, 1)
    
    os.makedirs(output_dir, exist_ok=True)
    print(f"Rendering {num_frames} frames at {scale_factor}x resolution ({frame_rate} fps)...")
    print(f"Total Video Duration: {duration_sec:.2f}s (Idle time skipped)")

    generated_audio_file = None
    if audio_tap and audio_scratch:
        audio_out_path = os.path.join(output_dir, "generated_audio.wav")
        generated_audio_file = generate_audio_track(stroke_timeline, duration_ms, audio_tap, audio_scratch, audio_out_path)

    frame_number = 0
    
    for frame_idx in range(num_frames):
        current_time = frame_idx * frame_interval_ms
        
        current_page = data['pages'][0] 
        for stroke_info in stroke_timeline:
            if stroke_info['startTime'] <= current_time <= stroke_info['endTime']:
                current_page = stroke_info['page']
                break
        
        base_w, base_h = int(current_page['width']), int(current_page['height'])
        scaled_w, scaled_h = int(base_w * scale_factor), int(base_h * scale_factor)
        
        # Create main surface with ARGB32 format to support transparency for compositing
        surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, scaled_w, scaled_h)
        ctx = cairo.Context(surface)
        ctx.scale(scale_factor, scale_factor)
        
        # Draw Background with Dotted Pattern
        draw_background(ctx, base_w, base_h, current_page.get('background', {}))

        ctx.set_line_cap(cairo.LINE_CAP_ROUND)
        ctx.set_line_join(cairo.LINE_JOIN_ROUND)
        ctx.set_antialias(cairo.ANTIALIAS_BEST)
        
        cursor_pos = None
        cursor_is_eraser = False
        cursor_color = (0,0,0)
        
        # Check if background is dark for cursor adjustments
        is_bg_dark = is_background_dark(current_page.get('background', {}).get('color', {}))

        for stroke_info in stroke_timeline:
            if stroke_info['page']['pageIndex'] != current_page['pageIndex']:
                continue
            if current_time < stroke_info['startTime']:
                continue

            stroke = stroke_info['stroke']
            stroke_rel_time = current_time - stroke_info['startTime']
            points = stroke_info['normalizedPoints']
            visible_points = []
            
            for p in points:
                if p['t'] <= stroke_rel_time:
                    visible_points.append(p)
                else:
                    break 
            
            if len(visible_points) < 2: continue
            
            s_color = stroke.get('color', {'r':0,'g':0,'b':0})
            base_width = stroke.get('width', 1.5)
            # Check if this is an eraser stroke - first check tool type, then motion point flags
            # Short-circuit evaluation avoids unnecessary iteration if tool is already 'eraser'
            is_eraser = (stroke.get('tool') == 'eraser') or any(p.get('isEraser', False) for p in visible_points)
            
            if is_eraser:
                # For eraser strokes, use DEST_OUT operator to actually erase content
                # This removes pixels where the eraser path is drawn, creating realistic erasing effect
                ctx.save()
                ctx.set_operator(cairo.OPERATOR_DEST_OUT)
                # Draw the eraser path with full opacity to erase underlying content
                ctx.set_source_rgba(1.0, 1.0, 1.0, 1.0)
                draw_width = base_width
            else:
                ctx.set_source_rgb(s_color.get('r',0)/255, s_color.get('g',0)/255, s_color.get('b',0)/255)
                draw_width = base_width

            for i in range(1, len(visible_points)):
                p1, p2 = visible_points[i-1], visible_points[i]
                pressure = get_normalized_pressure(p2)
                ctx.set_line_width(max(0.5, draw_width * pressure))
                ctx.move_to(p1['x'], p1['y'])
                ctx.line_to(p2['x'], p2['y'])
                ctx.stroke()
            
            if is_eraser:
                ctx.restore()  # Restore normal compositing operator
                
            if show_cursor and current_time <= stroke_info['endTime']:
                cursor_pos = (visible_points[-1]['x'], visible_points[-1]['y'])
                cursor_is_eraser = is_eraser
                cursor_color = (s_color.get('r',0)/255, s_color.get('g',0)/255, s_color.get('b',0)/255)

        if show_cursor and cursor_pos:
            if cursor_is_eraser:
                draw_realistic_eraser(ctx, cursor_pos[0], cursor_pos[1], scale=scale_factor, cursor_scale=cursor_scale, is_dark_bg=is_bg_dark)
            else:
                draw_realistic_pencil(ctx, cursor_pos[0], cursor_pos[1], cursor_color, scale=scale_factor, cursor_scale=cursor_scale, is_dark_bg=is_bg_dark)
            
        surface.write_to_png(os.path.join(output_dir, f'frame_{frame_number:06d}.png'))
        frame_number += 1
        
        if num_frames > 0 and frame_number % 30 == 0:
            print(f"  Progress: {int(frame_number/num_frames*100)}%", end='\r')

    print(f"\nRendering complete. {frame_number} frames saved.")
    
    if encode_to_video:
        encode_video(output_dir, encode_to_video, frame_rate, generated_audio_file, video_quality, cleanup_frames)

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('metadata')
    parser.add_argument('output_dir')
    parser.add_argument('--fps', type=int, default=None)
    parser.add_argument('--video', '-v', default=None)
    parser.add_argument('--quality', '-q', default='high')
    parser.add_argument('--cleanup', '-c', action='store_true')
    parser.add_argument('--cursor', action='store_true')
    parser.add_argument('--scale', type=float, default=2.0, help='Scale factor for resolution')
    parser.add_argument('--cursor-scale', type=float, default=0.4, help='Relative size of the cursor')
    parser.add_argument('--audio-tap', default="tap.wav", help='Path to tap sound file (e.g., tap.wav)')
    parser.add_argument('--audio-scratch', default="scratch.wav", help='Path to scratch sound file (e.g., scratch.wav)')
    
    args = parser.parse_args()
    render_motion_video(args.metadata, args.output_dir, args.fps, args.video, args.quality, args.cleanup, args.cursor, args.scale, args.cursor_scale, args.audio_tap, args.audio_scratch)

if __name__ == '__main__':
    main()