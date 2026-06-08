#include "claude_app.h"
#include "shared_state.h"
#include "secrets.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>

static const char *TAG = "claude_app";

#define POLL_INTERVAL_S    120
#define WIFI_CONNECTED_BIT BIT0

#define USAGE_URL "https://api.anthropic.com/api/oauth/usage"
#define TOKEN_URL "https://platform.claude.com/v1/oauth/token"
#define CLAUDE_CLIENT_ID "9d1c250a-e61b-44d9-88ed-5944d1962f5e"

#define NVS_NAMESPACE "claude"
#define NVS_KEY_ACCESS  "access"
#define NVS_KEY_REFRESH "refresh"
#define NVS_KEY_SEED    "seed"     // the secrets.h refresh token NVS was seeded from

static char s_access_token[512];
static char s_refresh_token[512];  // updated if server rotates it
static char resp_buf[1024];
static int  resp_len;

// ── Token persistence (NVS) ───────────────────────────────────────────────────
static void tokens_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, NVS_KEY_ACCESS,  s_access_token);
    nvs_set_str(h, NVS_KEY_REFRESH, s_refresh_token);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "tokens saved to NVS");
}

// Seed NVS from secrets.h and record which seed was used.
static void tokens_seed_from_secrets(void)
{
    strlcpy(s_access_token,  CLAUDE_ACCESS_TOKEN,  sizeof(s_access_token));
    strlcpy(s_refresh_token, CLAUDE_REFRESH_TOKEN, sizeof(s_refresh_token));

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY_ACCESS,  s_access_token);
        nvs_set_str(h, NVS_KEY_REFRESH, s_refresh_token);
        nvs_set_str(h, NVS_KEY_SEED,    CLAUDE_REFRESH_TOKEN);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "seeded tokens from secrets.h");
}

// Load tokens. If secrets.h was manually changed since NVS was seeded, re-seed.
// Returns true if usable tokens are loaded.
static void tokens_init(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        tokens_seed_from_secrets();
        return;
    }

    char seed[512] = {0};
    size_t len = sizeof(seed);
    esp_err_t r = nvs_get_str(h, NVS_KEY_SEED, seed, &len);

    // If secrets.h refresh token differs from the recorded seed, the user
    // manually updated it — prefer secrets.h and re-seed NVS.
    if (r != ESP_OK || strcmp(seed, CLAUDE_REFRESH_TOKEN) != 0) {
        nvs_close(h);
        tokens_seed_from_secrets();
        return;
    }

    len = sizeof(s_access_token);
    nvs_get_str(h, NVS_KEY_ACCESS, s_access_token, &len);
    len = sizeof(s_refresh_token);
    nvs_get_str(h, NVS_KEY_REFRESH, s_refresh_token, &len);
    nvs_close(h);
    ESP_LOGI(TAG, "loaded tokens from NVS");
}

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

static void log_token_expiry(cJSON *root)
{
    cJSON *exp = cJSON_GetObjectItem(root, "expires_in");
    if (exp) {
        int secs = (int)exp->valuedouble;
        ESP_LOGI(TAG, "new token expires in: %dh %02dm", secs / 3600, (secs % 3600) / 60);
    }
}

static bool do_refresh(void)
{
    ESP_LOGI(TAG, "refreshing token...");

    resp_len = 0;
    memset(resp_buf, 0, sizeof(resp_buf));

    char body[640];
    snprintf(body, sizeof(body),
        "{\"grant_type\":\"refresh_token\",\"refresh_token\":\"%s\",\"client_id\":\"%s\"}",
        s_refresh_token, CLAUDE_CLIENT_ID);

    esp_http_client_config_t cfg = {
        .url               = TOKEN_URL,
        .event_handler     = http_evt,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "refresh failed: HTTP %d", status);
        return false;
    }

    cJSON *root = cJSON_Parse(resp_buf);
    if (!root) {
        ESP_LOGE(TAG, "refresh: invalid JSON");
        return false;
    }

    cJSON *tok = cJSON_GetObjectItem(root, "access_token");
    cJSON *rtok = cJSON_GetObjectItem(root, "refresh_token");

    if (!tok || !tok->valuestring) {
        ESP_LOGE(TAG, "refresh: no access_token in response");
        cJSON_Delete(root);
        return false;
    }

    strlcpy(s_access_token, tok->valuestring, sizeof(s_access_token));
    ESP_LOGI(TAG, "new CLAUDE_ACCESS_TOKEN:\n%s", s_access_token);

    if (rtok && rtok->valuestring) {
        strlcpy(s_refresh_token, rtok->valuestring, sizeof(s_refresh_token));
        ESP_LOGI(TAG, "new CLAUDE_REFRESH_TOKEN:\n%s", s_refresh_token);
    }

    log_token_expiry(root);
    cJSON_Delete(root);

    tokens_save();  // persist so they survive reboots
    return true;
}

