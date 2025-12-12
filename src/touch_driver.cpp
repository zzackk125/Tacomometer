/*
 * File: touch_driver.cpp
 * Description: Touch Screen Driver Implementation
 * Author: zzackk125
 * License: MIT
 */

#include "touch_driver.h"
#include "board_config.h"

static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data);
static int touch_rotation = 0;

void setTouchRotation(int degrees) {
    touch_rotation = degrees;
}

void initTouch() {
    // Create Input Device
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}

static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    uint8_t cmd = 0x02;
    uint8_t buf[5] = {0};

    Wire.beginTransmission(DISP_TOUCH_ADDR);
    Wire.write(cmd);
    Wire.endTransmission();

    Wire.requestFrom(DISP_TOUCH_ADDR, 5);
    if (Wire.available() == 5) {
        buf[0] = Wire.read(); // Status
        buf[1] = Wire.read(); // X High
        buf[2] = Wire.read(); // X Low
        buf[3] = Wire.read(); // Y High
        buf[4] = Wire.read(); // Y Low
    }

    if (buf[0]) { // Touch detected
        uint16_t tp_x = (((uint16_t)buf[1] & 0x0f) << 8) | (uint16_t)buf[2];
        uint16_t tp_y = (((uint16_t)buf[3] & 0x0f) << 8) | (uint16_t)buf[4];

        // Constrain coordinates
        if (tp_x > LCD_H_RES) tp_x = LCD_H_RES;
        if (tp_y > LCD_V_RES) tp_y = LCD_V_RES;

        if (touch_rotation == 180) {
            data->point.x = LCD_H_RES - tp_x;
            data->point.y = LCD_V_RES - tp_y;
        } else if (touch_rotation == 90) {
            data->point.x = tp_y;
            data->point.y = LCD_H_RES - tp_x;
        } else if (touch_rotation == 270) {
            data->point.x = LCD_V_RES - tp_y;
            data->point.y = tp_x;
        } else {
            data->point.x = tp_x;
            data->point.y = tp_y;
        }
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
