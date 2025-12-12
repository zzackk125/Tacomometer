/*
 * File: lvgl_port.c
 * Description: LVGL Display Porting Implementation
 * Author: zzackk125
 * License: MIT
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "board_config.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "sh8601/esp_lcd_sh8601.h"

#define LCD_HOST    SPI2_HOST
#define LCD_BIT_PER_PIXEL 16

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mux = NULL;
static esp_lcd_panel_io_handle_t amoled_panel_io_handle = NULL;
static lv_display_t * disp_handle = NULL;
static int current_rotation = 0; // 0, 90, 180, 270

// Forward Declarations
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
static void example_lvgl_rounder_cb(lv_event_t * e);
static void example_increase_lvgl_tick(void *arg);
static void example_lvgl_port_task(void *arg);

// Init commands from demo
static const sh8601_lcd_init_cmd_t sh8601_lcd_init_cmds[] = 
{
  {0x11, (uint8_t []){0x00}, 0, 80},   
  {0xC4, (uint8_t []){0x80}, 1, 0},
  {0x53, (uint8_t []){0x20}, 1, 1},
  {0x63, (uint8_t []){0xFF}, 1, 1},
  {0x51, (uint8_t []){0x00}, 1, 1},
  {0x29, (uint8_t []){0x00}, 0, 10},
  {0x51, (uint8_t []){0xFF}, 1, 0},
};

void lvgl_port_init(void)
{
  // 1. SPI Bus Init
  spi_bus_config_t buscfg = {
      .data0_io_num = LCD_D0_PIN,
      .data1_io_num = LCD_D1_PIN,
      .sclk_io_num = LCD_PCLK_PIN,
      .data2_io_num = LCD_D2_PIN,
      .data3_io_num = LCD_D3_PIN,
      .max_transfer_sz = (LCD_H_RES * LCD_V_RES * LCD_BIT_PER_PIXEL / 8),
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  // 2. Panel IO Init
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = LCD_CS_PIN,
      .dc_gpio_num = -1,
      .spi_mode = 0,
      .pclk_hz = 40 * 1000 * 1000,
      .trans_queue_depth = 10,
      .on_color_trans_done = example_notify_lvgl_flush_ready,
      .user_ctx = NULL,
      .lcd_cmd_bits = 32,
      .lcd_param_bits = 8,
      .flags = {
          .quad_mode = true,
      },
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
  amoled_panel_io_handle = io_handle;

  // 3. Panel Driver Init
  sh8601_vendor_config_t vendor_config = {
      .flags = {
          .use_qspi_interface = 1,
      },
      .init_cmds = sh8601_lcd_init_cmds,
      .init_cmds_size = sizeof(sh8601_lcd_init_cmds) / sizeof(sh8601_lcd_init_cmds[0]),
  };

  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = LCD_RST_PIN,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .bits_per_pixel = LCD_BIT_PER_PIXEL,
      .vendor_config = &vendor_config,
  };

  esp_lcd_panel_handle_t panel_handle = NULL;
  ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  // 4. LVGL Init (v9)
  lv_init();
  
  // Create Display
  disp_handle = lv_display_create(LCD_H_RES, LCD_V_RES);
  lv_display_set_flush_cb(disp_handle, example_lvgl_flush_cb);
  
  // Add Rounder Callback to ensure coordinates are even (required by SH8601)
  lv_display_add_event_cb(disp_handle, example_lvgl_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);
  
  lv_display_set_user_data(disp_handle, panel_handle);

  // Allocate buffers
  size_t buf_size = LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(uint16_t); // RGB565 = 2 bytes
  void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
  void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
  assert(buf1);
  assert(buf2);

  // Set buffers (v9: size is in bytes, render mode partial)
  lv_display_set_buffers(disp_handle, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // 5. Timer and Task
  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &example_increase_lvgl_tick,
      .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

  lvgl_mux = xSemaphoreCreateMutex();
  xTaskCreate(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  if (disp_handle) {
      lv_display_flush_ready(disp_handle);
  }
  return false;
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) lv_display_get_user_data(disp);
  
  // Standard logic: No manual offsets here.
  // Gaps handled by esp_lcd_panel_set_gap.
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  
  // Swap bytes for SH8601 (Little Endian -> Big Endian)
  // px_map is uint8_t*, but represents uint16_t pixels.
  // We need to swap every pair of bytes.
  uint32_t len = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
  uint16_t *buf16 = (uint16_t *)px_map;
  for(uint32_t i = 0; i < len; i++) {
      buf16[i] = (buf16[i] << 8) | (buf16[i] >> 8);
  }

  esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static void example_lvgl_rounder_cb(lv_event_t * e)
{
    lv_area_t * area = (lv_area_t *)lv_event_get_param(e);
    
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;
    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // round the start of coordinate down to the nearest 2M number
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    // round the end of coordinate up to the nearest 2N+1 number
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

static void example_increase_lvgl_tick(void *arg)
{
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_port_task(void *arg)
{
  uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
  for(;;)
  {
    if (xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE)
    {
      task_delay_ms = lv_timer_handler();
      xSemaphoreGive(lvgl_mux);
    }
    if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

void lvgl_port_lock(int timeout_ms) {
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    xSemaphoreTake(lvgl_mux, timeout_ticks);
}

void lvgl_port_unlock(void) {
    xSemaphoreGive(lvgl_mux);
}

void lvgl_port_set_rotation(int degrees) {
    if (!disp_handle) return;
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) lv_display_get_user_data(disp_handle);
    
    current_rotation = degrees;
    
    // Standard MADCTL Mappings for Rotation
    // 0:   Default
    // 90:  MV + MX
    // 180: MX + MY
    // 270: MV + MY
    
    switch (degrees) {
        case 90:
            esp_lcd_panel_swap_xy(panel_handle, true);
            esp_lcd_panel_mirror(panel_handle, true, false);
            // Gap Tuning: 7 Verified Best for 466px on 480px RAM.
            // Mathematical center ((480-466)/2 = 7).
            esp_lcd_panel_set_gap(panel_handle, 0, 7); 
            break;
        case 180:
            esp_lcd_panel_swap_xy(panel_handle, false);
            esp_lcd_panel_mirror(panel_handle, true, true);
            // Gap Tuning for 466px on 480px RAM
            // 0/180 Normal Mode: Gap X=6, Y=0 (Original working state).
            esp_lcd_panel_set_gap(panel_handle, 6, 0);
            break;
        case 270:
            esp_lcd_panel_swap_xy(panel_handle, true);
            esp_lcd_panel_mirror(panel_handle, false, true);
            // Gap Tuning: 7 Verified Best.
            esp_lcd_panel_set_gap(panel_handle, 0, 7);
            break;
        case 0:
        default:
            esp_lcd_panel_swap_xy(panel_handle, false);
            esp_lcd_panel_mirror(panel_handle, false, false);
            // 0/180 Normal Mode: Gap X=6, Y=0 (Original working state).
            esp_lcd_panel_set_gap(panel_handle, 6, 0);
            break;
    }
}

int lvgl_port_get_rotation(void) {
    return current_rotation;
}
