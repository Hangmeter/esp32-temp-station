#include <stdio.h>
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_err.h"

static const char *TAG = "TEMP_SENSORS";

// Настройки Wi-Fi и MQTT
#define WIFI_SSID            "ASUS_10"
#define WIFI_PASS            "Protei_ST!"
#define MQTT_BROKER_URL      "mqtt://192.168.8.126" // Укажите IP вашего брокера


// Определяем enum тип device_state_t для состояний устройства  
typedef enum {
    ST_INIT,
    ST_CONNECTING,
    ST_READING,
    ST_PUBLISHING,
    ST_ERROR
} device_state_t;

// Глобальная переменная для хранения текущего состояния устройства
static device_state_t current_state = ST_INIT;
// Глобальная переменная для MQTT-клиента
static esp_mqtt_client_handle_t mqtt_client;
// Глобальная переменная для Wi-Fi подключения
static volatile bool is_connected = false;

// Обработчик событий MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            is_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected");
            is_connected = false;
            break;
        default:
            break;
    }
}

// Обработчик событий Wi-Fi
static void wifi_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); // Драйвер готов, подключаемся к роутеру

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected! Retrying...");
        is_connected = false;
        esp_wifi_connect(); // Автоматическое переподключение при обрыве Wi-Fi
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        vTaskDelay(pdMS_TO_TICKS(5000)); // Небольшая задержка перед запуском MQTT
        //esp_mqtt_client_start(mqtt_client); // Запускаем MQTT только после получения IP
    }
}

static void wifi_init_station(void) {
    ESP_LOGI(TAG, "Initialasing network interface");
    ESP_ERROR_CHECK (esp_netif_init());
    ESP_LOGI(TAG, "esp_event_loop_create_default");
    ESP_ERROR_CHECK (esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t init_wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK (esp_wifi_init(&init_wifi_config));

    // Регистрируем наш обработчик на любые события Wi-Fi и получение IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Режим клиента
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start()); // Запуск Wi-Fi драйвера (вызовет событие STA_START)
}

static void mqtt_init_client(void) {
    // Создает структуру настроек, где указывает адрес вашего MQTT-брокера.
    const esp_mqtt_client_config_t mqtt_cfg = { .broker.address.uri = MQTT_BROKER_URL };
    // Выделяет оперативную память под экземпляр MQTT-клиента
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    // Регистрирует обработчик событий MQTT-клиента, который будет вызываться при различных событиях, таких как подключение, отключение, получение данных и т
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    // Запускает MQTT-клиент, устанавливая соединение с брокером и позволяя ему обрабатывать события.
    // Этот вызов не блокирует основной код 
    esp_mqtt_client_start(mqtt_client);
} 

void app_main(void)
{
 esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Инициализация Wi-Fi
    wifi_init_station();
    // mqtt_init_client();
    
    while (1) {
        switch (current_state) {
            case ST_INIT:
                ESP_LOGI(TAG, "Initializing device...");
                // Инициализация датчиков и других компонентов
                vTaskDelay(pdMS_TO_TICKS(3000)); // Задержка для имитации инициализации
                current_state = ST_CONNECTING;
                break;

            case ST_CONNECTING:
                ESP_LOGI(TAG, "Connecting to Wi-Fi...");
                // Код для подключения к Wi-Fi
                // Если подключение успешно, переходим к следующему состоянию
                vTaskDelay(pdMS_TO_TICKS(3000)); // Задержка для имитации инициализации
                current_state = ST_READING;
                break;

            case ST_READING:
                ESP_LOGI(TAG, "Reading sensor data...");
                // Код для чтения данных с датчиков
                // Если данные успешно считаны, переходим к следующему состоянию
                current_state = ST_PUBLISHING;
                break;

            case ST_PUBLISHING:
                ESP_LOGI(TAG, "Publishing data to MQTT broker...");
                // Код для публикации данных на MQTT-брокер
                // Если публикация успешна, возвращаемся к состоянию чтения данных
                current_state = ST_READING;
                break;

            case ST_ERROR:
                ESP_LOGE(TAG, "An error occurred!");
                // Код для обработки ошибок
                // В зависимости от типа ошибки можно попытаться восстановиться или перезагрузить устройство
                break;

            default:
                ESP_LOGW(TAG, "Unknown state!");
                current_state = ST_ERROR;
                break;
        }
    }
}