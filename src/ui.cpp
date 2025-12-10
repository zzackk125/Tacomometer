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
// static lv_obj_t * label_critical; // Dynamic removed
static lv_obj_t * lbl_crit_static[4][3]; // [RotationIndex][TypeIndex]
// Rotations: 0=0, 1=90, 2=180, 3=270
// Types: 0=Pitch, 1=Roll, 2=Both
static lv_obj_t * overlay_status; // New stable container for status
static lv_obj_t * label_status_0;
static lv_obj_t * label_status_90;
static lv_obj_t * label_status_180;
static lv_obj_t * label_status_270;

// Max Angle Markers
static lv_obj_t * dot_roll_left;
static lv_obj_t * dot_roll_right;
static lv_obj_t * dot_pitch_fwd;
static lv_obj_t * dot_pitch_back;

// Max Values (Persisted)
// Max Values (Persisted)
static float max_roll_left = 0;
static float max_roll_right = 0;
static float max_pitch_fwd = 0;
static float max_pitch_back = 0;
static Preferences ui_prefs;

// Settings
static int ui_rotation = 0;
static int critical_angle = 50;

// Main Container for Rotation
// REMOVED due to bootloop issues


// Calibration State
static bool is_calibrating = false;
static bool calibration_trigger_active = false; // New flag for one-shot trigger
static uint32_t calibration_start_time = 0;

// Toast Object
static lv_obj_t * toast_obj = NULL;

// Auto-Save State
static uint32_t last_save_time = 0;
static bool dirty = false;

// Layout Caching
static int last_applied_rotation = -1;

void showToast(const char* text) {
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
}

