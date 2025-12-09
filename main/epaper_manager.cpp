#include <esp_log.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "goodisplay/gdeq037T31.h"
#include "epaper_manager.hpp"
#include "Seven_Segment9pt7b.h"


EpdSpi io;
Gdeq037T31 display(io);

static const char *TAG = "EPAPER MGR";
static bool epd_initialized = false;

// Task and queue for non-blocking updates
static TaskHandle_t epaper_task_handle = NULL;
static QueueHandle_t epaper_queue = NULL;

// Display update request structure
typedef struct {
    float temperature;
    float humidity;
    uint16_t aqi;
    uint16_t tvoc;
    uint16_t co2;
    bool matter_connected;
} epaper_update_t;

/**
 * @brief Internal function to update display with pre-fetched sensor data
 * 
 * @param data Pointer to epaper_update_t structure
 */
static void epaper_update_internal(const epaper_update_t* data) {
    if (!epd_initialized || !data) {
        return;
    }

    char line[64];

    display.fillScreen(EPD_WHITE);
    display.setTextColor(EPD_BLACK);

    const int16_t scr_w = display.width();   // 416
    const int16_t scr_h = display.height();  // 240
    const int16_t padding = 20;

    int16_t x, y;
    uint16_t w, h;

    // Left section: 0 to (175)
    const int16_t left_section_x = 0;
    const int16_t left_section_w = 175;
    
    // Right section: (scr_w - 175) to scr_w (175px wide)
    const int16_t right_section_x = 175;
    const int16_t right_section_w = scr_w - 175;
    
    /**
     * @brief LEFT SECTION: TEMP 
     * 
     */

    // Time (top right of left section)
    display.setTextSize(1);
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    bool time_valid = timeinfo.tm_year >= (2020 - 1900);
    if (time_valid) {
        // Format time as 12-hour with AM/PM
        int hour = timeinfo.tm_hour;
        const char* am_pm = (hour >= 12) ? "PM" : "AM";
        if (hour > 12) hour -= 12;
        if (hour == 0) hour = 12;
        snprintf(line, sizeof(line), "%d:%02d %s", hour, timeinfo.tm_min, am_pm);
    } else {
        // Show placeholder until SNTP sync completes
        snprintf(line, sizeof(line), "--:--");
    }

    display.getTextBounds(line, 0, 0, &x, &y, &w, &h);
    int16_t time_x = (left_section_x + left_section_w) - w - padding;
    int16_t time_y = padding - y;

    display.setCursor(time_x, time_y);
    display.print(line);

    // CENTER: TEMP label and value (centered in left section)
    display.setTextSize(1);
    snprintf(line, sizeof(line), "TEMP");
    // Measure label
    int16_t label_x_off, label_y_off;
    uint16_t label_w, label_h;
    display.getTextBounds(line, 0, 0, &label_x_off, &label_y_off, &label_w, &label_h);
    // Measure value
    display.setTextSize(5);
    if (isnan(data->temperature)) {
        snprintf(line, sizeof(line), "--C");
    } else {
        snprintf(line, sizeof(line), "%.0fC", data->temperature);
    }
    int16_t value_x_off, value_y_off;
    uint16_t value_w, value_h;
    display.getTextBounds(line, 0, 0, &value_x_off, &value_y_off, &value_w, &value_h);
    // Vertical layout
    const int16_t gap = 8; // vertical gap between label and value
    int16_t block_h   = label_h + gap + value_h;
    int16_t block_top = (scr_h - block_h) / 2;   // top of the whole TEMP block
    int16_t temp_value_x = left_section_x + (left_section_w - value_w) / 2;
    int16_t temp_label_x = temp_value_x;
    int16_t temp_label_baseline_y = block_top - label_y_off;   // top = baseline + y_off
    int16_t value_top = block_top + label_h + gap;
    int16_t temp_value_baseline_y = value_top - value_y_off;
    // Draw label
    display.setTextSize(1);
    snprintf(line, sizeof(line), "TEMP");
    display.setCursor(temp_label_x, temp_label_baseline_y);
    display.print(line);
    display.setTextSize(5);
    // Draw value
    if (isnan(data->temperature)) {
        snprintf(line, sizeof(line), "--C");
    } else {
        snprintf(line, sizeof(line), "%.0fC", data->temperature);
    }
    display.setCursor(temp_value_x, temp_value_baseline_y);
    display.print(line);

    // Product Name (bottom left)
    display.setTextSize(1);
    snprintf(line, sizeof(line), "AURA CLIMATE");
    display.getTextBounds(line, 0, 0, &x, &y, &w, &h);
    display.setCursor(padding, scr_h - padding - h - y);
    display.print(line);

    // Version number (bottom right of left section)
    const esp_app_desc_t* app_desc = esp_app_get_description();
    snprintf(line, sizeof(line), "%s", app_desc->version);
    display.getTextBounds(line, 0, 0, &x, &y, &w, &h);
    int16_t version_x = (left_section_x + left_section_w) - w - padding;
    display.setCursor(version_x, scr_h - padding - h - y);
    display.print(line);

    /**
     * @brief RIGHT SECTION: AQI 
     * 
     */

    // Vertical divider line
    const int16_t divider_x = left_section_w;
    display.drawLine(divider_x, 28, divider_x, scr_h - 38, EPD_BLACK);

    // AQI Label
    display.setTextSize(1);
    snprintf(line, sizeof(line), "AQI");
    int16_t aqi_label_x_off, aqi_label_y_off;
    uint16_t aqi_label_w, aqi_label_h;
    display.getTextBounds(line, 0, 0, &aqi_label_x_off, &aqi_label_y_off, &aqi_label_w, &aqi_label_h);
    // Label position (left + top of right section)
    int16_t aqi_label_x = right_section_x + padding;
    int16_t aqi_label_baseline_y = padding - aqi_label_y_off;
    // Draw label
    display.setCursor(aqi_label_x, aqi_label_baseline_y);
    display.print(line);
    // AQI value as text (Good, Moderate, etc.)
    display.setTextSize(5);
    const char* aqi_text;
    uint8_t aqi_val = data->aqi;
    if (aqi_val == 1) {
        aqi_text = "GOOD";
    } else if (aqi_val == 2) {
        aqi_text = "MODE";  // Moderate abbreviated
    } else if (aqi_val == 3) {
        aqi_text = "POOR";
    } else if (aqi_val == 4) {
        aqi_text = "BAD";
    } else if (aqi_val == 5) {
        aqi_text = "VBAD";  // Very Bad abbreviated
    } else {
        aqi_text = "UNKN";  // Unknown
    }
    // Measure value
    int16_t aqi_value_x_off, aqi_value_y_off;
    uint16_t aqi_value_w, aqi_value_h;
    display.getTextBounds(aqi_text, 0, 0, &aqi_value_x_off, &aqi_value_y_off, &aqi_value_w, &aqi_value_h);
    int16_t aqi_value_top = padding + aqi_label_h + gap;
    int16_t aqi_value_x = aqi_label_x;       // left-aligned with label
    int16_t aqi_value_baseline_y = aqi_value_top - aqi_value_y_off;
    // Draw value
    display.setCursor(aqi_value_x, aqi_value_baseline_y);
    display.print(aqi_text);

    // Compute where AQI value ends, so we can start grid below it
    int16_t aqi_value_top_px    = aqi_value_baseline_y + aqi_value_y_off; // top of AQI text box
    int16_t aqi_value_bottom_px = aqi_value_top_px + aqi_value_h;         // bottom of AQI text box
    // Start Y for the grid (a bit below AQI text)
    const int16_t grid_top = aqi_value_bottom_px + 18;
    // Inner area inside right section (respect padding on left/right)
    const int16_t right_inner_x = right_section_x + padding;
    const int16_t right_inner_w = right_section_w - 2 * padding;
    const int16_t col_gap = 12;
    const int16_t col_w   = (right_inner_w - col_gap) / 2;
    const int16_t col_left_x  = right_inner_x;
    const int16_t col_right_x = right_inner_x + col_w + col_gap;
    // Vertical spacing between first and second row
    const int16_t row_gap = 18;  // between top value and next label

    
    // HUM
    {
        display.setTextSize(1);
        snprintf(line, sizeof(line), "HUM");
        int16_t lbl_x_off, lbl_y_off;
        uint16_t lbl_w, lbl_h;
        display.getTextBounds(line, 0, 0, &lbl_x_off, &lbl_y_off, &lbl_w, &lbl_h);

        int16_t hum_label_x = col_left_x;
        int16_t hum_label_baseline_y = grid_top - lbl_y_off;
        display.setCursor(hum_label_x, hum_label_baseline_y);
        display.print(line);

        display.setTextSize(2);
        if (isnan(data->humidity)) {
            snprintf(line, sizeof(line), "--.-%%");
        } else {
            snprintf(line, sizeof(line), "%.1f%%", data->humidity);
        }

        int16_t val_x_off, val_y_off;
        uint16_t val_w, val_h;
        display.getTextBounds(line, 0, 0, &val_x_off, &val_y_off, &val_w, &val_h);

        int16_t hum_value_top = grid_top + lbl_h + 4; // small gap below label
        int16_t hum_value_x   = col_left_x;
        int16_t hum_value_baseline_y = hum_value_top - val_y_off;
        display.setCursor(hum_value_x, hum_value_baseline_y);
        display.print(line);
    }

    // TVOC
    {
        display.setTextSize(1);
        snprintf(line, sizeof(line), "TVOC");
        int16_t lbl_x_off, lbl_y_off;
        uint16_t lbl_w, lbl_h;
        display.getTextBounds(line, 0, 0, &lbl_x_off, &lbl_y_off, &lbl_w, &lbl_h);

        int16_t tvoc_label_x = col_right_x;
        int16_t tvoc_label_baseline_y = grid_top - lbl_y_off;
        display.setCursor(tvoc_label_x, tvoc_label_baseline_y);
        display.print(line);

        display.setTextSize(2);
        if (data->tvoc == 0) {
            snprintf(line, sizeof(line), "--ppb");
        } else {
            snprintf(line, sizeof(line), "%uppb", data->tvoc);
        }

        int16_t val_x_off, val_y_off;
        uint16_t val_w, val_h;
        display.getTextBounds(line, 0, 0, &val_x_off, &val_y_off, &val_w, &val_h);

        int16_t tvoc_value_top = grid_top + lbl_h + 4;
        int16_t tvoc_value_x   = col_right_x;
        int16_t tvoc_value_baseline_y = tvoc_value_top - val_y_off;
        display.setCursor(tvoc_value_x, tvoc_value_baseline_y);
        display.print(line);
    }

    // For second row, compute a baseline Y just under the first row’s values.
    // We can use HUM value metrics as reference:
    int16_t first_row_val_top    = grid_top + /* lbl_h */ 10 + 4; // approx; or reuse from HUM calc
    int16_t second_row_label_top = first_row_val_top + 20 + row_gap;

    // eCO2
    {
        display.setTextSize(1);
        snprintf(line, sizeof(line), "ECO2");
        int16_t lbl_x_off, lbl_y_off;
        uint16_t lbl_w, lbl_h;
        display.getTextBounds(line, 0, 0, &lbl_x_off, &lbl_y_off, &lbl_w, &lbl_h);

        int16_t eco2_label_x = col_left_x;
        int16_t eco2_label_baseline_y = second_row_label_top - lbl_y_off;
        display.setCursor(eco2_label_x, eco2_label_baseline_y);
        display.print(line);

        display.setTextSize(2);
        if (data->co2 == 0) {
            snprintf(line, sizeof(line), "--ppm");
        } else {
            snprintf(line, sizeof(line), "%uppm", data->co2);
        }

        int16_t val_x_off, val_y_off;
        uint16_t val_w, val_h;
        display.getTextBounds(line, 0, 0, &val_x_off, &val_y_off, &val_w, &val_h);

        int16_t eco2_value_top = second_row_label_top + lbl_h + 4;
        int16_t eco2_value_x   = col_left_x;
        int16_t eco2_value_baseline_y = eco2_value_top - val_y_off;
        display.setCursor(eco2_value_x, eco2_value_baseline_y);
        display.print(line);
    }

    // MATTER / CONNECTED
    {
        display.setTextSize(1);
        snprintf(line, sizeof(line), "MATTER");
        int16_t lbl_x_off, lbl_y_off;
        uint16_t lbl_w, lbl_h;
        display.getTextBounds(line, 0, 0, &lbl_x_off, &lbl_y_off, &lbl_w, &lbl_h);

        int16_t matter_label_x = col_right_x;
        int16_t matter_label_baseline_y = second_row_label_top - lbl_y_off;
        display.setCursor(matter_label_x, matter_label_baseline_y);
        display.print(line);

        display.setTextSize(2);
        const char* matter_text = data->matter_connected ? "CONN" : "DISC";

        int16_t val_x_off, val_y_off;
        uint16_t val_w, val_h;
        display.getTextBounds(matter_text, 0, 0, &val_x_off, &val_y_off, &val_w, &val_h);

        int16_t matter_value_top = second_row_label_top + lbl_h + 4;
        int16_t matter_value_x   = col_right_x;
        int16_t matter_value_baseline_y = matter_value_top - val_y_off;
        display.setCursor(matter_value_x, matter_value_baseline_y);
        display.print(matter_text);
    }

    display.update();
}

