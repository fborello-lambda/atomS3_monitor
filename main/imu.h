#pragma once
#include <stdbool.h>

// AtomS3 internal I2C for MPU6886
#define IMU_SDA_GPIO  38
#define IMU_SCL_GPIO  39
#define MPU6886_ADDR  0x68

// Call once at startup. Returns false if IMU not found.
bool imu_init(void);

// Starts a FreeRTOS task that calls on_tap() on each detected tap.
void imu_start_tap_task(void (*on_tap)(void));
