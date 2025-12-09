#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino_GFX_Library.h>

// Pin Definitions for Waveshare ESP32-C6 Touch AMOLED 1.43
// Display (QSPI)
#define TFT_CS    10
#define TFT_SCK   11 // PCLK
#define TFT_D0    4
#define TFT_D1    5
#define TFT_D2    6
#define TFT_D3    7
#define TFT_RST   3
#define TFT_BL    -1 // No backlight pin or controlled differently (User config says -1)

// Touch Pins (FT6146) - Shared I2C with IMU
#define TOUCH_SDA 18
#define TOUCH_SCL 8
#define TOUCH_INT -1
#define TOUCH_RST -1

// Color Definitions (RGB565)
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define ORANGE  0xFD20

// Initialize QSPI Bus
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);

// Initialize SH8601 Display (Waveshare 1.43" AMOLED)
// Demo code uses SH8601, so we trust that over Wiki text.
Arduino_GFX *gfx = new Arduino_SH8601(bus, TFT_RST, 0 /* rotation */, false /* IPS */, 466, 466);

void initDisplay() {
    // Init Display
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    // Note: Power and Backlight are handled in main setup()
}

#endif
