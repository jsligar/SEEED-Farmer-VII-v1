#include "sx1262.h"
#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_hal_seeed.h"
#include "esp_log.h"

static const char *TAG = "SX1262";

static spi_device_handle_t spi_handle = NULL;

// Internal functions
static esp_err_t sx1262_write_command(uint8_t cmd, const uint8_t *data, uint8_t len);
static esp_err_t sx1262_read_command(uint8_t cmd, uint8_t *data, uint8_t len);
static esp_err_t sx1262_write_register(uint16_t addr, const uint8_t *data, uint8_t len);
static esp_err_t sx1262_read_register(uint16_t addr, uint8_t *data, uint8_t len);
static void sx1262_wait_busy(void);

esp_err_t sx1262_init(void) {
    ESP_LOGI(TAG, "Initializing SX1262");
    
    // Configure SPI
    spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 8000000,  // 8 MHz
        .mode = 0,                   // SPI mode 0
        .spics_io_num = PIN_SX1262_CS,
        .queue_size = 7,
        .flags = 0  // Remove SPI_DEVICE_HALFDUPLEX flag
    };
    
    ret = spi_bus_add_device(SPI2_HOST, &dev_config, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure GPIO pins
    gpio_config_t io_conf = {};
    
    // Reset pin - output
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_SX1262_RESET);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // BUSY pin - input
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_SX1262_BUSY);
    gpio_config(&io_conf);
    
    // DIO1 pin - input with interrupt
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_SX1262_DIO1);
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    gpio_config(&io_conf);
    
    // DIO2 pin - output (RF switch control)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_SX1262_DIO2);
    gpio_config(&io_conf);
    
    // Reset the radio
    ret = sx1262_reset();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Initialize with basic LoRa configuration
    sx1262_set_standby();
    sx1262_set_packet_type(PACKET_TYPE_LORA);
    
    // Set RF frequency to 915 MHz (US915)
    sx1262_set_rf_frequency(915000000);
    
    // Set modulation parameters - LongFast preset
    lora_mod_params_t mod_params = {
        .bandwidth = LORA_BW_250,
        .spreading_factor = LORA_SF11,
        .coding_rate = LORA_CR_4_8,
        .low_data_rate_optimize = false
    };
    sx1262_set_modulation_params(&mod_params);
    
    // Set packet parameters
    lora_packet_params_t pkt_params = {
        .preamble_length = 8,
        .header_type = false, // explicit header
        .payload_length = 255,
        .crc_on = true,
        .invert_iq = false
    };
    sx1262_set_packet_params(&pkt_params);
    
    // Set buffer addresses
    sx1262_set_buffer_base_address(0x00, 0x00);
    
    // Configure PA
    sx1262_set_pa_config();
    sx1262_set_tx_params(14, 0x04); // 14 dBm, 200us ramp
    
    // Configure DIO and IRQ
    sx1262_set_dio_irq_params(IRQ_RADIO_ALL, IRQ_TX_DONE | IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT);
    
    // Calibrate image
    sx1262_calibrate_image(915000000);
    
    ESP_LOGI(TAG, "SX1262 initialized successfully");
    return ESP_OK;
}

