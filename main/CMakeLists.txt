idf_component_register(
    SRCS
        "main.c"
        "imu.c"
        "apps/clock_app.c"
        "apps/bitcoin_app.c"
        "views/clock_view.c"
        "views/bitcoin_view.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        driver
        esp_lcd
        espressif__esp_lcd_gc9107
        esp_lvgl_port
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        esp-tls
        driver
        json
        log
        freertos
)

target_link_libraries(${COMPONENT_LIB} PRIVATE m)
