#pragma once

#include "esp_err.h"

typedef struct {
    float temperature_c;
} sensor_data_t;

esp_err_t sensor_component_init(void);
esp_err_t sensor_component_read(sensor_data_t *out_data);
