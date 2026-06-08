#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <time.h>

typedef struct {
    float  five_hour;
    float  seven_day;
    time_t reset_5h;
    time_t reset_7d;
    bool   fetching;
    bool   wifi_ok;
    bool   error;
    time_t last_ok_epoch;
} claude_state_t;

void claude_app_start(EventGroupHandle_t wifi_events);
