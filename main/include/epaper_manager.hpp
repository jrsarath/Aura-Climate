#ifndef EPAPER_MANAGER_HPP
#define EPAPER_MANAGER_HPP

#include "sensors.hpp"

/**
 * @brief Initialize e-paper display with configured pins
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t epaper_init(void);

/**
 * @brief Update e-paper display with current sensor data
 * 
 * @param sensor_manager Pointer to the SensorManager instance
 */
void epaper_update_display(const SensorManager* sensor_manager);

/**
 * @brief Clear the e-paper display to white
 */
void epaper_clear_display(void);

/**
 * @brief Put e-paper display into deep sleep mode
 */
void epaper_sleep(void);

/**
 * @brief Test e-paper display with simple pattern
 */
void epaper_test_display(void);

#endif
