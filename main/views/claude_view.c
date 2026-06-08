#include "views.h"
#include "shared_state.h"
#include <stdio.h>
#include <time.h>

#define CLAUDE_ORANGE 0xC15F3C

static lv_obj_t *lbl_clock;
static lv_obj_t *lbl_fetch;
static lv_obj_t *lbl_5h_pct;
static lv_obj_t *bar_5h;
static lv_obj_t *lbl_7d_pct;
static lv_obj_t *bar_7d;
static lv_obj_t *lbl_rst_5h;
static lv_obj_t *lbl_rst_7d;
static lv_obj_t *lbl_wifi;

// Layout (128x128):
//  0-18  title bar
// 22-36  HH:MM clock
// 46-56  5h  [bar]  xx%
// 68-78  7d  [bar]  xx%
// 90     5h rst HH:MM
// 104    7d rst dd/mm HH:MM

lv_obj_t *claude_view_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // ── Title bar ────────────────────────────────────────────────────────────
    lv_obj_t *titlebar = lv_obj_create(scr);
    lv_obj_set_size(titlebar, 128, 18);
    lv_obj_align(titlebar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(titlebar, lv_color_hex(0x0a0a16), 0);
    lv_obj_set_style_border_width(titlebar, 0, 0);
    lv_obj_set_style_pad_all(titlebar, 0, 0);
    lv_obj_set_style_radius(titlebar, 0, 0);

    lv_obj_t *title = lv_label_create(titlebar);
    lv_label_set_text(title, "CLAUDE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(CLAUDE_ORANGE), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, -6, 0);

    lbl_wifi = lv_label_create(titlebar);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_wifi, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lbl_wifi, LV_ALIGN_RIGHT_MID, -2, 0);

    // ── Clock HH:MM + last fetch time ─────────────────────────────────────────
    lbl_clock = lv_label_create(scr);
    lv_label_set_text(lbl_clock, "--:--");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl_clock, LV_ALIGN_TOP_MID, -16, 22);

    lbl_fetch = lv_label_create(scr);
    lv_label_set_text(lbl_fetch, "");
    lv_obj_set_style_text_font(lbl_fetch, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_fetch, lv_color_hex(0x555555), 0);
    lv_obj_align_to(lbl_fetch, lbl_clock, LV_ALIGN_OUT_RIGHT_MID, 6, 1);

    // ── 5h row ───────────────────────────────────────────────────────────────
    lv_obj_t *tag5 = lv_label_create(scr);
    lv_label_set_text(tag5, "5h");
    lv_obj_set_style_text_font(tag5, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(tag5, lv_color_hex(0x888888), 0);
    lv_obj_align(tag5, LV_ALIGN_TOP_LEFT, 6, 46);

    bar_5h = lv_bar_create(scr);
    lv_obj_set_size(bar_5h, 72, 8);
    lv_obj_align(bar_5h, LV_ALIGN_TOP_LEFT, 26, 50);
    lv_bar_set_range(bar_5h, 0, 100);
    lv_bar_set_value(bar_5h, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_5h, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_5h, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_5h, lv_color_hex(CLAUDE_ORANGE), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_5h, 3, LV_PART_INDICATOR);

    lbl_5h_pct = lv_label_create(scr);
    lv_label_set_text(lbl_5h_pct, "--%");
    lv_obj_set_style_text_font(lbl_5h_pct, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_5h_pct, lv_color_white(), 0);
    lv_obj_align(lbl_5h_pct, LV_ALIGN_TOP_RIGHT, -4, 46);

    // ── 7d row ───────────────────────────────────────────────────────────────
    lv_obj_t *tag7 = lv_label_create(scr);
    lv_label_set_text(tag7, "7d");
    lv_obj_set_style_text_font(tag7, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(tag7, lv_color_hex(0x888888), 0);
    lv_obj_align(tag7, LV_ALIGN_TOP_LEFT, 6, 68);

    bar_7d = lv_bar_create(scr);
    lv_obj_set_size(bar_7d, 72, 8);
    lv_obj_align(bar_7d, LV_ALIGN_TOP_LEFT, 26, 72);
    lv_bar_set_range(bar_7d, 0, 100);
    lv_bar_set_value(bar_7d, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_7d, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_7d, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_7d, lv_color_hex(CLAUDE_ORANGE), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_7d, 3, LV_PART_INDICATOR);

    lbl_7d_pct = lv_label_create(scr);
    lv_label_set_text(lbl_7d_pct, "--%");
    lv_obj_set_style_text_font(lbl_7d_pct, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_7d_pct, lv_color_white(), 0);
    lv_obj_align(lbl_7d_pct, LV_ALIGN_TOP_RIGHT, -4, 68);

    // ── Reset times ───────────────────────────────────────────────────────────
    lbl_rst_5h = lv_label_create(scr);
    lv_label_set_text(lbl_rst_5h, "5h rst: --:--");
    lv_obj_set_style_text_font(lbl_rst_5h, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_rst_5h, lv_color_hex(0x607080), 0);
    lv_obj_align(lbl_rst_5h, LV_ALIGN_TOP_MID, 0, 90);

    lbl_rst_7d = lv_label_create(scr);
    lv_label_set_text(lbl_rst_7d, "7d rst: --/--");
    lv_obj_set_style_text_font(lbl_rst_7d, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_rst_7d, lv_color_hex(0x607080), 0);
    lv_obj_align(lbl_rst_7d, LV_ALIGN_TOP_MID, 0, 104);

    return scr;
}

static lv_color_t usage_color(float pct)
{
    if (pct >= 80.0f) return lv_palette_main(LV_PALETTE_RED);
    if (pct >= 50.0f) return lv_palette_main(LV_PALETTE_YELLOW);
    return lv_color_hex(CLAUDE_ORANGE);
}

void claude_view_update(void)
{
    if (!lbl_clock) return;

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour % 24, t.tm_min % 60);
    lv_label_set_text(lbl_clock, buf);

    claude_state_t s;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    s = g_state.claude;
    xSemaphoreGive(g_state_mutex);

    lv_obj_set_style_text_color(lbl_wifi,
        s.wifi_ok ? lv_palette_main(LV_PALETTE_GREEN)
                  : lv_palette_main(LV_PALETTE_RED), 0);

    if (s.error) {
        lv_label_set_text(lbl_5h_pct, "ERR");
        lv_label_set_text(lbl_7d_pct, "ERR");
        return;
    }

    if (s.last_ok_epoch == 0) {
        lv_label_set_text(lbl_rst_5h, s.fetching ? "fetching..." : "no data");
        return;
    }

    // Last fetch time next to the clock
    struct tm tf;
    localtime_r(&s.last_ok_epoch, &tf);
    snprintf(buf, sizeof(buf), "/ %02d:%02d", tf.tm_hour % 24, tf.tm_min % 60);
    lv_label_set_text(lbl_fetch, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", s.five_hour);
    lv_label_set_text(lbl_5h_pct, buf);
    lv_bar_set_value(bar_5h, (int32_t)s.five_hour, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_5h, usage_color(s.five_hour), LV_PART_INDICATOR);

    snprintf(buf, sizeof(buf), "%.0f%%", s.seven_day);
    lv_label_set_text(lbl_7d_pct, buf);
    lv_bar_set_value(bar_7d, (int32_t)s.seven_day, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_7d, usage_color(s.seven_day), LV_PART_INDICATOR);

    struct tm tr;
    if (s.reset_5h > 0) {
        localtime_r(&s.reset_5h, &tr);
        snprintf(buf, sizeof(buf), "5h rst: %02d:%02d", tr.tm_hour % 24, tr.tm_min % 60);
        lv_label_set_text(lbl_rst_5h, buf);
    }
    if (s.reset_7d > 0) {
        localtime_r(&s.reset_7d, &tr);
        snprintf(buf, sizeof(buf), "7d rst: %02d/%02d %02d:%02d",
                 tr.tm_mday % 32, (tr.tm_mon + 1) % 13, tr.tm_hour % 24, tr.tm_min % 60);
        lv_label_set_text(lbl_rst_7d, buf);
    }
}
