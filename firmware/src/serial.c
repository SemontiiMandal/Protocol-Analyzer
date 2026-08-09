#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "hardware_matrix.h"

const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

volatile bool is_capturing = false; 

// Bring in the semaphore defined in the parser engine
extern struct k_sem capture_semaphore;

void stream_packet_to_pc(const char* packet) {
   // Capture hardware time
    uint32_t timestamp_us = k_ticks_to_us_floor32(k_cycle_get_32());
    
    //  Prepend the timestamp, add a comma delimiter, then send packet so that timestamps are more accurate and don't suffer from os level serial buffering timestamps lose precision 
    printk("%lu,%s\n", timestamp_us, packet);
}

// Listen to UART commands from the onboard USB-to-UART bridge
void uart_rx_isr(const struct device *dev, void *user_data) {
    uint8_t rx_char;

    if (!uart_irq_update(dev)) {
        return;
    }

    if (uart_irq_rx_ready(dev)) {
        
        while (uart_fifo_read(dev, &rx_char, 1) == 1) {
            
            switch (rx_char) {
                case '0': 
                    printk("Command Rx: Routing Matrix to IDLE\n");
                    configure_hardware_matrix(MODE_IDLE);
                    break;
                
                case 'G': 
                    printk("Command Rx: START CAPTURE\n");
                    is_capturing = true;
                    k_sem_give(&capture_semaphore);
                    break;
                    
                case 'H': 
                    printk("Command Rx: STOP CAPTURE\n");
                    is_capturing = false;
                    break;

                case 'S': 
                    printk("Command Rx: Routing Matrix for SPI\n");
                    configure_hardware_matrix(MODE_SPI_ONLY);
                    break;
                    
                case 'I': 
                    printk("Command Rx: Routing Matrix for I2C\n");
                    configure_hardware_matrix(MODE_I2C_ONLY);
                    break;

                case 'C': 
                    printk("Command Rx: Routing Matrix for CAN\n");
                    configure_hardware_matrix(MODE_CAN_ONLY);
                    break;

                case 'U': 
                    printk("Command Rx: Routing Matrix for UART\n");
                    configure_hardware_matrix(MODE_UART_ONLY);
                    break;
                    
                case '1': 
                    printk("Command Rx: Routing Matrix for Dual SPI + I2C\n");
                    configure_hardware_matrix(MODE_DUAL_SPI_I2C);
                    break;

                case '2': 
                    printk("Command Rx: Routing Matrix for SPI + CAN + UART\n");
                    configure_hardware_matrix(MODE_SPI_CAN_UART);
                    break;

                case '3': 
                    printk("Command Rx: Routing Matrix for I2C + CAN + UART\n");
                    configure_hardware_matrix(MODE_I2C_CAN_UART);
                    break;
                    
                default:
                    break;
            }
        }
    }
}

void init_serial_streamer(void) {
    if (!device_is_ready(uart_dev)) {
        printk("Error: UART device not ready\n");
        return;
    }

    uart_irq_callback_set(uart_dev, uart_rx_isr);
    uart_irq_rx_enable(uart_dev);

    printk("Serial Streamer Initialized and waiting for PC App commands.\n");
}