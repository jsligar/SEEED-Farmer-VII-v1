#pragma once
#include "esp_err.h"

esp_err_t board_hal_init(void);
// Pin definitions for Seeed XIAO ESP32S3 + Wio-SX1262
#define PIN_SPI_MOSI     9
#define PIN_SPI_MISO     8
#define PIN_SPI_SCK      7
#define PIN_SX1262_CS    41
#define PIN_SX1262_RESET 42
#define PIN_SX1262_DIO1  1
#define PIN_SX1262_BUSY  2
#define PIN_SX1262_DIO2  21
// Expansion
#define PIN_I2C_SDA      5
#define PIN_I2C_SCL      6
#define PIN_SD_CS        3
#define PIN_BUZZER       4
#define PIN_BUTTON       43
#define PIN_SERVO        44
