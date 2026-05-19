#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "mqtt_component.h"
#include "sensor_component.h"
#include "wifi_component.h"

static const char *TAG = "TEMP_STATION";

#define WIFI_SSID       "ASUS_10"
#define WIFI_PASS       "Protei_ST!"
#define MQTT_BROKER_URL "mqtt://192.168.8.126"
#define MQTT_TOPIC      "temp_station/temperature"

typedef enum {
    ST_INIT,
    ST_CONNECTING,
    ST_READING,
    ST_PUBLISHING,
    ST_ERROR
} device_state_t;

static device_state_t current_state = ST_INIT;
static sensor_data_t current_sensor_data = {0};

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    while (1) {
        switch (current_state) {
            case ST_INIT:
                ESP_LOGI(TAG, "Initializing components...");
                ESP_ERROR_CHECK(wifi_component_init_station(WIFI_SSID, WIFI_PASS));
                ESP_ERROR_CHECK(mqtt_component_init(MQTT_BROKER_URL));
                ESP_ERROR_CHECK(sensor_component_init());
                current_state = ST_CONNECTING;
                break;

            case ST_CONNECTING:
                if (wifi_component_is_connected() && mqtt_component_is_connected()) {
                    ESP_LOGI(TAG, "Connectivity ready");
                    current_state = ST_READING;
                } else {
                    ESP_LOGI(TAG, "Waiting for Wi-Fi/MQTT...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                break;

            case ST_READING:
                if (sensor_component_read(&current_sensor_data) == ESP_OK) {
                    current_state = ST_PUBLISHING;
                } else {
                    current_state = ST_ERROR;
                }
                break;

            case ST_PUBLISHING: {
                char payload[64];
                snprintf(payload, sizeof(payload), "{\"temperature\":%.2f}", current_sensor_data.temperature_c);
                if (mqtt_component_publish(MQTT_TOPIC, payload, 1, 0) == ESP_OK) {
                    ESP_LOGI(TAG, "Published: %s", payload);
                    current_state = ST_READING;
                    vTaskDelay(pdMS_TO_TICKS(5000));
                } else {
                    ESP_LOGW(TAG, "Publish failed, waiting for reconnect");
                    current_state = ST_CONNECTING;
                }
                break;
            }

            case ST_ERROR:
                ESP_LOGE(TAG, "Runtime error, retrying init");
                vTaskDelay(pdMS_TO_TICKS(1000));
                current_state = ST_INIT;
                break;

            default:
                current_state = ST_ERROR;
                break;
        }
    }
}
