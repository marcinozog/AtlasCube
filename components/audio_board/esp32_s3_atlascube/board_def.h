#pragma once

#include "driver/gpio.h"
#include "defines.h"   // I2S_DATA/BCK/LCK — single source of truth (main/include)


// ===================================== ONLY FOR ADF =====================================

// PA
#define BOARD_PA_GAIN             (0)
#define PA_ENABLE_GPIO            (-1)

// I2C for ADF
#define I2C_SDA -1
#define I2C_SCL -1


// I2S pins (I2S_DATA/BCK/LCK) come from defines.h, included above.


#define ESP_SD_PIN_CLK  -1
#define ESP_SD_PIN_CMD  -1
#define ESP_SD_PIN_D0   -1
#define ESP_SD_PIN_D1   -1
#define ESP_SD_PIN_D2   -1
#define ESP_SD_PIN_D3   -1
#define ESP_SD_PIN_D4   -1
#define ESP_SD_PIN_D5   -1
#define ESP_SD_PIN_D6   -1
#define ESP_SD_PIN_D7   -1
#define ESP_SD_PIN_CD   -1
#define ESP_SD_PIN_WP   -1

#define SDCARD_OPEN_FILE_NUM_MAX 5
#define SDCARD_INTR_GPIO -1
#define SDCARD_PWR_CTRL -1
