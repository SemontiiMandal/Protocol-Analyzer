#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include "hardware_matrix.h"

// Get USB device reference from a devicetree compatible
const struct device *usb_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

volatile bool is_capturing = false; 

// Bring in the semaphore defined in the parser engine
extern struct k_sem capture_semaphore;

// Listen to USB commands from PC App on the COM port; Interrupts fired, use FIFO
void usb_rx_isr(const struct device *dev, void *user_data) {
    uint8_t rx_char;

    // Check if the interrupt was valid
    if (!uart_irq_update(dev)) {
        return;
    }

    // Check if the interrupt was triggered by incoming data (RX)
    if (uart_irq_rx_ready(dev)) {
        
        // Read the incoming byte(s) out of the hardware FIFO
        while (uart_fifo_read(dev, &rx_char, 1) == 1) {
            
            // The PC App sends simple ASCII characters to switch modes
            switch (rx_char) {
                case '0': // PC sent "0" -> Switch to IDLE Mode 
                    printk("Command Rx: Routing Matrix to IDLE\n");
                    configure_hardware_matrix(MODE_IDLE);
                    break;
                
                case 'G': // "Go" -> Start streaming data to PC
                    printk("Command Rx: START CAPTURE\n");
                    is_capturing = true;
                    // Wake parser thread 
                    k_sem_give(&capture_semaphore);
                    break;
                    
                case 'H': // "Halt" -> Pause streaming
                    printk("Command Rx: STOP CAPTURE\n");
                    is_capturing = false;
                    break;

                case 'S': // PC sent "S" -> Switch to SPI Mode
                    printk("Command Rx: Routing Matrix for SPI\n");
                    configure_hardware_matrix(MODE_SPI_ONLY);
                    break;
                    
                case 'I': // PC sent "I" -> Switch to I2C Mode
                    printk("Command Rx: Routing Matrix for I2C\n");
                    configure_hardware_matrix(MODE_I2C_ONLY);
                    break;

                case 'C': // PC sent "C" -> Switch to CAN Mode
                    printk("Command Rx: Routing Matrix for CAN\n");
                    configure_hardware_matrix(MODE_CAN_ONLY);
                    break;

                case 'U': // PC sent "U" -> Switch to UART Mode
                    printk("Command Rx: Routing Matrix for UART\n");
                    configure_hardware_matrix(MODE_UART_ONLY);
                    break;
                    
                case '1': // PC sent "1" -> Multiple: SPI + I2C + UART
                    printk("Command Rx: Routing Matrix for SPI + I2C + UART\n");
                    configure_hardware_matrix(MODE_SPI_I2C_UART);
                    break;

                case '2': // PC sent "2" -> Multiple: SPI + CAN + UART
                    printk("Command Rx: Routing Matrix for SPI + CAN + UART\n");
                    configure_hardware_matrix(MODE_SPI_CAN_UART);
                    break;

                case '3': // PC sent "3" -> Multiple: I2C + CAN + UART
                    printk("Command Rx: Routing Matrix for I2C + CAN + UART\n");
                    configure_hardware_matrix(MODE_I2C_CAN_UART);
                    break;
                    
                default:
                    // Ignore unrecognized commands
                    break;
            }
        }
    }
}

// Initialization Function to init USB, called in main
void init_usb_streamer(void) {
    if (!device_is_ready(usb_dev)) {
        printk("Error: USB CDC device not ready\n");
        return;
    }

    // Enable USB
    int ret = usb_enable(NULL);
    if (ret != 0) {
        printk("Error: Failed to enable USB\n");
        return;
    }

    // Register listener function as a callback to listen to USB port
    uart_irq_callback_set(usb_dev, usb_rx_isr);
    
    // Enable RX interrupt 
    uart_irq_rx_enable(usb_dev);

    printk("USB Streamer Initialized and waiting for PC App commands.\n");
}