/**
 * @brief E-paper display update task
 * 
 * @param pvParameters Task parameters (unused)
 */
static void epaper_task(void *pvParameters) {
    epaper_update_t update_data;
    
    ESP_LOGI(TAG, "E-paper display task started");
    
    while (1) {
        // Wait for update request from queue
        if (xQueueReceive(epaper_queue, &update_data, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Processing display update request");
            epaper_update_internal(&update_data);
            ESP_LOGI(TAG, "Display update complete");
        }
    }
}

/**
 * @brief Helper function to draw centered tex
 * 
 * @param text 
 * @param y 
 * @param size 
 */
static void draw_centered(const char *text, int16_t y, uint8_t size) {
    display.setTextSize(size);
    int16_t x, yoff;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x, &yoff, &w, &h);
    int16_t cx = (display.width() - w) / 2;
    display.setCursor(cx, y);
    display.print(text);
}

static void draw_centered_box(const char *text, int16_t x0, int16_t box_w, int16_t y, uint8_t size) {
    display.setTextSize(size);
    int16_t x, yoff;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x, &yoff, &w, &h);
    int16_t cx = x0 + (box_w - w) / 2;
    display.setCursor(cx, y);
    display.print(text);
}

/**
 * @brief Initialize the e-paper display using CalEPD library
 * 
 * Configure your e-paper by:
 * 1. Selecting the correct display model header above
 * 2. Setting GPIO pins via menuconfig
 * 
 * @return esp_err_t 
 */
