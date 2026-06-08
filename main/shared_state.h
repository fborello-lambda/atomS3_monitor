#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "apps/bitcoin_app.h"
#include "apps/claude_app.h"

typedef struct {
    btc_state_t    btc;
    claude_state_t claude;
} app_state_t;

extern app_state_t       g_state;
extern SemaphoreHandle_t g_state_mutex;
