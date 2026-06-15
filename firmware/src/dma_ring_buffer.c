#include <zephyr/kernel.h>
#include "analyzer_types.h"

// Hardware headers for DMA 
#include <hal/dma_types.h>
#include <hal/gdma_ll.h>
#include <soc/gdma_struct.h>

// RAM block where data is stored
la_sample_t capture_ram[RING_BUFFER_SAMPLES];

// DMA Linked List Nodes (Must be word-aligned for ESP32)
__attribute__((aligned(4))) dma_descriptor_t dma_nodes[2];

// Assuming GDMA Channel 0 is used for PARLIO RX
#define PARLIO_GDMA_CHANNEL 0

void init_dma_ring_buffer(void) {
    
    // NODE 0: The First Half of RAM
    dma_nodes[0].buffer = (uint8_t *)&capture_ram[0];
    dma_nodes[0].dw0.size = (RING_BUFFER_SAMPLES / 2) * sizeof(la_sample_t);
    dma_nodes[0].dw0.length = dma_nodes[0].dw0.size;
    dma_nodes[0].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    dma_nodes[0].dw0.suc_eof = 0;
    dma_nodes[0].next = &dma_nodes[1]; // Point to Node 1

    // NODE 1: The Second Half of RAM 
    dma_nodes[1].buffer = (uint8_t *)&capture_ram[RING_BUFFER_SAMPLES / 2];
    dma_nodes[1].dw0.size = (RING_BUFFER_SAMPLES / 2) * sizeof(la_sample_t);
    dma_nodes[1].dw0.length = dma_nodes[1].dw0.size;
    dma_nodes[1].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    dma_nodes[1].dw0.suc_eof = 1; // Mark the end of the logical frame
    dma_nodes[1].next = &dma_nodes[0]; // INFINITE LOOP: Point back to Node 0

   // Reset DMA channel
    gdma_ll_rx_reset_channel(&GDMA, PARLIO_GDMA_CHANNEL);
    
    // Connect the DMA channel to PARLIO hardware trigger
    gdma_ll_rx_connect_to_periph(&GDMA, PARLIO_GDMA_CHANNEL, GDMA_TRIG_PERIPH_PARLIO_RX);
    
    // Load custom linked list n
    gdma_ll_rx_set_desc_addr(&GDMA, PARLIO_GDMA_CHANNEL, (uint32_t)&dma_nodes[0]);
    
    // Start DMA
    gdma_ll_rx_start(&GDMA, PARLIO_GDMA_CHANNEL);
    
    printk("Hardware DMA Linked List built. Buffer initialized.\n");
}

// Calculates where DMA is currently writing
uint32_t get_current_dma_write_index(void) {
    // &GDMA is the global struct representing the DMA peripheral base address in ESP-IDF.
    uint32_t current_addr = gdma_ll_rx_get_success_eof_desc_addr(&GDMA, PARLIO_GDMA_CHANNEL);
    
    // If the DMA hasn't started yet or returns a null pointer return index 0
    if (current_addr == 0) {
        return 0;
    }

    // Subtract the start of our array to get the offset in bytes
    uint32_t offset_bytes = current_addr - (uint32_t)&capture_ram[0];
    
    // Divide by size of the sample (1 byte here, as 8-bit bus) to get array index
    return offset_bytes / sizeof(la_sample_t);
}