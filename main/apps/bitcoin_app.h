#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <time.h>

typedef struct {
    double btc_usd;
    double change_24h;
    bool   fetching;
    bool   wifi_ok;
    bool   error;
    char   error_msg[32];
    time_t last_ok_epoch;
} btc_state_t;

void bitcoin_app_start(EventGroupHandle_t wifi_events);
