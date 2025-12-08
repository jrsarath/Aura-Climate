#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_matter.h>
#include <esp_matter_ota.h>
#include <esp_matter_console.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>

#include "config.hpp"
#include "variables.hpp"
#include "driver.hpp"
#include "ota_manager.hpp"
#include "utils.hpp"
#include "epaper_manager.hpp"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "AURA";
static SensorManager* sensor_manager = nullptr;

/**
 * @brief Application event callback
 * 
 * @param event Pointer to the ChipDeviceEvent
 * @param arg   Argument passed during registration
 */
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address Changed");
            OTAManager::getInstance().setNetworkReady(true);
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Commissioning complete");
            argb_stop_commissioning();
            break;

        case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
            ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
            argb_stop_commissioning();
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
            ESP_LOGI(TAG, "Commissioning session started");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
            ESP_LOGI(TAG, "Commissioning session stopped");
            argb_stop_commissioning();
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
            ESP_LOGI(TAG, "Commissioning window opened");
            // Start non-blocking commissioning glow on GPIO 8 (single pixel)
            argb_start_commissioning(CONFIG_GPIO_INDICATOR_LED, 1);
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
            ESP_LOGI(TAG, "Commissioning window closed");
            argb_stop_commissioning();
            break;

        default:
            break;
    }
}

/**
 * @brief  Identification callback
 * 
 * @param type        Type of the identification event
 * @param endpoint_id Endpoint ID of the identified device
 * @param effect_id   Effect ID
 * @param effect_variant Effect variant
 * @param priv_data   Private data pointer
 */
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    if (type == identification::callback_type_t::START) {
        driver_identify_pulse(endpoint_id);
    } else if (type == identification::callback_type_t::STOP) {
        driver_identify_stop();
    }
    return ESP_OK;
}

/**
 * @brief Attribute update callback
 * 
 * @param type          Type of the callback (PRE_UPDATE/POST_UPDATE)
 * @param endpoint_id   Endpoint ID of the attribute
 * @param cluster_id    Cluster ID of the attribute
 * @param attribute_id  Attribute ID
 * @param val           Pointer to the attribute value
 * @param priv_data     Private data pointer
 */
static esp_err_t app_attribute_update_cb(callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data) {
    esp_err_t err = ESP_OK;
    if (type == PRE_UPDATE) {
        // Do Nothing
    }
    return err;
}

/**
 * @brief Application main entry point
 * 
 */
extern "C" void app_main() {
    esp_err_t err = ESP_OK;

    // Initialize NVS
    err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %d", err);
        return;
    }

    // Initialize OTA manager
    err = OTAManager::getInstance().initialize();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OTA manager");
        return;
    }
    // Enable automatic update checks
    OTAManager::getInstance().enableAutoCheck(true);
    ESP_LOGI(TAG, "OTA manager initialized, running version: %s", OTAManager::getInstance().getCurrentVersion());

     // Initialize e-paper display
    err = epaper_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize e-paper display");
        // Continue without e-paper display
    } else {
        // Test display with pattern
        epaper_test_display();
    }
    
    // Initialize sensor manager
    sensor_manager = new SensorManager();
    err = sensor_manager->initialize();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor manager");
        return;
    }

    // Initialize reset button
    driver_handle button_handle = driver_button_init();
    if (!button_handle) {
        ESP_LOGE(TAG, "Failed to initialize button");
        return;
    }
    app_reset_button_register(button_handle);

    // Create Matter node
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    // Configure Temperature Sensor
    temperature_sensor::config_t temperature_config;
    temperature_config.temperature_measurement.measured_value = sensor_manager->getSHT40Sensor()->getTemperature();
    endpoint_t *temperature_endpoint = temperature_sensor::create(node, &temperature_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!temperature_endpoint) {
        ESP_LOGE(TAG, "Failed to create temperature endpoint");
        return;
    }
    temperature_endpoint_id = endpoint::get_id(temperature_endpoint);
    ESP_LOGI(TAG, "Temperature endpoint created with ID %d", temperature_endpoint_id);

    // Configure Humidity Sensor
    humidity_sensor::config_t humidity_config;
    humidity_config.relative_humidity_measurement.measured_value = sensor_manager->getSHT40Sensor()->getHumidity();
    endpoint_t *humidity_endpoint = humidity_sensor::create(node, &humidity_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!humidity_endpoint) {
        ESP_LOGE(TAG, "Failed to create humidity endpoint");
        return;
    }
    humidity_endpoint_id = endpoint::get_id(humidity_endpoint);
    ESP_LOGI(TAG, "Humidity endpoint created with ID %d", humidity_endpoint_id);

    // Configure Air Quality Sensor
    air_quality_sensor::config_t air_quality_config;
    endpoint_t *air_quality_endpoint = air_quality_sensor::create(node, &air_quality_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!air_quality_endpoint) {
        ESP_LOGE(TAG, "Failed to create air quality endpoint");
        return;
    }
    air_quality_endpoint_id = endpoint::get_id(air_quality_endpoint);
    ESP_LOGI(TAG, "AQI endpoint created with ID %d", air_quality_endpoint_id);
    
    #if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        // Set OpenThread platform config
        esp_openthread_platform_config_t config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        };
        set_openthread_platform_config(&config);
    #endif

    // Start Matter
    err = esp_matter::start(app_event_cb);
    abort_on_failure(err == ESP_OK, TAG, "Failed to start Matter, err:%d", err);

    // Initialize Air Quality Instance for attribute management
    // This is required because the AirQuality attribute is managed internally by CHIP
    err = driver_air_quality_init(air_quality_endpoint_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Air Quality Instance");
        return;
    }

    // Start sensor readings
    err = sensor_manager->startReadings();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start sensor readings");
        return;
    }
    
    // Initial Matter attribute update
    update_matter_with_sensor_values(sensor_manager);

    #if CONFIG_ENABLE_CHIP_SHELL
        esp_matter::console::diagnostics_register_commands();
        esp_matter::console::wifi_register_commands();
        esp_matter::console::factoryreset_register_commands();
    #if CONFIG_OPENTHREAD_CLI
        esp_matter::console::otcli_register_commands();
    #endif
        esp_matter::console::init();
    #endif
}