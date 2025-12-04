
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
        self.width = self.disp.width
        self.height = self.disp.height
        self.center_x = self.width // 2
        self.center_y = self.height // 2
        
        # Load fonts
        try:
            # Roboto or similar clean font would be ideal, falling back to DejaVu
            self.font_value = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 28)
            self.font_label = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 10)
            self.font_scale = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 10)
        except IOError:
            self.font_value = ImageFont.load_default()
            self.font_label = ImageFont.load_default()
            self.font_scale = ImageFont.load_default()

    def draw_background(self, draw):
        """Draws the static background elements (ticks, labels)"""
        draw.rectangle((0, 0, self.width, self.height), fill=COLOR_BG)
        
        radius_outer = 119
        radius_inner_major = 105
        radius_inner_minor = 112
        radius_text = 92

        # Draw ticks
        for angle in range(0, 360, 10):
            rad = math.radians(angle - 90) # -90 to start at top
            cos_a = math.cos(rad)
            sin_a = math.sin(rad)
            
            is_major = (angle % 30 == 0)
            
            r_in = radius_inner_major if is_major else radius_inner_minor
            color = COLOR_TICK_MAJOR if is_major else COLOR_TICK
            width = 2 if is_major else 1
            
            x_out = self.center_x + radius_outer * cos_a
            y_out = self.center_y + radius_outer * sin_a
            x_in = self.center_x + r_in * cos_a
            y_in = self.center_y + r_in * sin_a
            
            draw.line((x_in, y_in, x_out, y_out), fill=color, width=width)
            
            # Draw numbers for major ticks
            # We'll display 0 at top/bottom, 90 at sides
            if is_major:
                val = angle
                if val > 180: val = 360 - val
                if val == 180: val = 0 # Bottom is 0 too for symmetry in this context? 
                # Or let's just show 30, 60, 90...
                
                # Skip 0 (top/bottom) and 90 (sides) to avoid cluttering main UI elements
                if val % 90 != 0:
                    text = str(val)
                    bbox = draw.textbbox((0, 0), text, font=self.font_scale)
                    w = bbox[2] - bbox[0]
                    h = bbox[3] - bbox[1]
                    
                    x_txt = self.center_x + radius_text * cos_a
                    y_txt = self.center_y + radius_text * sin_a
                    
                    draw.text((x_txt - w/2, y_txt - h/2), text, font=self.font_scale, fill=COLOR_TEXT_DIM)

    def get_truck_rear_layer(self, roll_angle):
        # Reduced size
        size = 64 
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        
        # Scale factors
        s = 0.6 # Scale down drawing coordinates
        
        # Tires
        tire_w, tire_h = 10 * s, 20 * s
        tire_offset_x = 28 * s
        tire_offset_y = 8 * s
        
        # Draw tires
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
        size = 64
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        s = 0.6
        
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
        size = 6
        if direction == "right":
            points = [(x, y-size), (x+size, y), (x, y+size)]
        else:
            points = [(x, y-size), (x-size, y), (x, y+size)]
        draw.polygon(points, fill=color)

    def update(self, roll, pitch):
        image = Image.new("RGB", (self.width, self.height), COLOR_BG)
        draw = ImageDraw.Draw(image)
        
        self.draw_background(draw)
        
        # --- Center Divider & Arrows ---
        line_y = self.center_y
        line_start_x = self.center_x - 50
        line_end_x = self.center_x + 50
        
        # Dashed line? Or solid. Reference has dashed.
        # Simple solid for now with arrows
        draw.line((line_start_x, line_y, line_end_x, line_y), fill=COLOR_TICK, width=1)
        
        # Arrows
        self.draw_arrow(draw, line_start_x, line_y, "left")
        self.draw_arrow(draw, line_end_x, line_y, "right")
        
        # --- Labels ---
        # "ROLL" (Left side)
        draw.text((self.center_x - 40, self.center_y + 8), "ROLL", font=self.font_label, fill=COLOR_TEXT_DIM, anchor="mt")
        
        # "PITCH" (Right side)
        draw.text((self.center_x + 40, self.center_y + 8), "PITCH", font=self.font_label, fill=COLOR_TEXT_DIM, anchor="mt")
        
        # --- Values ---
        # Roll Value (Top Left quadrant)
        roll_text = f"{int(roll)}°"
        draw.text((self.center_x - 40, self.center_y - 25), roll_text, font=self.font_value, fill=COLOR_TEXT, anchor="mb")

        # Pitch Value (Top Right quadrant)
        pitch_text = f"{int(pitch)}°"
        draw.text((self.center_x + 40, self.center_y - 25), pitch_text, font=self.font_value, fill=COLOR_TEXT, anchor="mb")
        
        # --- Trucks ---
        # Roll Truck (Top Center)
        roll_layer = self.get_truck_rear_layer(roll)
        # Position: centered horizontally, upper half vertically
        image.paste(roll_layer, (self.center_x - 32, self.center_y - 85), roll_layer)
        
        # Pitch Truck (Bottom Center)
        pitch_layer = self.get_truck_side_layer(pitch)
        # Position: centered horizontally, lower half vertically
        image.paste(pitch_layer, (self.center_x - 32, self.center_y + 25), pitch_layer)
        
        self.disp.ShowImage(image)

def main():
    ui = InclinometerUI()
    logger.info("Starting Refined Inclinometer UI...")
    
    try:
        roll = 0
        pitch = 0
        step = 1
        while True:
            ui.update(roll, pitch)
            
            roll += step
            pitch += step * 0.5
            
            if roll > 30 or roll < -30:
                step *= -1
            
            # time.sleep(0.01)
            
    except KeyboardInterrupt:
        logger.info("Exiting...")

if __name__ == "__main__":
    main()
