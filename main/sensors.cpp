#include <string.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sht4x.h>
#include <ens160.h>


#include "sensors.hpp"
#include "variables.hpp"
#include "driver.hpp"
#include "epaper_manager.hpp"
#include "utils.hpp"

static const char* TAG = "SENSORS";

// Shared I2C master bus for all I2C sensors on this board
i2c_master_bus_handle_t i2c0_bus_hdl = nullptr;

/**
 * @brief Initialize the I2C master bus for sensor communication.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
static esp_err_t init_i2c_bus() {
    if (i2c0_bus_hdl != nullptr) {
        return ESP_OK;  // already initialized
    }

    ESP_LOGI(TAG, "Initializing I2C master bus on SDA=%d, SCL=%d",
             CONFIG_GPIO_I2C_MASTER_SDA, CONFIG_GPIO_I2C_MASTER_SCL);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = static_cast<gpio_num_t>(CONFIG_GPIO_I2C_MASTER_SDA),
        .scl_io_num        = static_cast<gpio_num_t>(CONFIG_GPIO_I2C_MASTER_SCL),
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority     = 0,
        .trans_queue_depth = 0,
        .flags = {
            // true if you rely on internal pullups, false if you have external resistors
            .enable_internal_pullup = 1,
        },
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c0_bus_hdl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        i2c0_bus_hdl = nullptr;
    }
    return ret;
}

/** 
 * @brief Update Matter attributes with current sensor readings.
 * 
 */
SHT40Sensor::SHT40Sensor()
    : SensorBase("SHT40"), temperature(0.0f), humidity(0.0f), sht_handle(nullptr) {}

/**
 * @brief Destroy the SHT40Sensor::SHT40Sensor object
 * 
 */
SHT40Sensor::~SHT40Sensor() {
    if (sht_handle) {
        sht4x_delete(sht_handle);
        sht_handle = nullptr;
    }
}

/**
 * @brief Initialize the SHT40 sensor.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t SHT40Sensor::initialize() {
    ESP_LOGI(TAG, "Initializing %s via esp_sht4x", sensor_name);

    if (i2c0_bus_hdl == nullptr) {
        ESP_LOGE(TAG, "i2c0_bus_hdl not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Default configuration - avoid using macro due to field order issue
    sht4x_config_t cfg = {};
    cfg.i2c_address = I2C_SHT4X_DEV_ADDR_LO;
    cfg.i2c_clock_speed = I2C_SHT4X_DEV_CLK_SPD;
    cfg.repeat_mode = SHT4X_REPEAT_HIGH;
    cfg.heater_mode = SHT4X_HEATER_OFF;

    // If you want to override, you can define these in config.hpp:
    //   SHT4X_REPEAT_MODE (sht4x_repeat_modes_t)
    //   SHT4X_HEATER_MODE (sht4x_heater_modes_t)
    #ifdef SHT4X_REPEAT_MODE
        cfg.repeat_mode = SHT4X_REPEAT_MODE;
    #endif
    #ifdef SHT4X_HEATER_MODE
        cfg.heater_mode = SHT4X_HEATER_MODE;
    #endif

    sht4x_handle_t dev_hdl = nullptr;
    esp_err_t ret = sht4x_init(i2c0_bus_hdl, &cfg, &dev_hdl);
    if (ret != ESP_OK || dev_hdl == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize SHT40: %s", esp_err_to_name(ret));
        return (ret == ESP_OK) ? ESP_FAIL : ret;
    }

    sht_handle  = dev_hdl;
    initialized = true;
    return ESP_OK;
}

/**
 * @brief Read temperature and humidity from SHT40 sensor.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t SHT40Sensor::read() {
    if (!initialized || !sht_handle) {
        ESP_LOGE(TAG, "%s sensor not initialized", sensor_name);
        return ESP_ERR_INVALID_STATE;
    }

    float temp = 0.0f;
    float rh   = 0.0f;

    esp_err_t ret = sht4x_get_measurement(sht_handle, &temp, &rh);
    if (ret != ESP_OK) {
        error_count++;
        ESP_LOGW(TAG, "%s read failed: %s", sensor_name, esp_err_to_name(ret));
        return ret;
    }

    temperature = temp;
    humidity    = rh;
    error_count = 0;
    return ESP_OK;
}

/**
 * @brief Reset the SHT40 sensor readings and error count.
 * 
 */
