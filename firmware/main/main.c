/*
 * main.c - Parallel Meshtastic simplified main and console
 * Supports two console backends controlled by sdkconfig:
 *  - USB Serial/JTAG (CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED)
 *  - UART0 (default)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#if defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED)
#include <esp_vfs_dev.h>
#else
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#endif

#include "board_hal_seeed.h"
#include "sx1262.h"

static const char *TAG = "MAIN";

// Function declarations
void process_command(char *cmd);
void cmd_ping_simple();
void cmd_send_simple(const char *message);
void cmd_receive_simple(uint32_t timeout_ms);
void cmd_status_simple();

// Console task - one implementation that uses fgets on stdin which will be
// hooked up to USB Serial/JTAG if enabled. If UART is used we install the
// UART driver and also register it to the VFS so stdin/stdout map to UART0.
void console_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting console task");

#if defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED)
    ESP_LOGI(TAG, "Console task: Using USB Serial/JTAG");
    // USB Serial/JTAG: register VFS to redirect stdio to USB Serial/JTAG
    esp_vfs_usb_serial_jtag_use_driver();
    ESP_LOGI(TAG, "Console task: VFS driver registered");
    printf("Parallel Meshtastic Console (USB)\n");
    ESP_LOGI(TAG, "Console task: Welcome message sent");
#else
    // UART0 backend: configure and install UART driver and register to VFS
    const uart_port_t uart_num = UART_NUM_0;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(uart_num, 256, 0, 0, NULL, 0));
    // Register uart driver with VFS so we can use standard stdio functions
    esp_vfs_dev_uart_use_driver(uart_num);
    printf("Parallel Meshtastic Console (UART0)\n");
#endif

    printf("Available commands: ping, send <message>, receive [timeout], status, help\n");
    ESP_LOGI(TAG, "Console task: Entering command loop");

    char line[256];

    while (1) {
        printf("mesh> ");
        fflush(stdout);
        ESP_LOGI(TAG, "Console task: Waiting for input...");

        if (fgets(line, sizeof(line), stdin) == NULL) {
            // fgets returned NULL - could be no data yet; yield and retry
            ESP_LOGI(TAG, "Console task: fgets returned NULL, retrying...");
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // strip newline
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            if (len > 1 && line[len-2] == '\r') line[len-2] = '\0';
        }

        if (strlen(line) == 0) continue;

        ESP_LOGI(TAG, "Received command: %s", line);
        process_command(line);
    }
}

void process_command(char *cmd) {
    char *token = strtok(cmd, " ");
    if (token == NULL) return;
    
    if (strcmp(token, "ping") == 0) {
        cmd_ping_simple();
    } else if (strcmp(token, "send") == 0) {
        char *message = strtok(NULL, "");
        if (message) {
            cmd_send_simple(message);
        } else {
            printf("Usage: send <message>\n");
        }
    } else if (strcmp(token, "receive") == 0) {
        char *timeout_str = strtok(NULL, " ");
        uint32_t timeout = 5000;
        if (timeout_str) {
            timeout = atoi(timeout_str);
        }
        cmd_receive_simple(timeout);
    } else if (strcmp(token, "status") == 0) {
        cmd_status_simple();
    } else if (strcmp(token, "help") == 0) {
        printf("Available commands:\n");
        printf("  ping - Send a ping packet\n");
        printf("  send <message> - Send a message\n");
        printf("  receive [timeout_ms] - Listen for packets\n");
        printf("  status - Show radio status\n");
        printf("  help - Show this help\n");
    } else {
        printf("Unknown command. Type 'help' for available commands.\n");
    }
}

void cmd_ping_simple() {
    printf("Sending ping packet...\n");
    const char *ping_msg = "PING";
    esp_err_t ret = sx1262_send_packet((const uint8_t *)ping_msg, strlen(ping_msg));
    if (ret == ESP_OK) {
        printf("Ping sent successfully\n");
    } else {
        printf("Ping failed: %s\n", esp_err_to_name(ret));
    }
}

void cmd_send_simple(const char *message) {
    printf("Sending message: %s\n", message);
    esp_err_t ret = sx1262_send_packet((const uint8_t *)message, strlen(message));
    if (ret == ESP_OK) {
        printf("Message sent successfully\n");
    } else {
        printf("Send failed: %s\n", esp_err_to_name(ret));
    }
}

void cmd_receive_simple(uint32_t timeout_ms) {
    printf("Listening for packets (timeout: %lu ms)...\n", timeout_ms);
    uint8_t rx_buffer[256];
    uint8_t rx_size = 0;
    esp_err_t ret = sx1262_receive_packet(rx_buffer, &rx_size, timeout_ms);
    if (ret == ESP_OK && rx_size > 0) {
        printf("Received %d bytes: ", rx_size);
        for (int i = 0; i < rx_size; i++) printf("%c", rx_buffer[i]);
        printf("\n");
        packet_status_t pkt_status = sx1262_get_packet_status();
        printf("RSSI: %d dBm, SNR: %d dB\n", pkt_status.rssi_pkt, pkt_status.snr_pkt);
    } else {
        printf("Receive timeout or error: %s\n", esp_err_to_name(ret));
    }
}

void cmd_status_simple() {
    printf("=== SX1262 Radio Status ===\n");
    printf("BUSY pin: %s\n", sx1262_is_busy() ? "HIGH" : "LOW");
    uint16_t irq_status = sx1262_get_irq_status();
    printf("IRQ Status: 0x%04X\n", irq_status);
    if (irq_status & IRQ_TX_DONE) printf("  - TX Done\n");
    if (irq_status & IRQ_RX_DONE) printf("  - RX Done\n");
    if (irq_status & IRQ_PREAMBLE_DETECTED) printf("  - Preamble Detected\n");
    if (irq_status & IRQ_SYNCWORD_VALID) printf("  - Syncword Valid\n");
    if (irq_status & IRQ_HEADER_VALID) printf("  - Header Valid\n");
    if (irq_status & IRQ_HEADER_ERROR) printf("  - Header Error\n");
    if (irq_status & IRQ_CRC_ERROR) printf("  - CRC Error\n");
    if (irq_status & IRQ_CAD_DONE) printf("  - CAD Done\n");
    if (irq_status & IRQ_RX_TX_TIMEOUT) printf("  - RX/TX Timeout\n");
}

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "=== Parallel Meshtastic Firmware ===");
    ESP_LOGI(TAG, "Wave 0: Hardware Bring-up");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize board hardware
    ret = board_hal_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Board HAL init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize SX1262 radio
    ret = sx1262_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SX1262 init failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Hardware initialization complete");
    ESP_LOGI(TAG, "Available commands: ping, send, receive, status");
    ESP_LOGI(TAG, "Type 'help' for command help");

    // Create console task
    xTaskCreate(console_task, "console", 8192, NULL, 5, NULL);

    // Main loop - just monitor system
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 second heartbeat
        ESP_LOGI(TAG, "System running, free heap: %lu bytes", esp_get_free_heap_size());
    }
}
