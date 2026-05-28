#pragma once

#include <stdbool.h>

#include "freertos/queue.h"

typedef struct {
    float temperature;
    float humidity;
    bool is_valid;
} dht_data_t;

void dht_worker_start(QueueHandle_t output_queue);
