#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t wifi_component_init_station(const char *ssid, const char *password);
bool wifi_component_is_connected(void);
