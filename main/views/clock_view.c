#include "views.h"
#include <time.h>
#include <stdio.h>

static lv_obj_t *label_time = NULL;
static lv_obj_t *label_date = NULL;

lv_obj_t *clock_view_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    label_time = lv_label_create(scr);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);
    lv_label_set_text(label_time, "00:00");
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, -18);

    lv_obj_t *label_sec = lv_label_create(scr);
    lv_obj_set_style_text_font(label_sec, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_sec, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_label_set_text(label_sec, ":00");
    lv_obj_align_to(label_sec, label_time, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 2);

    label_date = lv_label_create(scr);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_date, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_label_set_text(label_date, "---");
    lv_obj_align(label_date, LV_ALIGN_CENTER, 0, 38);

    lv_obj_set_user_data(scr, label_sec);
    clock_view_tick();
    return scr;
}

void clock_view_tick(void)
{
    if (!label_time) return;

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    char hhmm[8], ss[5], date[24];
    int hour = t.tm_hour % 24, min = t.tm_min % 60, sec = t.tm_sec % 61;
    int year = (t.tm_year + 1900) % 10000;
    int mon  = (t.tm_mon  + 1)   % 13;
    int mday = t.tm_mday         % 32;
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", hour, min);
    snprintf(ss,   sizeof(ss),   ":%02d",     sec);
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, mon, mday);

    lv_label_set_text(label_time, hhmm);
    lv_label_set_text(label_date, date);

    lv_obj_t *scr       = lv_obj_get_parent(label_time);
    lv_obj_t *label_sec = lv_obj_get_user_data(scr);
    if (label_sec) {
        lv_label_set_text(label_sec, ss);
        lv_obj_align_to(label_sec, label_time, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 2);
    }
}
