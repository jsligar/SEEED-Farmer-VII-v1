#include "board_hal_seeed.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "BOARD_HAL";

// Peripheral init functions (declared here, implemented in respective files)
extern void oled_init(void);
extern void rtc_init(void); 
extern void sd_init(void);
extern void buzzer_init(void);
extern void button_init(void);
extern void servo_init(void);

esp_err_t board_hal_init(void) {
    ESP_LOGI(TAG, "Initializing Board HAL for Seeed XIAO ESP32S3");
    
    // Initialize I2C for OLED and RTC
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000 // 400kHz
    };
    
    esp_err_t ret = i2c_param_config(I2C_NUM_0, &i2c_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize all peripherals
    oled_init();
    rtc_init();
    sd_init();
    buzzer_init();
    button_init();
    servo_init();
    
    ESP_LOGI(TAG, "Board HAL initialized successfully");
    return ESP_OK;
}