esp_err_t sx1262_reset(void) {
    // Reset pulse: low for 1ms, then high
    gpio_set_level(PIN_SX1262_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(PIN_SX1262_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Wait for BUSY to go low
    sx1262_wait_busy();
    return ESP_OK;
}

bool sx1262_is_busy(void) {
    return gpio_get_level(PIN_SX1262_BUSY) == 1;
}

static void sx1262_wait_busy(void) {
    int timeout = 1000; // 1 second timeout
    while (sx1262_is_busy() && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (timeout <= 0) {
        ESP_LOGE(TAG, "Timeout waiting for BUSY to clear");
    }
}

static esp_err_t sx1262_write_command(uint8_t cmd, const uint8_t *data, uint8_t len) {
    sx1262_wait_busy();
    
    spi_transaction_t trans = {};
    uint8_t tx_buf[256];
    
    tx_buf[0] = cmd;
    if (data && len > 0) {
        memcpy(&tx_buf[1], data, len);
    }
    
    trans.length = (1 + len) * 8;
    trans.tx_buffer = tx_buf;
    
    return spi_device_transmit(spi_handle, &trans);
}

static esp_err_t sx1262_read_command(uint8_t cmd, uint8_t *data, uint8_t len) {
    sx1262_wait_busy();
    
    spi_transaction_t trans = {};
    uint8_t tx_buf[256] = {0};
    uint8_t rx_buf[256] = {0};
    
    tx_buf[0] = cmd;
    
    trans.length = (2 + len) * 8; // cmd + status + data
    trans.tx_buffer = tx_buf;
    trans.rx_buffer = rx_buf;
    
    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret == ESP_OK && data && len > 0) {
        memcpy(data, &rx_buf[2], len); // Skip cmd echo and status
    }
    
    return ret;
}

esp_err_t sx1262_set_standby(void) {
    uint8_t standby_mode = 0x00; // STDBY_RC
    return sx1262_write_command(SX1262_SET_STANDBY, &standby_mode, 1);
}

esp_err_t sx1262_set_packet_type(uint8_t packet_type) {
    return sx1262_write_command(SX1262_SET_PACKETTYPE, &packet_type, 1);
}

esp_err_t sx1262_set_rf_frequency(uint32_t frequency) {
    uint8_t freq_buf[4];
    uint32_t freq_raw = (uint32_t)((double)frequency / 32000000.0 * 16777216.0);
    
    freq_buf[0] = (freq_raw >> 24) & 0xFF;
    freq_buf[1] = (freq_raw >> 16) & 0xFF;
    freq_buf[2] = (freq_raw >> 8) & 0xFF;
    freq_buf[3] = freq_raw & 0xFF;
    
    return sx1262_write_command(SX1262_SET_RFFREQUENCY, freq_buf, 4);
}

esp_err_t sx1262_set_modulation_params(const lora_mod_params_t *params) {
    uint8_t mod_buf[4];
    
    mod_buf[0] = params->spreading_factor;
    mod_buf[1] = params->bandwidth;
    mod_buf[2] = params->coding_rate;
    mod_buf[3] = params->low_data_rate_optimize ? 0x01 : 0x00;
    
    return sx1262_write_command(SX1262_SET_MODULATIONPARAMS, mod_buf, 4);
}

esp_err_t sx1262_set_packet_params(const lora_packet_params_t *params) {
    uint8_t pkt_buf[6];
    
    pkt_buf[0] = (params->preamble_length >> 8) & 0xFF;
    pkt_buf[1] = params->preamble_length & 0xFF;
    pkt_buf[2] = params->header_type ? 0x01 : 0x00;
    pkt_buf[3] = params->payload_length;
    pkt_buf[4] = params->crc_on ? 0x01 : 0x00;
    pkt_buf[5] = params->invert_iq ? 0x01 : 0x00;
    
    return sx1262_write_command(SX1262_SET_PACKETPARAMS, pkt_buf, 6);
}

esp_err_t sx1262_set_buffer_base_address(uint8_t tx_addr, uint8_t rx_addr) {
    uint8_t addr_buf[2] = {tx_addr, rx_addr};
    return sx1262_write_command(SX1262_SET_BUFFERBASEADDRESS, addr_buf, 2);
}

esp_err_t sx1262_set_dio_irq_params(uint16_t irq_mask, uint16_t dio1_mask) {
    uint8_t irq_buf[8];
    
    irq_buf[0] = (irq_mask >> 8) & 0xFF;
    irq_buf[1] = irq_mask & 0xFF;
    irq_buf[2] = (dio1_mask >> 8) & 0xFF;
    irq_buf[3] = dio1_mask & 0xFF;
    irq_buf[4] = 0x00; // DIO2 mask
    irq_buf[5] = 0x00;
    irq_buf[6] = 0x00; // DIO3 mask
    irq_buf[7] = 0x00;
    
    return sx1262_write_command(SX1262_SET_DIOIRQPARAMS, irq_buf, 8);
}

esp_err_t sx1262_calibrate_image(uint32_t frequency) {
    uint8_t cal_freq[2];
    
    if (frequency >= 902000000 && frequency <= 928000000) {
        cal_freq[0] = 0xE1; // 902-928 MHz
        cal_freq[1] = 0xE9;
    } else if (frequency >= 863000000 && frequency <= 870000000) {
        cal_freq[0] = 0xD7; // 863-870 MHz
        cal_freq[1] = 0xDB;
    } else {
        cal_freq[0] = 0xE1; // Default to 902-928
        cal_freq[1] = 0xE9;
    }
    
    return sx1262_write_command(SX1262_CALIBRATEIMAGE, cal_freq, 2);
}

esp_err_t sx1262_set_pa_config(void) {
    uint8_t pa_buf[4];
    
    pa_buf[0] = 0x04; // PA duty cycle
    pa_buf[1] = 0x07; // HP max
    pa_buf[2] = 0x00; // Device select: +22dBm on PA_BOOST
    pa_buf[3] = 0x01; // PA LUT
    
    return sx1262_write_command(SX1262_SET_PACONFIG, pa_buf, 4);
}

esp_err_t sx1262_set_tx_params(int8_t power, uint8_t ramp_time) {
    uint8_t tx_buf[2];
    
    tx_buf[0] = power;
    tx_buf[1] = ramp_time;
    
    return sx1262_write_command(SX1262_SET_TXPARAMS, tx_buf, 2);
}

esp_err_t sx1262_write_buffer(uint8_t offset, const uint8_t *data, uint8_t size) {
    uint8_t write_buf[256];
    
    write_buf[0] = offset;
    memcpy(&write_buf[1], data, size);
    
    return sx1262_write_command(SX1262_WRITEBUFFER, write_buf, size + 1);
}

esp_err_t sx1262_read_buffer(uint8_t offset, uint8_t *data, uint8_t size) {
    uint8_t offset_buf[1] = {offset};
    return sx1262_read_command(SX1262_READBUFFER, data, size);
}

esp_err_t sx1262_set_tx(uint32_t timeout) {
    uint8_t timeout_buf[3];
    
    timeout_buf[0] = (timeout >> 16) & 0xFF;
    timeout_buf[1] = (timeout >> 8) & 0xFF;
    timeout_buf[2] = timeout & 0xFF;
    
    return sx1262_write_command(SX1262_SET_TX, timeout_buf, 3);
}

esp_err_t sx1262_set_rx(uint32_t timeout) {
    uint8_t timeout_buf[3];
    
    timeout_buf[0] = (timeout >> 16) & 0xFF;
    timeout_buf[1] = (timeout >> 8) & 0xFF;
    timeout_buf[2] = timeout & 0xFF;
    
    return sx1262_write_command(SX1262_SET_RX, timeout_buf, 3);
}

uint16_t sx1262_get_irq_status(void) {
    uint8_t irq_buf[2] = {0};
    sx1262_read_command(SX1262_GET_IRQSTATUS, irq_buf, 2);
    return (irq_buf[0] << 8) | irq_buf[1];
}

esp_err_t sx1262_clear_irq_status(uint16_t irq_mask) {
    uint8_t irq_buf[2];
    
    irq_buf[0] = (irq_mask >> 8) & 0xFF;
    irq_buf[1] = irq_mask & 0xFF;
    
    return sx1262_write_command(SX1262_CLR_IRQSTATUS, irq_buf, 2);
}

esp_err_t sx1262_get_rx_buffer_status(uint8_t *payload_length, uint8_t *rx_start_buffer_pointer) {
    uint8_t status_buf[2] = {0};
    esp_err_t ret = sx1262_read_command(SX1262_GET_RXBUFFERSTATUS, status_buf, 2);
    
    if (ret == ESP_OK) {
        *payload_length = status_buf[0];
        *rx_start_buffer_pointer = status_buf[1];
    }
    
    return ret;
}

packet_status_t sx1262_get_packet_status(void) {
    uint8_t pkt_status[3] = {0};
    packet_status_t status = {0};
    
    if (sx1262_read_command(SX1262_GET_PACKETSTATUS, pkt_status, 3) == ESP_OK) {
        status.rssi_pkt = -pkt_status[0] / 2;
        status.snr_pkt = pkt_status[1] / 4;
        status.signal_rssi_pkt = -pkt_status[2] / 2;
    }
    
    return status;
}

// High-level API
esp_err_t sx1262_send_packet(const uint8_t *data, uint8_t size) {
    esp_err_t ret;
    
    // Write data to buffer
    ret = sx1262_write_buffer(0x00, data, size);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Update packet length
    lora_packet_params_t pkt_params = {
        .preamble_length = 8,
        .header_type = false,
        .payload_length = size,
        .crc_on = true,
        .invert_iq = false
    };
    ret = sx1262_set_packet_params(&pkt_params);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Clear IRQ
    sx1262_clear_irq_status(IRQ_RADIO_ALL);
    
    // Start transmission (3 second timeout)
    ret = sx1262_set_tx(3000000); // 3s in microseconds
    
    ESP_LOGI(TAG, "Packet sent, size: %d bytes", size);
    return ret;
}

esp_err_t sx1262_receive_packet(uint8_t *data, uint8_t *size, uint32_t timeout_ms) {
    esp_err_t ret;
    uint16_t irq_status;
    uint8_t payload_length, rx_start_pointer;
    uint32_t start_time = xTaskGetTickCount();
    
    // Clear IRQ and start RX
    sx1262_clear_irq_status(IRQ_RADIO_ALL);
    ret = sx1262_set_rx(timeout_ms * 1000); // Convert to microseconds
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Wait for packet or timeout
    while (1) {
        irq_status = sx1262_get_irq_status();
        
        if (irq_status & IRQ_RX_DONE) {
            // Packet received successfully
            sx1262_clear_irq_status(IRQ_RX_DONE);
            
            ret = sx1262_get_rx_buffer_status(&payload_length, &rx_start_pointer);
            if (ret != ESP_OK) {
                return ret;
            }
            
            if (payload_length > 0) {
                ret = sx1262_read_buffer(rx_start_pointer, data, payload_length);
                if (ret == ESP_OK) {
                    *size = payload_length;
                    ESP_LOGI(TAG, "Packet received, size: %d bytes", payload_length);
                }
            }
            
            return ret;
        }
        
        if (irq_status & (IRQ_RX_TX_TIMEOUT | IRQ_CRC_ERROR | IRQ_HEADER_ERROR)) {
            // Error or timeout
            sx1262_clear_irq_status(IRQ_RADIO_ALL);
            return ESP_ERR_TIMEOUT;
        }
        
        // Check timeout
        if ((xTaskGetTickCount() - start_time) > pdMS_TO_TICKS(timeout_ms)) {
            sx1262_set_standby();
            return ESP_ERR_TIMEOUT;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool sx1262_packet_available(void) {
    uint16_t irq_status = sx1262_get_irq_status();
    return (irq_status & IRQ_RX_DONE) != 0;
}
