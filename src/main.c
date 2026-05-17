#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

// Queue to hold the intercepted data
K_MSGQ_DEFINE(uart_msgq, sizeof(char), 256, 4);

// The hardware interrupt - fires every time a byte arrives
static void uart_cb(const struct device *dev, void *user_data)
{
    if (!uart_irq_update(dev)) return;
    if (!uart_irq_rx_ready(dev)) return;

    char c;
    while (uart_fifo_read(dev, &c, 1) == 1) {
        k_msgq_put(&uart_msgq, &c, K_NO_WAIT);
    }
}

int main(void)
{
    // Using UART2 (RX2)
    const struct device *uart_sniffer = DEVICE_DT_GET(DT_NODELABEL(uart2));

    if (!device_is_ready(uart_sniffer)) {
        printk("UART Sniffer not ready!\n");
        return 0;
    }

    // Attach the interrupt
    uart_irq_callback_set(uart_sniffer, uart_cb);
    uart_irq_rx_enable(uart_sniffer);

    printk("Listening for UART traffic on ESP32 RX2 ...\n");

    while (1) {
        char c;
        // Wait for data to hit the queue
        if (k_msgq_get(&uart_msgq, &c, K_FOREVER) == 0) {
            // Print the raw hex and the character
            printk("Caught: 0x%02X (%c)\n", c, c); 
        }
    }
}