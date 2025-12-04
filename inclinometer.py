
import time
import math
from lib.LCD_1inch28 import LCD_1inch28
from PIL import Image, ImageDraw, ImageFont
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Colors
COLOR_BG = "#11111d"
COLOR_ACCENT = "#ff6600"
COLOR_TEXT = "#FFFFFF"
COLOR_TICK = "#555555"
COLOR_TICK_HIGHLIGHT = "#ff6600"

class InclinometerUI:
    def __init__(self):
        self.disp = LCD_1inch28()
        self.width = self.disp.width
        self.height = self.disp.height
        self.center_x = self.width // 2
        self.center_y = self.height // 2
        
        # Load fonts
        try:
            self.font_large = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 32)
            self.font_small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
            self.font_label = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 10)
        except IOError:
            self.font_large = ImageFont.load_default()
            self.font_small = ImageFont.load_default()
            self.font_label = ImageFont.load_default()

    def draw_background(self, draw):
        """Draws the static background elements (ticks, labels)"""
        # Draw background circle
        draw.rectangle((0, 0, self.width, self.height), fill=COLOR_BG)
        
        # Draw ticks
        # We want ticks around the edge. 
        # Let's say radius is 115 (slightly inside 120)
        radius_outer = 118
        radius_inner_long = 105
        radius_inner_short = 112
        
        for angle in range(0, 360, 10):
            rad = math.radians(angle)
            cos_a = math.cos(rad)
            sin_a = math.sin(rad)
            
            x_out = self.center_x + radius_outer * cos_a
            y_out = self.center_y + radius_outer * sin_a
            
            is_major = (angle % 30 == 0)
            r_in = radius_inner_long if is_major else radius_inner_short
            color = COLOR_TICK_HIGHLIGHT if is_major else COLOR_TICK
            width = 3 if is_major else 1
            
            x_in = self.center_x + r_in * cos_a
            y_in = self.center_y + r_in * sin_a
            
            draw.line((x_in, y_in, x_out, y_out), fill=color, width=width)
            
            # Draw numbers for major ticks
            if is_major:
                # Calculate text position slightly further in
                r_text = radius_inner_long - 15
                x_text = self.center_x + r_text * cos_a
                y_text = self.center_y + r_text * sin_a
                
                # Convert angle to -90 to 90 range for display if needed, 
                # but for now let's just show 10, 20, 30 etc relative to vertical?
                # Actually, the reference image shows 0 in center and 10, 20, 30 going out.
                # Let's stick to the reference image style later. 
                # For now, generic ticks are fine.
                pass

    def draw_truck_rear(self, draw, roll_angle):
        """Draws the rear view of the truck (Roll)"""
        # Center of top half
        cx, cy = self.center_x, self.center_y - 60
        
        # Create a separate image for rotation
        truck_size = 80
        truck_img = Image.new("RGBA", (truck_size, truck_size), (0, 0, 0, 0))
        t_draw = ImageDraw.Draw(truck_img)
        
        # Truck Body (Rectangle)
        # Coordinates relative to truck_img center (40, 40)
        tcx, tcy = truck_size // 2, truck_size // 2
        
        # Main body
        body_w, body_h = 50, 30
        t_draw.rectangle(
            (tcx - body_w//2, tcy - body_h//2, tcx + body_w//2, tcy + body_h//2), 
            fill=COLOR_ACCENT, outline=None
        )
        
        # Tires (Left and Right)
        tire_w, tire_h = 10, 20
        # Left Tire
        t_draw.rectangle(
            (tcx - body_w//2 - tire_w, tcy + 5, tcx - body_w//2, tcy + 5 + tire_h),
            fill="#333333"
        )
        # Right Tire
        t_draw.rectangle(
            (tcx + body_w//2, tcy + 5, tcx + body_w//2 + tire_w, tcy + 5 + tire_h),
            fill="#333333"
        )
        
        # Rotate
        rotated_truck = truck_img.rotate(-roll_angle, resample=Image.BICUBIC, expand=False)
        
        # Paste onto main image
        # Calculate position to center it at cx, cy
        paste_x = cx - truck_size // 2
        paste_y = cy - truck_size // 2
        
        draw.bitmap((paste_x, paste_y), rotated_truck, fill=None) # Bitmap might not handle RGBA correctly with mask?
        # Better to use paste with mask
        # But we are drawing on 'draw' object which is associated with the main image.
        # We can't paste onto 'draw'. We need the main image.
        # So we should return the layer or modify the design.
        # Let's change the method signature to take the image, not draw.
        pass

    def get_truck_rear_layer(self, roll_angle):
        size = 100
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        
        # Tires
        tire_w, tire_h = 12, 24
        tire_offset_x = 32
        tire_offset_y = 10
        draw.rectangle((cx - tire_offset_x - tire_w, cy + tire_offset_y, cx - tire_offset_x, cy + tire_offset_y + tire_h), fill="#444")
        draw.rectangle((cx + tire_offset_x, cy + tire_offset_y, cx + tire_offset_x + tire_w, cy + tire_offset_y + tire_h), fill="#444")
        
        # Axle
        draw.line((cx - tire_offset_x, cy + tire_offset_y + tire_h//2, cx + tire_offset_x, cy + tire_offset_y + tire_h//2), fill="#333", width=4)

        # Body
        body_w, body_h = 60, 35
        draw.polygon([
            (cx - body_w//2, cy - body_h//2), # Top Left
            (cx + body_w//2, cy - body_h//2), # Top Right
            (cx + body_w//2, cy + body_h//2), # Bottom Right
            (cx - body_w//2, cy + body_h//2), # Bottom Left
        ], fill=COLOR_ACCENT)
        
        # Window
        win_w, win_h = 40, 15
        draw.rectangle((cx - win_w//2, cy - body_h//2 + 5, cx + win_w//2, cy - body_h//2 + 5 + win_h), fill="#222")

        return img.rotate(-roll_angle, resample=Image.BICUBIC)

    def get_truck_side_layer(self, pitch_angle):
        size = 100
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        
        # Wheels
        wheel_r = 10
        wheel_dist = 25
        draw.ellipse((cx - wheel_dist - wheel_r, cy + 15 - wheel_r, cx - wheel_dist + wheel_r, cy + 15 + wheel_r), fill="#444")
        draw.ellipse((cx + wheel_dist - wheel_r, cy + 15 - wheel_r, cx + wheel_dist + wheel_r, cy + 15 + wheel_r), fill="#444")
        
        # Body
        # Simple pickup shape
        poly = [
            (cx - 35, cy + 15), # Rear bottom
            (cx + 35, cy + 15), # Front bottom
            (cx + 35, cy - 5),  # Front hood
            (cx + 15, cy - 5),  # Windshield base
            (cx + 5, cy - 20),  # Roof front
            (cx - 15, cy - 20), # Roof rear
            (cx - 15, cy - 5),  # Bed front
            (cx - 35, cy - 5),  # Bed rear
        ]
        draw.polygon(poly, fill=COLOR_ACCENT)
        
        return img.rotate(pitch_angle, resample=Image.BICUBIC)

    def update(self, roll, pitch):
        # Create base image
        image = Image.new("RGB", (self.width, self.height), COLOR_BG)
        draw = ImageDraw.Draw(image)
        
        self.draw_background(draw)
        
        # Draw Center Line/Horizon
        draw.line((self.center_x - 60, self.center_y, self.center_x + 60, self.center_y), fill=COLOR_TICK, width=1)
        
        # Draw Text Values
        # Roll (Top)
        roll_text = f"{int(roll)}°"
        bbox = draw.textbbox((0, 0), roll_text, font=self.font_large)
        w = bbox[2] - bbox[0]
        draw.text((self.center_x - w//2 - 40, self.center_y - 20), roll_text, font=self.font_large, fill="WHITE")
        draw.text((self.center_x - 40, self.center_y + 15), "ROLL", font=self.font_label, fill="GRAY", anchor="ms")

        # Pitch (Bottom)
        pitch_text = f"{int(pitch)}°"
        bbox = draw.textbbox((0, 0), pitch_text, font=self.font_large)
        w = bbox[2] - bbox[0]
        draw.text((self.center_x - w//2 + 40, self.center_y - 20), pitch_text, font=self.font_large, fill="WHITE")
        draw.text((self.center_x + 40, self.center_y + 15), "PITCH", font=self.font_label, fill="GRAY", anchor="ms")
        
        # Draw Trucks
        # Roll Truck (Top)
        roll_layer = self.get_truck_rear_layer(roll)
        image.paste(roll_layer, (self.center_x - 50, self.center_y - 100), roll_layer)
        
        # Pitch Truck (Bottom)
        pitch_layer = self.get_truck_side_layer(pitch)
        image.paste(pitch_layer, (self.center_x - 50, self.center_y + 10), pitch_layer)
        
        self.disp.ShowImage(image)

def main():
    ui = InclinometerUI()
    
    logger.info("Starting Inclinometer UI Demo...")
    try:
        # Demo loop
        roll = 0
        pitch = 0
        step = 1
        while True:
            ui.update(roll, pitch)
            
            # Simple animation
            roll += step
            pitch += step * 0.5
            
            if roll > 30 or roll < -30:
                step *= -1
                
            # time.sleep(0.05) # Max framerate
            
    except KeyboardInterrupt:
        logger.info("Exiting...")

if __name__ == "__main__":
    main()
