#pragma once

#include "driver/gpio.h"

// --- I2C ---
#define ESP32_SCL_NUM (GPIO_NUM_8)
#define ESP32_SDA_NUM (GPIO_NUM_18)

// --- DISPLAY ---
#define LCD_H_RES 466
#define LCD_V_RES 466
#define LVGL_BUF_HEIGHT 50 

#define LCD_CS_PIN         GPIO_NUM_10
#define LCD_PCLK_PIN       GPIO_NUM_11
#define LCD_D0_PIN         GPIO_NUM_4
#define LCD_D1_PIN         GPIO_NUM_5
#define LCD_D2_PIN         GPIO_NUM_6
#define LCD_D3_PIN         GPIO_NUM_7
#define LCD_RST_PIN        GPIO_NUM_3
#define BK_LIGHT_PIN       (-1)

// --- TOUCH ---
#define DISP_TOUCH_ADDR    0x38

// --- LVGL ---
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2
// --- IO EXPANDER (TCA9554) ---
#define IO_EXPANDER_ADDR       0x20
#define IO_EXPANDER_CONFIG_REG 0x03
#define IO_EXPANDER_OUTPUT_REG 0x01
#define IO_EXPANDER_PIN_6_MASK 0x40 // Display Power Control

// --- BUTTONS ---
#define BOOT_BUTTON_PIN        9 // ESP32-C6 Boot Button
