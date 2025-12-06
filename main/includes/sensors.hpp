#pragma once

#include <esp_err.h>
#include <i2cdev.h>
#include <sht4x.h>
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
 * @brief SHT40 sensor class for temperature and humidity measurements.
 * 
 */
class SHT40Sensor : public SensorBase {
    private:
        float temperature;
        float humidity;
        sht4x_t sht_dev;

    public:
        SHT40Sensor();
        ~SHT40Sensor() override = default;

        esp_err_t initialize() override;
        esp_err_t read() override;
        void reset() override;

        float getTemperature() const { return temperature; }
        float getHumidity() const { return humidity; }
        bool validateReading() const;
};

/**
 * @brief ENS160 sensor class for air quality measurements.
 * 
 */
class ENS160Sensor : public SensorBase {
    private:
        i2c_dev_t dev;
        uint8_t aqi;
        uint16_t tvoc_ppb;
        uint16_t eco2_ppm;

        esp_err_t write_reg(uint8_t reg, uint8_t value);
        esp_err_t read_reg(uint8_t reg, uint8_t* value);
        esp_err_t read_word(uint8_t reg, uint16_t* value);
        esp_err_t wait_data_ready(uint32_t timeout_ms);

    public:
        ENS160Sensor();
        ~ENS160Sensor() override = default;

        esp_err_t initialize() override;
        esp_err_t read() override;
        void reset() override;

        uint8_t getAQI() const { return aqi; }
        uint16_t getTVOCppb() const { return tvoc_ppb; }
        uint16_t getECO2ppm() const { return eco2_ppm; }
        bool validateReading() const;
};

/**
 * @brief Manages all sensors and their readings.
 * 
 */
class SensorManager {
    private:
        SHT40Sensor* sht_sensor;
        ENS160Sensor* ens_sensor;
        bool running;
        TaskHandle_t task_handle;

        static void readingTask(void* parameters);

    public:
        SensorManager();
        ~SensorManager();

        esp_err_t initialize();
        esp_err_t startReadings();
        void stopReadings();
        
        const SHT40Sensor* getSHT40Sensor() const { return sht_sensor; }
        const ENS160Sensor* getENS160Sensor() const { return ens_sensor; }
};