esp_err_t epaper_init(void) {
    ESP_LOGI(TAG, "Initializing e-paper display with CalEPD library");
    ESP_LOGI(TAG, "CalEPD version: %s\n", CALEPD_VERSION);
    
    display.init();
    display.setRotation(3);
    display.fast_mode = true;

    display.setFont(&Seven_Segment9pt7b);
    display.fillScreen(EPD_WHITE);
    display.setTextColor(EPD_BLACK);

    epd_initialized = true;
    ESP_LOGI(TAG, "E-paper display initialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Start the e-paper display update task
 * 
 * @return esp_err_t 
 */
esp_err_t epaper_start_task(void) {
    if (!epd_initialized) {
        ESP_LOGE(TAG, "Display not initialized, call epaper_init() first");
        return ESP_FAIL;
    }
    
    if (epaper_task_handle != NULL) {
        ESP_LOGW(TAG, "E-paper task already running");
        return ESP_OK;
    }
    
    // Create queue for display update requests (depth of 1, only keep latest)
    epaper_queue = xQueueCreate(1, sizeof(epaper_update_t));
    if (epaper_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create e-paper queue");
        return ESP_FAIL;
    }
    
    // Create display update task
    BaseType_t ret = xTaskCreate(
        epaper_task,
        "epaper_task",
        4096,  // Stack size
        NULL,
        5,     // Priority
        &epaper_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create e-paper task");
        vQueueDelete(epaper_queue);
        epaper_queue = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "E-paper display task started successfully");
    return ESP_OK;
}

/**
 * @brief Request a non-blocking display update
 * 
 * @param sensor_manager Pointer to SensorManager
 * @param matter_connected Matter connection status
 */
void epaper_request_update(const SensorManager* sensor_manager, bool matter_connected) {
    if (!epd_initialized) {
        ESP_LOGW(TAG, "E-paper display not initialized");
        return;
    }
    if (!sensor_manager) {
        ESP_LOGW(TAG, "Sensor manager is null");
        return;
    }
    if (epaper_queue == NULL) {
        ESP_LOGW(TAG, "E-paper task not started, call epaper_start_task() first");
        return;
    }
    
    // Fetch sensor data
    const SHT40Sensor* sht  = sensor_manager->getSHT40Sensor();
    const ENS160Sensor* ens = sensor_manager->getENS160Sensor();
    
    epaper_update_t update_data = {
        .temperature = (sht && sht->isInitialized()) ? sht->getTemperature() : NAN,
        .humidity = (sht && sht->isInitialized()) ? sht->getHumidity() : NAN,
        .aqi = (uint16_t)((ens && ens->isInitialized()) ? ens->getAQI() : 0),
        .tvoc = (uint16_t)((ens && ens->isInitialized()) ? ens->getTVOCppb() : 0),
        .co2 = (uint16_t)((ens && ens->isInitialized()) ? ens->getECO2ppm() : 0),
        .matter_connected = matter_connected
    };
    
    // Send to queue (overwrite if queue is full to always have latest data)
    if (xQueueOverwrite(epaper_queue, &update_data) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue display update request");
    }
}

/**
 * @brief Update the e-paper display with sensor data (blocking)
 * 
 * @param sensor_manager Pointer to SensorManager
 * @param matter_connected Matter connection status
 */
void epaper_update_display(const SensorManager* sensor_manager, bool matter_connected) {
    if (!epd_initialized) {
        ESP_LOGW(TAG, "E-paper display not initialized");
        return;
    }
    if (!sensor_manager) {
        ESP_LOGW(TAG, "Sensor manager is null");
        return;
    }
    
    ESP_LOGI(TAG, "Updating e-paper display with sensor data (blocking)");
    
    // Fetch sensor data
    const SHT40Sensor* sht = sensor_manager->getSHT40Sensor();
    const ENS160Sensor* ens = sensor_manager->getENS160Sensor();
    
    epaper_update_t update_data = {
        .temperature = (sht && sht->isInitialized()) ? sht->getTemperature() : NAN,
        .humidity = (sht && sht->isInitialized()) ? sht->getHumidity() : NAN,
        .aqi = (uint16_t)((ens && ens->isInitialized()) ? ens->getAQI() : 0),
        .tvoc = (uint16_t)((ens && ens->isInitialized()) ? ens->getTVOCppb() : 0),
        .co2 = (uint16_t)((ens && ens->isInitialized()) ? ens->getECO2ppm() : 0),
        .matter_connected = matter_connected
    };
    
    // Use the shared internal function
    epaper_update_internal(&update_data);
    ESP_LOGI(TAG, "E-paper display updated");
}

/**
 * @brief Clear the e-paper display to white
 */
void epaper_clear_display(void) {
    if (!epd_initialized) {
        ESP_LOGW(TAG, "E-paper display not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Clearing e-paper display");
    
    // TODO: Implement with CalEPD
    // display.fillScreen(GxEPD_WHITE);
    // display.update();
}

/**
 * @brief Put the e-paper display into deep sleep mode
 */
void epaper_sleep(void) {
    if (!epd_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Putting e-paper display to sleep");
    
    // TODO: Implement with CalEPD
    // display.powerDown();
}

/**
 * @brief Display a splash screen on the e-paper display
 */
void epaper_display_splash(void) {
    if (!epd_initialized) {
        ESP_LOGW(TAG, "E-paper display not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Testing e-paper display - displaying test pattern");
    
    display.fillScreen(EPD_WHITE);
    display.setTextColor(EPD_BLACK);

    const char *line1 = "AURA CLIMATE";
    const char *line2 = "BY 48 STUDIOS";

    // Line 1: size 3 – compute width/height with current font
    display.setTextSize(3);
    int16_t x1, y1;
    uint16_t w1, h1;
    display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    int16_t cx1 = (display.width() - w1) / 2;
    int16_t cy1 = (display.height() / 2);  // move up by its own height for balance
    display.setCursor(cx1, cy1);
    display.print(line1);

    // Line 2: size 2 – recompute bounds after changing size
    display.setTextSize(2);
    int16_t x2, y2;
    uint16_t w2, h2;
    display.getTextBounds(line2, 0, 0, &x2, &y2, &w2, &h2);
    int16_t cx2 = (display.width() - w2) / 2;
    int16_t cy2 = cy1 + h1;  // place below line1 with small gap
    display.setCursor(cx2, cy2);
    display.print(line2);

    display.update();
    
    ESP_LOGI(TAG, "Display test complete");
}
