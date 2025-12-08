#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "epaper_manager.hpp"
#include "goodisplay/gdeq037T31.h"

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
    display.update();

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
 * @brief Test e-paper display with simple pattern
 */
void epaper_test_display(void) {
    if (!epd_initialized) {
        ESP_LOGW(TAG, "E-paper display not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Testing e-paper display - displaying test pattern");
    
    display.fillScreen(EPD_BLACK);
    display.setTextColor(EPD_WHITE);
    display.setTextSize(2);
    display.print("Hello CalEPD!");
    display.update();
    
    ESP_LOGI(TAG, "Display test complete");
}
