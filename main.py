
import time
from lib.LCD_1inch28 import LCD_1inch28
from PIL import Image, ImageDraw, ImageFont
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def main():
    try:
        logger.info("Initializing Display...")
        disp = LCD_1inch28()
        
        # Create blank image for drawing.
        # Make sure to create image with mode 'RGB' for full color.
        image = Image.new("RGB", (disp.width, disp.height), "BLACK")
        draw = ImageDraw.Draw(image)

        # Draw a white circle outline
        draw.ellipse((5, 5, 235, 235), outline="WHITE", width=2)
        
        # Draw text
        logger.info("Drawing Text...")
        try:
            # Try to load a nicer font, fallback to default
            font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24)
        except IOError:
            font = ImageFont.load_default()
            
        text = "Hello World"
        
        # Calculate text position to center it
        # Get text bounding box
        left, top, right, bottom = draw.textbbox((0, 0), text, font=font)
        text_width = right - left
        text_height = bottom - top
        
        x = (disp.width - text_width) // 2
        y = (disp.height - text_height) // 2
        
        draw.text((x, y), text, font=font, fill="WHITE")
        
        # Display image
        logger.info("Sending image to display...")
        disp.ShowImage(image)
        
        logger.info("Done! Press Ctrl+C to exit.")
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        logger.info("Exiting...")
    except Exception as e:
        logger.error(f"Error: {e}")
    finally:
        # disp.cleanup() # Optional: keep display on after exit
        pass

if __name__ == "__main__":
    main()
