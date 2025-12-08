#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_matter.h>
#include "iot_button.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "utils.hpp"

static const char *TAG = "UTILS";
static bool perform_factory_reset = false;


static TaskHandle_t s_commissioning_task = NULL;
static led_strip_handle_t s_commissioning_strip = NULL;
static volatile bool s_commissioning_running = false;
static uint32_t s_commissioning_count = 0;

/**
 * @brief Button factory reset pressed callback
 * 
 * @param arg 
 * @param data 
 */
static void button_factory_reset_pressed_cb(void *arg, void *data) {
    if (!perform_factory_reset) {
        ESP_LOGI(TAG, "Factory reset triggered. Release the button to start factory reset.");
        perform_factory_reset = true;
    }
}

/**
 * @brief Button factory reset released callback
 * 
 * @param arg 
 * @param data 
 */
static void button_factory_reset_released_cb(void *arg, void *data) {
    if (perform_factory_reset) {
        ESP_LOGI(TAG, "Starting factory reset");
        esp_matter::factory_reset();
        perform_factory_reset = false;
    }
}

/**
 * @brief Log an error message and abort the program if the condition is false.
 * 
 * @param ok Condition to check.
 * @param tag Tag to use in the log message.
 * @param fmt Format string for the log message.
 * @param ... Additional arguments for the format string.
 */
void abort_on_failure(bool ok, const char *tag, const char *fmt, ...) {
    if (ok) {
        return;
    }

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    ESP_LOGE(tag, "%s", buf);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    abort();
}

/**
 * @brief Register the reset button callbacks
 * 
 * @param handle 
 * @return esp_err_t 
 */
esp_err_t app_reset_button_register(void *handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Handle cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    button_handle_t button_handle = (button_handle_t)handle;
    esp_err_t err = ESP_OK;
    err |= iot_button_register_cb(button_handle, BUTTON_LONG_PRESS_HOLD, NULL, button_factory_reset_pressed_cb, NULL);
    err |= iot_button_register_cb(button_handle, BUTTON_PRESS_UP, NULL, button_factory_reset_released_cb, NULL);
    return err;
}

/**
 * @brief Initialize an ARGB/LED strip using RMT. Returns a handle or nullptr on failure.
 * 
 * @param gpio_num 
 * @param max_leds 
 * @param model 
 * @return led_strip_handle_t 
 */
led_strip_handle_t argb_init(int gpio_num, uint32_t max_leds, led_model_t model) {
    led_strip_handle_t strip = NULL;
    led_strip_config_t cfg = {
        .strip_gpio_num = gpio_num,
        .max_leds = max_leds,
        .led_model = model,
        .flags = { .invert_out = 0 },
    };
    led_strip_rmt_config_t rmt_cfg;
    memset(&rmt_cfg, 0, sizeof(rmt_cfg));
    esp_err_t ret = led_strip_new_rmt_device(&cfg, &rmt_cfg, &strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init led_strip (err=%d)", ret);
        return NULL;
    }
    // Clear initial state
    led_strip_clear(strip);
    led_strip_refresh(strip);
    return strip;
}

/**
 * @brief Set a single pixel color (0-based index) and refresh immediately.
 * 
 * @param strip 
 * @param index 
 * @param r 
 * @param g 
 * @param b 
 * @return esp_err_t 
 */
esp_err_t argb_set_pixel(led_strip_handle_t strip, uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (!strip) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = led_strip_set_pixel(strip, index, r, g, b);
    if (ret != ESP_OK) return ret;
    return led_strip_refresh(strip);
}

/**
 * @brief Set all pixels to the same color and refresh.
 * 
 * @param strip 
 * @param count 
 * @param r 
 * @param g 
 * @param b 
 * @return esp_err_t 
 */
esp_err_t argb_set_all(led_strip_handle_t strip, uint32_t count, uint8_t r, uint8_t g, uint8_t b) {
    if (!strip) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = ESP_OK;
    for (uint32_t i = 0; i < count; ++i) {
        ret |= led_strip_set_pixel(strip, i, r, g, b);
    }
    ret |= led_strip_refresh(strip);
    return ret;
}

/**
 * @brief Clear (turn off) all pixels and refresh.
 * 
 * @param strip 
 * @return esp_err_t 
 */
esp_err_t argb_clear(led_strip_handle_t strip) {
    if (!strip) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = led_strip_clear(strip);
    if (ret != ESP_OK) return ret;
    return led_strip_refresh(strip);
}

/**
 * @brief Blink all pixels a number of times (blocking): set color, wait, clear, wait.
 * 
 * @param strip 
 * @param count 
 * @param r 
 * @param g 
 * @param b 
 * @param times 
 * @param interval_ms 
 * @return esp_err_t 
 */
