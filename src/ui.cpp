/*
 * File: ui.cpp
 * Description: UI Logic Implementation
 * Author: zzackk125
 * License: MIT
 */

#include "ui.h"
#include "assets.h"
#include <stdio.h>
#include "imu_driver.h" // For calibration access if needed, or we pass callback
#include <Preferences.h>

LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_48);

// UI Objects
static lv_obj_t * bg_img;
static lv_obj_t * truck_roll_img;
static lv_obj_t * truck_pitch_img;
static lv_obj_t * pointer_roll;
static lv_obj_t * pointer_pitch;
static lv_obj_t * label_roll_val;
static lv_obj_t * label_pitch_val;
static lv_obj_t * label_status;
static lv_obj_t * overlay_alert;
static lv_obj_t * overlay_critical;
static lv_obj_t * label_critical;

// Max Angle Markers
static lv_obj_t * dot_roll_left;
static lv_obj_t * dot_roll_right;
static lv_obj_t * dot_pitch_fwd;
static lv_obj_t * dot_pitch_back;

// Max Values (Persisted)
static float max_roll_left = 0;
static float max_roll_right = 0;
static float max_pitch_fwd = 0;
static float max_pitch_back = 0;
static Preferences ui_prefs;

// Calibration State
static bool is_calibrating = false;
static uint32_t calibration_start_time = 0;

// Calibration Callback
static void calibration_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_LONG_PRESSED) {
        // Trigger Calibration
        is_calibrating = true;
        calibration_start_time = lv_tick_get();
        lv_label_set_text(label_status, "CALIBRATING...");
        lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        
        // Reset Max Values
        max_roll_left = 0;
        max_roll_right = 0;
        max_pitch_fwd = 0;
        max_pitch_back = 0;
        
        ui_prefs.putFloat("m_rl", 0);
        ui_prefs.putFloat("m_rr", 0);
        ui_prefs.putFloat("m_pf", 0);
        ui_prefs.putFloat("m_pb", 0);
    }
}

// Helper to create a red dot
static lv_obj_t* create_marker_dot(lv_obj_t* parent) {
    lv_obj_t* dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 10, 10); // Smaller than pointer (20x30)
    lv_obj_set_style_radius(dot, 5, 0); // Circle
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_opa(dot, 255, 0);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_IGNORE_LAYOUT); // Manual positioning
    return dot;
}

