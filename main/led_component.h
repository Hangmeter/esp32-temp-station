#pragma once

#include "device_state.h"
#include "esp_err.h"

esp_err_t led_component_init(void);
void led_component_set_state(device_state_t state);
