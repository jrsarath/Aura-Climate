#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle network-ready event to start SNTP and auto timezone detection.
 */
void time_manager_handle_ip_available(void);

#ifdef __cplusplus
}
#endif
