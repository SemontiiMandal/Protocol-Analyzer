#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Bring in USB helper to output decoded data packet
extern void stream_packet_to_pc(const char* packet);

// Previous hardware state
static uint8_t previous_spi_bits = 0xFF; 

// Internal state machine variables
static uint8_t current_mosi_byte = 0;
static uint8_t current_miso_byte = 0;
static uint8_t bit_count = 0;
static bool transaction_active = false;

// Bit 0 - CS, Bit 1 - CLK, Bit 2 - MISO, Bit 3 - MOSI
void feed_spi_decoder(uint8_t current_bits) {
    
    // Extract individual pin states
    bool cs   = (current_bits & 0x01) != 0;
    bool clk  = (current_bits & 0x02) != 0;
    bool miso = (current_bits & 0x04) != 0;
    bool mosi = (current_bits & 0x08) != 0;

    // Extract previous pin states
    bool prev_cs  = (previous_spi_bits & 0x01) != 0;
    bool prev_clk = (previous_spi_bits & 0x02) != 0;

    // Detect CS Falling Edge (Start of transaction)
    if (!cs && prev_cs) {
        transaction_active = true;
        bit_count = 0;
        current_mosi_byte = 0;
        current_miso_byte = 0;
        stream_packet_to_pc("[SPI] CS LOW (Start)");
    }

    // Detect CS Rising Edge (End of transaction)
    if (cs && !prev_cs) {
        transaction_active = false;
        stream_packet_to_pc("[SPI] CS HIGH (End)");
    }

    // Capture Data on CLK Rising Edge (Only if CS is active)
    if (transaction_active && clk && !prev_clk) {
        
        // Shift bits left and insert the new data at bit 0
        current_mosi_byte = (current_mosi_byte << 1) | (mosi ? 1 : 0);
        current_miso_byte = (current_miso_byte << 1) | (miso ? 1 : 0);
        bit_count++;

        // When we have a full 8-bit byte, print it and reset
        if (bit_count == 8) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "[SPI] MOSI: 0x%02X | MISO: 0x%02X", 
                     current_mosi_byte, current_miso_byte);
            stream_packet_to_pc(buffer);
            
            bit_count = 0;
            current_mosi_byte = 0;
            current_miso_byte = 0;
        }
    }

    // Save current state for next tick
    previous_spi_bits = current_bits;
}