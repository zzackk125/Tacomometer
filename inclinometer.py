
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
        self.scale = 2
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
        self.alpha = 0.2 # Smoothing factor (0.0 - 1.0)

    def draw_background(self, draw):
        """Draws the static background elements (ticks, labels)"""
        draw.rectangle((0, 0, self.width, self.height), fill=COLOR_BG)
        
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
                    if angle > 180: # Left side
                        val = 270 - angle
                    else: # Right side
                        val = 90 - angle
                    
                    # Invert sign so Up is positive?
                    # Usually Up is positive pitch/roll in these displays
                    val = -val 

                    if abs(val) <= 30:
                        text = str(val)
                        bbox = draw.textbbox((0, 0), text, font=self.font_scale)
                        w = bbox[2] - bbox[0]
                        h = bbox[3] - bbox[1]
                        
                        x_txt = self.center_x + radius_text * cos_a
                        y_txt = self.center_y + radius_text * sin_a
                        
                        draw.text((x_txt - w/2, y_txt - h/2), text, font=self.font_scale, fill=COLOR_TEXT_DIM)

    def get_truck_rear_layer(self, roll_angle):
        size = 64 * self.scale
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        s = 0.6 * self.scale
        
        # Tires
        tire_w, tire_h = 10 * s, 20 * s
        tire_offset_x = 28 * s
        tire_offset_y = 8 * s
        
        draw.rectangle((cx - tire_offset_x - tire_w, cy + tire_offset_y, cx - tire_offset_x, cy + tire_offset_y + tire_h), fill="#444")
        draw.rectangle((cx + tire_offset_x, cy + tire_offset_y, cx + tire_offset_x + tire_w, cy + tire_offset_y + tire_h), fill="#444")
        
        # Axle
        draw.line((cx - tire_offset_x, cy + tire_offset_y + tire_h/2, cx + tire_offset_x, cy + tire_offset_y + tire_h/2), fill="#333", width=int(3*s))

        # Body
        body_w, body_h = 50 * s, 30 * s
        draw.polygon([
            (cx - body_w//2, cy - body_h//2), 
            (cx + body_w//2, cy - body_h//2), 
            (cx + body_w//2, cy + body_h//2), 
            (cx - body_w//2, cy + body_h//2), 
        ], fill=COLOR_ACCENT)
        
        # Window
        win_w, win_h = 34 * s, 12 * s
        draw.rectangle((cx - win_w//2, cy - body_h//2 + 4*s, cx + win_w//2, cy - body_h//2 + 4*s + win_h), fill="#222")

        return img.rotate(-roll_angle, resample=Image.BICUBIC)

    def get_truck_side_layer(self, pitch_angle):
        size = 64 * self.scale
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        s = 0.6 * self.scale
        
        # Wheels
        wheel_r = 9 * s
        wheel_dist = 22 * s
        draw.ellipse((cx - wheel_dist - wheel_r, cy + 12*s - wheel_r, cx - wheel_dist + wheel_r, cy + 12*s + wheel_r), fill="#444")
        draw.ellipse((cx + wheel_dist - wheel_r, cy + 12*s - wheel_r, cx + wheel_dist + wheel_r, cy + 12*s + wheel_r), fill="#444")
        
        # Body
        poly = [
            (cx - 30*s, cy + 12*s), 
            (cx + 30*s, cy + 12*s), 
            (cx + 30*s, cy - 4*s),  
            (cx + 12*s, cy - 4*s),  
            (cx + 4*s, cy - 16*s),  
            (cx - 12*s, cy - 16*s), 
            (cx - 12*s, cy - 4*s),  
            (cx - 30*s, cy - 4*s),  
        ]
        draw.polygon(poly, fill=COLOR_ACCENT)
        
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
        
        # Draw at 2x resolution
        image = Image.new("RGB", (self.width, self.height), COLOR_BG)
        draw = ImageDraw.Draw(image)
        
        self.draw_background(draw)
        
        # --- Pointers ---
        # Roll (Left Scale, centered at 270)
        # +Roll -> Up (Counter-clockwise from 270) -> 270 - Roll? No, 270 is West.
        # 0 -> 270. +10 -> 260 (Up). -10 -> 280 (Down).
        # So Angle = 270 - Roll
        roll_angle_map = 270 - self.curr_roll
        self.draw_pointer(draw, roll_angle_map, 100 * self.scale)
        
        # Pitch (Right Scale, centered at 90)
        # +Pitch -> Up (Clockwise from 90? No, 90 is East. 0 is North, 180 South).
        # 0 -> 90. +10 -> 80 (Up). -10 -> 100 (Down).
        # So Angle = 90 - Pitch
        pitch_angle_map = 90 - self.curr_pitch
        self.draw_pointer(draw, pitch_angle_map, 100 * self.scale)
        
        # --- Labels ---
        label_offset_y = 8 * self.scale
        label_offset_x = 40 * self.scale
        
        # --- Values & Icons Layout ---
        # Roll Value (Left)
        # Move closer to center: 25 * scale instead of 50
        roll_text = f"{int(self.curr_roll)}°"
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
        
        # Downscale for display (High Quality Anti-Aliasing)
        final_image = image.resize((self.disp.width, self.disp.height), resample=Image.LANCZOS)
        
        self.disp.ShowImage(final_image)

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
