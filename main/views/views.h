#pragma once
#include "lvgl.h"

lv_obj_t *clock_view_create(void);
void       clock_view_tick(void);

lv_obj_t *bitcoin_view_create(void);
void       bitcoin_view_update(void);

lv_obj_t *claude_view_create(void);
void       claude_view_update(void);
