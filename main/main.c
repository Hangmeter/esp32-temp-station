#include <stdbool.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "bh1750_component.h"
#include "dht_component.h"
#include "device_state.h"
#include "led_component.h"
#include "mqtt_component.h"
#include "sensor_component.h"
#include "wifi_component.h"

static const char *TAG = "MAIN_APP";

//#define WIFI_SSID       "ASUS_10"
//#define WIFI_PASS       "Protei_ST!"
#define WIFI_SSID       "GL_AXT1800"
#define WIFI_PASS       "WRHD24ERWM"
#define MQTT_BROKER_URL "mqtt://192.168.8.126"
#define MQTT_TOPIC      "esp32-e6e4/env"

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

    QueueHandle_t dht_queue = xQueueCreate(1, sizeof(dht_data_t));
    ESP_ERROR_CHECK(dht_queue != NULL ? ESP_OK : ESP_FAIL);

    QueueHandle_t bh1750_queue = xQueueCreate(1, sizeof(bh1750_data_t));
    ESP_ERROR_CHECK(bh1750_queue != NULL ? ESP_OK : ESP_FAIL);

    dht_data_t local_dht_storage = {
        .temperature = 0.0f,
        .humidity = 0.0f,
        .is_valid = false,
    };

    float bh1750_lux = 0.0f;
    bool bh1750_valid = false;

    ESP_ERROR_CHECK(led_component_init());
    led_component_set_state(ST_INIT);
    dht_worker_start(dht_queue);

    while (1) {
        switch (current_state) {
            case ST_INIT:
                ESP_LOGI(TAG, "Initializing components...");
                ESP_ERROR_CHECK(wifi_component_init_station(WIFI_SSID, WIFI_PASS));
                ESP_ERROR_CHECK(mqtt_component_init(MQTT_BROKER_URL));
                ESP_ERROR_CHECK(sensor_component_init());
                esp_err_t bh1750_ret = bh1750_component_init();
                if (bh1750_ret == ESP_OK) {
                    bh1750_worker_start(bh1750_queue);
                } else {
                    ESP_LOGW(TAG, "BH1750 initialization failed: %s", esp_err_to_name(bh1750_ret));
                    bh1750_valid = false;
                }
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

            case ST_READING: {
                dht_data_t incoming_dht = {0};
                if (xQueueReceive(dht_queue, &incoming_dht, 0) == pdPASS) {
                    local_dht_storage = incoming_dht;
                }

                if (sensor_component_read(&current_sensor_data) == ESP_OK) {
                    bh1750_data_t incoming_bh1750 = {0};
                    if (xQueueReceive(bh1750_queue, &incoming_bh1750, 0) == pdPASS) {
                        bh1750_lux = incoming_bh1750.lux;
                        bh1750_valid = incoming_bh1750.is_valid;
                    }
                    set_device_state(ST_PUBLISHING);
                } else {
                    set_device_state(ST_ERROR);
                }
                break;
            }

            case ST_PUBLISHING: {
                char payload[224];
                char dht_temperature[16];
                char dht_humidity[16];
                char bh1750_lux_json[16];

                if (local_dht_storage.is_valid) {
                    snprintf(dht_temperature, sizeof(dht_temperature), "%.1f", local_dht_storage.temperature);
                    snprintf(dht_humidity, sizeof(dht_humidity), "%.1f", local_dht_storage.humidity);
                } else {
                    snprintf(dht_temperature, sizeof(dht_temperature), "null");
                    snprintf(dht_humidity, sizeof(dht_humidity), "null");
                }

                if (bh1750_valid) {
                    snprintf(bh1750_lux_json, sizeof(bh1750_lux_json), "%.1f", bh1750_lux);
                } else {
                    snprintf(bh1750_lux_json, sizeof(bh1750_lux_json), "null");
                }

                snprintf(payload,
                         sizeof(payload),
                         "{\"temperature\":%.1f,\"dht_temperature\":%s,\"dht_humidity\":%s,\"lux\":%s}",
                         current_sensor_data.temperature_c,
                         dht_temperature,
                         dht_humidity,
                         bh1750_lux_json);

                if (mqtt_component_publish(MQTT_TOPIC, payload, 1, 0) == ESP_OK) {
                    ESP_LOGI(TAG, "Published: %s", payload);
                    set_device_state(ST_READING);
                    vTaskDelay(pdMS_TO_TICKS(5000));
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