void SHT40Sensor::reset() {
    temperature = 0.0f;
    humidity    = 0.0f;
    error_count = 0;
}

/**
 * @brief Validate the current SHT40 readings against defined ranges.
 * 
 * @return true if readings are valid, false otherwise.
 */
bool SHT40Sensor::validateReading() const {
    return temperature >= SHT4X_MIN_TEMPERATURE &&
           temperature <= SHT4X_MAX_TEMPERATURE &&
           humidity    >= SHT4X_MIN_HUMIDITY &&
           humidity    <= SHT4X_MAX_HUMIDITY;
}

/**
 * @brief Constructor for ENS160Sensor class.
 * 
 */
ENS160Sensor::ENS160Sensor()
    : SensorBase("ENS160"), aqi(0), tvoc_ppb(0), eco2_ppm(0), ens_handle(nullptr) {}

/**
 * @brief Destructor to clean up ENS160 resources.
 * 
 */
ENS160Sensor::~ENS160Sensor() {
    if (ens_handle) {
        ens160_delete(ens_handle);
        ens_handle = nullptr;
    }
}

/**
 * @brief Initialize the ENS160 air quality sensor.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t ENS160Sensor::initialize() {
    ESP_LOGI(TAG, "Initializing %s sensor using esp_ens160 driver", sensor_name);

    if (i2c0_bus_hdl == nullptr) {
        ESP_LOGE(TAG, "i2c0_bus_hdl not set; cannot init ENS160");
        return ESP_ERR_INVALID_STATE;
    }

    ens160_config_t dev_cfg = I2C_ENS160_CONFIG_DEFAULT;
    dev_cfg.i2c_address     = ENS160_I2C_ADDR;  // your board’s address macro

    ens160_handle_t dev_hdl = nullptr;
    esp_err_t ret = ens160_init(i2c0_bus_hdl, &dev_cfg, &dev_hdl);
    if (ret != ESP_OK || dev_hdl == nullptr) {
        ESP_LOGE(TAG, "ENS160 init failed: %s", esp_err_to_name(ret));
        return (ret == ESP_OK) ? ESP_FAIL : ret;
    }

    ens_handle  = dev_hdl;
    initialized = true;
    return ESP_OK;
}

/**
 * @brief Read air quality data from ENS160 sensor.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t ENS160Sensor::read() {
    if (!initialized || !ens_handle) {
        ESP_LOGE(TAG, "%s sensor not initialized", sensor_name);
        return ESP_ERR_INVALID_STATE;
    }

    ens160_validity_flags_t dev_flag = ENS160_VALFLAG_INVALID_OUTPUT;
    esp_err_t ret = ens160_get_validity_status(ens_handle, &dev_flag);
    if (ret != ESP_OK) {
        error_count++;
        ESP_LOGW(TAG, "%s status read failed: %s", sensor_name, esp_err_to_name(ret));
        return ret;
    }

    if (dev_flag == ENS160_VALFLAG_WARMUP || dev_flag == ENS160_VALFLAG_INITIAL_STARTUP) {
        error_count++;
        ESP_LOGW(TAG, "%s warming up (state=%u)", sensor_name, static_cast<unsigned>(dev_flag));
        return ESP_ERR_INVALID_STATE;
    }

    ens160_air_quality_data_t aq_data = {};
    ret = ens160_get_measurement(ens_handle, &aq_data);
    if (ret != ESP_OK) {
        error_count++;
        ESP_LOGW(TAG, "%s device read failed (%s)",
                 sensor_name, esp_err_to_name(ret));
        return ret;
    }

    // Optional range checks – adjust to taste or remove entirely
    if (aq_data.uba_aqi >= ENS160_MIN_AQI && aq_data.uba_aqi <= ENS160_MAX_AQI &&
        aq_data.eco2 >= ENS160_MIN_ECO2_PPM && aq_data.eco2 <= ENS160_MAX_ECO2_PPM) {

        aqi       = static_cast<uint8_t>(aq_data.uba_aqi);
        tvoc_ppb  = aq_data.tvoc;
        eco2_ppm  = aq_data.eco2;
        error_count = 0;
        return ESP_OK;
    }

    error_count++;
    ESP_LOGW(TAG, "%s reading out of range", sensor_name);
    return ESP_FAIL;
}

/**
 * @brief Reset ENS160 sensor readings and error count.
 * 
 */
