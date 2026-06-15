#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

extern void stream_packet_to_pc(const char* packet);

// 10 MHz Sample Rate / 115200 Baud Rate = ~86 samples per bit
#define SAMPLES_PER_BIT 86 

typedef enum {
    UART_IDLE,
    UART_START_BIT,
    UART_DATA_BITS,
    UART_STOP_BIT
} uart_state_t;

static uart_state_t state = UART_IDLE;
static uint8_t previous_uart_bits = 0xFF;

static uint32_t sample_timer = 0;
static uint8_t current_byte = 0;
static uint8_t bit_index = 0;

// Bit 0 - UART TX, Bit 1 - UART RX 
 
void feed_uart_decoder(uint8_t current_bits) {
    
    // Dcode TX line
    bool tx_pin = (current_bits & 0x01) != 0;
    bool prev_tx = (previous_uart_bits & 0x01) != 0;

    switch (state) {
        case UART_IDLE:
            // Detect START BIT (Falling Edge)
            if (!tx_pin && prev_tx) {
                state = UART_START_BIT;
                // Wait 1.5 bit widths to sample the dead-center of the first data bit
                sample_timer = SAMPLES_PER_BIT + (SAMPLES_PER_BIT / 2); 
                current_byte = 0;
                bit_index = 0;
            }
            break;

        case UART_START_BIT:
            if (--sample_timer == 0) {
                state = UART_DATA_BITS;
                sample_timer = SAMPLES_PER_BIT; // Reset timer for 1 full bit width
                
                // Read Bit 0
                current_byte |= (tx_pin ? 1 : 0) << bit_index;
                bit_index++;
            }
            break;

        case UART_DATA_BITS:
            if (--sample_timer == 0) {
                sample_timer = SAMPLES_PER_BIT;
                
                // Read Bits 1 through 7
                current_byte |= (tx_pin ? 1 : 0) << bit_index;
                bit_index++;

                if (bit_index == 8) {
                    state = UART_STOP_BIT;
                }
            }
            break;

        case UART_STOP_BIT:
            if (--sample_timer == 0) {
                // The Stop Bit should be HIGH. If it's LOW, we have a framing error.
                if (tx_pin) {
                    char buffer[64];
                    // UART sends ASCII text a lot, so we print the hex AND the character
                    snprintf(buffer, sizeof(buffer), "[UART] 0x%02X ('%c')", 
                             current_byte, (current_byte >= 32 && current_byte <= 126) ? current_byte : '.');
                    stream_packet_to_pc(buffer);
                } else {
                    stream_packet_to_pc("[UART] ERROR: Framing Error (Missing Stop Bit)");
                }
                state = UART_IDLE; // Ready for the next byte
            }
            break;
    }

    previous_uart_bits = current_bits;
}