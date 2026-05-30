#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2cdev.h"
#include "bh1750_component.h"
#include <bh1750.h>

#define BH1750_I2C_PORT I2C_NUM_0
#define BH1750_I2C_ADDRESS BH1750_ADDR_LO
#define BH1750_SDA_GPIO GPIO_NUM_18
#define BH1750_SCL_GPIO GPIO_NUM_19
#define BH1750_POLL_INTERVAL_MS 10000

#ifndef BH1750_CONTINUOUS_HIGH_RES_MODE
#define BH1750_CONTINUOUS_HIGH_RES_MODE BH1750_CONTINUE_1LX_RES
#endif

static const char *TAG = "BH1750_COMPONENT";

static i2c_dev_t s_bh1750_dev;
static bool s_is_initialized = false;
static bool s_worker_started = false;

//static i2c_master_bus_handle_t s_i2c_bus = NULL;
//static bh1750_handle_t s_bh1750 = NULL;


static esp_err_t bh1750_get_lux(float *lux)
{
    if (!s_is_initialized || lux == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t lux_raw = 0;
    esp_err_t ret = bh1750_read(&s_bh1750_dev, &lux_raw);
    if (ret != ESP_OK) {
        return ret;
    }

    *lux = (float)lux_raw;
    return ESP_OK;
}

static void bh1750_task(void *arg)
{
    QueueHandle_t output_queue = (QueueHandle_t)arg;

    vTaskDelay(pdMS_TO_TICKS(180));

    while (1) {
        bh1750_data_t data = {
            .lux = 0.0f,
            .is_valid = false,
        };

        esp_err_t ret = bh1750_get_lux(&data.lux);
        if (ret == ESP_OK) {
            data.is_valid = true;
        } else {
            ESP_LOGW(TAG, "Failed to read BH1750 lux value: %s", esp_err_to_name(ret));
        }

        if (xQueueSend(output_queue, &data, 0) != pdPASS) {
            (void)xQueueOverwrite(output_queue, &data);
        }

        vTaskDelay(pdMS_TO_TICKS(BH1750_POLL_INTERVAL_MS));
    }
}

esp_err_t bh1750_component_init(void)
{
    if (s_is_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = i2cdev_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize i2cdev subsystem: %s", esp_err_to_name(ret));
        return ret;
    }

    memset(&s_bh1750_dev, 0, sizeof(s_bh1750_dev));

    ret = bh1750_init_desc(&s_bh1750_dev,
                           BH1750_I2C_ADDRESS,
                           BH1750_I2C_PORT,
                           BH1750_SDA_GPIO,
                           BH1750_SCL_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BH1750 descriptor: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = bh1750_power_on(&s_bh1750_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on BH1750: %s", esp_err_to_name(ret));
        (void)bh1750_free_desc(&s_bh1750_dev);
        return ret;
    }

    ret = bh1750_setup(&s_bh1750_dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure BH1750 continuous high resolution mode: %s", esp_err_to_name(ret));
        (void)bh1750_free_desc(&s_bh1750_dev);
        return ret;
    }

    s_is_initialized = true;
    ESP_LOGI(TAG, "BH1750 initialized on SDA GPIO %d, SCL GPIO %d, address 0x%02x",
             BH1750_SDA_GPIO,
             BH1750_SCL_GPIO,
             BH1750_I2C_ADDRESS);
    return ESP_OK;
}

void bh1750_worker_start (QueueHandle_t output_queue)
{
    if (output_queue == NULL) {
        ESP_LOGE(TAG, "output_queue is NULL");
        return;
    }

    if (!s_is_initialized) {
        ESP_LOGE(TAG, "BH1750 component must be initialized before starting worker");
        return;
    }

    if (s_worker_started) {
        return;
    }

    BaseType_t task_ok = xTaskCreate(bh1750_task, "bh1750_task", 4096, output_queue, 5, NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create bh1750_task");
        return;
    }

    s_worker_started = true;
}