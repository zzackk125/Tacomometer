import os
import sys
import math
from PIL import Image, ImageDraw, ImageFont

# Configuration
OUTPUT_DIR = "."
TARGET_WIDTH = 466
TARGET_HEIGHT = 466
SCALE_FACTOR = TARGET_WIDTH / 240.0

# Colors
COLOR_BG = "#121212"
COLOR_ACCENT = "#FF6D00"
COLOR_TEXT = "#E0E0E0"
COLOR_TEXT_DIM = "#9E9E9E"
COLOR_TICK = "#424242"
COLOR_TICK_MAJOR = "#FF6D00"

def color565(r, g, b):
    """Convert RGB888 to RGB565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def write_c_files(name, data, width, height, is_argb=False):
    type_str = "uint32_t" if is_argb else "uint16_t"
    
    with open(f"{name}.h", 'w') as f:
        f.write(f"#ifndef {name.upper()}_H\n")
        f.write(f"#define {name.upper()}_H\n\n")
        f.write(f"#include <Arduino.h>\n")
        f.write(f"#include <pgmspace.h>\n\n")
        f.write(f"extern const uint16_t {name}_width;\n")
        f.write(f"extern const uint16_t {name}_height;\n")
        f.write(f"extern const {type_str} {name}_data[] PROGMEM;\n\n")
        f.write(f"#endif // {name.upper()}_H\n")

    with open(f"{name}.cpp", 'w') as f:
        f.write(f"#include \"{name}.h\"\n\n")
        f.write(f"const uint16_t {name}_width = {width};\n")
        f.write(f"const uint16_t {name}_height = {height};\n\n")
        f.write(f"const {type_str} {name}_data[] PROGMEM = {{\n")
        
        items_per_line = 8 if is_argb else 16
        for i in range(0, len(data), items_per_line):
            chunk = data[i:i+items_per_line]
            if is_argb:
                hex_chunk = [f"0x{x:08X}" for x in chunk]
            else:
                hex_chunk = [f"0x{x:04X}" for x in chunk]
            f.write("  " + ", ".join(hex_chunk) + ",\n")
            
        f.write("};\n")

def generate_assets():
    print("Generating Assets v5 (ARGB8888)...")
    
    # 1. Background (RGB565 - Opaque)
    print("Generating Background...")
    img_bg = Image.new("RGB", (TARGET_WIDTH, TARGET_HEIGHT), COLOR_BG)
    draw = ImageDraw.Draw(img_bg)
    
    center_x = TARGET_WIDTH // 2
    center_y = TARGET_HEIGHT // 2
    s = SCALE_FACTOR
    
    radius_outer = 119 * s
    radius_inner_major = 105 * s
    radius_inner_minor = 112 * s
    radius_text = 92 * s
    
    try:
        font_scale = ImageFont.truetype("arial.ttf", int(10 * s))
    except:
        font_scale = ImageFont.load_default()

    # Draw ticks for Left (Roll) and Right (Pitch) arcs
    # Left: Centered at 180 (West). Range +/- 40.
    # Right: Centered at 0 (East). Range +/- 40.
    tick_ranges = [range(180-40, 180+41, 5), range(-40, 41, 5)]
    
    for rng in tick_ranges:
        for angle in rng:
            # Angle is already in degrees for cos/sin
            rad = math.radians(angle)
            cos_a = math.cos(rad)
            sin_a = math.sin(rad)
            
            is_major = (angle % 10 == 0)
            
            r_in = radius_inner_major if is_major else radius_inner_minor
            color = COLOR_TICK_MAJOR if is_major else COLOR_TICK
            width = int(2 * s) if is_major else int(1 * s)
            
            x_out = center_x + radius_outer * cos_a
            y_out = center_y + radius_outer * sin_a
            x_in = center_x + r_in * cos_a
            y_in = center_y + r_in * sin_a
            
            draw.line((x_in, y_in, x_out, y_out), fill=color, width=width)
            
            if is_major:
                # Value relative to center (180 or 0)
                if angle > 90 and angle < 270: # Left side (Roll)
                    val = 180 - angle
                    text = str(abs(val))
                else: # Right side (Pitch)
                    val = -angle # Positive Up (negative angle), Negative Down (positive angle)
                    # Wait, standard circle: 0 is Right. -angle goes Up (Counter-Clockwise? No, Clockwise is +Y in PIL)
                    # PIL: +Y is Down. +Angle is Clockwise.
                    # 0 is Right. 90 is Bottom. -90 is Top.
                    # So -40 is Top-Right. +40 is Bottom-Right.
                    # User wants Pitch: Positive Up?
                    # If angle is -40 (Top Right), val should be +40?
                    # val = -angle.
                    text = str(val)

                if abs(val) <= 30:
                    bbox = draw.textbbox((0, 0), text, font=font_scale)
                    w = bbox[2] - bbox[0]
                    h = bbox[3] - bbox[1]
                    
                    x_txt = center_x + radius_text * cos_a
                    y_txt = center_y + radius_text * sin_a
                    
                    draw.text((x_txt - w/2, y_txt - h/2), text, font=font_scale, fill=COLOR_TEXT_DIM)

    bg_data = [color565(r, g, b) for r, g, b in img_bg.getdata()]
    write_c_files("background", bg_data, TARGET_WIDTH, TARGET_HEIGHT, is_argb=False)

    # 2. Truck Sprites (ARGB8888 - Transparent)
    truck_size = int(64 * s)
    cx, cy = truck_size//2, truck_size//2
    ts = 0.6 * s

    # REAR
    print("Generating Truck Rear...")
    img_rear = Image.new("RGBA", (truck_size, truck_size), (0, 0, 0, 0)) # Transparent
    draw_rear = ImageDraw.Draw(img_rear)
    
    # Tires
    tire_w, tire_h = 12 * ts, 22 * ts
    tire_offset_x = 30 * ts
    tire_offset_y = 10 * ts
    draw_rear.rectangle((cx - tire_offset_x - tire_w, cy + tire_offset_y, cx - tire_offset_x, cy + tire_offset_y + tire_h), fill="#333333")
    draw_rear.rectangle((cx + tire_offset_x, cy + tire_offset_y, cx + tire_offset_x + tire_w, cy + tire_offset_y + tire_h), fill="#333333")
    # Axle
    draw_rear.rectangle((cx - tire_offset_x, cy + tire_offset_y + tire_h//2 - 2*ts, cx + tire_offset_x, cy + tire_offset_y + tire_h//2 + 2*ts), fill="#222222")
    # Body
    body_w, body_h = 64 * ts, 28 * ts
    body_top = cy - body_h//2
    body_bottom = cy + body_h//2
    draw_rear.rectangle((cx - body_w//2, body_top, cx + body_w//2, body_bottom), fill=COLOR_ACCENT)
    # Cabin
    cab_w_top = 48 * ts
    cab_w_bottom = 56 * ts
    cab_h = 24 * ts
    cab_bottom = body_top
    cab_top = cab_bottom - cab_h
    draw_rear.polygon([(cx - cab_w_top//2, cab_top), (cx + cab_w_top//2, cab_top), (cx + cab_w_bottom//2, cab_bottom), (cx - cab_w_bottom//2, cab_bottom)], fill=COLOR_ACCENT)
    # Window
    win_w_top = 44 * ts
    win_w_bottom = 50 * ts
    win_h = 14 * ts
    win_top = cab_top + 4*ts
    draw_rear.polygon([(cx - win_w_top//2, win_top), (cx + win_w_top//2, win_top), (cx + win_w_bottom//2, win_top + win_h), (cx - win_w_bottom//2, win_top + win_h)], fill="#222222")

    # Convert to ARGB8888 (0xAARRGGBB)
    rear_data = []
    for r, g, b, a in img_rear.getdata():
        rear_data.append((a << 24) | (r << 16) | (g << 8) | b)
    write_c_files("truck_rear", rear_data, truck_size, truck_size, is_argb=True)

    # SIDE
    print("Generating Truck Side...")
    img_side = Image.new("RGBA", (truck_size, truck_size), (0, 0, 0, 0))
    draw_side = ImageDraw.Draw(img_side)
    
    wheel_r = 11 * ts
    wheel_front_x = cx + 28 * ts
    wheel_rear_x = cx - 36 * ts
    wheel_y = cy + 14 * ts
    draw_side.ellipse((wheel_front_x - wheel_r, wheel_y - wheel_r, wheel_front_x + wheel_r, wheel_y + wheel_r), fill="#333333")
    draw_side.ellipse((wheel_rear_x - wheel_r, wheel_y - wheel_r, wheel_rear_x + wheel_r, wheel_y + wheel_r), fill="#333333")
    
    poly = [
        (cx + 42*ts, wheel_y - 5*ts), (cx + 42*ts, cy - 6*ts), (cx + 25*ts, cy - 8*ts),
        (cx + 10*ts, cy - 24*ts), (cx - 16*ts, cy - 24*ts), (cx - 21*ts, cy - 8*ts),
        (cx - 56*ts, cy - 8*ts), (cx - 56*ts, wheel_y - 5*ts), (cx - 32*ts, wheel_y - 5*ts),
    ]
    draw_side.polygon(poly, fill=COLOR_ACCENT)
    
    side_data = []
    for r, g, b, a in img_side.getdata():
        side_data.append((a << 24) | (r << 16) | (g << 8) | b)
    write_c_files("truck_side", side_data, truck_size, truck_size, is_argb=True)
    
    # 3. Pointer (ARGB8888 - Transparent)
    print("Generating Pointer...")
    pointer_w, pointer_h = 20, 30
    img_ptr = Image.new("RGBA", (pointer_w, pointer_h), (0, 0, 0, 0))
    draw_ptr = ImageDraw.Draw(img_ptr)
    
    # Triangle pointing UP
    # Base at bottom, point at top
    draw_ptr.polygon([(pointer_w//2, 0), (0, pointer_h), (pointer_w, pointer_h)], fill=COLOR_ACCENT)
    
    ptr_data = []
    for r, g, b, a in img_ptr.getdata():
        ptr_data.append((a << 24) | (r << 16) | (g << 8) | b)
    write_c_files("pointer", ptr_data, pointer_w, pointer_h, is_argb=True)
    
    print("Done.")

if __name__ == "__main__":
    generate_assets()
