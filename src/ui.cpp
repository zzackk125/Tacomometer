/*
 * File: ui.cpp
 * Description: UI Logic Implementation
 * Author: zzackk125
 * License: MIT
 */

#include "ui.h"
#include "web_server.h" // For startAPMode
#include "assets.h"
#include <stdio.h>
#include "imu_driver.h" // For calibration access if needed, or we pass callback
#include <Preferences.h>
#include "lvgl_port.h" // For hardware rotation
#include "touch_driver.h" // For touch rotation

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
// static lv_obj_t * label_status; // Removed duplicate
static lv_obj_t * lbl_roll; 
static lv_obj_t * lbl_pitch;
static lv_obj_t * overlay_alert;
static lv_obj_t * overlay_critical;

static lv_obj_t * lbl_critical_dynamic; // Single Object
// static lv_obj_t * lbl_crit_static[4][3]; // REMOVED OOM

// Rotations: 0=0, 1=90, 2=180, 3=270
// Types: 0=Pitch, 1=Roll, 2=Both
static lv_obj_t * overlay_status; // New stable container for status
static lv_obj_t * lbl_status_dynamic; // Single Object
// static lv_obj_t * label_status_0; // REMOVED OOM
// static lv_obj_t * label_status_90;
// static lv_obj_t * label_status_180;
// static lv_obj_t * label_status_270;

// Max Angle Markers
static lv_obj_t * dot_roll_left;
static lv_obj_t * dot_roll_right;
static lv_obj_t * dot_pitch_fwd;
static lv_obj_t * dot_pitch_back;

// Max Values (Persisted)
// Max Values (Persisted - All Time)
static float all_time_roll_left = 0;
static float all_time_roll_right = 0;
static float all_time_pitch_fwd = 0;
static float all_time_pitch_back = 0;

// Max Values (Session - RAM only)
static float session_roll_left = 0;
static float session_roll_right = 0;
static float session_pitch_fwd = 0;
static float session_pitch_back = 0;
static Preferences ui_prefs;

// Settings
static int ui_rotation = 0;
// Split Critical Angles
static int critical_roll = 50;
static int critical_pitch = 50;
// Color Theme
static int ui_color_idx = 0;

// Color Palette (Orange, Blue, Green, Red, Purple, Yellow)
static lv_color_t theme_colors[] = {
    lv_color_hex(0xFF6D00),
    lv_color_hex(0x2196F3),
    lv_color_hex(0x4CAF50),
    lv_color_hex(0xF44336),
    lv_color_hex(0x9C27B0),
    lv_color_hex(0xFFEB3B)
};

void applyUIColor(int idx); // Forward decl



// Calibration State
static bool is_calibrating = false;
static bool calibration_trigger_active = false; // New flag for one-shot trigger
static uint32_t calibration_start_time = 0;



// Auto-Save State
static uint32_t last_save_time = 0;
static bool dirty = false;

// Pixel Shifting
static bool pixel_shift_enabled = true;
static lv_timer_t * pixel_shift_timer = NULL;

static void pixel_shift_cb(lv_timer_t * t) {
    if (!pixel_shift_enabled) {
        // Ensure reset
        lv_obj_set_style_translate_x(lv_scr_act(), 0, 0);
        lv_obj_set_style_translate_y(lv_scr_act(), 0, 0);
        return;
    }
    
    // Generate small random offset (-2 to +2 pixels)
    int x_offset = (rand() % 5) - 2; 
    int y_offset = (rand() % 5) - 2;
    
    // Apply to Active Screen (shifts everything)
    lv_obj_set_style_translate_x(lv_scr_act(), x_offset, 0);
    lv_obj_set_style_translate_y(lv_scr_act(), y_offset, 0);
    
    Serial.printf("Pixel Shift: %d, %d\n", x_offset, y_offset);
}

// Toast Timer
static lv_obj_t * toast_obj = NULL;
static uint32_t toast_show_time = 0;
static uint32_t toast_duration = 2000;