esp_err_t argb_blink_all(led_strip_handle_t strip, uint32_t count, uint8_t r, uint8_t g, uint8_t b, int times, uint32_t interval_ms) {
    if (!strip) return ESP_ERR_INVALID_ARG;
    if (times <= 0) return ESP_ERR_INVALID_ARG;
    for (int t = 0; t < times; ++t) {
        argb_set_all(strip, count, r, g, b);
        vTaskDelay(interval_ms / portTICK_PERIOD_MS);
        argb_clear(strip);
        vTaskDelay(interval_ms / portTICK_PERIOD_MS);
    }
    return ESP_OK;
}

/**
 * @brief Commissioning task for ARGB LED strip
 * 
 * @param arg 
 */
static void commissioning_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "ARGB commissioning task started (smooth fade)");

    const uint8_t max_brightness = 140; // peak blue brightness (0-255)
    const uint8_t min_brightness = 2;   // minimal visible level
    const int steps = 48;               // steps per ramp
    const TickType_t step_delay = pdMS_TO_TICKS(15); // ms per step

    while (s_commissioning_running) {
        // Ramp up
        for (int i = 0; i <= steps && s_commissioning_running; ++i) {
            uint8_t b = (uint8_t)(min_brightness + ((max_brightness - min_brightness) * i) / steps);
            if (s_commissioning_strip) {
                argb_set_all(s_commissioning_strip, s_commissioning_count, 0, 0, b);
            }
            vTaskDelay(step_delay);
        }
        // Ramp down
        for (int i = steps; i >= 0 && s_commissioning_running; --i) {
            uint8_t b = (uint8_t)(min_brightness + ((max_brightness - min_brightness) * i) / steps);
            if (s_commissioning_strip) {
                argb_set_all(s_commissioning_strip, s_commissioning_count, 0, 0, b);
            }
            vTaskDelay(step_delay);
        }
    }

    // Clear on exit
    if (s_commissioning_strip) {
        argb_clear(s_commissioning_strip);
    }
    ESP_LOGI(TAG, "ARGB commissioning task stopping");
    // mark task handle as NULL before deleting
    TaskHandle_t t = s_commissioning_task;
    s_commissioning_task = NULL;
    // delete ourselves
    if (t) {
        vTaskDelete(NULL);
    }
}

/**
 * @brief Start the ARGB commissioning task
 * 
 * @param gpio_num 
 * @param count 
 */
void argb_start_commissioning(int gpio_num, uint32_t count) {
    if (s_commissioning_running) {
        ESP_LOGW(TAG, "Commissioning already running");
        return;
    }

    if (count == 0) {
        ESP_LOGE(TAG, "Invalid LED count for commissioning");
        return;
    }

    // Initialize strip for the requested gpio and count
    led_strip_handle_t strip = argb_init(gpio_num, count, LED_MODEL_WS2812);
    if (!strip) {
        ESP_LOGE(TAG, "Failed to init strip for commissioning on GPIO %d", gpio_num);
        return;
    }

    s_commissioning_strip = strip;
    s_commissioning_count = count;
    s_commissioning_running = true;

    // Create a background task to pulse the LED
    BaseType_t created = xTaskCreate(commissioning_task, "argb_comm", 4096, NULL, tskIDLE_PRIORITY + 1, &s_commissioning_task);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create commissioning task");
        s_commissioning_running = false;
        if (s_commissioning_strip) {
            led_strip_del(s_commissioning_strip);
            s_commissioning_strip = NULL;
        }
    }
}

/**
 * @brief Stop the ARGB commissioning task
 * 
 */
void argb_stop_commissioning(void) {
    if (!s_commissioning_running && s_commissioning_task == NULL) {
        ESP_LOGW(TAG, "Commissioning not running");
        return;
    }

    s_commissioning_running = false;

    // Wait for task to exit (it will clear s_commissioning_task)
    const TickType_t wait_ticks = pdMS_TO_TICKS(1000);
    const TickType_t poll_ticks = pdMS_TO_TICKS(50);
    TickType_t waited = 0;
    while (s_commissioning_task != NULL && waited < wait_ticks) {
        vTaskDelay(poll_ticks);
        waited += poll_ticks;
    }

    // If strip exists, delete it
    if (s_commissioning_strip) {
        led_strip_del(s_commissioning_strip);
        s_commissioning_strip = NULL;
    }

    if (s_commissioning_task != NULL) {
        // If still alive, force delete (not ideal but ensures stop)
        vTaskDelete(s_commissioning_task);
        s_commissioning_task = NULL;
    }

    ESP_LOGI(TAG, "Commissioning stopped");
}