#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

void clock_app_start(EventGroupHandle_t wifi_events);