static int fetch_usage(void)
{
    resp_len = 0;
    memset(resp_buf, 0, sizeof(resp_buf));

    char auth[560];
    snprintf(auth, sizeof(auth), "Bearer %s", s_access_token);

    esp_http_client_config_t cfg = {
        .url               = USAGE_URL,
        .event_handler     = http_evt,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "anthropic-beta", "oauth-2025-04-20");
    esp_http_client_set_header(client, "User-Agent", "claude-code/1.0");

    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    // status > 0 means we got an HTTP response — use it even if err != ESP_OK
    if (status > 0) return status;
    if (err != ESP_OK) return -1;
    return status;
}

// Parse ISO-8601 UTC timestamp: "2026-06-08T00:20:00.707303+00:00"
static time_t parse_utc(const char *s)
{
    int Y, M, D, h, m, sec = 0;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec) < 5) return 0;
    struct tm tm = {
        .tm_year = Y - 1900, .tm_mon = M - 1, .tm_mday = D,
        .tm_hour = h, .tm_min = m, .tm_sec = sec, .tm_isdst = 0,
    };
    setenv("TZ", "UTC0", 1); tzset();
    time_t t = mktime(&tm);
    setenv("TZ", "ART3", 1); tzset();
    return t;
}

static void poll_usage(void)
{
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.claude.fetching = true;
    xSemaphoreGive(g_state_mutex);

    ESP_LOGI(TAG, "fetching usage...");
    int status = fetch_usage();

    if (status == 401) {
        ESP_LOGW(TAG, "401 — access token expired, refreshing...");
        if (do_refresh()) {
            ESP_LOGI(TAG, "retrying usage fetch with new token");
            status = fetch_usage();
        }
    } else if (status == 429) {
        ESP_LOGW(TAG, "429 — rate limited, retrying in 30s");
        vTaskDelay(pdMS_TO_TICKS(30000));
        status = fetch_usage();
    }

    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.claude.fetching = false;

    if (status != 200) {
        g_state.claude.error = true;
        xSemaphoreGive(g_state_mutex);
        ESP_LOGE(TAG, "usage fetch failed: HTTP %d", status);
        return;
    }

    cJSON *root   = cJSON_Parse(resp_buf);
    cJSON *five_h  = root ? cJSON_GetObjectItem(root, "five_hour")  : NULL;
    cJSON *seven_d = root ? cJSON_GetObjectItem(root, "seven_day")  : NULL;

    if (!five_h || !seven_d) {
        g_state.claude.error = true;
        xSemaphoreGive(g_state_mutex);
        cJSON_Delete(root);
        return;
    }

    cJSON *u5  = cJSON_GetObjectItem(five_h,  "utilization");
    cJSON *u7  = cJSON_GetObjectItem(seven_d, "utilization");
    cJSON *r5s = cJSON_GetObjectItem(five_h,  "resets_at");
    cJSON *r7s = cJSON_GetObjectItem(seven_d, "resets_at");

    g_state.claude.error = false;
    if (u5) g_state.claude.five_hour = (float)u5->valuedouble;
    if (u7) g_state.claude.seven_day = (float)u7->valuedouble;
    if (r5s && r5s->valuestring) g_state.claude.reset_5h = parse_utc(r5s->valuestring);
    if (r7s && r7s->valuestring) g_state.claude.reset_7d = parse_utc(r7s->valuestring);
    g_state.claude.last_ok_epoch = time(NULL);

    ESP_LOGI(TAG, "usage: 5h=%.0f%%  7d=%.0f%%",
             g_state.claude.five_hour, g_state.claude.seven_day);

    xSemaphoreGive(g_state_mutex);
    cJSON_Delete(root);
}

static EventGroupHandle_t s_wifi_events;

static void claude_task(void *arg)
{
    (void)arg;
    xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    tokens_init();

    vTaskDelay(pdMS_TO_TICKS(10000));  // wait 10s so bitcoin fetch goes first

    while (1) {
        poll_usage();
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_S * 1000));
    }
}

void claude_app_start(EventGroupHandle_t wifi_events)
{
    s_wifi_events = wifi_events;
    xTaskCreatePinnedToCore(claude_task, "claude", 8192, NULL, 2, NULL, 0);
}
