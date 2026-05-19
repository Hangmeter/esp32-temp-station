idf_component_register(
    SRCS
        "main.c"
        "wifi_component.c"
        "mqtt_component.c"
        "sensor_component.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif mqtt esp_event esp_system
)
