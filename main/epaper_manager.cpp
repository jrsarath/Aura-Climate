#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "goodisplay/gdeq037T31.h"
#include "epaper_manager.hpp"
#include "FreeSans9pt7b.h"
#include "digital_79pt7b.h"


EpdSpi io;
Gdeq037T31 display(io);

static const char *TAG = "EPAPER MGR";
static bool epd_initialized = false;

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
    
    display.init(true);

    ESP_LOGI(TAG, "Display rotation: %d\n", display.getRotation());
    display.setRotation(3);
    ESP_LOGI(TAG, "Display rotation: %d\n", display.getRotation());

    display.fast_mode = true;

    display.setFont(&digital_79pt7b);
    display.fillScreen(EPD_WHITE);
    display.setTextColor(EPD_BLACK);

    epd_initialized = true;
    ESP_LOGI(TAG, "E-paper display initialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Update the e-paper display with sensor data
 * 
 * @param sensor_manager Pointer to SensorManager
 */
void epaper_update_display(const SensorManager* sensor_manager) {
    if (!epd_initialized) {
        ESP_LOGW(TAG, "E-paper display not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Updating e-paper display with sensor data");

    // TODO: Implement display update with CalEPD
    // Example:
    // display.setTextSize(1);
    // display.setTextColor(GxEPD_BLACK);
    // display.setCursor(0, 0);
    // display.println("Sensor Data:");
    // display.print("Temp: ");
    // display.println(sensor_manager->temperature);
    // display.update();

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