void initUI() {
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

    // Load Max Values
    ui_prefs.begin("ui", false);
    max_roll_left = ui_prefs.getFloat("m_rl", 0);
    max_roll_right = ui_prefs.getFloat("m_rr", 0);
    max_pitch_fwd = ui_prefs.getFloat("m_pf", 0);
    max_pitch_back = ui_prefs.getFloat("m_pb", 0);

    // 1. Background Image
    bg_img = lv_image_create(scr);
    lv_image_set_src(bg_img, &img_background);
    lv_obj_center(bg_img);
    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_CLICKABLE); // Enable input for long press
    lv_obj_add_event_cb(bg_img, calibration_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    // 2. Roll Truck (Top Center)
    truck_roll_img = lv_image_create(scr);
    lv_image_set_src(truck_roll_img, &img_truck_rear);
    lv_obj_align(truck_roll_img, LV_ALIGN_CENTER, 0, -80);
    lv_image_set_pivot(truck_roll_img, 32, 32); // Center of 64x64 image

    // 3. Pitch Truck (Bottom Center)
    truck_pitch_img = lv_image_create(scr);
    lv_image_set_src(truck_pitch_img, &img_truck_side);
    lv_obj_align(truck_pitch_img, LV_ALIGN_CENTER, 0, 100); // Moved down
    lv_obj_set_style_transform_scale(truck_pitch_img, 320, 0); // 1.25x Scale
    lv_image_set_pivot(truck_pitch_img, 32, 32);

    // 4. Max Angle Markers (Behind pointers, but on top of BG)
    dot_roll_left = create_marker_dot(scr);
    dot_roll_right = create_marker_dot(scr);
    dot_pitch_fwd = create_marker_dot(scr);
    dot_pitch_back = create_marker_dot(scr);

    // 5. Pointers
    pointer_roll = lv_image_create(scr);
    lv_image_set_src(pointer_roll, &img_pointer);
    lv_obj_add_flag(pointer_roll, LV_OBJ_FLAG_IGNORE_LAYOUT); // Manual pos
    lv_image_set_pivot(pointer_roll, 10, 15); // Center of 20x30 image

    pointer_pitch = lv_image_create(scr);
    lv_image_set_src(pointer_pitch, &img_pointer);
    lv_obj_add_flag(pointer_pitch, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_image_set_pivot(pointer_pitch, 10, 15);

    // 6. Labels
    // Roll Value (Left)
    label_roll_val = lv_label_create(scr);
    lv_label_set_text(label_roll_val, "0°");
    // Target: 42px. Font: 28px. Scale: 1.5x (384)
    lv_obj_set_style_text_font(label_roll_val, &lv_font_montserrat_28, 0); 
    lv_obj_set_style_text_color(label_roll_val, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_transform_scale(label_roll_val, 384, 0); 
    lv_obj_set_style_transform_pivot_x(label_roll_val, LV_PCT(50), 0); // Pivot Center
    lv_obj_set_style_transform_pivot_y(label_roll_val, LV_PCT(50), 0);
    lv_obj_set_style_text_align(label_roll_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_roll_val, LV_ALIGN_CENTER, -75, -25); 

    lv_obj_t * lbl_roll = lv_label_create(scr);
    lv_label_set_text(lbl_roll, "ROLL");
    // Target: 28px. Font: 28px. Scale: 1.0x (256)
    lv_obj_set_style_text_font(lbl_roll, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_roll, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_transform_scale(lbl_roll, 256, 0); 
    lv_obj_set_style_transform_pivot_x(lbl_roll, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_roll, LV_PCT(50), 0);
    lv_obj_set_style_text_align(lbl_roll, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_roll, LV_ALIGN_CENTER, -75, 25); 

    // Pitch Value (Right)
    label_pitch_val = lv_label_create(scr);
    lv_label_set_text(label_pitch_val, "0°");
    // Target: 42px. Font: 28px. Scale: 1.5x (384)
    lv_obj_set_style_text_font(label_pitch_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_pitch_val, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_transform_scale(label_pitch_val, 384, 0); 
    lv_obj_set_style_transform_pivot_x(label_pitch_val, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(label_pitch_val, LV_PCT(50), 0);
    lv_obj_set_style_text_align(label_pitch_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_pitch_val, LV_ALIGN_CENTER, 75, -25); 

    lv_obj_t * lbl_pitch = lv_label_create(scr);
    lv_label_set_text(lbl_pitch, "PITCH");
    // Target: 28px. Font: 28px. Scale: 1.0x (256)
    lv_obj_set_style_text_font(lbl_pitch, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_pitch, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_transform_scale(lbl_pitch, 256, 0); 
    lv_obj_set_style_transform_pivot_x(lbl_pitch, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_pitch, LV_PCT(50), 0);
    lv_obj_set_style_text_align(lbl_pitch, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_pitch, LV_ALIGN_CENTER, 75, 25); 

    // Status Label (Hidden by default)
    label_status = lv_label_create(scr);
    lv_label_set_text(label_status, "CALIBRATING...");
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_28, 0);
    lv_obj_set_style_transform_scale(label_status, 256, 0); // 1x Zoom
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF6D00), 0);
    lv_obj_align(label_status, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(label_status, LV_OBJ_FLAG_HIDDEN);

    // 6. Warning Alert Overlay (Red Border)
    overlay_alert = lv_obj_create(scr);
    lv_obj_set_size(overlay_alert, 466, 466);
    lv_obj_center(overlay_alert);
    lv_obj_set_style_bg_color(overlay_alert, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(overlay_alert, 0, 0); // Hidden
    lv_obj_set_style_border_width(overlay_alert, 20, 0); // Thick Border
    lv_obj_set_style_border_color(overlay_alert, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_opa(overlay_alert, 0, 0); // Hidden initially
    lv_obj_set_style_radius(overlay_alert, 233, 0); // Circular
    lv_obj_remove_flag(overlay_alert, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(overlay_alert, LV_OBJ_FLAG_CLICKABLE);

    // 7. Critical Alert Overlay (Full Screen, High Z-Index)
    overlay_critical = lv_obj_create(scr);
    lv_obj_set_size(overlay_critical, 466, 466);
    lv_obj_center(overlay_critical);
    lv_obj_set_style_bg_color(overlay_critical, lv_color_hex(0x000000), 0); // Start Black
    lv_obj_set_style_bg_opa(overlay_critical, 255, 0); // Opaque
    lv_obj_set_style_border_width(overlay_critical, 0, 0);
    lv_obj_set_style_radius(overlay_critical, 233, 0);
    lv_obj_remove_flag(overlay_critical, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay_critical, LV_OBJ_FLAG_HIDDEN); // Hidden by default
    // Allow clicks to pass through to background for calibration
    lv_obj_remove_flag(overlay_critical, LV_OBJ_FLAG_CLICKABLE); 
    // Remove padding to ensure text has full width
    lv_obj_set_style_pad_all(overlay_critical, 0, 0);

    label_critical = lv_label_create(overlay_critical);
    lv_label_set_text(label_critical, "CRITICAL");
    // Target: 48px. Font: 48px. Scale: 1.0x (256)
    lv_obj_set_style_text_font(label_critical, &lv_font_montserrat_48, 0);
    
    // Scaling Fix:
    // Reset to Native Scale (256) to rule out scaling issues
    lv_obj_set_width(label_critical, LV_SIZE_CONTENT);
    lv_obj_set_style_transform_scale(label_critical, 256, 0); 
    lv_obj_set_style_transform_pivot_x(label_critical, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(label_critical, LV_PCT(50), 0);
    
    // 4. Center the object in the parent
    lv_obj_set_style_text_align(label_critical, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label_critical);
}

void updateUI(float roll, float pitch) {
    char buf[16];
    static char current_roll_text[16] = "";
    static char current_pitch_text[16] = "";
    static bool was_critical = false;
    static bool was_warning = false;
    static int last_critical_type = 0; // 0: None, 1: Pitch/Roll, 2: Pitch, 3: Roll
    static uint32_t last_blink_time = 0;
    static bool blink_state = false;
    static uint32_t last_save_time = 0;
    static bool dirty = false;

    // SWAP DATA SOURCES: Sensor orientation implies roll var is PitchData, pitch var is RollData.
    float effective_roll = pitch;
    float effective_pitch = roll;

    // --- Max Angle Tracking ---
    // Roll (Left is negative, Right is positive in our logic? Or vice versa?)
    // Let's assume standard: Left -, Right +. 
    // But we display abs(). 
    // Let's track raw signed values.
    
    if (effective_roll > max_roll_right) { max_roll_right = effective_roll; dirty = true; }
    if (effective_roll < max_roll_left) { max_roll_left = effective_roll; dirty = true; }
    
    if (effective_pitch > max_pitch_back) { max_pitch_back = effective_pitch; dirty = true; }
    if (effective_pitch < max_pitch_fwd) { max_pitch_fwd = effective_pitch; dirty = true; }

    // Save to NVS (Throttle: Only if dirty and > 5s since last save)
    if (dirty && lv_tick_elaps(last_save_time) > 5000) {
        ui_prefs.putFloat("m_rl", max_roll_left);
        ui_prefs.putFloat("m_rr", max_roll_right);
        ui_prefs.putFloat("m_pf", max_pitch_fwd);
        ui_prefs.putFloat("m_pb", max_pitch_back);
        dirty = false;
        last_save_time = lv_tick_get();
    }

    // --- Update Text ---
    snprintf(buf, sizeof(buf), "%d°", (int)abs(effective_roll));
    if (strcmp(buf, current_roll_text) != 0) {
        lv_label_set_text(label_roll_val, buf);
        strcpy(current_roll_text, buf);
    }
    
    snprintf(buf, sizeof(buf), "%d°", (int)effective_pitch);
    if (strcmp(buf, current_pitch_text) != 0) {
        lv_label_set_text(label_pitch_val, buf);
        strcpy(current_pitch_text, buf);
    }

    // --- Update Graphics ---
    lv_image_set_rotation(truck_roll_img, (int32_t)(-effective_roll * 10)); 
    lv_image_set_rotation(truck_pitch_img, (int32_t)(-effective_pitch * 10));

    float radius = 195.0;
    
    // Roll Pointer
    float rad_roll = (180.0 - effective_roll) * 3.14159 / 180.0;
    int px_roll = 233 + (int)(radius * cos(rad_roll));
    int py_roll = 233 + (int)(radius * sin(rad_roll));
    lv_obj_set_pos(pointer_roll, px_roll - 10, py_roll - 15); 
    lv_image_set_rotation(pointer_roll, (int32_t)((270 - effective_roll) * 10));

    // Pitch Pointer
    float rad_pitch = (0.0 - effective_pitch) * 3.14159 / 180.0;
    int px_pitch = 233 + (int)(radius * cos(rad_pitch));
    int py_pitch = 233 + (int)(radius * sin(rad_pitch));
    lv_obj_set_pos(pointer_pitch, px_pitch - 10, py_pitch - 15); 
    lv_image_set_rotation(pointer_pitch, (int32_t)((90 - effective_pitch) * 10));

    // --- Update Max Markers ---
    // Clamp display values to +/- 45 degrees so they stay on scale
    float disp_rl = (max_roll_left < -45.0) ? -45.0 : max_roll_left;
    float disp_rr = (max_roll_right > 45.0) ? 45.0 : max_roll_right;
    float disp_pf = (max_pitch_fwd < -45.0) ? -45.0 : max_pitch_fwd;
    float disp_pb = (max_pitch_back > 45.0) ? 45.0 : max_pitch_back;

    // 1. Roll Left (Min Roll)
    // Angle = 180 - disp_rl
    float rad_rl = (180.0 - disp_rl) * 3.14159 / 180.0;
    int px_rl = 233 + (int)(radius * cos(rad_rl));
    int py_rl = 233 + (int)(radius * sin(rad_rl));
    lv_obj_set_pos(dot_roll_left, px_rl - 5, py_rl - 5); // Center 10x10 dot

    // 2. Roll Right (Max Roll)
    float rad_rr = (180.0 - disp_rr) * 3.14159 / 180.0;
    int px_rr = 233 + (int)(radius * cos(rad_rr));
    int py_rr = 233 + (int)(radius * sin(rad_rr));
    lv_obj_set_pos(dot_roll_right, px_rr - 5, py_rr - 5);

    // 3. Pitch Fwd (Min Pitch)
    float rad_pf = (0.0 - disp_pf) * 3.14159 / 180.0;
    int px_pf = 233 + (int)(radius * cos(rad_pf));
    int py_pf = 233 + (int)(radius * sin(rad_pf));
    lv_obj_set_pos(dot_pitch_fwd, px_pf - 5, py_pf - 5);

    // 4. Pitch Back (Max Pitch)
    float rad_pb = (0.0 - disp_pb) * 3.14159 / 180.0;
    int px_pb = 233 + (int)(radius * cos(rad_pb));
    int py_pb = 233 + (int)(radius * sin(rad_pb));
    lv_obj_set_pos(dot_pitch_back, px_pb - 5, py_pb - 5);


    // Handle Calibration Timeout
    if (is_calibrating && lv_tick_elaps(calibration_start_time) > 2000) {
        is_calibrating = false;
        lv_obj_add_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        // Trigger actual zeroing here if we had access to IMU object
    }

    // --- Critical Angle Alert (> 50 degrees) ---
    // Overrides the Warning Alert
    float max_angle = 0;
    if (abs(effective_roll) > max_angle) max_angle = abs(effective_roll);
    if (abs(effective_pitch) > max_angle) max_angle = abs(effective_pitch);

    if (max_angle > 50.0) {
        // Ensure Warning Overlay is hidden
        if (was_warning) {
            lv_obj_set_style_border_opa(overlay_alert, 0, 0);
            was_warning = false;
        }
        
        // Show Critical Overlay
        if (!was_critical) {
            lv_obj_clear_flag(overlay_critical, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(overlay_critical, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(overlay_critical, 255, 0);
            lv_obj_set_style_border_color(overlay_critical, lv_color_hex(0xFF0000), 0);
            lv_obj_set_style_border_width(overlay_critical, 20, 0);
            lv_obj_set_style_text_color(label_critical, lv_color_hex(0xFF0000), 0);
            was_critical = true;
        }
        
        // Determine Text (Only update if changed)
        int current_type = 0;
        if (abs(effective_roll) > 50.0 && abs(effective_pitch) > 50.0) current_type = 1;
        else if (abs(effective_pitch) > 50.0) current_type = 2;
        else current_type = 3;

        if (current_type != last_critical_type) {
            if (current_type == 1) lv_label_set_text(label_critical, "CRITICAL\nPITCH / ROLL");
            else if (current_type == 2) lv_label_set_text(label_critical, "CRITICAL\nPITCH");
            else lv_label_set_text(label_critical, "CRITICAL\nROLL");
            last_critical_type = current_type;
        }

        // Simple Blink Animation (No Float Math)
        if (lv_tick_elaps(last_blink_time) > 250) { // 4Hz Blink
            blink_state = !blink_state;
            lv_obj_set_style_border_opa(overlay_critical, blink_state ? 255 : 100, 0);
            last_blink_time = lv_tick_get();
        }

    } else {
        // Hide Critical Overlay
        if (was_critical) {
            lv_obj_add_flag(overlay_critical, LV_OBJ_FLAG_HIDDEN);
            was_critical = false;
            last_critical_type = 0;
            
            // Restore text color (in case we came from Warning state)
            lv_obj_set_style_text_color(label_roll_val, lv_color_hex(0xE0E0E0), 0);
            lv_obj_set_style_text_color(label_pitch_val, lv_color_hex(0xE0E0E0), 0);
        }

        // --- Warning Alert (30 to 50 degrees) ---
        if (max_angle > 30.0) {
            was_warning = true;
            // Calculate intensity (0.0 to 1.0) based on 30-50 range
            float intensity = (max_angle - 30.0) / 20.0; // Range is now 20 degrees (30 to 50)
            if (intensity > 1.0) intensity = 1.0;
            if (intensity < 0.0) intensity = 0.0;

            // Flash frequency: Low (1Hz) to High (10Hz)
            float freq = 1.0 + (intensity * 9.0);
            
            float t = lv_tick_get() / 1000.0;
            float flash = (sin(t * freq * 2 * 3.14159) + 1.0) / 2.0;

            // Flash BORDER opacity
            int opacity = (int)(flash * 255);
            
            lv_obj_set_style_border_opa(overlay_alert, opacity, 0);
            lv_obj_set_style_bg_opa(overlay_alert, 0, 0); 
            
            // Text RED
            lv_obj_set_style_text_color(label_roll_val, lv_color_hex(0xFF0000), 0);
            lv_obj_set_style_text_color(label_pitch_val, lv_color_hex(0xFF0000), 0);
        } else {
            if (was_warning) {
                lv_obj_set_style_border_opa(overlay_alert, 0, 0);
                lv_obj_set_style_bg_opa(overlay_alert, 0, 0);
                
                // Restore text color
                lv_obj_set_style_text_color(label_roll_val, lv_color_hex(0xE0E0E0), 0);
                lv_obj_set_style_text_color(label_pitch_val, lv_color_hex(0xE0E0E0), 0);
                was_warning = false;
            }
        }
    }
}

bool isCalibrating() {
    return is_calibrating;
}
