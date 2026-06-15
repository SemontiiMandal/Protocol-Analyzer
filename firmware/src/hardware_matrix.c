#include "hardware_matrix.h"

// Include the raw ESP-IDF headers to access the GPIO Matrix and Parallel IO (PARLIO)

#include <hal/gpio_hal.h>
#include <soc/gpio_sig_map.h>

// Board pinout and connections
#define PIN_SPI_CS   2
#define PIN_SPI_CLK  3
#define PIN_SPI_MISO 4
#define PIN_SPI_MOSI 5
#define PIN_I2C_SCL  0
#define PIN_I2C_SDA  1
#define PIN_UART_TX  6
#define PIN_UART_RX  7
#define PIN_CAN_TX   8
#define PIN_CAN_RX   9

// Global state used by parser thread
volatile analyzer_mode_t current_system_mode = MODE_IDLE;

void configure_hardware_matrix(analyzer_mode_t mode) {
    // Updated system state
    current_system_mode = mode;

    // Clear previous PARLIO routing to prevent signal collisions
    // (Routing to a non-existent pin 0x38 safely disconnects the signal)
    for (int i = 0; i < 8; i++) {
        esp_rom_gpio_connect_in_signal(0x38, PARL_RX_DATA0_IDX + i, false);
    }

    // Apply new physical-to-silicon map
    switch (mode) {
    
        case MODE_SPI_ONLY:
            // Route physical pins to Lines 0-3
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  PARL_RX_DATA1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, PARL_RX_DATA2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, PARL_RX_DATA3_IDX, false);
            break;

        case MODE_I2C_ONLY:
            // Route physical pins to Lines 0-1
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL, PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA, PARL_RX_DATA1_IDX, false);
            break;

        case MODE_UART_ONLY:
            // Route physical pins to Lines 0-1
            esp_rom_gpio_connect_in_signal(PIN_UART_TX, PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX, PARL_RX_DATA1_IDX, false);
            break;

        case MODE_CAN_ONLY:
            // Route physical pins to Lines 0-1 
            esp_rom_gpio_connect_in_signal(PIN_CAN_TX, PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_RX, PARL_RX_DATA1_IDX, false);
            break;

        case MODE_DUAL_SPI_I2C:
            // SPI gets Lines 0-3
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  PARL_RX_DATA1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, PARL_RX_DATA2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, PARL_RX_DATA3_IDX, false);
            
            // I2C gets Lines 4-5
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL, PARL_RX_DATA4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA, PARL_RX_DATA5_IDX, false);
            break;

        case MODE_SPI_I2C_UART:
            // SPI gets Lines 0-3 
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  PARL_RX_DATA1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, PARL_RX_DATA2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, PARL_RX_DATA3_IDX, false);
            
            // I2C gets Lines 4-5
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL, PARL_RX_DATA4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA, PARL_RX_DATA5_IDX, false);

            // UART gets Lines 6-7
            esp_rom_gpio_connect_in_signal(PIN_UART_TX, PARL_RX_DATA6_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX, PARL_RX_DATA7_IDX, false);
            break;

        case MODE_SPI_CAN_UART:
            // SPI gets Lines 0-3
            esp_rom_gpio_connect_in_signal(PIN_SPI_CS,   PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_CLK,  PARL_RX_DATA1_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MISO, PARL_RX_DATA2_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_SPI_MOSI, PARL_RX_DATA3_IDX, false);
            
            // CAN gets Lines 4-5
            esp_rom_gpio_connect_in_signal(PIN_CAN_TX, PARL_RX_DATA4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_RX, PARL_RX_DATA5_IDX, false);

            // UART gets Lines 6-7 
            esp_rom_gpio_connect_in_signal(PIN_UART_TX, PARL_RX_DATA6_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX, PARL_RX_DATA7_IDX, false);
            break;

        case MODE_I2C_CAN_UART:
            // I2C gets Lines 0-1
            esp_rom_gpio_connect_in_signal(PIN_I2C_SCL, PARL_RX_DATA0_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_I2C_SDA, PARL_RX_DATA1_IDX, false);

            // CAN gets Lines 4-5
            esp_rom_gpio_connect_in_signal(PIN_CAN_TX, PARL_RX_DATA4_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_CAN_RX, PARL_RX_DATA5_IDX, false);

            // UART gets Lines 6-7
            esp_rom_gpio_connect_in_signal(PIN_UART_TX, PARL_RX_DATA6_IDX, false);
            esp_rom_gpio_connect_in_signal(PIN_UART_RX, PARL_RX_DATA7_IDX, false);
            break;

        case MODE_IDLE:
        default:
            // Leave everything disconnected
            break;
    }
}