#include "includes/variables.h"

sgp40_t sgp;

// Sensor values
float temperature = 0.0;
float humidity = 0.0;
int32_t voc_index = 0;

// Endpoints
uint16_t temperature_endpoint_id = 1;
uint16_t humidity_endpoint_id = 2;
uint16_t voc_endpoint_id = 3;
