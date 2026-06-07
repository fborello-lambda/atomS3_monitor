# ATOMS3 Monitor

A desktop monitor firmware for the [M5Stack AtomS3](https://docs.m5stack.com/en/core/AtomS3) — a tiny ESP32-S3 device with a 128×128 IPS display.

Displays a clock and Bitcoin price, switchable via tap on the built-in IMU.

## Views

- **Clock** — local time (Argentina / GMT-3) with date
- **Bitcoin** — BTC/USD price and 24h change from CoinGecko, updated every 5 minutes

Tap the device to cycle between views.

## Hardware

| | |
|---|---|
| MCU | ESP32-S3 (M5Stack AtomS3) |
| Flash | 8 MB |
| Display | GC9107 128×128 IPS via SPI |
| IMU | MPU6886 via I2C (tap detection) |

## Requirements

- [ESP-IDF](https://github.com/espressif/esp-idf) v5.2+
- Components fetched automatically via `idf_component.yml`: LVGL 9.2, esp_lvgl_port, esp_lcd_gc9107

## Setup

1. Clone the repo
2. Copy credentials:
   ```
   cp main/secrets.h.example main/secrets.h
   ```
3. Edit `main/secrets.h` with your WiFi SSID and password
4. Copy IDE config (optional):
   ```
   cp .vscode/settings.json.example .vscode/settings.json
   ```
5. Build and flash:
   ```
   idf.py set-target esp32s3
   idf.py build flash monitor
   ```

## Project structure

```
main/
  main.c              # WiFi, display, LVGL init, view manager
  imu.c/h             # MPU6886 tap detection
  shared_state.h      # shared state across apps and views
  apps/
    clock_app.c/h     # SNTP time sync
    bitcoin_app.c/h   # CoinGecko fetch + btc_state_t
  views/
    views.h
    clock_view.c      # LVGL clock screen
    bitcoin_view.c    # LVGL bitcoin screen
```
