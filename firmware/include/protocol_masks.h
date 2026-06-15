#pragma once

// Because we dynamically swap CAN, UART, and I2C, the parser assigns a protocol to a slot based on the current active mode.
 

// SPI Lines
// Reserved for: SPI (CS, CLK, MISO, MOSI)
#define MASK_SLOT_1   0x0F  // Binary: 0000 1111
#define SHIFT_SLOT_1  0

// I2C (SCL, SDA) or CAN (TX, RX)
#define MASK_SLOT_2   0x30  // Binary: 0011 0000
#define SHIFT_SLOT_2  4

// UART (TX, RX) or CAN (TX, RX)
#define MASK_SLOT_3   0xC0  // Binary: 1100 0000
#define SHIFT_SLOT_3  6

// Individual Pin Extraction Byte
// Used by State Machines to read shifted bytes.

// For the SPI Decoder (Looking at a 4-bit isolated byte)
#define PIN_SPI_CS    0x01
#define PIN_SPI_CLK   0x02
#define PIN_SPI_MISO  0x04
#define PIN_SPI_MOSI  0x08

// For the I2C, UART, and CAN Decoders 
#define PIN_COM_0     0x01 // SCL / TX / CAN_TX
#define PIN_COM_1     0x02 // SDA / RX / CAN_RX