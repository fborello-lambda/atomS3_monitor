#include "imu.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static const char *TAG = "imu";

#define I2C_PORT    I2C_NUM_0
#define I2C_FREQ_HZ 100000

#define REG_WHO_AM_I   0x75
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_CFG  0x1C
#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG     0x1A
#define REG_ACCEL_XOUT 0x3B

#define ACCEL_SCALE     4096.0f
#define TAP_THRESHOLD_G 1.5f
#define TAP_DEBOUNCE_MS 400

static esp_err_t imu_write(uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6886_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return r;
}

static esp_err_t imu_read(uint8_t reg, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6886_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);  // repeated start
    i2c_master_write_byte(cmd, (MPU6886_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1)
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return r;
}

bool imu_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(200));  // wait for MPU6886 power-on reset

    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = IMU_SDA_GPIO,
        .scl_io_num       = IMU_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    uint8_t who = 0;
    esp_err_t r = imu_read(REG_WHO_AM_I, &who, 1);
    ESP_LOGI(TAG, "WHO_AM_I=0x%02x (err=%d)  — 0x19=MPU6886 0x60=ICM42607 0x67=ICM42670", who, r);

    if (r != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed");
        return false;
    }

    imu_write(REG_PWR_MGMT_1, 0x01);  // wake, gyro clock
    vTaskDelay(pdMS_TO_TICKS(10));
    imu_write(REG_SMPLRT_DIV, 9);     // 100 Hz
    imu_write(REG_CONFIG,     0x03);  // DLPF ~44 Hz
    imu_write(REG_ACCEL_CFG,  0x10);  // ±8g

    ESP_LOGI(TAG, "IMU ready");
    return true;
}

// ── Tap detection task ────────────────────────────────────────────────────────
typedef struct { void (*cb)(void); } tap_arg_t;

static void tap_task(void *arg)
{
    tap_arg_t *a = (tap_arg_t *)arg;
    void (*on_tap)(void) = a->cb;
    free(a);

    TickType_t last_tap = 0;
    float prev_mag = 1.0f;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));

        uint8_t raw[6];
        if (imu_read(REG_ACCEL_XOUT, raw, 6) != ESP_OK) continue;

        int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
        int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
        int16_t az = (int16_t)((raw[4] << 8) | raw[5]);

        float gx  = ax / ACCEL_SCALE;
        float gy  = ay / ACCEL_SCALE;
        float gz  = az / ACCEL_SCALE;
        float mag = sqrtf(gx*gx + gy*gy + gz*gz);

        TickType_t now = xTaskGetTickCount();
        if (prev_mag < 1.2f && mag > TAP_THRESHOLD_G &&
            (now - last_tap) > pdMS_TO_TICKS(TAP_DEBOUNCE_MS))
        {
            ESP_LOGI(TAG, "tap! mag=%.2fg", mag);
            last_tap = now;
            on_tap();
        }
        prev_mag = mag;
    }
}

void imu_start_tap_task(void (*on_tap)(void))
{
    tap_arg_t *arg = malloc(sizeof(*arg));
    arg->cb = on_tap;
    xTaskCreatePinnedToCore(tap_task, "imu", 3072, arg, 3, NULL, 1);
}
