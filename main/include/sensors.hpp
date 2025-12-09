#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <sht4x.h>
#include <ens160.h>
#include "config.hpp"

// Sanity ranges for SHT40; adjust if you prefer
#ifndef SHT4X_MIN_TEMPERATURE
#define SHT4X_MIN_TEMPERATURE  (-40.0f)
#endif

#ifndef SHT4X_MAX_TEMPERATURE
#define SHT4X_MAX_TEMPERATURE  (125.0f)
#endif

#ifndef SHT4X_MIN_HUMIDITY
#define SHT4X_MIN_HUMIDITY     (0.0f)
#endif

#ifndef SHT4X_MAX_HUMIDITY
#define SHT4X_MAX_HUMIDITY     (100.0f)
#endif

/**
 * @brief Base class for all sensors.
 * 
 */
class SensorBase {
protected:
        bool        initialized;
        uint8_t     error_count;
        const char* sensor_name;

    public:
        explicit SensorBase(const char* name)
            : initialized(false),
            error_count(0),
            sensor_name(name) {}

        virtual ~SensorBase() = default;

        virtual esp_err_t initialize() = 0;
        virtual esp_err_t read() = 0;
        virtual void      reset() = 0;

        bool        isInitialized() const { return initialized; }
        const char* getName()      const { return sensor_name; }
        uint8_t     getErrorCount() const { return error_count; }
};

/**
 * @brief SHT40 sensor class for temperature and humidity measurements.
 * 
 */
class SHT40Sensor : public SensorBase {
    private:
        float            temperature;
        float            humidity;
        mutable float    last_updated_temperature;
        mutable float    last_updated_humidity;
        sht4x_handle_t   sht_handle;

    public:
        SHT40Sensor();
        ~SHT40Sensor() override;

        esp_err_t initialize() override;
        esp_err_t read() override;
        void      reset() override;

        float getTemperature() const { return temperature; }
        float getHumidity()    const { return humidity; }
        bool  validateReading() const;
        bool  hasChanged() const;
        void  markUpdated() const;
};

/**
 * @brief ENS160 sensor class for air quality measurements.
 * 
 */
class ENS160Sensor : public SensorBase {
    private:
        uint8_t         aqi;
        uint16_t        tvoc_ppb;
        uint16_t        eco2_ppm;
        mutable uint8_t         last_updated_aqi;
        mutable uint16_t        last_updated_tvoc_ppb;
        mutable uint16_t        last_updated_eco2_ppm;
        ens160_handle_t ens_handle;

    public:
        ENS160Sensor();
        ~ENS160Sensor() override;

        esp_err_t initialize() override;
        esp_err_t read() override;
        void      reset() override;

        uint8_t  getAQI()      const { return aqi; }
        uint16_t getTVOCppb()  const { return tvoc_ppb; }
        uint16_t getECO2ppm()  const { return eco2_ppm; }
        bool     validateReading() const;
        bool     hasChanged() const;
        void     markUpdated() const;
};

/**
 * @brief Manages all sensors and their readings.
 * 
 */
class SensorManager {
    private:
        SHT40Sensor* sht_sensor;
        ENS160Sensor* ens_sensor;
        bool          running;
        TaskHandle_t  task_handle;

        static void readingTask(void* parameters);

    public:
        SensorManager();
        ~SensorManager();

        esp_err_t initialize();
        esp_err_t startReadings();
        void      stopReadings();

        const SHT40Sensor*  getSHT40Sensor()  const { return sht_sensor; }
        const ENS160Sensor* getENS160Sensor() const { return ens_sensor; }
};

