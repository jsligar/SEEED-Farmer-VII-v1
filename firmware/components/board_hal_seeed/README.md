# Board HAL - Seeed XIAO ESP32S3 + Wio-SX1262

This component provides hardware abstraction for the Seeed XIAO ESP32S3 and Wio-SX1262 LoRa radio.

## Pin Definitions
See `board_hal_seeed.h` for all pin mappings.

## Initialization Steps
- Initialize all GPIOs
- Set up SPI, I2C, and peripheral interfaces
- Verify hardware presence

## TODO
- Add peripheral drivers (OLED, RTC, SD, Buzzer, Button, Servo)
- Implement board-specific power management
