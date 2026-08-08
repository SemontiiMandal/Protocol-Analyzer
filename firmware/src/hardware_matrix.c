#include "hardware_matrix.h"

// Include the raw ESP-IDF headers to access the GPIO Matrix and I2S signal definitions.

#include <hal/gpio_hal.h>
#include <soc/gpio_sig_map.h>
#include <esp_rom_gpio.h>

// Fallback block for I2S0 Data Input Signals on classic ESP32
#ifndef I2S0I_DATA_IN0_IDX
#define I2S0I_DATA_IN0_IDX 39
#define I2S0I_DATA_IN1_IDX 40
#define I2S0I_DATA_IN2_IDX 41
#define I2S0I_DATA_IN3_IDX 42
#define I2S0I_DATA_IN4_IDX 43
#define I2S0I_DATA_IN5_IDX 44
#define I2S0I_DATA_IN6_IDX 45
#define I2S0I_DATA_IN7_IDX 46
#endif

#define PIN_SPI_CS   5
#define PIN_SPI_CLK  18
#define PIN_SPI_MISO 19
#define PIN_SPI_MOSI 23
#define PIN_I2C_SCL  33
#define PIN_I2C_SDA  32
#define PIN_UART_TX  4
#define PIN_UART_RX  5
#define PIN_CAN_TX   21
#define PIN_CAN_RX   22

// The global state brain used by the parser thread
volatile analyzer_mode_t current_system_mode = MODE_IDLE;

void configure_hardware_matrix(analyzer_mode_t mode) {
    current_system_mode = mode;

    // Clear any previous I2S routing to prevent signal collisions
    for (int i = 0; i < 8; i++) {
        esp_rom_gpio_connect_in_signal(0x38, I2S0I_DATA_IN0_IDX + i, false);
    }

    switch (mode) {
        // Single Protocol Modes
        case MODE_SPI_ONLY:
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  I2S0I_DATA_IN1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, I2S0I_DATA_IN2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, I2S0I_DATA_IN3_IDX, false);
            break;

        case MODE_I2C_ONLY:
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL, I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA, I2S0I_DATA_IN1_IDX, false);
            break;

        case MODE_UART_ONLY:
            esp_rom_gpio_connect_in_signal(PIN_UART_TX, I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX, I2S0I_DATA_IN1_IDX, false);
            break;

        case MODE_CAN_ONLY:
            esp_rom_gpio_connect_in_signal(PIN_CAN_TX, I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_RX, I2S0I_DATA_IN1_IDX, false);
            break;

        // Multiple Modes (Max 8 Wires)
        case MODE_DUAL_SPI_I2C:
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  I2S0I_DATA_IN1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, I2S0I_DATA_IN2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, I2S0I_DATA_IN3_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL,  I2S0I_DATA_IN4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA,  I2S0I_DATA_IN5_IDX, false);
            break;

        case MODE_SPI_I2C_UART:
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  I2S0I_DATA_IN1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, I2S0I_DATA_IN2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, I2S0I_DATA_IN3_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL,  I2S0I_DATA_IN4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA,  I2S0I_DATA_IN5_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_TX,  I2S0I_DATA_IN6_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX,  I2S0I_DATA_IN7_IDX, false);
            break;

        case MODE_SPI_CAN_UART:
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  I2S0I_DATA_IN1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, I2S0I_DATA_IN2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, I2S0I_DATA_IN3_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_TX,   I2S0I_DATA_IN4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_RX,   I2S0I_DATA_IN5_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_TX,  I2S0I_DATA_IN6_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX,  I2S0I_DATA_IN7_IDX, false);
            break;

        case MODE_I2C_CAN_UART:
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL, I2S0I_DATA_IN0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA, I2S0I_DATA_IN1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_TX,  I2S0I_DATA_IN4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_RX,  I2S0I_DATA_IN5_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_TX, I2S0I_DATA_IN6_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX, I2S0I_DATA_IN7_IDX, false);
            break;

        case MODE_IDLE:
        default:
            break;
    }
}