void ENS160Sensor::reset() {
    aqi       = 0;
    tvoc_ppb  = 0;
    eco2_ppm  = 0;
    error_count = 0;
}

/**
 * @brief Validate the current ENS160 readings against defined ranges.
 * 
 * @return true if readings are valid, false otherwise.
 */
bool ENS160Sensor::validateReading() const {
    return aqi      >= ENS160_MIN_AQI && aqi      <= ENS160_MAX_AQI &&
           eco2_ppm >= ENS160_MIN_ECO2_PPM && eco2_ppm <= ENS160_MAX_ECO2_PPM;
}

/**
 * @brief Constructor for SensorManager class.
 * 
 */
SensorManager::SensorManager()
    : sht_sensor(nullptr), ens_sensor(nullptr), running(false), task_handle(nullptr) {}

/**
 * @brief Destructor to clean up resources.
 * 
 */
SensorManager::~SensorManager() {
    stopReadings();
    delete sht_sensor;
    delete ens_sensor;
}

/**
 * @brief Initialize all sensors managed by SensorManager.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t SensorManager::initialize() {
    // Single I2C master bus for all sensors
    esp_err_t ret = init_i2c_bus();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    sht_sensor = new SHT40Sensor();
    ens_sensor = new ENS160Sensor();

    ret = sht_sensor->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SHT40 sensor");
        return ret;
    }

    ret = ens_sensor->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ENS160 sensor");
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Sensor reading task function.
 * 
 * @param parameters Pointer to SensorManager instance.
 */
void SensorManager::readingTask(void* parameters) {
    auto* manager = static_cast<SensorManager*>(parameters);
    uint32_t last_matter_update = 0;
    uint32_t last_epaper_update = 0;

    while (manager->running) {
        const uint32_t current_time = esp_timer_get_time() / 1000;  // us → ms

        esp_err_t sht_ret = ESP_FAIL;
        esp_err_t ens_ret = ESP_FAIL;

        if (manager->sht_sensor) {
            sht_ret = manager->sht_sensor->read();
        }
        if (manager->ens_sensor) {
            ens_ret = manager->ens_sensor->read();
        }

        if (sht_ret == ESP_OK && manager->sht_sensor->validateReading()) {
            ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%",
                     manager->sht_sensor->getTemperature(),
                     manager->sht_sensor->getHumidity());
        } else {
            ESP_LOGW(TAG, "Failed to read SHT40 sensor or invalid reading");
        }

        if (ens_ret == ESP_OK && manager->ens_sensor->validateReading()) {
            ESP_LOGI(TAG, "AQI: %u, TVOC: %uppb, eCO2: %uppm",
                     manager->ens_sensor->getAQI(),
                     manager->ens_sensor->getTVOCppb(),
                     manager->ens_sensor->getECO2ppm());
        } else {
            ESP_LOGW(TAG, "Failed to read ENS160 sensor or invalid reading");
        }

        // Update Matter attributes only at the specified interval
        if (current_time - last_matter_update >= MATTER_UPDATE_INTERVAL_MS) {
            update_matter_with_sensor_values(manager);
            last_matter_update = current_time;
            ESP_LOGI(TAG, "Matter attributes updated");
        }

        // Update e-paper independently of Matter commissioning (non-blocking)
        const uint32_t epaper_interval_ms = CONFIG_EPD_UPDATE_INTERVAL * 1000; // seconds -> ms
        if (current_time - last_epaper_update >= epaper_interval_ms) {
            last_epaper_update = current_time;
            bool matter_connected = is_matter_connected();
            epaper_request_update(manager, matter_connected);
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }

    vTaskDelete(nullptr);
}

/**
 * @brief Start the sensor reading task.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t SensorManager::startReadings() {
    if (running) {
        return ESP_OK;
    }

    running = true;
    const BaseType_t ret = xTaskCreate(
        readingTask,
        "sensor_task",
        SYSTEM_TASK_STACK_SIZE,
        this,
        SYSTEM_TASK_PRIORITY,
        &task_handle
    );

    if (ret != pdPASS) {
        running = false;
        task_handle = nullptr;
        ESP_LOGE(TAG, "Failed to create sensor reading task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Stop the sensor reading task and clean up resources.
 * 
 */
void SensorManager::stopReadings() {
    if (!running) {
        return;
    }

    running = false;
    if (task_handle != nullptr) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
}
