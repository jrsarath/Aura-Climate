#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <i2cdev.h>
#include <sht4x.h>

#include "includes/sensors.hpp"
#include "includes/variables.hpp"
#include "includes/driver.hpp"

static const char *TAG = "sensors";

// SHT40 Sensor Implementation
SHT40Sensor::SHT40Sensor()
    : SensorBase("SHT40"), temperature(0.0f), humidity(0.0f) {
    memset(&sht_dev, 0, sizeof(sht_dev));
}

/**
 * @brief Initialize the SHT40 sensor.
 *
 * @return esp_err_t
 */
esp_err_t SHT40Sensor::initialize() {
    ESP_LOGI(TAG, "Initializing %s sensor on I2C", sensor_name);
    esp_err_t ret = sht4x_init_desc(&sht_dev, (i2c_port_t)0, (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SDA, (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SCL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SHT40 descriptor: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = sht4x_init(&sht_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SHT40: %s", esp_err_to_name(ret));
        return ret;
    }

    // Apply configured heater and repeatability modes
    sht_dev.heater = SHT4X_HEATER_MODE;
    sht_dev.repeatability = SHT4X_REPEATABILITY;

    initialized = true;
    return ESP_OK;
}

/**
 * @brief Read data from the SHT40 sensor.
 *
 * @return esp_err_t
 */
esp_err_t SHT40Sensor::read() {
    if (!initialized) {
        ESP_LOGE(TAG, "%s sensor not initialized", sensor_name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    float temp_reading = 0.0f;
    float humid_reading = 0.0f;

    for (int i = 0; i < SHT4X_ERROR_RETRY_COUNT; i++) {
        ret = sht4x_measure(&sht_dev, &temp_reading, &humid_reading);
        if (ret == ESP_OK) {
            if (temp_reading >= SHT4X_MIN_TEMPERATURE && temp_reading <= SHT4X_MAX_TEMPERATURE &&
                humid_reading >= SHT4X_MIN_HUMIDITY && humid_reading <= SHT4X_MAX_HUMIDITY) {
                temperature = temp_reading;
                humidity = humid_reading;
                error_count = 0;
                return ESP_OK;
            }
        }
        error_count++;
        ESP_LOGW(TAG, "%s reading attempt %d failed", sensor_name, i + 1);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_RETRY_DELAY_MS));
    }

    ESP_LOGE(TAG, "%s reading failed after %d attempts", sensor_name, SHT4X_ERROR_RETRY_COUNT);
    return ESP_FAIL;
}

/**
 * @brief Reset the SHT40 sensor readings and error count.
 *
 */
void SHT40Sensor::reset() {
    temperature = 0.0f;
    humidity = 0.0f;
    error_count = 0;
}

/**
 * @brief Validate the SHT40 sensor readings.
 *
 */
bool SHT40Sensor::validateReading() const {
    return temperature >= SHT4X_MIN_TEMPERATURE && temperature <= SHT4X_MAX_TEMPERATURE &&
           humidity >= SHT4X_MIN_HUMIDITY && humidity <= SHT4X_MAX_HUMIDITY;
}

// ENS160 Sensor Implementation
// Register map helpers
static constexpr uint8_t ENS160_REG_OPMODE      = 0x10;
static constexpr uint8_t ENS160_REG_COMMAND     = 0x12;
static constexpr uint8_t ENS160_REG_STATUS      = 0x20;
static constexpr uint8_t ENS160_REG_DATA_AQI    = 0x21;
static constexpr uint8_t ENS160_REG_DATA_TVOC   = 0x22;
static constexpr uint8_t ENS160_REG_DATA_ECO2   = 0x24;

static constexpr uint8_t ENS160_OPMODE_RESET    = 0xF0;
static constexpr uint8_t ENS160_OPMODE_IDLE     = 0x01;
static constexpr uint8_t ENS160_OPMODE_STANDARD = 0x02;

static constexpr uint8_t ENS160_CMD_NORMAL      = 0x00;
static constexpr uint8_t ENS160_CMD_CLEAR_GPR   = 0xCC;

ENS160Sensor::ENS160Sensor()
    : SensorBase("ENS160"), aqi(0), tvoc_ppb(0), eco2_ppm(0) {
    memset(&dev, 0, sizeof(dev));
}

esp_err_t ENS160Sensor::write_reg(uint8_t reg, uint8_t value) {
    return i2c_dev_write_reg(&dev, reg, &value, 1);
}

esp_err_t ENS160Sensor::read_reg(uint8_t reg, uint8_t* value) {
    return i2c_dev_read_reg(&dev, reg, value, 1);
}

esp_err_t ENS160Sensor::read_word(uint8_t reg, uint16_t* value) {
    uint8_t buf[2] = {0};
    esp_err_t ret = i2c_dev_read_reg(&dev, reg, buf, 2);
    if (ret == ESP_OK) {
        *value = (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
    }
    return ret;
}

esp_err_t ENS160Sensor::wait_data_ready(uint32_t timeout_ms) {
    const uint64_t start = esp_timer_get_time();
    while (true) {
        uint8_t status = 0;
        esp_err_t ret = read_reg(ENS160_REG_STATUS, &status);
        if (ret != ESP_OK) {
            return ret;
        }
        if (status & BIT(1)) { // new_data bit
            return ESP_OK;
        }
        if ((esp_timer_get_time() - start) / 1000 >= timeout_ms) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Initialize the ENS160 sensor.
 *
 * @return esp_err_t
 */
esp_err_t ENS160Sensor::initialize() {
    ESP_LOGI(TAG, "Initializing %s sensor on I2C", sensor_name);
    esp_err_t ret = ESP_OK;
    dev.port = I2C_NUM_0;
    dev.addr = ENS160_I2C_ADDR;
    dev.cfg.sda_io_num = (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SDA;
    dev.cfg.scl_io_num = (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SCL;
#if HELPER_TARGET_IS_ESP32
    dev.cfg.master.clk_speed = 100000; // 100kHz
#endif
    ret = i2c_dev_create_mutex(&dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C mutex for ENS160: %s", esp_err_to_name(ret));
        return ret;
    }

    // Reset then set to standard mode
    ESP_RETURN_ON_ERROR(write_reg(ENS160_REG_OPMODE, ENS160_OPMODE_RESET), TAG, "ENS160 reset failed");
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_RETURN_ON_ERROR(write_reg(ENS160_REG_OPMODE, ENS160_OPMODE_IDLE), TAG, "ENS160 idle set failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_RETURN_ON_ERROR(write_reg(ENS160_REG_COMMAND, ENS160_CMD_NORMAL), TAG, "ENS160 command normal failed");
    ESP_RETURN_ON_ERROR(write_reg(ENS160_REG_COMMAND, ENS160_CMD_CLEAR_GPR), TAG, "ENS160 clear GPR failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(write_reg(ENS160_REG_COMMAND, ENS160_CMD_NORMAL), TAG, "ENS160 command normal (post-clear) failed");
    vTaskDelay(pdMS_TO_TICKS(5));

    ESP_RETURN_ON_ERROR(write_reg(ENS160_REG_OPMODE, ENS160_OPMODE_STANDARD), TAG, "ENS160 standard mode failed");
    vTaskDelay(pdMS_TO_TICKS(25));

    initialized = true;
    return ESP_OK;
}

/**
 * @brief Read data from the ENS160 sensor.
 *
 * @return esp_err_t
 */
esp_err_t ENS160Sensor::read() {
    if (!initialized) {
        ESP_LOGE(TAG, "%s sensor not initialized", sensor_name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wait_data_ready(ENS160_DATA_POLL_TIMEOUT_MS);
    if (ret != ESP_OK) {
        error_count++;
        ESP_LOGW(TAG, "%s data not ready or error: %s", sensor_name, esp_err_to_name(ret));
        return ret;
    }

    uint8_t aqi_read = 0;
    uint16_t tvoc_read = 0;
    uint16_t eco2_read = 0;

    ret = read_reg(ENS160_REG_DATA_AQI, &aqi_read);
    if (ret != ESP_OK) goto read_fail;
    ret = read_word(ENS160_REG_DATA_TVOC, &tvoc_read);
    if (ret != ESP_OK) goto read_fail;
    ret = read_word(ENS160_REG_DATA_ECO2, &eco2_read);
    if (ret != ESP_OK) goto read_fail;

    if (aqi_read >= ENS160_MIN_AQI && aqi_read <= ENS160_MAX_AQI &&
        tvoc_read <= ENS160_MAX_TVOC_PPB && tvoc_read >= ENS160_MIN_TVOC_PPB &&
        eco2_read <= ENS160_MAX_ECO2_PPM && eco2_read >= ENS160_MIN_ECO2_PPM) {
        aqi = aqi_read;
        tvoc_ppb = tvoc_read;
        eco2_ppm = eco2_read;
        error_count = 0;
        return ESP_OK;
    }

read_fail:
    error_count++;
    ESP_LOGW(TAG, "%s reading failed (%s)", sensor_name, esp_err_to_name(ret));
    return ret == ESP_OK ? ESP_FAIL : ret;
}

void ENS160Sensor::reset() {
    aqi = 0;
    tvoc_ppb = 0;
    eco2_ppm = 0;
    error_count = 0;
}

bool ENS160Sensor::validateReading() const {
    return aqi >= ENS160_MIN_AQI && aqi <= ENS160_MAX_AQI &&
           tvoc_ppb >= ENS160_MIN_TVOC_PPB && tvoc_ppb <= ENS160_MAX_TVOC_PPB &&
           eco2_ppm >= ENS160_MIN_ECO2_PPM && eco2_ppm <= ENS160_MAX_ECO2_PPM;
}

// Sensor Manager Implementation
SensorManager::SensorManager() : sht_sensor(nullptr), ens_sensor(nullptr), running(false), task_handle(nullptr) {}

/** 
 * @brief Destructor to clean up sensors and stop readings.
 * 
 */
SensorManager::~SensorManager() {
    stopReadings();
    delete sht_sensor;
    delete ens_sensor;
}

/** 
 * @brief Initialize all sensors.
 * 
 */
esp_err_t SensorManager::initialize() {
    // Initialize I2C bus once for all sensors
    ESP_LOGI(TAG, "Initializing I2C bus on SDA=%d, SCL=%d", CONFIG_GPIO_I2C_MASTER_SDA, CONFIG_GPIO_I2C_MASTER_SCL);
    esp_err_t ret = i2cdev_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    sht_sensor = new SHT40Sensor();
    // ens_sensor = new ENS160Sensor();

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
 * @brief Task to periodically read sensors and update Matter attributes.
 * 
 */
void SensorManager::readingTask(void* parameters) {
    SensorManager* manager = static_cast<SensorManager*>(parameters);
    uint32_t last_matter_update = 0;
    
    while (manager->running) {
        uint32_t current_time = esp_timer_get_time() / 1000;  // Convert to ms
        esp_err_t sht_ret = manager->sht_sensor->read();
        esp_err_t ens_ret = manager->ens_sensor->read();

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

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }

    vTaskDelete(NULL);
}

/**
 * @brief Start the sensor reading task.
 * 
 * @return esp_err_t 
 */
esp_err_t SensorManager::startReadings() {
    if (running) {
        return ESP_OK;
    }

    running = true;
    BaseType_t ret = xTaskCreate(
        readingTask,
        "sensor_task",
        SYSTEM_TASK_STACK_SIZE,
        this,
        SYSTEM_TASK_PRIORITY,
        &task_handle
    );

    if (ret != pdPASS) {
        running = false;
        ESP_LOGE(TAG, "Failed to create sensor reading task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Stop the sensor reading task.
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