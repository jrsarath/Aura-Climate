#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_matter.h>
#include "sensors.h"
#include "variables.h"
#include <driver/gpio.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

#define DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *driver_handle;
/**
 * @brief Initialize the button driver with sensor manager
 * 
 * @param sensor_manager Pointer to the sensor manager instance
 * @return driver_handle Handle to the button driver
 */
driver_handle driver_button_init(void* sensor_manager);

/**
 * @brief Update Matter attributes with current sensor values
 * 
 * @param sensor_manager Pointer to the sensor manager instance
 */
void update_matter_with_sensor_values(const SensorManager* sensor_manager);

/**
 * @brief Callback for device identification
 */
void device_identifier_cb(void);

/**
 * @brief Callback for commission window open event
 */
void device_commission_window_open_cb(void);

/**
 * @brief Callback for commission window close event
 */
void device_commission_window_close_cb(void);


#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                                           \
    {                                                                                   \
        .radio_mode = RADIO_MODE_NATIVE,                                                \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                                            \
    {                                                                                   \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,                              \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
#endif

#ifdef __cplusplus
}
#endif