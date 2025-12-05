
import time
import math
from lib.LCD_1inch28 import LCD_1inch28
from PIL import Image, ImageDraw, ImageFont
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Material Design Colors
COLOR_BG = "#121212"       # Dark background
COLOR_ACCENT = "#FF6D00"   # Deep Orange 
COLOR_TEXT = "#E0E0E0"     # High emphasis text
COLOR_TEXT_DIM = "#9E9E9E" # Medium emphasis text
COLOR_TICK = "#424242"     # Dark grey for minor ticks
COLOR_TICK_MAJOR = "#FF6D00" # Orange for major ticks

class InclinometerUI:
    def __init__(self):
        self.disp = LCD_1inch28()
        # Super-sampling factor
        # Reduced to 1 for Pi Zero performance (4x speedup)
        self.scale = 1
        self.width = self.disp.width * self.scale
        self.height = self.disp.height * self.scale
        self.center_x = self.width // 2
        self.center_y = self.height // 2
        
        # Load fonts (scaled up)
        try:
            self.font_value = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 28 * self.scale)
            self.font_label = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 10 * self.scale)
            self.font_scale = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 10 * self.scale)
        except IOError:
            self.font_value = ImageFont.load_default()
            self.font_label = ImageFont.load_default()
            self.font_scale = ImageFont.load_default()

        # Animation state
        self.curr_roll = 0.0
        self.curr_pitch = 0.0
        self.alpha = 0.25 # Snappier response (was 0.05)

        # Cache static background
        self._bg_cache = self._create_background()

    def _create_background(self):
        """Creates the static background image once"""
        image = Image.new("RGB", (self.width, self.height), COLOR_BG)
        draw = ImageDraw.Draw(image)
        
        radius_outer = 119 * self.scale
        radius_inner_major = 105 * self.scale
        radius_inner_minor = 112 * self.scale
        radius_text = 92 * self.scale

        # Draw ticks for Left (Roll) and Right (Pitch) arcs
        # Left: Centered at 270 (West). Range +/- 40.
        # Right: Centered at 90 (East). Range +/- 40.
        tick_ranges = [range(270-40, 270+41, 5), range(90-40, 90+41, 5)]
        
        for rng in tick_ranges:
            for angle in rng:
                # Normalize angle for math
                rad_angle = angle - 90
                rad = math.radians(rad_angle)
                cos_a = math.cos(rad)
                sin_a = math.sin(rad)
                
                is_major = (angle % 10 == 0)
                
                r_in = radius_inner_major if is_major else radius_inner_minor
                color = COLOR_TICK_MAJOR if is_major else COLOR_TICK
                width = (2 * self.scale) if is_major else (1 * self.scale)
                
                x_out = self.center_x + radius_outer * cos_a
                y_out = self.center_y + radius_outer * sin_a
                x_in = self.center_x + r_in * cos_a
                y_in = self.center_y + r_in * sin_a
                
                draw.line((x_in, y_in, x_out, y_out), fill=color, width=width)
                
                if is_major:
                    # Value relative to center (270 or 90)
                    if angle > 180: # Left side (Roll)
                        val = 270 - angle
                        # User wants no negatives for Roll (30-0-30)
                        text = str(abs(val))
                    else: # Right side (Pitch)
                        val = 90 - angle
                        # User wants signed for Pitch (Positive Up, Negative Down)
                        text = str(val)

                    if abs(val) <= 30:
                        bbox = draw.textbbox((0, 0), text, font=self.font_scale)
                        w = bbox[2] - bbox[0]
                        h = bbox[3] - bbox[1]
                        
                        x_txt = self.center_x + radius_text * cos_a
                        y_txt = self.center_y + radius_text * sin_a
                        
                        draw.text((x_txt - w/2, y_txt - h/2), text, font=self.font_scale, fill=COLOR_TEXT_DIM)
        return image

    def get_truck_rear_layer(self, roll_angle):
        size = 64 * self.scale
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        s = 0.6 * self.scale
        
        # 2006 Tacoma Rear View
        
        # Tires (Wider stance)
        tire_w, tire_h = 12 * s, 22 * s
        tire_offset_x = 30 * s
        tire_offset_y = 10 * s
        
        draw.rectangle((cx - tire_offset_x - tire_w, cy + tire_offset_y, cx - tire_offset_x, cy + tire_offset_y + tire_h), fill="#333")
        draw.rectangle((cx + tire_offset_x, cy + tire_offset_y, cx + tire_offset_x + tire_w, cy + tire_offset_y + tire_h), fill="#333")
        
        # Axle/Diff
        draw.rectangle((cx - tire_offset_x, cy + tire_offset_y + tire_h//2 - 2*s, cx + tire_offset_x, cy + tire_offset_y + tire_h//2 + 2*s), fill="#222")
        # Diff pumpkin
        draw.ellipse((cx - 4*s, cy + tire_offset_y + tire_h//2 - 4*s, cx + 4*s, cy + tire_offset_y + tire_h//2 + 4*s), fill="#222")

        # Body (Bed/Tailgate)
        body_w, body_h = 64 * s, 28 * s
        body_top = cy - body_h//2
        body_bottom = cy + body_h//2
        
        # Main Box
        draw.rectangle((cx - body_w//2, body_top, cx + body_w//2, body_bottom), fill=COLOR_ACCENT)
        
        # Fender Flares (The "Tacoma" look)
        flare_w = 6 * s
        draw.polygon([(cx - body_w//2, body_top), (cx - body_w//2 - flare_w, body_top + 5*s), (cx - body_w//2 - flare_w, body_bottom), (cx - body_w//2, body_bottom)], fill=COLOR_ACCENT)
        draw.polygon([(cx + body_w//2, body_top), (cx + body_w//2 + flare_w, body_top + 5*s), (cx + body_w//2 + flare_w, body_bottom), (cx + body_w//2, body_bottom)], fill=COLOR_ACCENT)

        # Cabin (Rear window)
        cab_w_bottom = 56 * s
        cab_w_top = 48 * s
        cab_h = 24 * s
        cab_bottom = body_top
        cab_top = cab_bottom - cab_h
        
        draw.polygon([
            (cx - cab_w_top//2, cab_top),
            (cx + cab_w_top//2, cab_top),
            (cx + cab_w_bottom//2, cab_bottom),
            (cx - cab_w_bottom//2, cab_bottom)
        ], fill=COLOR_ACCENT)
        
        # Rear Window (Dark Grey)
        win_w_top = 44 * s
        win_w_bottom = 50 * s
        win_h = 14 * s
        win_top = cab_top + 4*s
        
        draw.polygon([
            (cx - win_w_top//2, win_top),
            (cx + win_w_top//2, win_top),
            (cx + win_w_bottom//2, win_top + win_h),
            (cx - win_w_bottom//2, win_top + win_h)
        ], fill="#222")
        
        # Bumper
        bumper_h = 6 * s
        draw.rectangle((cx - body_w//2 - 2*s, body_bottom, cx + body_w//2 + 2*s, body_bottom + bumper_h), fill="#555")

        return img.rotate(-roll_angle, resample=Image.BICUBIC)

    def get_truck_side_layer(self, pitch_angle):
        size = 64 * self.scale
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        s = 0.6 * self.scale
        
        # 2006 Tacoma Double Cab Side View (Long Bed)
        
        # Wheels
        wheel_r = 11 * s
        wheel_front_x = cx + 28 * s
        wheel_rear_x = cx - 36 * s # Moved back slightly more
        wheel_y = cy + 14 * s
        
        draw.ellipse((wheel_front_x - wheel_r, wheel_y - wheel_r, wheel_front_x + wheel_r, wheel_y + wheel_r), fill="#333")
        draw.ellipse((wheel_rear_x - wheel_r, wheel_y - wheel_r, wheel_rear_x + wheel_r, wheel_y + wheel_r), fill="#333")
        
        # Body Line (Chassis/Rockers)
        draw.rectangle((wheel_rear_x, wheel_y - 8*s, wheel_front_x, wheel_y), fill="#222")

        # Main Body Shape
        # Nose, Hood, Windshield, Roof, Rear Window, Bed
        
        poly = [
            (cx + 42*s, wheel_y - 5*s),  # Front Bumper Bottom
            (cx + 42*s, cy - 6*s),       # Nose Top
            (cx + 25*s, cy - 8*s),       # Hood Rear / Windshield Base
            (cx + 10*s, cy - 24*s),      # Roof Front
            (cx - 16*s, cy - 24*s),      # Roof Rear (Shortened Cab)
            (cx - 21*s, cy - 8*s),       # Cab Rear Base (Shortened Cab)
            (cx - 56*s, cy - 8*s),       # Bed Rear Top (Lengthened Bed)
            (cx - 56*s, wheel_y - 5*s),  # Bed Rear Bottom / Bumper
            (cx - 32*s, wheel_y - 5*s),  # Wheel Well Rear
        ]
        
        # Draw main body
        draw.polygon(poly, fill=COLOR_ACCENT)
        
        # Windows (Double Cab)
        # Side windows are usually black/dark
        win_poly = [
            (cx + 8*s, cy - 20*s),       # Front Top
            (cx + 20*s, cy - 8*s),       # Front Bottom (A-Pillar base)
            (cx - 14*s, cy - 8*s),       # Rear Bottom (Shortened Cab)
            (cx - 12*s, cy - 20*s),      # Rear Top (Shortened Cab)
        ]
        draw.polygon(win_poly, fill="#222")
        
        # B-Pillar (Body color strip)
        draw.line((cx - 2*s, cy - 20*s, cx - 2*s, cy - 8*s), fill=COLOR_ACCENT, width=int(3*s))
        
        # Bed separation line
        draw.line((cx - 21*s, cy - 8*s, cx - 21*s, wheel_y - 5*s), fill="#111", width=1)

        return img.rotate(pitch_angle, resample=Image.BICUBIC)

    def draw_pointer(self, draw, angle_deg, radius):
        """Draws a triangular pointer at the given angle and radius"""
        rad = math.radians(angle_deg - 90)
        
        # Tip of the triangle (pointing outwards)
        tip_x = self.center_x + radius * math.cos(rad)
        tip_y = self.center_y + radius * math.sin(rad)
        
        # Base center (slightly inwards)
        base_dist = 15 * self.scale
        base_cx = self.center_x + (radius - base_dist) * math.cos(rad)
        base_cy = self.center_y + (radius - base_dist) * math.sin(rad)
        
        # Base width
        width = 8 * self.scale
        
        # Perpendicular vector
        perp_x = -math.sin(rad)
        perp_y = math.cos(rad)
        
        p1 = (base_cx + width * perp_x, base_cy + width * perp_y)
        p2 = (base_cx - width * perp_x, base_cy - width * perp_y)
        
        draw.polygon([p1, p2, (tip_x, tip_y)], fill=COLOR_ACCENT)

    def update(self, target_roll, target_pitch):
        # Smooth animation
        self.curr_roll += (target_roll - self.curr_roll) * self.alpha
        self.curr_pitch += (target_pitch - self.curr_pitch) * self.alpha
        
        # Start with cached background
        image = self._bg_cache.copy()
        draw = ImageDraw.Draw(image)
        
        # --- Pointers ---
        # Roll (Left Scale, centered at 270)
        roll_angle_map = 270 - self.curr_roll
        self.draw_pointer(draw, roll_angle_map, 100 * self.scale)
        
        # Pitch (Right Scale, centered at 90)
        pitch_angle_map = 90 - self.curr_pitch
        self.draw_pointer(draw, pitch_angle_map, 100 * self.scale)
        
        # --- Labels ---
        label_offset_y = 8 * self.scale
        label_offset_x = 40 * self.scale
        
        # --- Values & Icons Layout ---
        # Roll Value (Left)
        # Move closer to center: 25 * scale instead of 50
        roll_text = f"{abs(int(self.curr_roll))}°"
        draw.text((self.center_x - 25 * self.scale, self.center_y), roll_text, font=self.font_value, fill=COLOR_TEXT, anchor="rm")
        draw.text((self.center_x - 25 * self.scale, self.center_y + 20 * self.scale), "ROLL", font=self.font_label, fill=COLOR_TEXT_DIM, anchor="rm")

        # Pitch Value (Right)
        # Move closer to center: 25 * scale instead of 50
        pitch_text = f"{int(self.curr_pitch)}°"
        draw.text((self.center_x + 25 * self.scale, self.center_y), pitch_text, font=self.font_value, fill=COLOR_TEXT, anchor="lm")
        draw.text((self.center_x + 25 * self.scale, self.center_y + 20 * self.scale), "PITCH", font=self.font_label, fill=COLOR_TEXT_DIM, anchor="lm")
        
        # Roll Truck (Top Center)
        roll_layer = self.get_truck_rear_layer(self.curr_roll)
        image.paste(roll_layer, (self.center_x - (32 * self.scale), self.center_y - (80 * self.scale)), roll_layer)
        
        # Pitch Truck (Bottom Center)
        pitch_layer = self.get_truck_side_layer(self.curr_pitch)
        image.paste(pitch_layer, (self.center_x - (32 * self.scale), self.center_y + (40 * self.scale)), pitch_layer)
        
        # No resizing needed (Scale = 1)
        self.disp.ShowImage(image)

def main():
    ui = InclinometerUI()
    logger.info("Starting Modern Inclinometer UI...")
    
    try:
        target_roll = 0
        target_pitch = 0
        step = 5
        while True:
            ui.update(target_roll, target_pitch)
            
            # Change targets periodically to test smoothing
            if abs(ui.curr_roll - target_roll) < 1:
                target_roll += step
                target_pitch += step * 0.5
                if target_roll > 30 or target_roll < -30:
                    step *= -1
            
            # time.sleep(0.01)
            
    except KeyboardInterrupt:
        logger.info("Exiting...")

if __name__ == "__main__":
    main()
