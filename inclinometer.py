
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
        
        # Pre-render truck rotations for performance
        self._pre_render_trucks()

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

    def _pre_render_trucks(self):
        """Pre-renders rotated truck images for -45 to +45 degrees"""
        self.truck_rear_cache = {}
        self.truck_side_cache = {}
        
        # Generate base images once
        base_rear = self._generate_base_rear()
        base_side = self._generate_base_side()
        
        # Pre-rotate for expected range (+/- 45 degrees)
        for angle in range(-45, 46):
            self.truck_rear_cache[angle] = base_rear.rotate(-angle, resample=Image.BICUBIC)
            self.truck_side_cache[angle] = base_side.rotate(angle, resample=Image.BICUBIC)

    def _generate_base_rear(self):
        size = 64 * self.scale
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        s = 0.6 * self.scale
        
        # 2006 Tacoma Rear View
        # Tires
        tire_w, tire_h = 12 * s, 22 * s
        tire_offset_x = 30 * s
        tire_offset_y = 10 * s
        draw.rectangle((cx - tire_offset_x - tire_w, cy + tire_offset_y, cx - tire_offset_x, cy + tire_offset_y + tire_h), fill="#333")
        draw.rectangle((cx + tire_offset_x, cy + tire_offset_y, cx + tire_offset_x + tire_w, cy + tire_offset_y + tire_h), fill="#333")
        # Axle
        draw.rectangle((cx - tire_offset_x, cy + tire_offset_y + tire_h//2 - 2*s, cx + tire_offset_x, cy + tire_offset_y + tire_h//2 + 2*s), fill="#222")
        draw.ellipse((cx - 4*s, cy + tire_offset_y + tire_h//2 - 4*s, cx + 4*s, cy + tire_offset_y + tire_h//2 + 4*s), fill="#222")
        # Body
        body_w, body_h = 64 * s, 28 * s
        body_top = cy - body_h//2
        body_bottom = cy + body_h//2
        draw.rectangle((cx - body_w//2, body_top, cx + body_w//2, body_bottom), fill=COLOR_ACCENT)
        # Flares
        flare_w = 6 * s
        draw.polygon([(cx - body_w//2, body_top), (cx - body_w//2 - flare_w, body_top + 5*s), (cx - body_w//2 - flare_w, body_bottom), (cx - body_w//2, body_bottom)], fill=COLOR_ACCENT)
        draw.polygon([(cx + body_w//2, body_top), (cx + body_w//2 + flare_w, body_top + 5*s), (cx + body_w//2 + flare_w, body_bottom), (cx + body_w//2, body_bottom)], fill=COLOR_ACCENT)
        # Cabin
        cab_w_bottom = 56 * s
        cab_w_top = 48 * s
        cab_h = 24 * s
        cab_bottom = body_top
        cab_top = cab_bottom - cab_h
        draw.polygon([(cx - cab_w_top//2, cab_top), (cx + cab_w_top//2, cab_top), (cx + cab_w_bottom//2, cab_bottom), (cx - cab_w_bottom//2, cab_bottom)], fill=COLOR_ACCENT)
        # Window
        win_w_top = 44 * s
        win_w_bottom = 50 * s
        win_h = 14 * s
        win_top = cab_top + 4*s
        draw.polygon([(cx - win_w_top//2, win_top), (cx + win_w_top//2, win_top), (cx + win_w_bottom//2, win_top + win_h), (cx - win_w_bottom//2, win_top + win_h)], fill="#222")
        # Bumper
        bumper_h = 6 * s
        draw.rectangle((cx - body_w//2 - 2*s, body_bottom, cx + body_w//2 + 2*s, body_bottom + bumper_h), fill="#555")
        return img

    def _generate_base_side(self):
        size = 64 * self.scale
        img = Image.new("RGBA", (size, size), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        
        cx, cy = size//2, size//2
        s = 0.6 * self.scale
        
        # 2006 Tacoma Side View
        wheel_r = 11 * s
        wheel_front_x = cx + 28 * s
        wheel_rear_x = cx - 36 * s
        wheel_y = cy + 14 * s
        draw.ellipse((wheel_front_x - wheel_r, wheel_y - wheel_r, wheel_front_x + wheel_r, wheel_y + wheel_r), fill="#333")
        draw.ellipse((wheel_rear_x - wheel_r, wheel_y - wheel_r, wheel_rear_x + wheel_r, wheel_y + wheel_r), fill="#333")
        draw.rectangle((wheel_rear_x, wheel_y - 8*s, wheel_front_x, wheel_y), fill="#222")
        poly = [
            (cx + 42*s, wheel_y - 5*s), (cx + 42*s, cy - 6*s), (cx + 25*s, cy - 8*s),
            (cx + 10*s, cy - 24*s), (cx - 16*s, cy - 24*s), (cx - 21*s, cy - 8*s),
            (cx - 56*s, cy - 8*s), (cx - 56*s, wheel_y - 5*s), (cx - 32*s, wheel_y - 5*s),
        ]
        draw.polygon(poly, fill=COLOR_ACCENT)
        win_poly = [
            (cx + 8*s, cy - 20*s), (cx + 20*s, cy - 8*s),
            (cx - 14*s, cy - 8*s), (cx - 12*s, cy - 20*s),
        ]
        draw.polygon(win_poly, fill="#222")
        draw.line((cx - 2*s, cy - 20*s, cx - 2*s, cy - 8*s), fill=COLOR_ACCENT, width=int(3*s))
        draw.line((cx - 21*s, cy - 8*s, cx - 21*s, wheel_y - 5*s), fill="#111", width=1)
        return img

    def get_truck_rear_layer(self, roll_angle):
        # Clamp angle to cache range
        angle = int(max(-45, min(45, roll_angle)))
        return self.truck_rear_cache.get(angle, self.truck_rear_cache[0])

    def get_truck_side_layer(self, pitch_angle):
        # Clamp angle to cache range
        angle = int(max(-45, min(45, pitch_angle)))
        return self.truck_side_cache.get(angle, self.truck_side_cache[0])

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
        
        # --- Critical/Warning Logic ---
        abs_roll = abs(self.curr_roll)
        abs_pitch = abs(self.curr_pitch)
        
        # Critical Threshold (> 45 degrees) - OVERRIDE UI
        if abs_roll > 45 or abs_pitch > 45:
            # Determine Critical Text
            if abs_roll > 45 and abs_pitch > 45:
                crit_text = "CRITICAL\nPITCH/ROLL"
            elif abs_roll > 45:
                crit_text = "CRITICAL\nROLL"
            else:
                crit_text = "CRITICAL\nPITCH"
            
            # Flash State (Red/Black)
            # Fast flash: 5Hz
            is_red = int(time.time() * 10) % 2 == 0
            
            bg_color = "#FF0000" if is_red else "#000000"
            text_color = "#000000" if is_red else "#FF0000"
            
            # Create Critical Frame
            crit_img = Image.new("RGB", (self.width, self.height), bg_color)
            crit_draw = ImageDraw.Draw(crit_img)
            
            # Draw Text Centered
            # Use a large font (scale up the value font or load a bigger one)
            # We'll just use the value font scaled up a bit more if possible, or just standard
            font = self.font_value
            
            # Multiline text support
            lines = crit_text.split('\n')
            total_h = len(lines) * (30 * self.scale) # Approx height
            current_y = self.center_y - total_h / 2
            
            for line in lines:
                bbox = crit_draw.textbbox((0, 0), line, font=font)
                w = bbox[2] - bbox[0]
                crit_draw.text((self.center_x - w/2, current_y), line, font=font, fill=text_color)
                current_y += 35 * self.scale
                
            self.disp.ShowImage(crit_img)
            return

        # Normal UI Rendering
        
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
        
        # Warning Threshold (30 - 45 degrees) - OVERLAY UI
        max_angle = max(abs_roll, abs_pitch)
        if max_angle > 30:
            # Calculate intensity (0.0 to 1.0)
            # 30 deg -> 0.0
            # 45 deg -> 1.0
            intensity = (max_angle - 30) / 15.0
            intensity = max(0.0, min(1.0, intensity))
            
            # Flash frequency increases with intensity
            # Low: 1Hz, High: 10Hz
            freq = 1.0 + (intensity * 9.0)
            
            # Sine wave for smooth flashing
            # Result is 0.0 to 1.0
            flash = (math.sin(time.time() * freq * 2 * math.pi) + 1) / 2
            
            # Max opacity is 50% (0.5) scaled by intensity
            alpha = flash * intensity * 0.5
            
            if alpha > 0.01:
                # Blend with Red
                # Image.blend requires two images of same size/mode
                if not hasattr(self, 'red_overlay'):
                    self.red_overlay = Image.new("RGB", (self.width, self.height), "#FF0000")
                
                image = Image.blend(image, self.red_overlay, alpha)

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
