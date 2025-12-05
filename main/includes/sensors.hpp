#pragma once

#include <esp_err.h>
#include <sgp40.h>
#include <driver/gpio.h>
#include "config.hpp"

/**
 * @brief Base class for all sensors.
 * 
 */
class SensorBase {
    protected:
        bool initialized;
        uint8_t error_count;
        const char* sensor_name;

    public:
        SensorBase(const char* name) : initialized(false), error_count(0), sensor_name(name) {}
        virtual ~SensorBase() = default;
        
        virtual esp_err_t initialize() = 0;
        virtual esp_err_t read() = 0;
        virtual void reset() = 0;
        
        bool isInitialized() const { return initialized; }
        const char* getName() const { return sensor_name; }
};

/**
 * @brief DHT sensor class for temperature and humidity measurements.
 * 
 */
class DHTSensor : public SensorBase {
    private:
        gpio_num_t gpio_pin;
        float temperature;
        float humidity;

    public:
        explicit DHTSensor(gpio_num_t pin);
        ~DHTSensor() override = default;

        esp_err_t initialize() override;
        esp_err_t read() override;
        void reset() override;

        float getTemperature() const { return temperature; }
        float getHumidity() const { return humidity; }
        bool validateReading() const;
};

/**
 * @brief SGP40 sensor class for VOC index measurements.
 * 
 */
class SGP40Sensor : public SensorBase {
    private:
        uint8_t i2c_addr;
        int32_t voc_index;
        sgp40_t sgp_dev;

    public:
        explicit SGP40Sensor(uint8_t addr = SGP40_I2C_ADDR);
        ~SGP40Sensor() override = default;

        esp_err_t initialize() override;
        esp_err_t read() override;
        void reset() override;

        int32_t getVOCIndex() const { return voc_index; }
        bool validateReading() const;
};

/**
 * @brief Manages all sensors and their readings.
 * 
 */
class SensorManager {
    private:
        DHTSensor* dht_sensor;
        SGP40Sensor* sgp_sensor;
        bool running;
        TaskHandle_t task_handle;

        static void readingTask(void* parameters);

    public:
        SensorManager();
        ~SensorManager();

        esp_err_t initialize();
        esp_err_t startReadings();
        void stopReadings();
        
        const DHTSensor* getDHTSensor() const { return dht_sensor; }
        const SGP40Sensor* getSGP40Sensor() const { return sgp_sensor; }
};