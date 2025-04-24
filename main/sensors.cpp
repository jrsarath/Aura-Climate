#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <dht.h>
#include <sgp40.h>

#include "includes/sensors.h"
#include "includes/variables.h"
#include "includes/driver.h"

static const char *TAG = "sensors";

// DHT Sensor Implementation
DHTSensor::DHTSensor(gpio_num_t pin) 
    : SensorBase("DHT22"), gpio_pin(pin), temperature(0.0f), humidity(0.0f) {}

esp_err_t DHTSensor::initialize() {
    ESP_LOGI(TAG, "Initializing %s sensor on GPIO %d", sensor_name, gpio_pin);
    initialized = true;
    return ESP_OK;
}

esp_err_t DHTSensor::read() {
    if (!initialized) {
        ESP_LOGE(TAG, "%s sensor not initialized", sensor_name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    float temp_reading = 0.0f;
    float humid_reading = 0.0f;

    for (int i = 0; i < DHT_ERROR_RETRY_COUNT; i++) {
        ret = dht_read_float_data(DHT_TYPE, gpio_pin, &humid_reading, &temp_reading);
        if (ret == ESP_OK) {
            if (temp_reading >= DHT_MIN_TEMPERATURE && temp_reading <= DHT_MAX_TEMPERATURE &&
                humid_reading >= DHT_MIN_HUMIDITY && humid_reading <= DHT_MAX_HUMIDITY) {
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

    ESP_LOGE(TAG, "%s reading failed after %d attempts", sensor_name, DHT_ERROR_RETRY_COUNT);
    return ESP_FAIL;
}

void DHTSensor::reset() {
    temperature = 0.0f;
    humidity = 0.0f;
    error_count = 0;
}

bool DHTSensor::validateReading() const {
    return temperature >= DHT_MIN_TEMPERATURE && temperature <= DHT_MAX_TEMPERATURE &&
           humidity >= DHT_MIN_HUMIDITY && humidity <= DHT_MAX_HUMIDITY;
}

// SGP40 Sensor Implementation
SGP40Sensor::SGP40Sensor(uint8_t addr) 
    : SensorBase("SGP40"), i2c_addr(addr), voc_index(0) {
    memset(&sgp_dev, 0, sizeof(sgp_dev));
}

esp_err_t SGP40Sensor::initialize() {
    ESP_LOGI(TAG, "Initializing %s sensor at address 0x%x", sensor_name, i2c_addr);
    
    // Initialize I2C
    esp_err_t ret = i2cdev_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C");
        return ret;
    }
    
    // Initialize SGP40
    ret = sgp40_init_desc(&sgp_dev, (i2c_port_t)0, (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SDA, (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SCL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SGP40 descriptor");
        return ret;
    }

    ret = sgp40_init(&sgp_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SGP40");
        return ret;
    }

    ESP_LOGI(TAG, "SGP40 initialized. Serial: 0x%04x%04x%04x", sgp_dev.serial[0], sgp_dev.serial[1], sgp_dev.serial[2]);
    initialized = true;
    
    // Record start time for warmup period
    sgp40_start_time_ms = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "SGP40 warmup period started");
    
    return ESP_OK;
}

esp_err_t SGP40Sensor::read() {
    if (!initialized) {
        ESP_LOGE(TAG, "%s sensor not initialized", sensor_name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    int32_t reading = 0;

    for (int i = 0; i < SGP40_ERROR_RETRY_COUNT; i++) {
        ret = sgp40_measure_voc(&sgp_dev, 0.0, 0.0, &reading);
        if (ret == ESP_OK) {
            if (reading >= SGP40_MIN_VOC_INDEX && reading <= SGP40_MAX_VOC_INDEX) {
                voc_index = reading;
                error_count = 0;
                return ESP_OK;
            }
        }
        error_count++;
        ESP_LOGW(TAG, "%s reading attempt %d failed", sensor_name, i + 1);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_RETRY_DELAY_MS));
    }

    ESP_LOGE(TAG, "%s reading failed after %d attempts", sensor_name, SGP40_ERROR_RETRY_COUNT);
    return ESP_FAIL;
}

void SGP40Sensor::reset() {
    voc_index = 0;
    error_count = 0;
}

bool SGP40Sensor::validateReading() const {
    return voc_index >= SGP40_MIN_VOC_INDEX && voc_index <= SGP40_MAX_VOC_INDEX;
}

// Sensor Manager Implementation
SensorManager::SensorManager() : dht_sensor(nullptr), sgp_sensor(nullptr), running(false), task_handle(nullptr) {}

SensorManager::~SensorManager() {
    stopReadings();
    delete dht_sensor;
    delete sgp_sensor;
}

esp_err_t SensorManager::initialize() {
    dht_sensor = new DHTSensor((gpio_num_t)CONFIG_GPIO_DHT22_PIN);
    sgp_sensor = new SGP40Sensor();

    esp_err_t ret = dht_sensor->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DHT sensor");
        return ret;
    }

    ret = sgp_sensor->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SGP40 sensor");
        return ret;
    }

    return ESP_OK;
}

void SensorManager::readingTask(void* parameters) {
    SensorManager* manager = static_cast<SensorManager*>(parameters);
    
    while (manager->running) {
        esp_err_t dht_ret = manager->dht_sensor->read();
        esp_err_t sgp_ret = manager->sgp_sensor->read();

        if (dht_ret == ESP_OK && manager->dht_sensor->validateReading()) {
            ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", 
                    manager->dht_sensor->getTemperature(),
                    manager->dht_sensor->getHumidity());
        } else {
            ESP_LOGW(TAG, "Failed to read DHT sensor or invalid reading");
        }

        if (sgp_ret == ESP_OK && manager->sgp_sensor->validateReading()) {
            ESP_LOGI(TAG, "VOC Index: %ld", manager->sgp_sensor->getVOCIndex());
            // VOC values are updated in the same update_matter_with_sensor_values call
        } else {
            ESP_LOGW(TAG, "Failed to read SGP sensor or invalid reading");
        }

        // Update Matter attributes
        update_matter_with_sensor_values(manager);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }

    vTaskDelete(NULL);
}

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