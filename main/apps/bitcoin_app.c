#include "bitcoin_app.h"
#include "shared_state.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>

static const char *TAG = "bitcoin_app";

#define POLL_INTERVAL_S 300
#define WIFI_CONNECTED_BIT BIT0

#define COINGECKO_URL \
    "https://api.coingecko.com/api/v3/simple/price" \
    "?ids=bitcoin&vs_currencies=usd&include_24hr_change=true"

static char resp_buf[512];
static int  resp_len;

static esp_err_t http_evt(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy = evt->data_len;
        if (resp_len + copy >= (int)sizeof(resp_buf) - 1)
            copy = (int)sizeof(resp_buf) - 1 - resp_len;
        if (copy > 0) {
            memcpy(resp_buf + resp_len, evt->data, copy);
            resp_len += copy;
        }
    }
    return ESP_OK;
}

static void fetch_price(void)
{
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.btc.fetching = true;
    xSemaphoreGive(g_state_mutex);

    resp_len = 0;
    memset(resp_buf, 0, sizeof(resp_buf));

    esp_http_client_config_t cfg = {
        .url               = COINGECKO_URL,
        .event_handler     = http_evt,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.btc.fetching = false;

    if (err != ESP_OK || status != 200) {
        g_state.btc.error = true;
        snprintf(g_state.btc.error_msg, sizeof(g_state.btc.error_msg), "HTTP %d", status);
        xSemaphoreGive(g_state_mutex);
        return;
    }

    cJSON *root = cJSON_Parse(resp_buf);
    cJSON *btc  = root ? cJSON_GetObjectItem(root, "bitcoin") : NULL;
    if (!btc) {
        g_state.btc.error = true;
        snprintf(g_state.btc.error_msg, sizeof(g_state.btc.error_msg), "parse err");
        xSemaphoreGive(g_state_mutex);
        cJSON_Delete(root);
        return;
    }

    cJSON *j_usd = cJSON_GetObjectItem(btc, "usd");
    cJSON *j_chg = cJSON_GetObjectItem(btc, "usd_24h_change");

    g_state.btc.error         = false;
    if (j_usd) g_state.btc.btc_usd    = j_usd->valuedouble;
    if (j_chg) g_state.btc.change_24h = j_chg->valuedouble;
    g_state.btc.last_ok_epoch = time(NULL);

    ESP_LOGI(TAG, "BTC $%.0f  24h %+.2f%%", g_state.btc.btc_usd, g_state.btc.change_24h);

    xSemaphoreGive(g_state_mutex);
    cJSON_Delete(root);
}

static EventGroupHandle_t s_wifi_events;

static void fetch_task(void *arg)
{
    (void)arg;
    xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    while (1) {
        fetch_price();
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_S * 1000));
    }
}

void bitcoin_app_start(EventGroupHandle_t wifi_events)
{
    s_wifi_events = wifi_events;
    xTaskCreatePinnedToCore(fetch_task, "btc_fetch", 8192, NULL, 2, NULL, 0);
}
