#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "apps/bitcoin_app.h"

typedef struct {
    btc_state_t btc;
} app_state_t;

extern app_state_t       g_state;
extern SemaphoreHandle_t g_state_mutex;
