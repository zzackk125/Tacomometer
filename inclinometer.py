
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
COLOR_TICK_MAJOR = "#E0E0E0" # Light grey for major ticks

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

        # Draw ticks
        for angle in range(0, 360, 10):
            rad = math.radians(angle - 90)
            cos_a = math.cos(rad)
            sin_a = math.sin(rad)
            
            is_major = (angle % 30 == 0)
            
            r_in = radius_inner_major if is_major else radius_inner_minor
            color = COLOR_TICK_MAJOR if is_major else COLOR_TICK
            width = (2 * self.scale) if is_major else (1 * self.scale)
            
            x_out = self.center_x + radius_outer * cos_a
            y_out = self.center_y + radius_outer * sin_a
            x_in = self.center_x + r_in * cos_a
            y_in = self.center_y + r_in * sin_a
            
            draw.line((x_in, y_in, x_out, y_out), fill=color, width=width)
            
            if is_major:
                val = angle
                if val > 180: val = 360 - val
                if val == 180: val = 0
                
                if val % 90 != 0:
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

    def draw_arrow(self, draw, x, y, direction="right", color=COLOR_ACCENT):
        size = 6 * self.scale
        if direction == "right":
            points = [(x, y-size), (x+size, y), (x, y+size)]
        else:
            points = [(x, y-size), (x-size, y), (x, y+size)]
        draw.polygon(points, fill=color)

    def update(self, target_roll, target_pitch):
        # Smooth animation
        self.curr_roll += (target_roll - self.curr_roll) * self.alpha
        self.curr_pitch += (target_pitch - self.curr_pitch) * self.alpha
        
        # Draw at 2x resolution
        image = Image.new("RGB", (self.width, self.height), COLOR_BG)
        draw = ImageDraw.Draw(image)
        
        self.draw_background(draw)
        
        # --- Center Divider & Arrows ---
        line_y = self.center_y
        line_start_x = self.center_x - (50 * self.scale)
        line_end_x = self.center_x + (50 * self.scale)
        
        draw.line((line_start_x, line_y, line_end_x, line_y), fill=COLOR_TICK, width=1*self.scale)
        self.draw_arrow(draw, line_start_x, line_y, "left")
        self.draw_arrow(draw, line_end_x, line_y, "right")
        
        # --- Labels ---
        label_offset_y = 8 * self.scale
        label_offset_x = 40 * self.scale
        draw.text((self.center_x - label_offset_x, self.center_y + label_offset_y), "ROLL", font=self.font_label, fill=COLOR_TEXT_DIM, anchor="mt")
        draw.text((self.center_x + label_offset_x, self.center_y + label_offset_y), "PITCH", font=self.font_label, fill=COLOR_TEXT_DIM, anchor="mt")
        
        # --- Values & Icons Layout ---
        # Roll: Truck Top, Value Below (but above center)
        roll_truck_y = self.center_y - (60 * self.scale)
        roll_text_y = self.center_y - (25 * self.scale)
        
        # Pitch: Truck Bottom, Value Above (but below center)
        # Actually, let's keep symmetry:
        # Roll (Left Side): Truck Top, Value Below
        # Pitch (Right Side): Truck Bottom, Value Above
        # Wait, the previous layout was: Roll Top-Left, Pitch Top-Right.
        # Let's try:
        # Roll: Truck centered in top half. Value below it.
        # Pitch: Truck centered in bottom half. Value above it.
        # But that conflicts with the center line labels.
        
        # Refined Layout:
        # Center Line separates Roll (Left) and Pitch (Right) labels.
        # But usually Roll is the main arc? 
        # Let's stick to the "Cockpit" look:
        # Top Half: Roll Truck. 
        # Bottom Half: Pitch Truck.
        # Values: Large numbers near the center line.
        
        # Roll Value (Left of center, above line)
        roll_text = f"{int(self.curr_roll)}°"
        draw.text((self.center_x - label_offset_x, self.center_y - (10 * self.scale)), roll_text, font=self.font_value, fill=COLOR_TEXT, anchor="mb")

        # Pitch Value (Right of center, above line)
        pitch_text = f"{int(self.curr_pitch)}°"
        draw.text((self.center_x + label_offset_x, self.center_y - (10 * self.scale)), pitch_text, font=self.font_value, fill=COLOR_TEXT, anchor="mb")
        
        # Roll Truck (Top Center)
        roll_layer = self.get_truck_rear_layer(self.curr_roll)
        image.paste(roll_layer, (self.center_x - (32 * self.scale), self.center_y - (90 * self.scale)), roll_layer)
        
        # Pitch Truck (Bottom Center)
        pitch_layer = self.get_truck_side_layer(self.curr_pitch)
        image.paste(pitch_layer, (self.center_x - (32 * self.scale), self.center_y + (20 * self.scale)), pitch_layer)
        
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
