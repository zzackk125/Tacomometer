import math
import os
from PIL import Image, ImageDraw, ImageFont, ImageOps

# Configuration
TARGET_WIDTH = 466
TARGET_HEIGHT = 466
SUPERSAMPLE = 8  # Draw at 8x resolution for maximum smoothness

# Colors
COLOR_BG = "#000000"
COLOR_ACCENT = "#FF6D00" # Tacoma Orange
COLOR_TEXT = "#E0E0E0"
COLOR_TEXT_DIM = "#606060"
COLOR_TICK_MAJOR = "#FFFFFF"
COLOR_TICK_MINOR = "#404040"
COLOR_POINTER = "#FF3D00" # Red-Orange

def write_c_file(name, img, cf_type="LV_COLOR_FORMAT_ARGB8888"):
    """
    Writes a C file compatible with LVGL image converter.
    cf_type: 
      - LV_COLOR_FORMAT_ARGB8888 (32-bit)
      - LV_COLOR_FORMAT_RGB565 (16-bit)
    """
    width, height = img.size
    
    print(f"Writing {name} ({width}x{height}) as {cf_type}...")
    
    with open(f"src/{name}.c", 'w') as f:
        f.write(f"#include <lvgl.h>\n\n")
        f.write(f"#ifndef LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write(f"#define LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write(f"#endif\n\n")
        
        f.write(f"const uint8_t {name}_map[] = {{\n")
        
        data = img.getdata()
        
        if cf_type == "LV_COLOR_FORMAT_ARGB8888":
            # 32-bit ARGB8888
            # LVGL v9 usually expects BGRA order for 32-bit color on little endian
            # But let's check if it's ARGB or BGRA.
            # Standard LVGL v9 ARGB8888 is: B, G, R, A (Little Endian uint32)
            for r, g, b, a in data:
                f.write(f"  0x{b:02x}, 0x{g:02x}, 0x{r:02x}, 0x{a:02x}, \n")
                
        elif cf_type == "LV_COLOR_FORMAT_RGB565":
            # 16-bit RGB565
            # Byte order: Low byte, High byte
            for r, g, b, a in data: # Alpha ignored
                c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                low = c & 0xFF
                high = (c >> 8) & 0xFF
                f.write(f"  0x{low:02x}, 0x{high:02x}, \n")
        
        f.write("};\n\n")
        
        f.write(f"const lv_image_dsc_t {name} = {{\n")
        f.write(f"  .header.cf = {cf_type},\n")
        f.write(f"  .header.magic = LV_IMAGE_HEADER_MAGIC,\n")
        f.write(f"  .header.w = {width},\n")
        f.write(f"  .header.h = {height},\n")
        f.write(f"  .data_size = sizeof({name}_map),\n")
        f.write(f"  .data = {name}_map,\n")
        f.write(f"}};\n")

    # Write Header
    with open(f"src/{name}.h", 'w') as f:
        f.write(f"#ifndef {name.upper()}_H\n")
        f.write(f"#define {name.upper()}_H\n\n")
        f.write(f"#include <lvgl.h>\n\n")
        f.write(f"extern const lv_image_dsc_t {name};\n\n")
        f.write(f"#endif\n")

class Canvas:
    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.img = Image.new("RGBA", (w * SUPERSAMPLE, h * SUPERSAMPLE), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.img)
        self.s = SUPERSAMPLE

    def finish(self):
        return self.img.resize((self.w, self.h), Image.Resampling.LANCZOS)

    def rect(self, x, y, w, h, color, radius=0):
        if radius == 0:
            self.draw.rectangle(
                (x * self.s, y * self.s, (x + w) * self.s, (y + h) * self.s),
                fill=color
            )
        else:
            self.draw.rounded_rectangle(
                (x * self.s, y * self.s, (x + w) * self.s, (y + h) * self.s),
                radius=radius * self.s,
                fill=color
            )

    def circle(self, cx, cy, r, color):
        self.draw.ellipse(
            ((cx - r) * self.s, (cy - r) * self.s, (cx + r) * self.s, (cy + r) * self.s),
            fill=color
        )

    def polygon(self, points, color):
        scaled_points = [(x * self.s, y * self.s) for x, y in points]
        self.draw.polygon(scaled_points, fill=color)

    def line(self, x1, y1, x2, y2, color, width=1):
        self.draw.line(
            (x1 * self.s, y1 * self.s, x2 * self.s, y2 * self.s),
            fill=color,
            width=int(width * self.s)
        )

def generate_background():
    c = Canvas(TARGET_WIDTH, TARGET_HEIGHT)
    
    # Fill Black
    c.rect(0, 0, TARGET_WIDTH, TARGET_HEIGHT, COLOR_BG)
    
    cx, cy = TARGET_WIDTH / 2, TARGET_HEIGHT / 2
    
    # Radii
    r_outer = 230
    r_major = 200
    r_minor = 215
    
    # Draw Scales
    # Roll: Bottom Half (Wait, Roll is usually top arc? Or full circle?)
    # Previous code: Roll on Left (270), Pitch on Right (90).
    # Let's follow the previous design:
    # Left Arc (Roll): 270 +/- 40 degrees (230 to 310)
    # Right Arc (Pitch): 90 +/- 40 degrees (50 to 130)
    
    # Left Arc (Roll)
    for angle in range(270 - 45, 270 + 46, 5): # Extended to 45
        rad = math.radians(angle - 90) # -90 because 0 is usually East, but we want 270 to be West (Left)
        # Wait, standard trig: 0=Right, 90=Down, 180=Left, 270=Up.
        # Let's just use cos/sin directly.
        # 180 is Left. 0 is Right.
        # So Roll is centered at 180.
        # Pitch is centered at 0.
        
        # Correction: The previous code used 270 and 90. Let's look at the UI.
        # "Roll Pointer (Left side, centered at 180)" -> ui.cpp
        # "Pitch Pointer (Right side, centered at 0)" -> ui.cpp
        
        # So:
        # Roll Scale: Centered at 180 (Left). Range: 180 +/- 45.
        # Pitch Scale: Centered at 0 (Right). Range: 0 +/- 45 (or 360 +/- 45).
        
        is_major = (angle % 10 == 0)
        width = 3 if is_major else 1
        color = COLOR_TICK_MAJOR if is_major else COLOR_TICK_MINOR
        r_in = r_major if is_major else r_minor
        
        # Roll (Left)
        a_roll = 180 + (angle - 270) # Map 270->180? No.
        # Let's just iterate angles centered at 180.
        pass

    # Correct Loop for Roll (Left, 180)
    for i in range(-45, 46, 5):
        angle = 180 + i
        rad = math.radians(angle)
        is_major = (i % 10 == 0)
        
        x1 = cx + r_in * math.cos(rad)
        y1 = cy + r_in * math.sin(rad)
        x2 = cx + r_outer * math.cos(rad)
        y2 = cy + r_outer * math.sin(rad)
        
        width = 4 if is_major else 2
        color = COLOR_TICK_MAJOR if is_major else COLOR_TICK_MINOR
        c.line(x1, y1, x2, y2, color, width)

    # Correct Loop for Pitch (Right, 0)
    for i in range(-45, 46, 5):
        angle = i
        rad = math.radians(angle)
        is_major = (i % 10 == 0)
        
        x1 = cx + r_in * math.cos(rad)
        y1 = cy + r_in * math.sin(rad)
        x2 = cx + r_outer * math.cos(rad)
        y2 = cy + r_outer * math.sin(rad)
        
        width = 4 if is_major else 2
        color = COLOR_TICK_MAJOR if is_major else COLOR_TICK_MINOR
        c.line(x1, y1, x2, y2, color, width)

    # We don't draw text here, LVGL handles text for better quality/dynamic.
    # But the user might want static scale numbers?
    # Previous code drew text. Let's draw text for the major ticks (0, 10, 20, 30, 40).
    
    # Load Font
    try:
        font = ImageFont.truetype("arial.ttf", 24 * SUPERSAMPLE) # Large font for supersampling
    except:
        font = ImageFont.load_default()

    # Draw Text
    r_text = 175
    
    def draw_tick_text(angle_deg, val_str):
        rad = math.radians(angle_deg)
        x = cx + r_text * math.cos(rad)
        y = cy + r_text * math.sin(rad)
        
        # Draw on the high-res image directly
        # We need to manually center the text
        bbox = c.draw.textbbox((0, 0), val_str, font=font)
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
        
        c.draw.text(
            (x * SUPERSAMPLE - w/2, y * SUPERSAMPLE - h/2),
            val_str,
            font=font,
            fill=COLOR_TEXT_DIM
        )

    for i in range(-40, 41, 10): # 0, 10, 20, 30, 40
        val = abs(i)
        if val == 0: continue # Skip 0? Or keep it? Let's keep it.
        
        # Roll (Left)
        draw_tick_text(180 + i, str(val))
        
        # Pitch (Right)
        draw_tick_text(i, str(val))

    img = c.finish()
    write_c_file("img_background", img, "LV_COLOR_FORMAT_RGB565")

def generate_truck_rear():
    # 64x64 Sprite
    w, h = 64, 64
    c = Canvas(w, h)
    
    cx, cy = w/2, h/2
    
    # Style: Modern Pickup Rear
    # Colors
    col_body = COLOR_ACCENT
    col_glass = "#202020"
    col_tire = "#303030"
    col_bumper = "#404040"
    
    # Tires (Wide)
    tw, th = 16, 26
    ty = cy + 6
    c.rect(cx - 28, ty, tw, th, col_tire, radius=4) # Left
    c.rect(cx + 12, ty, tw, th, col_tire, radius=4) # Right
    
    # Axle/Diff
    c.rect(cx - 10, ty + 12, 20, 8, "#151515", radius=2)
    c.rect(cx - 20, ty + 14, 40, 4, "#151515")

    # Bumper (Chrome/Grey)
    c.rect(cx - 30, cy + 4, 60, 8, col_bumper, radius=2)
    
    # Main Body (Bed/Tailgate)
    bw, bh = 58, 24
    by = cy - 12
    c.rect(cx - bw/2, by, bw, bh, col_body, radius=2)
    
    # Tailgate Handle (Black, center top)
    c.rect(cx - 5, by + 4, 10, 3, "#101010", radius=1)
    
    # Taillights (Vertical, Red/White/Red)
    # 2nd Gen has distinct vertical lights
    tl_w, tl_h = 6, 14
    tl_y = by + 2
    # Left
    c.rect(cx - bw/2 + 1, tl_y, tl_w, tl_h, "#AA0000", radius=1) 
    c.rect(cx - bw/2 + 1, tl_y + 4, tl_w, 4, "#DDDDDD", radius=0) # Reverse light
    # Right
    c.rect(cx + bw/2 - 7, tl_y, tl_w, tl_h, "#AA0000", radius=1)
    c.rect(cx + bw/2 - 7, tl_y + 4, tl_w, 4, "#DDDDDD", radius=0)

    # Cabin (Access Cab - slightly wider top)
    cw, ch = 50, 20
    cy_cab = by - ch + 2
    
    # Cabin Shape
    pts = [
        (cx - 22, cy_cab),      # Top Left
        (cx + 22, cy_cab),      # Top Right
        (cx + 25, cy_cab + ch), # Bottom Right
        (cx - 25, cy_cab + ch)  # Bottom Left
    ]
    c.polygon(pts, col_body)
    
    # Rear Window (Rectangular with rounded corners)
    c.rect(cx - 18, cy_cab + 4, 36, 10, col_glass, radius=2)
    
    # Side Mirrors (Large)
    c.rect(cx - 31, cy_cab + 6, 5, 8, col_body, radius=1)
    c.rect(cx + 26, cy_cab + 6, 5, 8, col_body, radius=1)

    img = c.finish()
    write_c_file("img_truck_rear", img, "LV_COLOR_FORMAT_ARGB8888")

def generate_truck_side():
    # 64x64 Sprite
    w, h = 64, 64
    c = Canvas(w, h)
    
    cx, cy = w/2, h/2
    col_body = COLOR_ACCENT
    col_glass = "#202020"
    col_tire = "#303030"
    col_plastic = "#252525" # For flares/bumper
    
    # Proportions Update: Long Bed
    # Total width available: ~64px
    # Bed Target: ~34px
    # Cab Target: ~30px
    
    # Split point (Bed/Cab junction): cx + 4
    x_split = cx + 4
    x_rear = cx - 32
    x_nose = cx + 32
    
    # Tires
    tr = 11
    # Rear wheel moved back under the long bed
    tx1 = cx - 16 
    # Front wheel moved under the cab
    tx2 = cx + 20
    ty = cy + 14
    c.circle(tx1, ty, tr, col_tire)
    c.circle(tx2, ty, tr, col_tire)
    
    # Rims
    c.circle(tx1, ty, 6, "#909090")
    c.circle(tx2, ty, 6, "#909090")
    c.circle(tx1, ty, 2, "#303030")
    c.circle(tx2, ty, 2, "#303030")
    
    # Frame
    c.rect(cx - 28, cy + 10, 56, 3, "#151515")

    # --- Main Body ---
    
    # Bed (Left) - LONG BED
    # Starts at x_rear, ends at x_split
    pts_bed = [
        (x_rear, cy - 6),   # Top Rear
        (x_split, cy - 6),  # Top Front
        (x_split, cy + 6),  # Bottom Front
        (tx1 + 8, cy + 6),  # Wheel Arch Start
        (tx1 - 8, cy + 6),  # Wheel Arch End
        (x_rear, cy + 4)    # Bottom Rear
    ]
    c.polygon(pts_bed, col_body)
    
    # Cab (Right) - Compressed Access Cab
    # Starts at x_split, ends at x_nose
    pts_cab = [
        (x_split, cy - 6),      # Back bottom
        (x_split, cy - 20),     # Back top
        (x_split + 12, cy - 20), # Roof end
        (x_split + 20, cy - 10), # Windshield base
        (x_nose, cy - 8),       # Hood flat
        (x_nose, cy + 6),       # Nose vertical
        (tx2 + 8, cy + 6),      # Wheel arch start
        (tx2 - 8, cy + 6),      # Wheel arch end
        (x_split, cy + 6)       # Bottom
    ]
    c.polygon(pts_cab, col_body)
    
    # Wheel Flares
    # Rear Flare
    pts_flare_r = [
        (tx1 - 10, cy - 4),
        (tx1 + 10, cy - 4),
        (tx1 + 10, cy + 4),
        (tx1 + 8, cy + 8),
        (tx1 - 8, cy + 8),
        (tx1 - 10, cy + 4)
    ]
    c.polygon(pts_flare_r, col_body)

    # Front Flare
    pts_flare_f = [
        (tx2 - 8, cy - 6),
        (tx2 + 8, cy - 6),
        (tx2 + 8, cy + 4),
        (tx2 + 6, cy + 8),
        (tx2 - 6, cy + 8),
        (tx2 - 8, cy + 4)
    ]
    c.polygon(pts_flare_f, col_body)

    # Windows
    # Front Door Window
    pts_win_f = [
        (x_split + 4, cy - 18),  # Top Rear
        (x_split + 12, cy - 18), # Top Front
        (x_split + 18, cy - 10), # Bottom Front
        (x_split + 4, cy - 10)   # Bottom Rear
    ]
    c.polygon(pts_win_f, col_glass)
    
    # Rear Access Door Window
    pts_win_r = [
        (x_split + 2, cy - 18),
        (x_split - 4, cy - 18),
        (x_split - 4, cy - 10),
        (x_split + 2, cy - 12)
    ]
    c.polygon(pts_win_r, col_glass)
    
    # B-Pillar
    c.rect(x_split + 2, cy - 18, 2, 8, col_body)

    # Headlight
    pts_light = [
        (x_nose - 2, cy - 8),
        (x_nose, cy - 7),
        (x_nose, cy - 4),
        (x_nose - 3, cy - 4)
    ]
    c.polygon(pts_light, "#DDDDDD")
    
    # Taillight
    c.rect(x_rear, cy - 4, 2, 8, "#AA0000")

    img = c.finish()
    write_c_file("img_truck_side", img, "LV_COLOR_FORMAT_ARGB8888")

def generate_pointer():
    # 20x30 Sprite
    w, h = 20, 30
    c = Canvas(w, h)
    
    cx = w/2
    
    # Triangle
    pts = [
        (cx, 0),    # Tip
        (w, h),     # Bottom Right
        (0, h)      # Bottom Left
    ]
    c.polygon(pts, COLOR_POINTER)
    
    # Inner detail (Darker spine)
    c.line(cx, 5, cx, h-5, "#AA0000", width=2)
    
    img = c.finish()
    write_c_file("img_pointer", img, "LV_COLOR_FORMAT_ARGB8888")

if __name__ == "__main__":
    if not os.path.exists("src"):
        os.makedirs("src")
        
    # generate_background()
    # generate_truck_rear()
    generate_truck_side()
    # generate_pointer()
    print("Done!")
