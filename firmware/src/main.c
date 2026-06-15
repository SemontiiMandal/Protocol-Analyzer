#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "hardware_matrix.h"

/* --- Subsystem Initializers --- */
extern void init_usb_streamer(void);
extern void init_dma_ring_buffer(void);

int main(void) {
    printk("Start 8-Bit Logic Analyzer.\n");

    // Initialize USB CDC-ACM 
    init_usb_streamer();

    // Configure Hardware Matrix, start in IDLE mode
    configure_hardware_matrix(MODE_IDLE);
    printk("[SYSTEM] Matrix configured to safe IDLE state.\n");

    // Start DMA Capture
    /* This allocates the 100KB RAM buffer and commands the ESP32-C6's  DMA engine to start the infinite recording loop. From this line orward, the DMA runs completely independently of the CPU.
     */
    init_dma_ring_buffer();

    printk("[SYSTEM] Boot complete!\n");
    printk("Waiting for PC App commands.\n");

    // Go to sleep
    k_sleep(K_FOREVER);

    return 0;
}