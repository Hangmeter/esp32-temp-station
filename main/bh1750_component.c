#include "bh1750_component.h"

#include "bh1750.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BH1750_I2C_PORT I2C_NUM_0
#define BH1750_SDA_GPIO GPIO_NUM_18
#define BH1750_SCL_GPIO GPIO_NUM_19
#define BH1750_POLL_INTERVAL_MS 10000

#ifndef BH1750_CONTINUOUS_HIGH_RES_MODE
#define BH1750_CONTINUOUS_HIGH_RES_MODE BH1750_CONTINUE_1LX_RES
#endif

static const char *TAG = "BH1750_COMPONENT";

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static bh1750_handle_t s_bh1750 = NULL;
static bool s_is_initialized = false;
static bool s_worker_started = false;

static esp_err_t bh1750_get_lux(float *lux)
{
    if (s_bh1750 == NULL || lux == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return bh1750_get_data(s_bh1750, lux);
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

        if (bh1750_get_lux(&data.lux) == ESP_OK) {
            data.is_valid = true;
        } else {
            ESP_LOGW(TAG, "Failed to read BH1750 lux value");
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

    const i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = BH1750_I2C_PORT,
        .sda_io_num = BH1750_SDA_GPIO,
        .scl_io_num = BH1750_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = bh1750_create(s_i2c_bus, BH1750_I2C_ADDRESS_DEFAULT, &s_bh1750);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create BH1750 handle: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = bh1750_power_on(s_bh1750);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = bh1750_set_measure_mode(s_bh1750, BH1750_CONTINUOUS_HIGH_RES_MODE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set BH1750 high resolution continuous mode: %s", esp_err_to_name(ret));
        return ret;
    }

    s_is_initialized = true;
    ESP_LOGI(TAG, "BH1750 initialized on SDA GPIO %d, SCL GPIO %d, address 0x%02x",
             BH1750_SDA_GPIO,
             BH1750_SCL_GPIO,
             BH1750_I2C_ADDRESS_DEFAULT);
    return ESP_OK;
}

void bh1750_worker_start(QueueHandle_t output_queue)
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
