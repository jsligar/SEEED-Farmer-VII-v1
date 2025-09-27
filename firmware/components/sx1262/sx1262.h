#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

// SX1262 Commands
#define SX1262_SET_SLEEP                    0x84
#define SX1262_SET_STANDBY                  0x80
#define SX1262_SET_FS                       0xC1
#define SX1262_SET_TX                       0x83
#define SX1262_SET_RX                       0x82
#define SX1262_SET_RXDUTYCYCLE             0x94
#define SX1262_SET_CAD                      0xC5
#define SX1262_SET_TXCONTINUOUSWAVE        0xD1
#define SX1262_SET_TXCONTINUOUSPREAMBLE    0xD2
#define SX1262_SET_REGULATORMODE           0x96
#define SX1262_CALIBRATE                   0x89
#define SX1262_CALIBRATEIMAGE              0x98
#define SX1262_SET_PACONFIG                0x95
#define SX1262_SET_RXGAIN                  0x96
#define SX1262_SET_TXPARAMS                0x8E
#define SX1262_SET_DIOIRQPARAMS            0x08
#define SX1262_GET_IRQSTATUS               0x12
#define SX1262_CLR_IRQSTATUS               0x02
#define SX1262_SET_DIO2ASRFSWITCHCTRL      0x9D
#define SX1262_SET_DIO3ASTCXOCTRL          0x97
#define SX1262_SET_RFFREQUENCY             0x86
#define SX1262_SET_PACKETTYPE              0x8A
#define SX1262_GET_PACKETTYPE              0x11
#define SX1262_SET_TXPARAMS                0x8E
#define SX1262_SET_MODULATIONPARAMS        0x8B
#define SX1262_SET_PACKETPARAMS            0x8C
#define SX1262_SET_CADPARAMS               0x88
#define SX1262_SET_BUFFERBASEADDRESS       0x8F
#define SX1262_SET_LORASYMBTIMEOUT         0xA0
#define SX1262_GET_STATUS                  0xC0
#define SX1262_GET_RSSIINST                0x15
#define SX1262_GET_RXBUFFERSTATUS          0x13
#define SX1262_GET_PACKETSTATUS            0x14
#define SX1262_GET_DEVICEERRORS            0x17
#define SX1262_CLR_DEVICEERRORS            0x07
#define SX1262_GET_STATS                   0x10
#define SX1262_RESET_STATS                 0x00
#define SX1262_CFG_DIOIRQ                  0x08
#define SX1262_GET_IRQSTATUS               0x12
#define SX1262_CLR_IRQSTATUS               0x02
#define SX1262_WRITEREGISTER               0x0D
#define SX1262_READREGISTER                0x1D
#define SX1262_WRITEBUFFER                 0x0E
#define SX1262_READBUFFER                  0x1E

// IRQ Masks
#define IRQ_RADIO_ALL                      0xFFFF
#define IRQ_TX_DONE                        0x0001
#define IRQ_RX_DONE                        0x0002
#define IRQ_PREAMBLE_DETECTED              0x0004
#define IRQ_SYNCWORD_VALID                 0x0008
#define IRQ_HEADER_VALID                   0x0010
#define IRQ_HEADER_ERROR                   0x0020
#define IRQ_CRC_ERROR                      0x0040
#define IRQ_CAD_DONE                       0x0080
#define IRQ_CAD_ACTIVITY_DETECTED          0x0100
#define IRQ_RX_TX_TIMEOUT                  0x0200

// Packet types
#define PACKET_TYPE_GFSK                   0x00
#define PACKET_TYPE_LORA                   0x01

// LoRa bandwidth
#define LORA_BW_7                          0x00
#define LORA_BW_10                         0x08
#define LORA_BW_15                         0x01
#define LORA_BW_20                         0x09
#define LORA_BW_31                         0x02
#define LORA_BW_41                         0x0A
#define LORA_BW_62                         0x03
#define LORA_BW_125                        0x04
#define LORA_BW_250                        0x05
#define LORA_BW_500                        0x06

// LoRa spreading factors
#define LORA_SF5                           0x05
#define LORA_SF6                           0x06
#define LORA_SF7                           0x07
#define LORA_SF8                           0x08
#define LORA_SF9                           0x09
#define LORA_SF10                          0x0A
#define LORA_SF11                          0x0B
#define LORA_SF12                          0x0C

// LoRa coding rates
#define LORA_CR_4_5                        0x01
#define LORA_CR_4_6                        0x02
#define LORA_CR_4_7                        0x03
#define LORA_CR_4_8                        0x04

typedef struct {
    uint8_t bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    bool low_data_rate_optimize;
} lora_mod_params_t;

typedef struct {
    uint16_t preamble_length;
    bool header_type; // false = explicit, true = implicit
    uint8_t payload_length;
    bool crc_on;
    bool invert_iq;
} lora_packet_params_t;

typedef struct {
    int16_t rssi_pkt;
    int8_t snr_pkt;
    int8_t signal_rssi_pkt;
} packet_status_t;

// Public API
esp_err_t sx1262_init(void);
esp_err_t sx1262_reset(void);
bool sx1262_is_busy(void);
esp_err_t sx1262_set_standby(void);
esp_err_t sx1262_set_packet_type(uint8_t packet_type);
esp_err_t sx1262_set_rf_frequency(uint32_t frequency);
esp_err_t sx1262_set_modulation_params(const lora_mod_params_t *params);
esp_err_t sx1262_set_packet_params(const lora_packet_params_t *params);
esp_err_t sx1262_set_buffer_base_address(uint8_t tx_addr, uint8_t rx_addr);
esp_err_t sx1262_set_dio_irq_params(uint16_t irq_mask, uint16_t dio1_mask);
esp_err_t sx1262_calibrate_image(uint32_t frequency);
esp_err_t sx1262_set_pa_config(void);
esp_err_t sx1262_set_tx_params(int8_t power, uint8_t ramp_time);
esp_err_t sx1262_write_buffer(uint8_t offset, const uint8_t *data, uint8_t size);
esp_err_t sx1262_read_buffer(uint8_t offset, uint8_t *data, uint8_t size);
esp_err_t sx1262_set_tx(uint32_t timeout);
esp_err_t sx1262_set_rx(uint32_t timeout);
uint16_t sx1262_get_irq_status(void);
esp_err_t sx1262_clear_irq_status(uint16_t irq_mask);
esp_err_t sx1262_get_rx_buffer_status(uint8_t *payload_length, uint8_t *rx_start_buffer_pointer);
packet_status_t sx1262_get_packet_status(void);

// High-level API
esp_err_t sx1262_send_packet(const uint8_t *data, uint8_t size);
esp_err_t sx1262_receive_packet(uint8_t *data, uint8_t *size, uint32_t timeout_ms);
bool sx1262_packet_available(void);
