#pragma once

#include <stdbool.h>
#include <esp_err.h>
#include "led_strip.h"

/**
 * @brief Log an error message and abort the program if the condition is false.
 *
 * @param ok Condition to check.
 * @param tag Tag to use in the log message.
 * @param fmt Format string for the log message.
 * @param ... Additional arguments for the format string.
 * @example abort_on_failure(err == ESP_OK, TAG, "Failed to start Matter, err:%d", err);
 */
void abort_on_failure(bool ok, const char *tag, const char *fmt, ...);

/**
 * @brief Register the reset button callbacks
 * 
 * @param handle Button handle pointer
 * @return esp_err_t 
 */
esp_err_t app_reset_button_register(void *handle);


/**
 * @brief Initialize an ARGB/LED strip using RMT. Returns a handle or nullptr on failure.
 * 
 * @param gpio_num 
 * @param max_leds 
 * @param model 
 * @return led_strip_handle_t 
 */
led_strip_handle_t argb_init(int gpio_num, uint32_t max_leds, led_model_t model = LED_MODEL_WS2812);

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
esp_err_t argb_set_pixel(led_strip_handle_t strip, uint32_t index, uint8_t r, uint8_t g, uint8_t b);

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
esp_err_t argb_set_all(led_strip_handle_t strip, uint32_t count, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Clear (turn off) all pixels and refresh.
 * 
 * @param strip 
 * @return esp_err_t 
 */
esp_err_t argb_clear(led_strip_handle_t strip);

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
esp_err_t argb_blink_all(led_strip_handle_t strip, uint32_t count, uint8_t r, uint8_t g, uint8_t b, int times, uint32_t interval_ms);

/**
 * @brief Start the ARGB commissioning task
 * 
 * @param gpio_num 
 * @param count 
 */
void argb_start_commissioning(int gpio_num, uint32_t count);

/**
 * @brief Stop the ARGB commissioning task
 * 
 */
void argb_stop_commissioning(void);

/**
 * @brief Check if Matter device is commissioned (has at least one fabric)
 * 
 * @return true if commissioned, false otherwise
 */
bool is_matter_connected(void);