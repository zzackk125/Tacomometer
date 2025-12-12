/*
 * File: lvgl_port.h
 * Description: LVGL Display Porting Interface
 * Author: zzackk125
 * License: MIT
 */

#pragma once

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_port_init(void);
void lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);
void lvgl_port_set_rotation(int degrees);
int lvgl_port_get_rotation(void);

#ifdef __cplusplus
}
#endif
