import os
import sys
import math
from PIL import Image, ImageDraw, ImageFont

# Mock the LCD module since we are on PC
sys.modules['lib.LCD_1inch28'] = type('obj', (object,), {'LCD_1inch28': type('obj', (object,), {'width': 240, 'height': 240})})

# Import the UI class (script is in old/, so inclinometer is local)
try:
    from inclinometer import InclinometerUI, COLOR_BG, COLOR_ACCENT, COLOR_TEXT, COLOR_TEXT_DIM, COLOR_TICK, COLOR_TICK_MAJOR
except ImportError:
    # Fallback if run from root with module path issues
    sys.path.append(os.path.join(os.getcwd(), 'old'))
    from inclinometer import InclinometerUI, COLOR_BG, COLOR_ACCENT, COLOR_TEXT, COLOR_TEXT_DIM, COLOR_TICK, COLOR_TICK_MAJOR

# Output Configuration
# Output Configuration
# Output to parent directory (project root) if run from old/
# Or current directory if run from root? 
# We want them in c:\Users\zzack\Documents\Tacomometer
# If script is run as "python old/generate_assets.py", CWD is root.
OUTPUT_DIR = "."
TARGET_WIDTH = 466
TARGET_HEIGHT = 466
SCALE_FACTOR = TARGET_WIDTH / 240.0

