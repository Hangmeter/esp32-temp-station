#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "device_state.h"
#include "led_component.h"
#include "mqtt_component.h"
#include "sensor_component.h"
#include "wifi_component.h"

static const char *TAG = "TEMP_STATION";

#define WIFI_SSID       "Emerald"
#define WIFI_PASS       "Protei_ST!"
//#define MQTT_BROKER_URL "mqtt://172.16.101.4"
#define MQTT_BROKER_URL "mqtt://10.10.251.13"
#define MQTT_TOPIC      "temp_station/temperature"

static device_state_t current_state = ST_INIT;
static sensor_data_t current_sensor_data = {0};

static void set_device_state(device_state_t new_state)
{
    current_state = new_state;
    led_component_set_state(new_state);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(led_component_init());
    led_component_set_state(ST_INIT);

    while (1) {
        switch (current_state) {
            case ST_INIT:
                ESP_LOGI(TAG, "Initializing components...");
                ESP_ERROR_CHECK(wifi_component_init_station(WIFI_SSID, WIFI_PASS));
                ESP_ERROR_CHECK(mqtt_component_init(MQTT_BROKER_URL));
                ESP_ERROR_CHECK(sensor_component_init());
                vTaskDelay(pdMS_TO_TICKS(1000));
                set_device_state(ST_CONNECTING);
                break;

            case ST_CONNECTING:
                if (wifi_component_is_connected() && mqtt_component_is_connected()) {
                    ESP_LOGI(TAG, "Connectivity ready");
                    set_device_state(ST_READING);
                } else {
                    ESP_LOGI(TAG, "Waiting for Wi-Fi/MQTT...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                break;

            case ST_READING:
                if (sensor_component_read(&current_sensor_data) == ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    set_device_state(ST_PUBLISHING);
                } else {
                    set_device_state(ST_ERROR);
                }
                break;

            case ST_PUBLISHING: {
                char payload[64];
                snprintf(payload, sizeof(payload), "{\"temperature\":%.2f}", current_sensor_data.temperature_c);
                if (mqtt_component_publish(MQTT_TOPIC, payload, 1, 0) == ESP_OK) {
                    ESP_LOGI(TAG, "Published: %s", payload);
                    set_device_state(ST_READING);
                    vTaskDelay(pdMS_TO_TICKS(500));
                } else {
                    ESP_LOGW(TAG, "Publish failed, waiting for reconnect");
                    set_device_state(ST_CONNECTING);
                }
                break;
            }

            case ST_ERROR:
                ESP_LOGE(TAG, "Runtime error, retrying init");
                vTaskDelay(pdMS_TO_TICKS(1000));
                set_device_state(ST_INIT);
                break;

            default:
                set_device_state(ST_ERROR);
                break;
        }
    }
}
