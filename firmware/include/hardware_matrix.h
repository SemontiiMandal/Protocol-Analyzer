#pragma once

#include <stdint.h>
#include <stdbool.h>
// Allowed states for the hardware matrix; Controlled by the PC App via the USB ISR.
 
// Using 8 Bit Data Bus Width
typedef enum {
    MODE_IDLE,
    MODE_SPI_ONLY,
    MODE_I2C_ONLY,
    MODE_UART_ONLY,
    MODE_CAN_ONLY,
    MODE_SPI_I2C_UART, // 4 + 2 + 2 = 8 pins
    MODE_DUAL_SPI_I2C,
    MODE_SPI_CAN_UART, // 4 + 2 + 2 = 8 pins 
    MODE_I2C_CAN_UART  // 2 + 2 + 2 = 6 pins 
    
} analyzer_mode_t;

// Re-routes the GPIO matrix 
void configure_hardware_matrix(analyzer_mode_t mode);

// Play/Pause Recording
extern volatile bool is_capturing;