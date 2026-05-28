#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/queue.h"

typedef struct {
    float lux;
    bool is_valid;
} bh1750_data_t;

esp_err_t bh1750_component_init(void);
void bh1750_worker_start(QueueHandle_t output_queue);
