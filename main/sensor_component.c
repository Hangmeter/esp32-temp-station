#include "sensor_component.h"

#include "esp_log.h"

static const char *TAG = "SENSOR_COMPONENT";

esp_err_t sensor_component_init(void)
{
    ESP_LOGI(TAG, "Sensor subsystem initialized (stub)");
    return ESP_OK;
}

esp_err_t sensor_component_read(sensor_data_t *out_data)
{
    if (out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Stub: replace by real sensor driver calls.
    out_data->temperature_c = 24.5f;
    return ESP_OK;
}
