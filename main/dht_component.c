#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dht_component.h"
#include <dht.h>

#define DHT_GPIO_PIN   GPIO_NUM_10
#define DHT_POLL_INTERVAL_MS 10000
#define DHT_SENSOR_TYPE DHT_TYPE_AM2301

static const char *TAG = "DHT_COMPONENT";

// Must be provided by a DHT driver in the project/SDK.
// extern esp_err_t dht_read_float_data(gpio_num_t gpio_num, float *humidity, float *temperature);

static void dht_task(void *arg)
{
    QueueHandle_t output_queue = (QueueHandle_t)arg;

    while (1) {
        dht_data_t data = {
            .temperature = 0.0f,
            .humidity = 0.0f,
            .is_valid = false,
        };

        float temperature = 0.0f;
        float humidity = 0.0f;
        esp_err_t result = dht_read_float_data(DHT_SENSOR_TYPE, DHT_GPIO_PIN, &humidity, &temperature);
         if (result == ESP_OK) {
            data.temperature = temperature;
            data.humidity = humidity;
            data.is_valid = true;
            ESP_LOGI(TAG, "Температура: %.1f °C, Влажность: %.1f %%", temperature, humidity);
        } else {
            ESP_LOGW(TAG, "Failed to read DHT22 from GPIO %d", DHT_GPIO_PIN);
        }

        if (xQueueSend(output_queue, &data, 0) != pdPASS) {
            (void)xQueueOverwrite(output_queue, &data);
        }
        vTaskDelay(pdMS_TO_TICKS(DHT_POLL_INTERVAL_MS));
    }
}

void dht_worker_start(QueueHandle_t output_queue)
{
    if (output_queue == NULL) {
        ESP_LOGE(TAG, "output_queue is NULL");
        return;
    }

    BaseType_t task_ok = xTaskCreate(dht_task, "dht_task", 4096, output_queue, 5, NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create dht_task");
    }
}
