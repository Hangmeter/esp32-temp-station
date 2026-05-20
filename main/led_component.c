#include "led_component.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_COMPONENT";

#define LED_RED_GPIO   GPIO_NUM_3
#define LED_GREEN_GPIO GPIO_NUM_4
#define LED_BLUE_GPIO  GPIO_NUM_5

static volatile device_state_t s_led_state = ST_INIT;

static void leds_all_off(void)
{
    gpio_set_level(LED_RED_GPIO, 0);
    gpio_set_level(LED_GREEN_GPIO, 0);
    gpio_set_level(LED_BLUE_GPIO, 0);
}

static void set_single_led_level(gpio_num_t gpio_num, int level)
{
    gpio_set_level(LED_RED_GPIO, 0);
    gpio_set_level(LED_GREEN_GPIO, 0);
    gpio_set_level(LED_BLUE_GPIO, 0);
    gpio_set_level(gpio_num, level);
}

static void led_blink_task(void *arg)
{
    (void)arg;
    bool is_on = false;

    while (1) {
        gpio_num_t active_led = LED_RED_GPIO;
        uint32_t period_ms = 1000;

        switch (s_led_state) {
            case ST_INIT:
                active_led = LED_RED_GPIO;
                period_ms = 1000; // 1 Hz
                break;
            case ST_CONNECTING:
                active_led = LED_GREEN_GPIO;
                period_ms = 500; // 2 Hz
                break;
            case ST_READING:
                active_led = LED_BLUE_GPIO;
                period_ms = 250; // 4 Hz
                break;
            case ST_PUBLISHING:
                active_led = LED_BLUE_GPIO;
                period_ms = 1000; // 1 Hz
                break;
            case ST_ERROR:
                active_led = LED_RED_GPIO;
                period_ms = 250; // 4 Hz
                break;
            default:
                active_led = LED_RED_GPIO;
                period_ms = 250;
                break;
        }

        is_on = !is_on;
        set_single_led_level(active_led, is_on ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(period_ms / 2));
    }
}

esp_err_t led_component_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_RED_GPIO) | (1ULL << LED_GREEN_GPIO) | (1ULL << LED_BLUE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    leds_all_off();

    BaseType_t task_ok = xTaskCreate(led_blink_task, "led_blink_task", 2048, NULL, 5, NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED blink task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void led_component_set_state(device_state_t state)
{
    s_led_state = state;
}