void showToast(const char* text, uint32_t duration_ms) {
    if (toast_obj == NULL) {
        toast_obj = lv_label_create(lv_layer_top());
        lv_obj_set_style_bg_color(toast_obj, lv_color_hex(0x222222), 0);
        lv_obj_set_style_bg_opa(toast_obj, 240, 0); 
        lv_obj_set_style_text_color(toast_obj, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_all(toast_obj, 20, 0); // Bigger padding
        lv_obj_set_style_radius(toast_obj, 10, 0);
        lv_obj_set_style_text_font(toast_obj, &lv_font_montserrat_28, 0); // Bigger Font
        lv_obj_set_style_text_align(toast_obj, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(toast_obj, 300); // Fixed width for multiline
        lv_obj_align(toast_obj, LV_ALIGN_CENTER, 0, 0); // Center screen
        lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_CLICKABLE); // Allow click to dismiss
        lv_obj_add_event_cb(toast_obj, [](lv_event_t* e){ 
            lv_obj_add_flag((lv_obj_t*)lv_event_get_target(e), LV_OBJ_FLAG_HIDDEN); 
        }, LV_EVENT_CLICKED, NULL);
    }
    
    lv_label_set_text(toast_obj, text);
    lv_obj_clear_flag(toast_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(toast_obj);
    lv_obj_move_foreground(toast_obj);
    toast_show_time = lv_tick_get(); 
    toast_duration = duration_ms; 
    // Reset Timer (We'll use last_save_time or similar? No, need dedicated static)
    // Actually we can add a timer callback or just check in updateUI.
    // Let's add a global timestamp.
}




void hideToast() {
    if (toast_obj) {
        lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_HIDDEN);
        toast_show_time = 0; // Clear
    }
}


// Gesture State
static int tap_count = 0;
static uint32_t last_tap_time = 0;

// Public function to trigger calibration
void triggerCalibrationUI() {
    is_calibrating = true;
    calibration_trigger_active = true; // Set trigger
    calibration_start_time = lv_tick_get();
    
    // Enable Stable Overlay
    // Enable Stable Overlay
    lv_obj_clear_flag(overlay_status, LV_OBJ_FLAG_HIDDEN);
    
    // Status Label Position Logic (Single Object)
    // 0 Deg: Top Center (0, -150)
    // 90 Deg: Right Center (150, 0) - Rotated 90
    // 180 Deg: Bottom Center (0, 150) - Rotated 180
    // 270 Deg: Left Center (-150, 0) - Rotated 270
    
    int x_off = 0;
    int y_off = -150;
    
    // Position & Rotate
    if (lbl_status_dynamic) {
        lv_obj_align(lbl_status_dynamic, LV_ALIGN_CENTER, x_off, y_off);
        lv_obj_set_style_transform_rotation(lbl_status_dynamic, 0, 0); // No Rotation
        
        // Ensure scale is reset
        lv_obj_set_style_transform_scale_x(lbl_status_dynamic, 256, 0);
        lv_obj_set_style_transform_scale_y(lbl_status_dynamic, 256, 0);
        
        lv_obj_clear_flag(lbl_status_dynamic, LV_OBJ_FLAG_HIDDEN);
    }
    
    
    // Reset Max Values
    // Reset Max Values (Session only?)
    // User requested "Calibrate" resets the inclinometer zero.
    // It implies starting a fresh measurement context?
    // Let's reset SESSION max values on calibration.
    session_roll_left = 0;
    session_roll_right = 0;
    session_pitch_fwd = 0;
    session_pitch_back = 0;
    
    // Do NOT reset All Time max values here.
    
    max_pitch_fwd = 0;
    max_pitch_back = 0;
    
    // RAM ONLY: Do not write to NVS here (inside LVGL lock).
    // These will be saved by the background auto-save or explicit save later.
    dirty = true; 
    
    // Force immediate UI update for markers
    // Trigger layout recalc
}

// Calibration & Gesture Callback
static void calibration_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_LONG_PRESSED) {
        triggerCalibrationUI();
    }
    else if (code == LV_EVENT_CLICKED) {
        // Gesture removed in favor of physical button
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

    // Load Max Values & Settings
    // Load Max Values & Settings
    ui_prefs.begin("ui", false);
    all_time_roll_left = ui_prefs.getFloat("m_rl", 0);
    // Compatibility: If keys exist (old version), they load into All Time.
    all_time_roll_left = ui_prefs.getFloat("m_rl", 0); 
    // Fix: Redundant line removed.
    
    all_time_roll_left = ui_prefs.getFloat("m_rl", 0);
    all_time_roll_right = ui_prefs.getFloat("m_rr", 0); // Corrected key (was implicit in logic)
    all_time_pitch_fwd = ui_prefs.getFloat("m_pf", 0);
    all_time_pitch_back = ui_prefs.getFloat("m_pb", 0);
    
    // Note: session max starts at 0.

    // Load Rotation
    ui_rotation = ui_prefs.getInt("rot", 0);
    if (ui_rotation % 90 != 0) ui_rotation = 0; // Sanitize

    // Load Critical Angles
    critical_roll = ui_prefs.getInt("crit_r", 50);
    critical_pitch = ui_prefs.getInt("crit_p", 50);
    
    // Load Color
    ui_color_idx = ui_prefs.getInt("color", 0);
    
    // Load Pixel Shift (Default 1/True)
    pixel_shift_enabled = ui_prefs.getBool("p_shift", true);
    
    // Start Pixel Shift Timer (Every 60s)
    pixel_shift_timer = lv_timer_create(pixel_shift_cb, 60000, NULL);
    // Trigger once to ensure state matches
    lv_timer_ready(pixel_shift_timer);

    // Sanitize Loaded Values
    if (critical_roll < 10 || critical_roll > 90) critical_roll = 50;
    if (critical_pitch < 10 || critical_pitch > 90) critical_pitch = 50;
    if (ui_color_idx < 0 || ui_color_idx > 5) ui_color_idx = 0; // Default Orange
    
    // Apply Hardware Rotation Immediately
    setUIRotation(ui_rotation);

    // Apply Color MOVED to end of function to avoid NULL pointer crash


    // Legacy critical_angle loading removed.


    // 1. Background Image
    bg_img = lv_image_create(scr);
    lv_image_set_src(bg_img, &img_background);
    lv_obj_center(bg_img);
    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_CLICKABLE); // Enable input for long press
    lv_obj_add_event_cb(bg_img, calibration_event_cb, LV_EVENT_ALL, NULL);
    lv_image_set_rotation(bg_img, 0); // Always 0 (HW handles rotation)


    // 2. Roll Truck (Top Center)
    truck_roll_img = lv_image_create(scr);
    lv_image_set_src(truck_roll_img, &img_truck_rear);
    // Position will be set in updateUI
    lv_image_set_pivot(truck_roll_img, 32, 32); // Center

    // 3. Pitch Truck (Bottom Center)
    truck_pitch_img = lv_image_create(scr);
    lv_image_set_src(truck_pitch_img, &img_truck_side);
    // Position set in updateUI
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
    lv_obj_add_flag(pointer_roll, LV_OBJ_FLAG_IGNORE_LAYOUT); 
    lv_image_set_pivot(pointer_roll, 10, 15); 

    pointer_pitch = lv_image_create(scr);
    lv_image_set_src(pointer_pitch, &img_pointer);
    lv_obj_add_flag(pointer_pitch, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_image_set_pivot(pointer_pitch, 10, 15);


    // 6. Labels
    // Roll Value
    label_roll_val = lv_label_create(scr);
    lv_label_set_text(label_roll_val, "0°");
    lv_obj_set_style_text_font(label_roll_val, &lv_font_montserrat_28, 0); 
    lv_obj_set_style_text_color(label_roll_val, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_transform_scale(label_roll_val, 384, 0); 
    lv_obj_set_style_transform_pivot_x(label_roll_val, LV_PCT(50), 0); 
    lv_obj_set_style_transform_pivot_y(label_roll_val, LV_PCT(50), 0);
    lv_obj_set_style_text_align(label_roll_val, LV_TEXT_ALIGN_CENTER, 0);
    // Pos set in updateUI

    lbl_roll = lv_label_create(scr);
    lv_label_set_text(lbl_roll, "ROLL");
    lv_obj_set_style_text_font(lbl_roll, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_roll, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_transform_scale(lbl_roll, 256, 0); 
    lv_obj_set_style_transform_pivot_x(lbl_roll, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_roll, LV_PCT(50), 0);
    lv_obj_set_style_text_align(lbl_roll, LV_TEXT_ALIGN_CENTER, 0);
    // Pos set in updateUI

    // Pitch Value
    label_pitch_val = lv_label_create(scr);
    lv_label_set_text(label_pitch_val, "0°");
    lv_obj_set_style_text_font(label_pitch_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_pitch_val, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_transform_scale(label_pitch_val, 384, 0); 
    lv_obj_set_style_transform_pivot_x(label_pitch_val, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(label_pitch_val, LV_PCT(50), 0);
    lv_obj_set_style_text_align(label_pitch_val, LV_TEXT_ALIGN_CENTER, 0);
    // Pos set in updateUI

    lbl_pitch = lv_label_create(scr);
    lv_label_set_text(lbl_pitch, "PITCH");
    lv_obj_set_style_text_font(lbl_pitch, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_pitch, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_transform_scale(lbl_pitch, 256, 0); 
    lv_obj_set_style_transform_pivot_x(lbl_pitch, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_pitch, LV_PCT(50), 0);
    lv_obj_set_style_text_align(lbl_pitch, LV_TEXT_ALIGN_CENTER, 0);
    // Pos set in updateUI
 

    // Status Label (Hidden by default)
    // 8. Status Overlay (Stable Container)
    overlay_status = lv_obj_create(scr);
    lv_obj_set_size(overlay_status, 466, 466);
    lv_obj_center(overlay_status);
    lv_obj_set_style_bg_opa(overlay_status, 0, 0); // Transparent
    lv_obj_set_style_border_width(overlay_status, 0, 0);
    lv_obj_remove_flag(overlay_status, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(overlay_status, LV_OBJ_FLAG_CLICKABLE); // Pass through clicks
    lv_obj_add_flag(overlay_status, LV_OBJ_FLAG_HIDDEN);

    // Create Single Dynamic Status Label
    lbl_status_dynamic = lv_label_create(overlay_status);
    lv_label_set_text(lbl_status_dynamic, "CALIBRATING...");
    lv_obj_set_style_text_font(lbl_status_dynamic, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_status_dynamic, lv_color_hex(0xFF6D00), 0);
    lv_obj_set_width(lbl_status_dynamic, LV_SIZE_CONTENT);
    lv_obj_set_style_transform_pivot_x(lbl_status_dynamic, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_status_dynamic, LV_PCT(50), 0);
    // lv_obj_add_flag(lbl_status_dynamic, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // Failed
    lv_obj_set_style_text_align(lbl_status_dynamic, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(lbl_status_dynamic, LV_OBJ_FLAG_HIDDEN);

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

    // Create Single Dynamic Critical Label
    lbl_critical_dynamic = lv_label_create(overlay_critical);
    lv_label_set_text(lbl_critical_dynamic, "CRITICAL");
    lv_obj_set_style_text_font(lbl_critical_dynamic, &lv_font_montserrat_48, 0);
    
    // Styling
    lv_obj_set_width(lbl_critical_dynamic, LV_SIZE_CONTENT);
    lv_obj_set_style_transform_scale(lbl_critical_dynamic, 256, 0); 
    lv_obj_set_style_transform_pivot_x(lbl_critical_dynamic, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl_critical_dynamic, LV_PCT(50), 0);
    lv_obj_set_style_text_align(lbl_critical_dynamic, LV_TEXT_ALIGN_CENTER, 0);
    // lv_obj_add_flag(lbl_critical_dynamic, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // Failed
    lv_obj_set_style_text_color(lbl_critical_dynamic, lv_color_hex(0xFF0000), 0);
    
    // Center
    lv_obj_center(lbl_critical_dynamic);
    
    // Hide initially
    lv_obj_add_flag(lbl_critical_dynamic, LV_OBJ_FLAG_HIDDEN);
    
    // 8. Apply Color Theme (Safe now that objects exist)
    applyUIColor(ui_color_idx);
}

// Removed extra brace 
// }

void updateUI(float roll, float pitch) {
    char buf[16];
    static char current_roll_text[16] = "";
    static char current_pitch_text[16] = "";
    static bool was_critical = false;
    static bool was_warning = false;
    static int last_critical_type = -1; // -1 to force first update
    static uint32_t last_blink_time = 0;
    static bool blink_state = false;

    // Fixed Mapping (Default 270 logic?)
    // Actually, looking at original code structure:
    // If Rot=0 was default: Roll=Pitch, Pitch=Roll (swapped).
    // If Rot=270 was default: Roll=Roll, Pitch=Pitch.
    // Let's assume User wants standard "Landscape" which on this screen is 270.
    // BUT! Since we are removing software rotation, we should just feed RAW data and let hardware rotation handle it later.
    // However, for NOW, we need to output something that looks correct in the current hardware state (which is probably 0).
    // In "Default" (0) block earlier: effective_roll = pitch; effective_pitch = roll;
    // Conditional Mapping based on Rotation
    float effective_roll, effective_pitch;
    
    if (ui_rotation == 90 || ui_rotation == 270) {
        // 90/270: Standard Mapping (Sensor X=Roll, Sensor Y=Pitch relative to screen)
        effective_roll = roll;
        // Pitch Flip Logic
        if (ui_rotation == 90) effective_pitch = -pitch; // Fix Inversion for 90
        else effective_pitch = pitch;
    } else {
        // 0/180: Swapped Mapping (Sensor X=Pitch, Sensor Y=Roll relative to screen)
        effective_roll = pitch;
        // Pitch Flip Logic
        if (ui_rotation == 180) effective_pitch = -roll; // Fix Inversion for 180
        else effective_pitch = roll;
    }

    // --- Max Angle Tracking ---
    if (!is_calibrating) {
        // Track All Time
        if (effective_roll > all_time_roll_right) { all_time_roll_right = effective_roll; dirty = true; }
        if (effective_roll < all_time_roll_left) { all_time_roll_left = effective_roll; dirty = true; }
        if (effective_pitch > all_time_pitch_back) { all_time_pitch_back = effective_pitch; dirty = true; }
        if (effective_pitch < all_time_pitch_fwd) { all_time_pitch_fwd = effective_pitch; dirty = true; }

        // Track Session
        if (effective_roll > session_roll_right) { session_roll_right = effective_roll; }
        if (effective_roll < session_roll_left) { session_roll_left = effective_roll; }
        if (effective_pitch > session_pitch_back) { session_pitch_back = effective_pitch; }
        if (effective_pitch < session_pitch_fwd) { session_pitch_fwd = effective_pitch; }
    }

    // Save to NVS
    if (dirty && !is_calibrating && lv_tick_elaps(last_save_time) > 5000) {
        ui_prefs.putFloat("m_rl", all_time_roll_left);
        ui_prefs.putFloat("m_rr", all_time_roll_right);
        ui_prefs.putFloat("m_pf", all_time_pitch_fwd);
        ui_prefs.putFloat("m_pb", all_time_pitch_back);
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
    // --- Update Graphics ---
    // Background Rotation FIXED to 0
    lv_image_set_rotation(bg_img, 0);

    // Dynamic Center Calculation
    lv_obj_t * scr = lv_scr_act();
    int32_t cx = lv_obj_get_width(scr) / 2;
    int32_t cy = lv_obj_get_height(scr) / 2;
    
    // Truck Offsets derived from original hardcoded values (233, 233)
    // Roll Truck (201, 121) -> 201 = 233-32, 121 = 233-112
    // Pitch Truck (201, 301) -> 201 = 233-32, 301 = 233+68
    
    // Roll Truck
    lv_obj_set_pos(truck_roll_img, cx - 32, cy - 112);
    
    // Pitch Truck
    lv_obj_set_pos(truck_pitch_img, cx - 32, cy + 68);
    
    // Labels are aligned with LV_ALIGN_CENTER, so they handle themselves automatically.
    
    // Label Roll Val: (-75, -25) relative to (233, 233) -> (158, 208)
    lv_obj_align(label_roll_val, LV_ALIGN_CENTER, -75, -25);
    lv_obj_set_style_transform_rotation(label_roll_val, 0, 0);

    // Label Roll Title: (-75, 25)
    lv_obj_align(lbl_roll, LV_ALIGN_CENTER, -75, 25);
    lv_obj_set_style_transform_rotation(lbl_roll, 0, 0);

    // Label Pitch Val: (75, -25)
    lv_obj_align(label_pitch_val, LV_ALIGN_CENTER, 75, -25);
    lv_obj_set_style_transform_rotation(label_pitch_val, 0, 0);

    // Label Pitch Title: (75, 25)
    lv_obj_align(lbl_pitch, LV_ALIGN_CENTER, 75, 25);
    lv_obj_set_style_transform_rotation(lbl_pitch, 0, 0);
    
    
    // --- DYNAMIC ELEMENTS (Update every frame) ---
    // Truck Rotations (0 Base)
    lv_image_set_rotation(truck_roll_img, (int32_t)(-effective_roll * 10));
    lv_image_set_rotation(truck_pitch_img, (int32_t)(-effective_pitch * 10));

    float radius = 195.0;
    
    // Roll Pointer (Base 0 Rotation)
    // Roll Pointer (Base 0 Rotation)
    float rad_roll = (180.0 - effective_roll) * 3.14159 / 180.0;
    int px_roll = cx + (int)(radius * cos(rad_roll));
    int py_roll = cy + (int)(radius * sin(rad_roll));
    lv_obj_set_pos(pointer_roll, px_roll - 10, py_roll - 15); 
    lv_image_set_rotation(pointer_roll, (int32_t)((270 - effective_roll) * 10));

    // Pitch Pointer (Base 0 Rotation)
    float rad_pitch = (0.0 - effective_pitch) * 3.14159 / 180.0;
    int px_pitch = cx + (int)(radius * cos(rad_pitch));
    int py_pitch = cy + (int)(radius * sin(rad_pitch));
    lv_obj_set_pos(pointer_pitch, px_pitch - 10, py_pitch - 15); 
    lv_image_set_rotation(pointer_pitch, (int32_t)((90 - effective_pitch) * 10));

    // --- Update Max Markers (USE SESSION MAX) ---
    float disp_rl = (session_roll_left < -45.0) ? -45.0 : session_roll_left;
    float disp_rr = (session_roll_right > 45.0) ? 45.0 : session_roll_right;
    float disp_pf = (session_pitch_fwd < -45.0) ? -45.0 : session_pitch_fwd;
    float disp_pb = (session_pitch_back > 45.0) ? 45.0 : session_pitch_back;
    
    // 1. Roll Left
    float rad_rl = (180.0 - disp_rl) * 3.14159 / 180.0;
    int px_rl = cx + (int)(radius * cos(rad_rl));
    int py_rl = cy + (int)(radius * sin(rad_rl));
    lv_obj_set_pos(dot_roll_left, px_rl - 5, py_rl - 5); 
    
    // 2. Roll Right
    float rad_rr = (180.0 - disp_rr) * 3.14159 / 180.0;
    int px_rr = cx + (int)(radius * cos(rad_rr));
    int py_rr = cy + (int)(radius * sin(rad_rr));
    lv_obj_set_pos(dot_roll_right, px_rr - 5, py_rr - 5);

    // 3. Pitch Fwd (Left side of Pitch gauge?)
    float rad_pf = (0.0 - disp_pf) * 3.14159 / 180.0;
    int px_pf = cx + (int)(radius * cos(rad_pf));
    int py_pf = cy + (int)(radius * sin(rad_pf));
    lv_obj_set_pos(dot_pitch_fwd, px_pf - 5, py_pf - 5);

    // 4. Pitch Back
    float rad_pb = (0.0 - disp_pb) * 3.14159 / 180.0;
    int px_pb = cx + (int)(radius * cos(rad_pb));
    int py_pb = cy + (int)(radius * sin(rad_pb));
    lv_obj_set_pos(dot_pitch_back, px_pb - 5, py_pb - 5);

    
    // Update Last Rotation
    // last_applied_rotation = ui_rotation; // Removed as rotation is fixed


    // Handle Calibration Timeout
    if (is_calibrating && lv_tick_elaps(calibration_start_time) > 2000) {
        is_calibrating = false;
        lv_obj_add_flag(overlay_status, LV_OBJ_FLAG_HIDDEN);
        // Trigger actual zeroing here if we had access to IMU object
    }

    // --- Critical Angle Alert (> 50 degrees) ---
    // Overrides the Warning Alert
    float max_angle = 0;
    if (abs(effective_roll) > max_angle) max_angle = abs(effective_roll);
    if (abs(effective_pitch) > max_angle) max_angle = abs(effective_pitch);

    // Check vs Split Limits
    bool is_crit = (abs(effective_roll) > (float)critical_roll) || (abs(effective_pitch) > (float)critical_pitch);

    if (is_crit && !is_calibrating) {
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
            lv_obj_set_style_border_width(overlay_critical, 20, 0);
            
            was_critical = true;
        }
        
        // Determine Current Type Index (0:Pitch, 1:Roll, 2:Both)
        // Adjusting Previous Logic: 1->Both, 2->Pitch, 3->Roll
        // New Mapping: 0->Pitch, 1->Roll, 2->Both
        
        // Logic Bug Fix:
        // if (pitch > 50) type = 0.
        // if (roll > 50) type = 1.
        // if (BOTH > 50) type = 2.
        
        // Previous logic:
        // if (roll > 50 && pitch > 50) both.
        // else if (pitch > 50) pitch.
        // else type = 1 (roll).  <-- BUG! This assumes if it's not both, and not pitch, it MUST be roll.
        // But what if NEITHER is > 50? (Shouldn't happen because we are inside `if (max_angle > critical)`)
        // BUT `max_angle` is `max(abs(roll), abs(pitch))`.
        // So at least one is > critical.
        // However, if `critical` = 50.
        // If Roll=60, Pitch=10. max=60 > 50. -> Not Both. Not Pitch. -> Roll. Correct.
        // If Pitch=60, Roll=10. max=60 > 50. -> Not Both. Pitch. Correct.
        // If Pitch=60, Roll=60. max=60 > 50. -> Both. Correct.
        
        // Wait, user says "Critical Pitch does not show up".
        // Maybe `effective_pitch` calculation is swapped/wrong?
        // Let's verify data source mapping (Lines 429-450).
        // 0 Deg: eff_pitch = roll. eff_roll = pitch. (Wait, DEFAULT is SWAPPED?)
        // Line 447: default (0): eff_roll = pitch, eff_pitch = roll.
        // User said "Pitch moves Roll" -> Needs SWAP. So we swapped them.
        
        // If the user says "Critical Pitch does not show up", maybe `abs(effective_pitch)` is NOT triggering?
        // Or maybe logic flow?
        // else type_idx = 1; // Roll
        // If Pitch is critical, it enters `else if (abs(pitch) > 50)`.
        
        // Logic Bug Fix: Explicitly check all cases
        // Explicit Logic for Split Limits
        bool roll_crit = abs(effective_roll) > (float)critical_roll;
        bool pitch_crit = abs(effective_pitch) > (float)critical_pitch;
        
        int type_idx = 0;
        if (roll_crit && pitch_crit) type_idx = 2; // Both
        else if (pitch_crit) type_idx = 0; // Pitch
        else type_idx = 1; // Roll
        
        // Only update visibility if changed
        if (type_idx != last_critical_type) { 
             // Set Text
             const char* msg = "";
             if (type_idx == 2) msg = "CRITICAL\nPITCH / ROLL";
             else if (type_idx == 0) msg = "CRITICAL\nPITCH";
             else msg = "CRITICAL\nROLL";
             
             // Use Static Text (Safety)
             if (lbl_critical_dynamic) {
                 // Use safe copy instead of static
                 lv_label_set_text(lbl_critical_dynamic, msg);

                 // Set Rotation (Dynamic) - FIXED 0
                 lv_obj_set_style_transform_rotation(lbl_critical_dynamic, 0, 0); 
                 
                 // Ensure scale is reset from any previous attempts
                 lv_obj_set_style_transform_scale_x(lbl_critical_dynamic, 256, 0);
                 lv_obj_set_style_transform_scale_y(lbl_critical_dynamic, 256, 0);
                 
                 // Show
                 lv_obj_clear_flag(lbl_critical_dynamic, LV_OBJ_FLAG_HIDDEN);
             }

             // Update last type
             last_critical_type = type_idx; 
        }

        // Static Full Opacity
        if (overlay_critical) {
            lv_obj_set_style_border_opa(overlay_critical, 255, 0);
        } else {
            // Safety
        }
        
    } else {
        // Hide Critical Overlay
        if (was_critical) {
            lv_obj_add_flag(overlay_critical, LV_OBJ_FLAG_HIDDEN);
            was_critical = false;
            last_critical_type = -1; // Reset so next trigger forces update
            
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

    // Handle Toast Timeout
    // Handle Toast Timeout
    if (toast_duration > 0 && toast_show_time > 0 && lv_tick_elaps(toast_show_time) > toast_duration) {
        hideToast();
    }
}

bool isCalibrating() {
    return is_calibrating;
}

// New function to consume trigger
bool consumeCalibrationTrigger() {
    if (calibration_trigger_active) {
        calibration_trigger_active = false;
        return true;
    }
    return false;
}

// --- Settings Implementation ---

void setUIRotation(int degrees) {
    if (degrees % 90 != 0) return; // Only 0, 90, 180, 270
    
    ui_rotation = degrees;
    ui_prefs.putInt("rot", ui_rotation);
    
    // Apply to Hardware
    lvgl_port_set_rotation(ui_rotation);
    setTouchRotation(ui_rotation);
    
    // Force Full Refresh to clear artifacts
    lv_obj_invalidate(lv_scr_act());
}

int getUIRotation() {
    return ui_rotation;
}

void setCriticalValues(int roll, int pitch) {
    critical_roll = roll;
    critical_pitch = pitch;
    
    if (critical_roll < 10) critical_roll = 10;
    if (critical_roll > 90) critical_roll = 90;
    
    if (critical_pitch < 10) critical_pitch = 10;
    if (critical_pitch > 90) critical_pitch = 90;

    ui_prefs.putInt("crit_r", critical_roll);
    ui_prefs.putInt("crit_p", critical_pitch);
}

int getCriticalRoll() { return critical_roll; }
int getCriticalPitch() { return critical_pitch; }

void setPixelShift(bool enabled) {
    pixel_shift_enabled = enabled;
    ui_prefs.putBool("p_shift", enabled);
    
    // Reset immediately if disabled
    if (!enabled) {
        lv_obj_set_style_translate_x(lv_scr_act(), 0, 0);
        lv_obj_set_style_translate_y(lv_scr_act(), 0, 0);
    }
}

bool getPixelShift() {
    return pixel_shift_enabled;
}

void resetAllTimeStats() {
    all_time_roll_left = 0;
    all_time_roll_right = 0;
    all_time_pitch_fwd = 0;
    all_time_pitch_back = 0;
    dirty = true;
    
    // Also reset session? User likely wants to clear everything.
    session_roll_left = 0;
    session_roll_right = 0;
    session_pitch_fwd = 0;
    session_pitch_back = 0;
}

void getAllTimeMax(float* r_left, float* r_right, float* p_fwd, float* p_back) {
    if(r_left) *r_left = all_time_roll_left;
    if(r_right) *r_right = all_time_roll_right;
    if(p_fwd) *p_fwd = all_time_pitch_fwd;
    if(p_back) *p_back = all_time_pitch_back;
}

void getSessionMax(float* r_left, float* r_right, float* p_fwd, float* p_back) {
    if(r_left) *r_left = session_roll_left;
    if(r_right) *r_right = session_roll_right;
    if(p_fwd) *p_fwd = session_pitch_fwd;
    if(p_back) *p_back = session_pitch_back;
    
}

void setUIColor(int color_idx) {
    if (color_idx < 0 || color_idx > 5) return;
    ui_color_idx = color_idx;
    ui_prefs.putInt("color", ui_color_idx);
    applyUIColor(ui_color_idx);
}

int getUIColor() { return ui_color_idx; }

void applyUIColor(int idx) {
    if (idx < 0 || idx > 5) idx = 0;
    lv_color_t color = theme_colors[idx];
    
    // Apply tint to assets
    // If idx 0 (Orange), we assume assets are native Orange?
    // Actually if we recolor with Orange, it might double saturation.
    // Let's rely on standard Recolor OPA.
    
    lv_opa_t opa = LV_OPA_COVER; // Full tint
    if (idx == 0) opa = LV_OPA_TRANSP; // Default: No tint? Or still tint to ensure uniformity? 
    // If default assets ARE Orange, then no tint needed.
    // Assuming default is NATIVE.
    
    if (idx == 0) {
        // Clear Recolor
        lv_obj_set_style_image_recolor_opa(truck_roll_img, LV_OPA_TRANSP, 0);
        lv_obj_set_style_image_recolor_opa(truck_pitch_img, LV_OPA_TRANSP, 0);
        lv_obj_set_style_image_recolor_opa(pointer_roll, LV_OPA_TRANSP, 0);
        lv_obj_set_style_image_recolor_opa(pointer_pitch, LV_OPA_TRANSP, 0);
    } else {
        // Apply Recolor
        lv_obj_set_style_image_recolor(truck_roll_img, color, 0);
        lv_obj_set_style_image_recolor_opa(truck_roll_img, LV_OPA_COVER, 0); // Warning: Might lose details? Use MIX/COVER properly?
        // PNG transparency is preserved. But full solid color might be flat.
        // Let's try it.
        
        lv_obj_set_style_image_recolor(truck_pitch_img, color, 0);
        lv_obj_set_style_image_recolor_opa(truck_pitch_img, LV_OPA_COVER, 0);
        
        lv_obj_set_style_image_recolor(pointer_roll, color, 0);
        lv_obj_set_style_image_recolor_opa(pointer_roll, LV_OPA_COVER, 0);
        
        lv_obj_set_style_image_recolor(pointer_pitch, color, 0);
        lv_obj_set_style_image_recolor_opa(pointer_pitch, LV_OPA_COVER, 0);
    }
}

void resetSettings() {
    // Reset NVS to defaults
    ui_prefs.putInt("crit_r", 50);
    ui_prefs.putInt("crit_p", 50);
    ui_prefs.putInt("color", 0); // Orange
    
    // Reset Max
    ui_prefs.putFloat("m_rl", 0);
    ui_prefs.putFloat("m_rr", 0);
    ui_prefs.putFloat("m_pf", 0);
    ui_prefs.putFloat("m_pb", 0);
    
    Serial.println("Settings Reset. Rebooting..."); 
    delay(100);
    ESP.restart(); 
}