void hideToast() {
    if (toast_obj) {
        lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_clear_flag(overlay_status, LV_OBJ_FLAG_HIDDEN);
    
    // Show Correct Static Label (Pre-Rotated)
    lv_obj_add_flag(label_status_0, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_status_90, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_status_180, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label_status_270, LV_OBJ_FLAG_HIDDEN);
    
    switch(ui_rotation) {
        case 0: lv_obj_clear_flag(label_status_0, LV_OBJ_FLAG_HIDDEN); break;
        case 90: lv_obj_clear_flag(label_status_90, LV_OBJ_FLAG_HIDDEN); break;
        case 180: lv_obj_clear_flag(label_status_180, LV_OBJ_FLAG_HIDDEN); break;
        case 270: lv_obj_clear_flag(label_status_270, LV_OBJ_FLAG_HIDDEN); break;
        default: lv_obj_clear_flag(label_status_0, LV_OBJ_FLAG_HIDDEN); break;
    }
    
    // Reset Max Values
    max_roll_left = 0;
    max_roll_right = 0;
    max_pitch_fwd = 0;
    max_pitch_back = 0;
    
    max_pitch_fwd = 0;
    max_pitch_back = 0;
    
    // RAM ONLY: Do not write to NVS here (inside LVGL lock).
    // These will be saved by the background auto-save or explicit save later.
    dirty = true; 
    
    // Force immediate UI update for markers
    last_applied_rotation = -1; // Trigger layout recalc
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
    ui_prefs.begin("ui", false);
    max_roll_left = ui_prefs.getFloat("m_rl", 0);
    max_roll_right = ui_prefs.getFloat("m_rr", 0);
    max_pitch_fwd = ui_prefs.getFloat("m_pf", 0);
    max_pitch_back = ui_prefs.getFloat("m_pb", 0);

    ui_rotation = ui_prefs.getInt("rot", 0);
    critical_angle = ui_prefs.getInt("crit", 50);

    // Sanitize Loaded Values (Prevent crashes/glitches from bad NVS)
    if (ui_rotation < 0 || ui_rotation >= 360) ui_rotation = 0;
    if (critical_angle < 10 || critical_angle > 90) critical_angle = 50;

    // 1. Background Image
    bg_img = lv_image_create(scr);
    lv_image_set_src(bg_img, &img_background);
    lv_obj_center(bg_img);
    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_CLICKABLE); // Enable input for long press
    lv_obj_add_event_cb(bg_img, calibration_event_cb, LV_EVENT_ALL, NULL);
    lv_image_set_rotation(bg_img, ui_rotation * 10);


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

    // Create 4 Static Labels (Pre-rotated/Pre-positioned)
    // Avoid dynamic changes during calibration to prevent crashing
    
    // Helper to init status label
    auto init_status_lbl = [&](lv_obj_t** lbl, int rot, int x_off, int y_off) {
        *lbl = lv_label_create(overlay_status);
        lv_label_set_text(*lbl, "CALIBRATING...");
        lv_obj_set_style_text_font(*lbl, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(*lbl, lv_color_hex(0xFF6D00), 0);
        lv_obj_set_width(*lbl, LV_SIZE_CONTENT);
        // lv_obj_set_style_transform_scale(*lbl, 256, 0); // Native Scale
        lv_obj_set_style_transform_pivot_x(*lbl, LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(*lbl, LV_PCT(50), 0);
        lv_obj_set_style_text_align(*lbl, LV_TEXT_ALIGN_CENTER, 0);
        
        // Position: Align Center then offset
        lv_obj_align(*lbl, LV_ALIGN_CENTER, x_off, y_off);
        
        // Rotation
        lv_obj_set_style_transform_rotation(*lbl, rot * 10, 0);
        
        // Hide initially
        lv_obj_add_flag(*lbl, LV_OBJ_FLAG_HIDDEN);
    };

    // 0 Deg: Top Center (0, -150)
    init_status_lbl(&label_status_0, 0, 0, -150);
    
    // 90 Deg: Right Center (150, 0) - Rotated 90
    init_status_lbl(&label_status_90, 90, 150, 0);
    
    // 180 Deg: Bottom Center (0, 150) - Rotated 180
    init_status_lbl(&label_status_180, 180, 0, 150);
    
    // 270 Deg: Left Center (-150, 0) - Rotated 270
    init_status_lbl(&label_status_270, 270, -150, 0);

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

    // Create 12 Static Critical Labels (4 Rotations * 3 Types)
    // Avoid dynamic text changes/layout at runtime
    const char* crit_texts[3] = { "CRITICAL\nPITCH", "CRITICAL\nROLL", "CRITICAL\nPITCH / ROLL" };
    int rots[4] = {0, 90, 180, 270};
    
    for(int r=0; r<4; r++) {
        for(int t=0; t<3; t++) {
             lv_obj_t* lbl = lv_label_create(overlay_critical);
             lbl_crit_static[r][t] = lbl;
             
             lv_label_set_text(lbl, crit_texts[t]);
             lv_obj_set_style_text_font(lbl, &lv_font_montserrat_48, 0);
             
             // Styling
             lv_obj_set_width(lbl, LV_SIZE_CONTENT);
             lv_obj_set_style_transform_scale(lbl, 256, 0); 
             lv_obj_set_style_transform_pivot_x(lbl, LV_PCT(50), 0);
             lv_obj_set_style_transform_pivot_y(lbl, LV_PCT(50), 0);
             lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
             lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF0000), 0);
             
             // Center first
             lv_obj_center(lbl);
             
             // Rotation
             lv_obj_set_style_transform_rotation(lbl, rots[r] * 10, 0);
             
             // Hide initially
             lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Coordinate Rotation Helper
void rotate_point(int cx, int cy, float angle_deg, int x, int y, int *nx, int *ny) {
    float rad = angle_deg * 3.14159 / 180.0;
    float s = sin(rad);
    float c = cos(rad);
    
    // Valid for clockwise rotation (screen coordinates y down)
    // x' = x*cos - y*sin
    // y' = x*sin + y*cos
    
    float dx = (float)(x - cx);
    float dy = (float)(y - cy);
    
    *nx = cx + (int)(dx * c - dy * s);
    *ny = cy + (int)(dx * s + dy * c);
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

    // Layout Caching
    // static int last_applied_rotation = -1; // Moved to global

    // SWAP DATA SOURCES: Map Sensor Data to UI Axes based on Rotation
    float effective_roll = 0;
    float effective_pitch = 0;
    
    // 0 Deg: Roll=Roll, Pitch=Pitch
    // 90 Deg: Roll=Pitch, Pitch=-Roll (Rotated CW, X becomes Y, Y becomes -X)
    // 180 Deg: Roll=-Roll, Pitch=-Pitch
    // 270 Deg: Roll=-Pitch, Pitch=Roll
    
    switch(ui_rotation) {
        case 90:
            // User feedback: "Perfect" (1:1 mapping)
            effective_roll = roll;
            effective_pitch = pitch;
            break;
        case 180:
            // User feedback: "Swapped AND Inverted"
            effective_roll = -pitch;
            effective_pitch = -roll;
            break;
        case 270:
            // User feedback: "Perfect" (1:1 mapping)
            effective_roll = roll;
            effective_pitch = pitch;
            break;
        default: // 0
            // User feedback: "Pitch moves Roll" -> Needs SWAP
            effective_roll = pitch;
            effective_pitch = roll;
            break;
    }

    // --- Max Angle Tracking ---
    // Roll (Left is negative, Right is positive in our logic? Or vice versa?)
    // Let's assume standard: Left -, Right +. 
    // But we display abs(). 
    // Let's track raw signed values.
    
    // --- Max Angle Tracking ---
    // Only update if not calibrating!
    // This prevents the "Double Calibration" bug where the old high value overwrites the reset 
    // in the split second before zeroIMU executes.
    if (!is_calibrating) {
        if (effective_roll > max_roll_right) { max_roll_right = effective_roll; dirty = true; }
        if (effective_roll < max_roll_left) { max_roll_left = effective_roll; dirty = true; }
        
        if (effective_pitch > max_pitch_back) { max_pitch_back = effective_pitch; dirty = true; }
        if (effective_pitch < max_pitch_fwd) { max_pitch_fwd = effective_pitch; dirty = true; }
    }

    // Save to NVS (Throttle: Only if dirty and > 5s since last save)
    // Prevent contention with zeroIMU (which runs on main thread) during calibration
    if (dirty && !is_calibrating && lv_tick_elaps(last_save_time) > 5000) {
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
    // Background Rotation (Simple)
    if (ui_rotation != last_applied_rotation) {
        lv_image_set_rotation(bg_img, ui_rotation * 10);
    }

    // Helper Vars
    int cx = 233; int cy = 233;
    int nx, ny; // New Coordinates
    
    // --- STATIC ELEMENTS (Update only if Rotation Changed) ---
    if (ui_rotation != last_applied_rotation) {
        // Truck Images (Position)
        // 1. Roll Truck (Original: 0, -80 from center) -> (233, 153)
        rotate_point(cx, cy, (float)ui_rotation, 233, 153, &nx, &ny);
        lv_obj_set_pos(truck_roll_img, nx - 32, ny - 32); // Pivot is center (32,32)

        // 2. Pitch Truck (Original: 0, 100 from center) -> (233, 333)
        rotate_point(cx, cy, (float)ui_rotation, 233, 333, &nx, &ny);
        lv_obj_set_pos(truck_pitch_img, nx - 32, ny - 32); 
        
        // Update Labels (Rotation + Position)
        // Label Roll Val: (-75, -25)
        // Use (0,0) as center for offsets because lv_obj_align(CENTER) adds these to 233,233
        rotate_point(0, 0, (float)ui_rotation, -75, -25, &nx, &ny);
        lv_obj_align(label_roll_val, LV_ALIGN_CENTER, nx, ny);
        lv_obj_set_style_transform_rotation(label_roll_val, ui_rotation * 10, 0);
        
        // Label Roll Title: (-75, 25)
        rotate_point(0, 0, (float)ui_rotation, -75, 25, &nx, &ny);
        lv_obj_align(lbl_roll, LV_ALIGN_CENTER, nx, ny);
        lv_obj_set_style_transform_rotation(lbl_roll, ui_rotation * 10, 0);

        // Label Pitch Val: (75, -25)
        rotate_point(0, 0, (float)ui_rotation, 75, -25, &nx, &ny);
        lv_obj_align(label_pitch_val, LV_ALIGN_CENTER, nx, ny);
        lv_obj_set_style_transform_rotation(label_pitch_val, ui_rotation * 10, 0);

        // Label Pitch Title: (75, 25)
        rotate_point(0, 0, (float)ui_rotation, 75, 25, &nx, &ny);
        lv_obj_align(lbl_pitch, LV_ALIGN_CENTER, nx, ny);
        lv_obj_set_style_transform_rotation(lbl_pitch, ui_rotation * 10, 0);
        
        // Status Label (Pre-Calculated Static Toggle)
        // No dynamic layout needed! logic is in triggerCalibrationUI
        
        // Critical Alert (Text Rotation)
        // Handled by Static Logic in Critical Block
    }
    
    // --- DYNAMIC ELEMENTS (Update every frame) ---
    
    // Truck Rotations (Depend on both UI Roto and Sensor)
    lv_image_set_rotation(truck_roll_img, (int32_t)((ui_rotation * 10) + (-effective_roll * 10)));
    lv_image_set_rotation(truck_pitch_img, (int32_t)((ui_rotation * 10) + (-effective_pitch * 10)));

    float radius = 195.0;
    
    // Roll Pointer
    // Angle: 180 (Left) to 360/0 (Right). Top is 270.
    // Logic: 180 - effective_roll. If effective_roll is 0, angle is 180 (Left? Wait).
    // Let's simple check original code:
    // float rad_roll = (180.0 - effective_roll) * PI / 180.0;
    // cos(180) = -1. x = center - radius. Correct.
    // So 0 roll = Left (9 o'clock)? 
    // Wait, standard gauge: 0 is UP. 
    // If 0 is UP (270 deg), then logic should be different.
    // Original: (180 - roll). If roll=0 => 180. That is Left.
    // Is the gauge sideways?
    // "Roll value (Left), Pitch Value (Right)".
    // Ah, the pointers are on the sides?
    // Let's trust original logic logic logic: (180.0 - effective_roll)
    // We just ADD ui_rotation (deg) to the input angle.
    // But coordinate system: 0 is Right (3 o'clock), 90 Down, 180 Left, 270 Up.
    // We rotate CLOCKWISE. So Angle + Rotation.
    
    float rad_roll = (180.0 - effective_roll + ui_rotation) * 3.14159 / 180.0;
    int px_roll = 233 + (int)(radius * cos(rad_roll));
    int py_roll = 233 + (int)(radius * sin(rad_roll));
    lv_obj_set_pos(pointer_roll, px_roll - 10, py_roll - 15); 
    // Image Rotation: (270 - effective_roll) -> This is absolute image rotation.
    // Add ui_rotation.
    lv_image_set_rotation(pointer_roll, (int32_t)((270 - effective_roll + ui_rotation) * 10));

    // Pitch Pointer
    // Original: (0.0 - effective_pitch). 0 is Right.
    float rad_pitch = (0.0 - effective_pitch + ui_rotation) * 3.14159 / 180.0;
    int px_pitch = 233 + (int)(radius * cos(rad_pitch));
    int py_pitch = 233 + (int)(radius * sin(rad_pitch));
    lv_obj_set_pos(pointer_pitch, px_pitch - 10, py_pitch - 15); 
    lv_image_set_rotation(pointer_pitch, (int32_t)((90 - effective_pitch + ui_rotation) * 10));

    // --- Update Max Markers (Combine Static UI Rotation + Dynamic Max Values) ---
    // Optimization: If max values haven't changed AND rotation hasn't changed, skip?
    // Max values change rarely. Rotation changes during settings.
    // Let's keep this as is, it's just 4 points.
    
    // Clamp display values to +/- 45 degrees so they stay on scale
    float disp_rl = (max_roll_left < -45.0) ? -45.0 : max_roll_left;
    float disp_rr = (max_roll_right > 45.0) ? 45.0 : max_roll_right;
    float disp_pf = (max_pitch_fwd < -45.0) ? -45.0 : max_pitch_fwd;
    float disp_pb = (max_pitch_back > 45.0) ? 45.0 : max_pitch_back;
    
    // Check if we need updates
    // Actually, markers depend on UI Rotation too.
    // We can just run this. It's not too heavy.
    // ... code ...
    
    // 1. Roll Left
    float rad_rl = (180.0 - disp_rl + ui_rotation) * 3.14159 / 180.0;
    int px_rl = 233 + (int)(radius * cos(rad_rl));
    int py_rl = 233 + (int)(radius * sin(rad_rl));
    lv_obj_set_pos(dot_roll_left, px_rl - 5, py_rl - 5); 

    // 2. Roll Right
    float rad_rr = (180.0 - disp_rr + ui_rotation) * 3.14159 / 180.0;
    int px_rr = 233 + (int)(radius * cos(rad_rr));
    int py_rr = 233 + (int)(radius * sin(rad_rr));
    lv_obj_set_pos(dot_roll_right, px_rr - 5, py_rr - 5);

    // 3. Pitch Fwd
    float rad_pf = (0.0 - disp_pf + ui_rotation) * 3.14159 / 180.0;
    int px_pf = 233 + (int)(radius * cos(rad_pf));
    int py_pf = 233 + (int)(radius * sin(rad_pf));
    lv_obj_set_pos(dot_pitch_fwd, px_pf - 5, py_pf - 5);

    // 4. Pitch Back
    float rad_pb = (0.0 - disp_pb + ui_rotation) * 3.14159 / 180.0;
    int px_pb = 233 + (int)(radius * cos(rad_pb));
    int py_pb = 233 + (int)(radius * sin(rad_pb));
    lv_obj_set_pos(dot_pitch_back, px_pb - 5, py_pb - 5);
    
    // Update Last Rotation
    last_applied_rotation = ui_rotation;



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

    if (max_angle > (float)critical_angle && !is_calibrating) {
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
        
        int type_idx = 0;
        if (abs(effective_roll) > 50.0 && abs(effective_pitch) > 50.0) type_idx = 2; // Both
        else if (abs(effective_pitch) > 50.0) type_idx = 0; // Pitch
        else type_idx = 1; // Roll
        
        // Determine Rotation Index
        int rot_idx = 0;
        if (ui_rotation == 90) rot_idx = 1;
        else if (ui_rotation == 180) rot_idx = 2;
        else if (ui_rotation == 270) rot_idx = 3;
        
        // Only update visibility if changed
        if (type_idx != last_critical_type) { 
             // Logic is complex because rotation might change too (though unlikely in critical).
             // Let's just Loop and Ensure correct one is shown.
             // It's 12 checks, fast enough.
             
             for(int r=0; r<4; r++) {
                 for(int t=0; t<3; t++) {
                     if (r == rot_idx && t == type_idx) {
                         lv_obj_clear_flag(lbl_crit_static[r][t], LV_OBJ_FLAG_HIDDEN);
                     } else {
                         lv_obj_add_flag(lbl_crit_static[r][t], LV_OBJ_FLAG_HIDDEN);
                     }
                 }
             }
             
             // Update last type
             last_critical_type = type_idx; 
        }

        // Blink Animation REMOVED to prevent crash
        // Static Full Opacity
        lv_obj_set_style_border_opa(overlay_critical, 255, 0);

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
    ui_rotation = degrees % 360;
    ui_prefs.putInt("rot", ui_rotation);
    // Note: Changes take effect on next updateUI call
}

int getUIRotation() {
    return ui_rotation;
}

void setCriticalAngle(int degrees) {
    critical_angle = degrees;
    if (critical_angle < 10) critical_angle = 10;
    if (critical_angle > 90) critical_angle = 90;
    ui_prefs.putInt("crit", critical_angle);
}

int getCriticalAngle() {
    return critical_angle;
}

void resetSettings() {
    // Reset NVS to defaults
    ui_prefs.putInt("rot", 0);
    ui_prefs.putInt("crit", 50);
    
    // Also clear max values?
    ui_prefs.putFloat("m_rl", 0);
    ui_prefs.putFloat("m_rr", 0);
    ui_prefs.putFloat("m_pf", 0);
    ui_prefs.putFloat("m_pb", 0);
    
    // We should also clear IMU offsets?
    // That requires accessing 'imu' namespace.
    // Let's assume user wants full factory reset.
    // For now, let's just do UI as requested + restart.
    
    // Let's assume user wants full factory reset.
    // For now, let's just do UI as requested + restart.
    
    Serial.println("Settings Reset. Updating UI..."); // No Restart
    // delay(100);
    // ESP.restart(); // Removed for seamless reset
    
    // Force immediate refresh
    last_applied_rotation = -1; 
}
