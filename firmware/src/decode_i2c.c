#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

extern void stream_packet_to_pc(const char* packet);

static uint8_t previous_i2c_bits = 0xFF;

static uint8_t current_byte = 0;
static uint8_t bit_count = 0;
static bool transaction_active = false;

// Bit 0 - SCL, Bit 1 - SDA
 
void feed_i2c_decoder(uint8_t current_bits) {
    
    // Extract individual pin states
    bool scl = (current_bits & 0x01) != 0;
    bool sda = (current_bits & 0x02) != 0;

    // Extract previous pin states
    bool prev_scl = (previous_i2c_bits & 0x01) != 0;
    bool prev_sda = (previous_i2c_bits & 0x02) != 0;

    // Detect START Condition - SDA falls while SCL is HIGH
    if (scl && prev_scl && !sda && prev_sda) {
        transaction_active = true;
        bit_count = 0;
        current_byte = 0;
        stream_packet_to_pc("[I2C] START Condition");
    }

    // Detect STOP Condition - SDA rises while SCL is HIGH
    else if (scl && prev_scl && sda && !prev_sda) {
        transaction_active = false;
        stream_packet_to_pc("[I2C] STOP Condition");
    }

    // Capture Data on SCL Rising Edge
    else if (transaction_active && scl && !prev_scl) {
        
        // The first 8 bits are data/address. The 9th bit is the ACK/NACK.
        if (bit_count < 8) {
            current_byte = (current_byte << 1) | (sda ? 1 : 0);
            bit_count++;
        } 
        else if (bit_count == 8) {
            // 9th clock tick; Read the ACK/NACK status.
            bool is_nack = sda; // HIGH means NACK, LOW means ACK
            
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "[I2C] Byte: 0x%02X [%s]", 
                     current_byte, is_nack ? "NACK" : "ACK");
            stream_packet_to_pc(buffer);
            
            // Reset for the next byte in this transaction
            bit_count = 0; 
            current_byte = 0;
        }
    }

    previous_i2c_bits = current_bits;
}