def color565(r, g, b):
    """Convert RGB888 to RGB565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def write_c_files(name, data, width, height):
    # Write Header (.h)
    with open(f"{name}.h", 'w') as f:
        f.write(f"#ifndef {name}_H\n")
        f.write(f"#define {name}_H\n\n")
        f.write(f"#include <Arduino.h>\n")
        f.write(f"#include <pgmspace.h>\n\n")
        f.write(f"extern const uint16_t {name}_width;\n")
        f.write(f"extern const uint16_t {name}_height;\n")
        f.write(f"extern const uint16_t {name}_data[] PROGMEM;\n\n")
        f.write(f"#endif // {name}_H\n")

    # Write Source (.cpp)
    with open(f"{name}.cpp", 'w') as f:
        f.write(f"#include \"{name}.h\"\n\n")
        f.write(f"const uint16_t {name}_width = {width};\n")
        f.write(f"const uint16_t {name}_height = {height};\n\n")
        f.write(f"const uint16_t {name}_data[] PROGMEM = {{\n")
        
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_chunk = [f"0x{x:04X}" for x in chunk]
            f.write("  " + ", ".join(hex_chunk) + ",\n")
            
        f.write("};\n")

def generate_assets():
    print("Initializing InclinometerUI for asset generation...")
    # We need to patch the InclinometerUI to use our scale or just resize the output
    # Since the original code uses self.scale = 1, we can just instantiate it, 
    # get the base images, and resize them with high quality resampling.
    
    ui = InclinometerUI()
    
    # 1. Generate Background
    print("Generating Background...")
    # The original background is 240x240. We want 466x466.
    # To get high quality, it would be better to render at higher resolution, 
    # but reusing the logic is easier by resizing.
    # However, the original code draws lines which might look blurry if resized up.
    # Let's try to monkey-patch the scale in the __init__ if possible, or just resize.
    # The __init__ sets self.scale = 1.
    
    # Let's create a new instance but modify the scale logic if we can, 
    # or just use the _create_background method with patched parameters.
    
    # Actually, we can just subclass or copy the logic. 
    # For now, let's resize the 240x240 image and see. 
    # If it looks bad, we might need to rewrite the drawing logic in this script adapted for 466px.
    
    # RE-IMPLEMENTING DRAWING FOR HIGH RES
    # It's safer to re-draw at 466x466 than to upscale a 240x240 raster.
    
    img_bg = Image.new("RGB", (TARGET_WIDTH, TARGET_HEIGHT), COLOR_BG)
    draw = ImageDraw.Draw(img_bg)
    
    center_x = TARGET_WIDTH // 2
    center_y = TARGET_HEIGHT // 2
    
    # Scale dimensions from 240 to 466
    s = SCALE_FACTOR
    
    radius_outer = 119 * s
    radius_inner_major = 105 * s
    radius_inner_minor = 112 * s
    radius_text = 92 * s
    
    # Load Fonts
    try:
        # Try to find a font, or use default
        font_scale = ImageFont.truetype("arial.ttf", int(10 * s))
    except:
        font_scale = ImageFont.load_default()

    tick_ranges = [range(270-40, 270+41, 5), range(90-40, 90+41, 5)]
    
    for rng in tick_ranges:
        for angle in rng:
            rad_angle = angle - 90
            rad = math.radians(rad_angle)
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
                if angle > 180: # Left side (Roll)
                    val = 270 - angle
                    text = str(abs(val))
                else: # Right side (Pitch)
                    val = 90 - angle
                    text = str(val)

                if abs(val) <= 30:
                    bbox = draw.textbbox((0, 0), text, font=font_scale)
                    w = bbox[2] - bbox[0]
                    h = bbox[3] - bbox[1]
                    
                    x_txt = center_x + radius_text * cos_a
                    y_txt = center_y + radius_text * sin_a
                    
                    draw.text((x_txt - w/2, y_txt - h/2), text, font=font_scale, fill=COLOR_TEXT_DIM)

    # Save Background
    # img_bg.save("background_preview.png")
    bg_data = [color565(r, g, b) for r, g, b in img_bg.getdata()]
    write_c_files("background", bg_data, TARGET_WIDTH, TARGET_HEIGHT)
    print("Background generated.")

    # 2. Generate Truck Sprites
    # We need high res truck sprites.
    # The original _generate_base_rear uses 64*scale size.
    # We want it scaled up by SCALE_FACTOR.
    
    truck_size = int(64 * s)
    
    # REAR
    img_rear = Image.new("RGBA", (truck_size, truck_size), (0,0,0,0))
    draw_rear = ImageDraw.Draw(img_rear)
    cx, cy = truck_size//2, truck_size//2
    ts = 0.6 * s # Truck scale inside the sprite
    
    # Copying logic from _generate_base_rear but using 'ts'
    # Tires
    tire_w, tire_h = 12 * ts, 22 * ts
    tire_offset_x = 30 * ts
    tire_offset_y = 10 * ts
    draw_rear.rectangle((cx - tire_offset_x - tire_w, cy + tire_offset_y, cx - tire_offset_x, cy + tire_offset_y + tire_h), fill="#333")
    draw_rear.rectangle((cx + tire_offset_x, cy + tire_offset_y, cx + tire_offset_x + tire_w, cy + tire_offset_y + tire_h), fill="#333")
    # Axle
    draw_rear.rectangle((cx - tire_offset_x, cy + tire_offset_y + tire_h//2 - 2*ts, cx + tire_offset_x, cy + tire_offset_y + tire_h//2 + 2*ts), fill="#222")
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
    draw_rear.polygon([(cx - win_w_top//2, win_top), (cx + win_w_top//2, win_top), (cx + win_w_bottom//2, win_top + win_h), (cx - win_w_bottom//2, win_top + win_h)], fill="#222")
    
    # Save Rear
    # img_rear.save("truck_rear_preview.png")
    # For sprites, we need to handle transparency. 
    # Arduino_GFX draw16bitRGBBitmap with transparent color?
    # Or we can just output 0x0000 for transparent pixels and use a specific color key.
    # Let's use Magenta (0xF81F) as transparency key if needed, or just 0.
    # But wait, we want anti-aliasing.
    # If we want AA on top of background, we need alpha blending at runtime or pre-composed.
    # Since the background is static but the truck rotates, we can't pre-compose easily.
    # We will export as RGB565 and maybe a separate Alpha channel?
    # Or just 1-bit alpha (mask).
    # Let's stick to simple color key transparency for now to save space/complexity.
    # We will use Black (0x0000) as transparent if the truck doesn't have black parts.
    # The truck has tires (#333) and window (#222). Pure black (0,0,0) is safe?
    # The background is #121212.
    # Let's use pure black (0,0,0) as transparent key.
    
    rear_data = []
    for r, g, b, a in img_rear.getdata():
        if a < 128:
            rear_data.append(0x0000) # Transparent
        else:
            c = color565(r, g, b)
            if c == 0x0000: c = 0x0001 # Avoid actual black in visible pixels
            rear_data.append(c)
            
    write_c_files("truck_rear", rear_data, truck_size, truck_size)
    print("Truck Rear generated.")

    # SIDE
    img_side = Image.new("RGBA", (truck_size, truck_size), (0,0,0,0))
    draw_side = ImageDraw.Draw(img_side)
    
    # Copying logic from _generate_base_side
    wheel_r = 11 * ts
    wheel_front_x = cx + 28 * ts
    wheel_rear_x = cx - 36 * ts
    wheel_y = cy + 14 * ts
    draw_side.ellipse((wheel_front_x - wheel_r, wheel_y - wheel_r, wheel_front_x + wheel_r, wheel_y + wheel_r), fill="#333")
    draw_side.ellipse((wheel_rear_x - wheel_r, wheel_y - wheel_r, wheel_rear_x + wheel_r, wheel_y + wheel_r), fill="#333")
    
    poly = [
        (cx + 42*ts, wheel_y - 5*ts), (cx + 42*ts, cy - 6*ts), (cx + 25*ts, cy - 8*ts),
        (cx + 10*ts, cy - 24*ts), (cx - 16*ts, cy - 24*ts), (cx - 21*ts, cy - 8*ts),
        (cx - 56*ts, cy - 8*ts), (cx - 56*ts, wheel_y - 5*ts), (cx - 32*ts, wheel_y - 5*ts),
    ]
    draw_side.polygon(poly, fill=COLOR_ACCENT)
    
    # Save Side
    side_data = []
    for r, g, b, a in img_side.getdata():
        if a < 128:
            side_data.append(0x0000)
        else:
            c = color565(r, g, b)
            if c == 0x0000: c = 0x0001
            side_data.append(c)
            
    write_c_files("truck_side", side_data, truck_size, truck_size)
    print("Truck Side generated.")

if __name__ == "__main__":
    generate_assets()
