#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "hardware_matrix.h"

// Subsystem Initializers
extern void init_serial_streamer(void);
extern void init_dma_ring_buffer(void);

int main(void) {
    printk("Start 8-Bit Logic Analyzer.\n");
    
    // Initialize Serial stream over the UART bridge
    init_serial_streamer();
    
    // Configure Hardware Matrix, start in IDLE mode
    configure_hardware_matrix(MODE_IDLE);
    printk("[SYSTEM] Matrix configured to safe IDLE state.\n");
    
    // Start DMA Capture engine
    init_dma_ring_buffer();
    printk("[SYSTEM] Boot complete!\n");
    printk("Waiting for PC App commands.\n");
    
    // Go to sleep, let background threads handle the capture & streaming
    k_sleep(K_FOREVER);
    return 0;
}