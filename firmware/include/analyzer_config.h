#pragma once

#include <stdint.h>

// Using 8 bit Data Bus Width for ESP32 PARLIO
#define PARLIO_BUS_WIDTH 8 
typedef uint8_t la_sample_t;  

// Define depth of the circular buffer.
// 100,000 samples @ 8-bit = 100 KB of HP (High-Performance) SRAM.
 
#define RING_BUFFER_SAMPLES 100000 

// Expose RAM buffer to the parser thread
extern la_sample_t capture_ram[RING_BUFFER_SAMPLES];