#include <zephyr/kernel.h>
#include <stdbool.h>
#include "analyzer_config.h"
#include "protocol_masks.h"
#include "hardware_matrix.h"

// Ring Buffer
extern la_sample_t capture_ram[RING_BUFFER_SAMPLES];

// Current System Mode, this is basically a State Machine
extern volatile analyzer_mode_t current_system_mode;
extern volatile bool is_capturing;

// Get current index of DMA Write in Circular Buffer
extern uint32_t get_current_dma_write_index(void);

// Decoders for different protocols; Will be called depending on the System State
extern void feed_spi_decoder(uint8_t isolated_bits);
extern void feed_i2c_decoder(uint8_t isolated_bits);
extern void feed_can_decoder(uint8_t isolated_bits);
extern void feed_uart_decoder(uint8_t isolated_bits);

// Create a binary semaphore (max count 1, starts at 0).
// This acts as the event trigger from the USB interrupt.

K_SEM_DEFINE(capture_semaphore, 0, 1);

// Runs in the background, called when trigger fires
void parser_thread(void) {
    uint32_t read_index = 0; 

    while (1) {
        
        // Thread sleeps normally, will only wake up when usb_streamer.c calls k_sem_give().
        k_sem_take(&capture_semaphore, K_FOREVER);

        // Wake up and sync read pointer to the DMA head 
        read_index = get_current_dma_write_index();

        // Running state 
        // Stay in this loop as long as capture ON
        while (is_capturing && current_system_mode != MODE_IDLE) {
            
            uint32_t write_index = get_current_dma_write_index();

            // Process all unread data in the circular buffer
            if (read_index != write_index) {

                // XOR the index by 1 to un-swap the bytes created by the ESP32 I2S DMA
                
                la_sample_t snapshot = capture_ram[read_index ^ 1];
                
                switch (current_system_mode) {
                    // Protocol modes supported
                    case MODE_SPI_ONLY:
                        feed_spi_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        break;
                    case MODE_I2C_ONLY:
                        feed_i2c_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        break;
                    case MODE_UART_ONLY:
                        feed_uart_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        break;
                    case MODE_CAN_ONLY:
                        feed_can_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        break;
                    case MODE_DUAL_SPI_I2C:
                        feed_spi_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        feed_i2c_decoder((snapshot & MASK_SLOT_2) >> SHIFT_SLOT_2);
                        break;
                    case MODE_SPI_I2C_UART:
                        feed_spi_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        feed_i2c_decoder((snapshot & MASK_SLOT_2) >> SHIFT_SLOT_2);
                        feed_uart_decoder((snapshot & MASK_SLOT_3) >> SHIFT_SLOT_3);
                        break;
                    case MODE_SPI_CAN_UART:
                        feed_spi_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        feed_can_decoder((snapshot & MASK_SLOT_2) >> SHIFT_SLOT_2);
                        feed_uart_decoder((snapshot & MASK_SLOT_3) >> SHIFT_SLOT_3);
                        break;
                    case MODE_I2C_CAN_UART:
                        feed_i2c_decoder((snapshot & MASK_SLOT_1) >> SHIFT_SLOT_1);
                        feed_can_decoder((snapshot & MASK_SLOT_2) >> SHIFT_SLOT_2);
                        feed_uart_decoder((snapshot & MASK_SLOT_3) >> SHIFT_SLOT_3);
                        break;
                        
                    default: break;
                }

                read_index = (read_index + 1) % RING_BUFFER_SAMPLES;
            } 
            else {
                // k_yield() only hands off to OTHER THREADS AT THE SAME
                // PRIORITY -- it never lets a lower-priority thread run.
                // With write_index stuck (the still-open DMA bug), this
                // loop was spinning here forever at priority 5, which
                // silently starved every lower-priority thread in the
                // system (e.g. dummy_stream_thread at priority 6) even
                // though it looked like an idle "wait for data" yield.
                // k_msleep() actually blocks this thread and removes it
                // from the ready queue, so lower-priority threads get to
                // run during the wait -- and it's also just more correct
                // for a real "poll every so often" loop than a busy-yield.
                k_msleep(1);
            }
        }
        
        // If the user sends 'H', is_capturing becomes false.
        // The inner loop breaks, and we loop back up to k_sem_take and go back to sleep.
    }
}

// Start parser thread at boot
K_THREAD_DEFINE(parser_id, 4096, parser_thread, NULL, NULL, NULL, 5, 0, 0);