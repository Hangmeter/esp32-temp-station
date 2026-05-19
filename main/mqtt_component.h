#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t mqtt_component_init(const char *broker_uri);
bool mqtt_component_is_connected(void);
esp_err_t mqtt_component_publish(const char *topic, const char *payload, int qos, int retain);
