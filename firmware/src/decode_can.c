#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// CRC stands for Cyclic Redundancy Check

extern void stream_packet_to_pc(const char* packet);

// 10 MHz Sample Rate / 500k Baud Rate = 20 samples per bit
#define CAN_SAMPLES_PER_BIT 20 

static uint32_t sample_timer = 0;
static bool decoding_active = false;

// Bit Stuffing trackers
static bool last_sampled_bit = true; // CAN idle is Recessive (HIGH/true)
static uint8_t consecutive_identical_bits = 0;

// Frame Data Trackers
static uint32_t current_can_frame_id = 0;
static uint8_t dlc = 0;
static uint8_t data_bytes[8] = {0};
static uint16_t received_crc = 0;
static uint16_t calculated_crc = 0;
static uint8_t bits_decoded = 0;

// The Decoder State Machine
typedef enum {
    STATE_ID,
    STATE_CTRL,  // RTR, IDE, r0, DLC (7 bits total)
    STATE_DATA,
    STATE_CRC
} can_decode_state_t;

static can_decode_state_t frame_state = STATE_ID;

// The CAN 2.0 15-bit CRC Polynomial
#define CAN_CRC_POLY 0x4599

// Helper function to process the shift-register math bit-by-bit
static void update_crc(bool next_bit) {
    bool bit_out = (calculated_crc >> 14) ^ next_bit;
    calculated_crc <<= 1;
    if (bit_out) {
        calculated_crc ^= CAN_CRC_POLY;
    }
    calculated_crc &= 0x7FFF; // Keep it exactly 15 bits
}

// Bit 0 - CAN TX, Bit 1 - CAN RX

void feed_can_decoder(uint8_t current_bits) {
    bool rx_pin = (current_bits & 0x02) != 0; // Bit 1
    
    // etect Start of Frame (SOF) - Dominant (LOW) state
    if (!decoding_active && !rx_pin) {
        decoding_active = true;
        // Wait 1.5 bit widths to get to the center of the first real ID bit
        sample_timer = CAN_SAMPLES_PER_BIT + (CAN_SAMPLES_PER_BIT / 2);
        
        last_sampled_bit = false; 
        consecutive_identical_bits = 1;
        
        // Reset all frame trackers
        current_can_frame_id = 0;
        dlc = 0;
        received_crc = 0;
        calculated_crc = 0;
        bits_decoded = 0;
        frame_state = STATE_ID;
        
        // SOF is included in the CRC calculation
        update_crc(false); 
        return;
    }

    // Sampling 
    if (decoding_active) {
        if (--sample_timer == 0) {
            sample_timer = CAN_SAMPLES_PER_BIT; // Reset for next bit
            
            // Bit stuffing removal logic
            if (consecutive_identical_bits == 5) {
                // Ignore if it is a "Stuff Bit", use it to reset our identical bit counter
                consecutive_identical_bits = 1;
                last_sampled_bit = rx_pin;
                return; // Do not process this bit as data
            }

            // Update identical bit counters
            if (rx_pin == last_sampled_bit) {
                consecutive_identical_bits++;
            } else {
                consecutive_identical_bits = 1;
                last_sampled_bit = rx_pin;
            }

            // Decode Frame 
            // We feed the unstuffed bits into our math engine ONLY before the CRC field
            if (frame_state != STATE_CRC) {
                update_crc(rx_pin);
            }

            switch (frame_state) {
                case STATE_ID:
                    // After the SOF, the next 11 bits are the Packet ID.
                    current_can_frame_id = (current_can_frame_id << 1) | (rx_pin ? 1 : 0);
                    bits_decoded++;
                    
                    if (bits_decoded == 11) {
                        frame_state = STATE_CTRL;
                        bits_decoded = 0;
                    }
                    break;

                case STATE_CTRL:
                    // Next 7 bits: RTR (1), IDE (1), r0 (1), DLC (4)
                    bits_decoded++;
                    if (bits_decoded > 3) {
                        dlc = (dlc << 1) | (rx_pin ? 1 : 0); // Last 4 bits are the DLC
                    }
                    
                    if (bits_decoded == 7) {
                        if (dlc > 8) dlc = 8; // Cap at 8 bytes max
                        frame_state = (dlc > 0) ? STATE_DATA : STATE_CRC;
                        bits_decoded = 0;
                    }
                    break;

                case STATE_DATA:
                    // Read 'dlc' number of bytes (8 bits each)
                    data_bytes[bits_decoded / 8] = (data_bytes[bits_decoded / 8] << 1) | (rx_pin ? 1 : 0);
                    bits_decoded++;
                    
                    if (bits_decoded == (dlc * 8)) {
                        frame_state = STATE_CRC;
                        bits_decoded = 0;
                    }
                    break;

                case STATE_CRC:
                    // Read the 15-bit CRC transmitted by the hardware
                    received_crc = (received_crc << 1) | (rx_pin ? 1 : 0);
                    bits_decoded++;
                    
                    if (bits_decoded == 15) {
                        char buffer[128];
                        bool crc_valid = (received_crc == calculated_crc);
                        
                        snprintf(buffer, sizeof(buffer), 
                                 "[CAN] ID: 0x%03X | DLC: %d | Rx CRC: 0x%04X | Calc CRC: 0x%04X [%s]", 
                                 current_can_frame_id, dlc, received_crc, calculated_crc, 
                                 crc_valid ? "VALID" : "ERROR");
                        
                        stream_packet_to_pc(buffer);
                        decoding_active = false; // Reset for the next packet
                    }
                    break;
            }
        }
    }
}