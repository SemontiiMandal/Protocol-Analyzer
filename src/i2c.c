#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(i2c_analyzer, LOG_LEVEL_INF);

// Define ESP32 Pins
#define PROBE_SCL_PIN 22
#define PROBE_SDA_PIN 21

// Updated I2C Packet struct
struct i2c_packet {
    uint8_t raw_byte;
    bool is_start_byte; // True if this is the first byte after a START condition
    bool ack_received;
};

K_MSGQ_DEFINE(i2c_msgq, sizeof(struct i2c_packet), 10, 4);

// ESP32 GPIO controller handle; gpio0 controls pins 0-31
const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

// Define states for the State Machine to detect edges
enum i2c_state { 
    STATE_IDLE, 
    STATE_READING, 
    STATE_WAITING_ACK 
};

// Thread 1 - The parser thread
void parser_thread(void){
    
    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("GPIO device not ready!");
        return;
    }

    gpio_pin_configure(gpio_dev, PROBE_SCL_PIN, GPIO_INPUT);
    gpio_pin_configure(gpio_dev, PROBE_SDA_PIN, GPIO_INPUT);

    LOG_INF("State Machine Initialized. Waiting for I2C Traffic...");

    // store State Machine previous values
    int prev_scl = 1, curr_scl = 1;
    int prev_sda = 1, curr_sda = 1;
    
    enum i2c_state state = STATE_IDLE;
    uint8_t current_byte = 0;
    int bit_count = 0;
    
    // NEW: Tracker for the address byte
    bool is_first_byte = false;

    while(1){
        // Read physical pins
        curr_scl = gpio_pin_get_raw(gpio_dev, PROBE_SCL_PIN);
        curr_sda = gpio_pin_get_raw(gpio_dev, PROBE_SDA_PIN);

        // Detect START (SDA falls while SCL is High)
        if (prev_scl == 1 && curr_scl == 1 && prev_sda == 1 && curr_sda == 0) {
            state = STATE_READING;
            current_byte = 0;
            bit_count = 0;
            is_first_byte = true; // Flag that a new transaction just started
        }

        // Detect STOP (SDA rises while SCL is High)
        if (prev_scl == 1 && curr_scl == 1 && prev_sda == 0 && curr_sda == 1) {
            state = STATE_IDLE;
        }

        // Sample Data on the SCL Rising Edge
        if (prev_scl == 0 && curr_scl == 1) {
            
            if (state == STATE_READING) {
                // Shift the bit in (so MSB is first)
                current_byte = (current_byte << 1) | curr_sda;
                bit_count++;

                if (bit_count == 8) {
                    state = STATE_WAITING_ACK;
                }
            }
            else if (state == STATE_WAITING_ACK) {
                // The 9th clock pulse determines the ACK / NACK
                bool is_ack = (curr_sda == 0); 

                // I2C full byte; Next pack it
                struct i2c_packet completed_frame = {
                    .raw_byte = current_byte,
                    .is_start_byte = is_first_byte,
                    .ack_received = is_ack
                };

                // Push to the queue
                k_msgq_put(&i2c_msgq, &completed_frame, K_NO_WAIT);

                // Reset to catch the next byte
                is_first_byte = false; // Subsequent bytes are data payloads
                current_byte = 0;
                bit_count = 0;
                state = STATE_READING;
            }
        }

        // Update history for the next iteration
        prev_scl = curr_scl;
        prev_sda = curr_sda;

        // Yield the CPU so the dispatcher thread can wake up and print
        k_yield();
    } 
}

// Thread 2 - The dispatcher thread
void dispatcher_thread(void){
    struct i2c_packet frame;

    while (1){
        // Block until we get a byte from the parser
        k_msgq_get(&i2c_msgq, &frame, K_FOREVER);

        if (frame.is_start_byte) {
            // Extract the 7-bit address and the 1-bit R/W flag
            uint8_t target_address = frame.raw_byte >> 1;
            bool is_read_op = frame.raw_byte & 0x01;

            printk("\n[START] Address: 0x%02X | Op: %s | ACK: %d\n", 
                   target_address, 
                   is_read_op ? "READ " : "WRITE", 
                   frame.ack_received);
        } 
        else {
            // If it's not the start byte, it's just payload data
            printk("        Data:    0x%02X | ACK: %d\n", 
                   frame.raw_byte, 
                   frame.ack_received);
        }
        
        // No sleep delay here because the dispatcher must run as fast as possible to empty the queue. 
        // K_FOREVER automatically yields the CPU when empty.
    }
}

// Start the threads
K_THREAD_DEFINE(parser_id, 1024, parser_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(dispatcher_id, 1024, dispatcher_thread, NULL, NULL, NULL, 7, 0, 0);