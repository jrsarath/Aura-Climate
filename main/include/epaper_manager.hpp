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
 * @brief Start the e-paper display update task
 * 
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t epaper_start_task(void);

/**
 * @brief Request a non-blocking display update (queues the request)
 * 
 * @param sensor_manager Pointer to the SensorManager instance
 * @param matter_connected Matter connection status (true if connected)
 */
void epaper_request_update(const SensorManager* sensor_manager, bool matter_connected = false);

/**
 * @brief Update e-paper display with current sensor data (blocking)
 * 
 * @param sensor_manager Pointer to the SensorManager instance
 * @param matter_connected Matter connection status (true if connected)
 * @note This is a blocking call. Use epaper_request_update() for non-blocking updates.
 */
void epaper_update_display(const SensorManager* sensor_manager, bool matter_connected = false);

/**
 * @brief Clear the e-paper display to white
 */
void epaper_clear_display(void);

/**
 * @brief Put e-paper display into deep sleep mode
 */
void epaper_sleep(void);

/**
 * @brief Display a splash screen on the e-paper display
 */
void epaper_display_splash(void);

#endif
