#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_matter.h>
#include <driver/gpio.h>
#include "variables.hpp"
#include "sensors.hpp"

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

typedef void *driver_handle;

/**
 * @brief Start a non-blocking identification pulse on the physical switch.
 *
 */
void driver_identify_pulse(uint16_t endpoint_id);

/**
 * @brief Stop any running identification pulse.
 * 
 */
void driver_identify_stop(void);

/**
 * @brief Update Matter attributes with current sensor values
 * 
 * @param sensor_manager Pointer to the sensor manager instance
 */
void update_matter_with_sensor_values(const SensorManager* sensor_manager);

/**
 * @brief Initialize the Air Quality Instance
 * 
 * @param endpoint_id The endpoint ID for the air quality sensor
 * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t driver_air_quality_init(uint16_t endpoint_id);

/**
 * @brief Initialize the button driver
 * 
 * @return driver_handle 
 */
driver_handle driver_button_init(void);