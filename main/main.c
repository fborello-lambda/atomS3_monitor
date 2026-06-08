#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9107.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "secrets.h"
#include "shared_state.h"
#include "imu.h"
#include "apps/clock_app.h"
#include "apps/bitcoin_app.h"
#include "apps/claude_app.h"
#include "views/views.h"

static const char *TAG = "main";

#define LCD_HOST     SPI2_HOST
#define LCD_PIN_MOSI 21
#define LCD_PIN_CLK  17
#define LCD_PIN_CS   15
#define LCD_PIN_DC   33
#define LCD_PIN_RST  34
#define LCD_PIN_BL   16
#define LCD_WIDTH    128
#define LCD_HEIGHT   128

// ── Shared state ──────────────────────────────────────────────────────────────
app_state_t       g_state       = {0};
SemaphoreHandle_t g_state_mutex = NULL;

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t wifi_events;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupSetBits(wifi_events, WIFI_FAIL_BIT);
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        g_state.btc.wifi_ok = false;
        xSemaphoreGive(g_state_mutex);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        g_state.btc.wifi_ok = true;
        xSemaphoreGive(g_state_mutex);
    }
}

static void wifi_init(void)
{
    wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,    &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,   IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wcfg = {
        .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK },
    };
    strlcpy((char *)wcfg.sta.ssid,     WIFI_SSID, sizeof(wcfg.sta.ssid));
    strlcpy((char *)wcfg.sta.password, WIFI_PASS, sizeof(wcfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
}

// ── Display ───────────────────────────────────────────────────────────────────
static esp_lcd_panel_handle_t    panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle    = NULL;

static void display_init(void)
{
    spi_bus_config_t buscfg = GC9107_PANEL_BUS_SPI_CONFIG(
        LCD_PIN_CLK, LCD_PIN_MOSI, LCD_WIDTH * LCD_HEIGHT * 2);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg =
        GC9107_PANEL_IO_SPI_CONFIG(LCD_PIN_CS, LCD_PIN_DC, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io_handle));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9107(io_handle, &panel_cfg, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 2, 1));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    gpio_config_t bl = { .pin_bit_mask = 1ULL << LCD_PIN_BL, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl);
    gpio_set_level(LCD_PIN_BL, 1);
}

// ── LVGL ──────────────────────────────────────────────────────────────────────
static void lvgl_init(void)
{
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_priority   = 4;
    lvgl_cfg.task_stack      = 6144;
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = io_handle,
        .panel_handle  = panel_handle,
        .buffer_size   = LCD_WIDTH * 20,
        .double_buffer = false,
        .hres          = LCD_WIDTH,
        .vres          = LCD_HEIGHT,
        .monochrome    = false,
        .color_format  = LV_COLOR_FORMAT_RGB565_SWAPPED,
        .rotation      = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags         = { .buff_dma = true },
    };
    if (!lvgl_port_add_disp(&disp_cfg)) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        abort();
    }
}

// ── View manager ──────────────────────────────────────────────────────────────
#define NUM_VIEWS 3
static lv_obj_t *views[NUM_VIEWS];
static int current_view = 0;

static void view_next(void)
{
    current_view = (current_view + 1) % NUM_VIEWS;
    lvgl_port_lock(0);
    lv_scr_load_anim(views[current_view], LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    lvgl_port_unlock();
}

static void on_tap(void) { view_next(); }

// ── Timers ────────────────────────────────────────────────────────────────────
static void clock_timer_cb(lv_timer_t *t)   { (void)t; clock_view_tick(); }
static void btc_timer_cb(lv_timer_t *t)     { (void)t; bitcoin_view_update(); }
static void claude_timer_cb(lv_timer_t *t)  { (void)t; claude_view_update(); }

// ── Entry point ───────────────────────────────────────────────────────────────
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    g_state_mutex = xSemaphoreCreateMutex();

    wifi_init();

    display_init();
    lvgl_init();

    lvgl_port_lock(0);
    views[0] = clock_view_create();
    views[1] = bitcoin_view_create();
    views[2] = claude_view_create();
    lv_scr_load(views[0]);
    lv_timer_create(clock_timer_cb,  1000, NULL);
    lv_timer_create(btc_timer_cb,     500, NULL);
    lv_timer_create(claude_timer_cb,  500, NULL);
    lvgl_port_unlock();

    clock_app_start(wifi_events);
    bitcoin_app_start(wifi_events);
    claude_app_start(wifi_events);

    if (imu_init())
        imu_start_tap_task(on_tap);
    else
        ESP_LOGW(TAG, "IMU not found");

    ESP_LOGI(TAG, "running");
}
