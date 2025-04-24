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
#include <app_reset.h>
#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include "includes/driver.h"
#include "includes/sensors.h"
#include "includes/variables.h"
#include "includes/config.h"
#include "includes/ota_manager.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "matter";
static SensorManager* sensor_manager = nullptr;

// Matter callbacks
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address Changed");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Commissioning complete");
            device_commission_window_close_cb();
            break;

        case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
            ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
            device_commission_window_close_cb();
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
            ESP_LOGI(TAG, "Commissioning session started");
            device_commission_window_open_cb();
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
            ESP_LOGI(TAG, "Commissioning session stopped");
            device_commission_window_close_cb();
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
            ESP_LOGI(TAG, "Commissioning window opened");
            device_commission_window_open_cb();
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
            ESP_LOGI(TAG, "Commissioning window closed");
            device_commission_window_close_cb();
            break;

        default:
            break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, 
                                     uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    device_identifier_cb();
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(callback_type_t type, uint16_t endpoint_id, 
                                       uint32_t cluster_id, uint32_t attribute_id,
                                       esp_matter_attr_val_t *val, void *priv_data) {
    if (type == PRE_UPDATE) {
        ESP_LOGI(TAG, "Attribute pre-update - endpoint: %u, cluster: %lu, attribute: %lu",
                 endpoint_id, cluster_id, attribute_id);
    }
    return ESP_OK;
}

static esp_err_t initialize_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

extern "C" void app_main() {
    esp_err_t err = ESP_OK;

    // Initialize NVS
    err = initialize_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
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
    ESP_LOGI(TAG, "OTA manager initialized, running version: %s", 
             OTAManager::getInstance().getCurrentVersion());

    // Initialize sensor manager
    sensor_manager = new SensorManager();
    err = sensor_manager->initialize();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor manager");
        return;
    }

    // Initialize button with sensor manager
    driver_handle button_handle = driver_button_init(sensor_manager);
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
    temperature_config.temperature_measurement.measured_value = sensor_manager->getDHTSensor()->getTemperature();
    endpoint_t *temperature_endpoint = temperature_sensor::create(node, &temperature_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!temperature_endpoint) {
        ESP_LOGE(TAG, "Failed to create temperature endpoint");
        return;
    }
    temperature_endpoint_id = endpoint::get_id(temperature_endpoint);
    ESP_LOGI(TAG, "Temperature endpoint created with ID %d", temperature_endpoint_id);

    // Configure Humidity Sensor
    humidity_sensor::config_t humidity_config;
    humidity_config.relative_humidity_measurement.measured_value = sensor_manager->getDHTSensor()->getHumidity();
    endpoint_t *humidity_endpoint = humidity_sensor::create(node, &humidity_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!humidity_endpoint) {
        ESP_LOGE(TAG, "Failed to create humidity endpoint");
        return;
    }
    humidity_endpoint_id = endpoint::get_id(humidity_endpoint);
    ESP_LOGI(TAG, "Humidity endpoint created with ID %d", humidity_endpoint_id);

    // Configure Air Quality Sensor
    air_quality_sensor::config_t voc_config;
    endpoint_t *voc_endpoint = air_quality_sensor::create(node, &voc_config, ENDPOINT_FLAG_NONE, nullptr);
    if (!voc_endpoint) {
        ESP_LOGE(TAG, "Failed to create air quality endpoint");
        return;
    }
    voc_endpoint_id = endpoint::get_id(voc_endpoint);
    ESP_LOGI(TAG, "AQI endpoint created with ID %d", voc_endpoint_id);

    #if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        esp_openthread_platform_config_t config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        };
        set_openthread_platform_config(&config);
    #endif

    // Start Matter
    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter: %s", esp_err_to_name(err));
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
        esp_matter::console::init();
    #endif

    ESP_LOGI(TAG, "Matter device initialized successfully");
}