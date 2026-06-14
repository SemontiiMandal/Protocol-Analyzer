#ifndef PROTOCOLS_SUPPORTED_H
#define PROTOCOLS_SUPPORTED_H

#include <zephyr/kernel.h>
#include <stdint.h>

typedef enum {
    I2C,
    SPI,
    CAN,
    UART
} protocol;

typedef struct{
    protocol name; // Protocol ID, eg: CAN, UART, etc.
    uint32_t timestamp_us; // Timestamp of payload
    uint8_t payload_length; // length of data packet
    uint8_t payload[8]; // Actual data in the packet, an unsigned int array of length 8, i.e. 8 bit data
    uint8_t *raw_data_ptr;      // Pointer to DMA memory block
} data_packet;

/*
    Zephyr feature: 
    Memory Slab (k_mem_slab) - Dedicated chunk SRAM specifically carved out for DMA to write to.
*/

// Declaring a global message queue to capture and decode data packets
extern struct k_msgq global msgq;

// Declare global memory slab
extern struct k_mem_slab dma_capture_slab;

#endif