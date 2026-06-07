#include "clock_app.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <time.h>

static const char *TAG = "clock_app";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_events;

static void sntp_task(void *arg)
{
    (void)arg;
    xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    setenv("TZ", "ART3", 1);
    tzset();

    esp_sntp_config_t scfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&scfg);

    for (int i = 0; i < 10; i++) {
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(500)) == ESP_OK) {
            ESP_LOGI(TAG, "time synced");
            break;
        }
    }

    vTaskDelete(NULL);
}

void clock_app_start(EventGroupHandle_t wifi_events)
{
    s_wifi_events = wifi_events;
    xTaskCreatePinnedToCore(sntp_task, "sntp", 3072, NULL, 2, NULL, 0);
}
