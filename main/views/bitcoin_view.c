#include "views.h"
#include "shared_state.h"
#include <stdio.h>
#include <time.h>

static lv_obj_t *lbl_usd;
static lv_obj_t *lbl_change;
static lv_obj_t *lbl_updated;
static lv_obj_t *lbl_wifi;

static void fmt_usd(double v, char *buf, size_t n)
{
    if      (v >= 1e6) snprintf(buf, n, "$%.2fM", v / 1e6);
    else if (v >= 1e3) snprintf(buf, n, "$%.1fk", v / 1e3);
    else               snprintf(buf, n, "$%.0f",  v);
}

lv_obj_t *bitcoin_view_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Title bar
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 128, 18);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x0a0a16), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, "BITCOIN");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF7931A), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, -6, 0);

    lbl_wifi = lv_label_create(bar);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_wifi, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lbl_wifi, LV_ALIGN_RIGHT_MID, -2, 0);

    // USD tag
    lv_obj_t *lbl_usd_tag = lv_label_create(scr);
    lv_label_set_text(lbl_usd_tag, "USD");
    lv_obj_set_style_text_font(lbl_usd_tag, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_usd_tag, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_usd_tag, LV_ALIGN_TOP_LEFT, 4, 22);

    // Price
    lbl_usd = lv_label_create(scr);
    lv_label_set_text(lbl_usd, "---");
    lv_obj_set_style_text_font(lbl_usd, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_usd, lv_color_white(), 0);
    lv_obj_align(lbl_usd, LV_ALIGN_TOP_MID, 0, 38);

    // 24h change
    lbl_change = lv_label_create(scr);
    lv_label_set_text(lbl_change, "24h  ---");
    lv_obj_set_style_text_font(lbl_change, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_change, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_change, LV_ALIGN_TOP_MID, 0, 82);

    // Status bar
    lv_obj_t *status_bg = lv_obj_create(scr);
    lv_obj_set_size(status_bg, 128, 14);
    lv_obj_align(status_bg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(status_bg, lv_color_hex(0x0c0c14), 0);
    lv_obj_set_style_border_width(status_bg, 0, 0);
    lv_obj_set_style_pad_all(status_bg, 0, 0);
    lv_obj_set_style_radius(status_bg, 0, 0);

    lbl_updated = lv_label_create(status_bg);
    lv_label_set_text(lbl_updated, "connecting...");
    lv_obj_set_style_text_font(lbl_updated, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_updated, lv_color_hex(0x607080), 0);
    lv_obj_align(lbl_updated, LV_ALIGN_LEFT_MID, 4, 0);

    return scr;
}

void bitcoin_view_update(void)
{
    if (!lbl_usd) return;

    btc_state_t s;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    s = g_state.btc;
    xSemaphoreGive(g_state_mutex);

    lv_obj_set_style_text_color(lbl_wifi,
        s.wifi_ok ? lv_palette_main(LV_PALETTE_GREEN)
                  : lv_palette_main(LV_PALETTE_RED), 0);

    if (s.error) {
        lv_label_set_text(lbl_usd, "ERR");
        return;
    }

    if (s.btc_usd == 0.0) {
        lv_label_set_text(lbl_updated, s.fetching ? "fetching..." : "no data");
        return;
    }

    char buf[32];

    fmt_usd(s.btc_usd, buf, sizeof(buf));
    lv_label_set_text(lbl_usd, buf);

    double ch = s.change_24h;
    snprintf(buf, sizeof(buf), "24h  %+.2f%%", ch);
    lv_label_set_text(lbl_change, buf);
    lv_obj_set_style_text_color(lbl_change,
        ch >= 0 ? lv_palette_main(LV_PALETTE_GREEN)
                : lv_palette_main(LV_PALETTE_RED), 0);

    if (s.last_ok_epoch > 0) {
        struct tm t;
        localtime_r(&s.last_ok_epoch, &t);
        int h = t.tm_hour % 24, m = t.tm_min % 60;
        snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
        lv_label_set_text(lbl_updated, buf);
        lv_obj_set_style_text_color(lbl_updated, lv_color_hex(0x607080), 0);
    } else if (s.fetching) {
        lv_label_set_text(lbl_updated, "updating...");
    }
}
