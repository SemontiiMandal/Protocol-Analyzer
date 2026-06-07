#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// I2C Packet struct
struct i2c_packet{
    uint8_t address;
    uint8_t data;
    bool is_read_cmd; // read or write flag
};

// Allocate K_MSGQ queue
/* 
    Arguments are:
    Queue Name, size of each item, max number of items, memory alignment (using 4 bytes as ESP32 MCU is 32 bit)
*/

K_MSGQ_DEFINE(i2c_msgq, sizeof(struct i2c_packet), 10, 4);

// Thread 1: The parser thread
// Parses I2C packets from the I2C bus

void parser_thread(void){

    // Parse from from I2C bus
    // Need to think how to set up DMA to do this
    while(1){
        struct i2c_packet new_frame = {
            .address = 0x00,
            .data = 0xFF,
            .is_read_cmd = true
        };

    // Push parsed packet to queue
    int ret = k_msgq_put(&i2c_msgq, &new_frame, K_NO_WAIT);

    if (ret != 0)
    printk("Warning! Queue is full, dropping frame!\n");

    k_msleep(200);

}

}

void dispatcher_thread(void){
    // pull i2c data packet from queue

    struct i2c_packet frame_read;

    while (1){

        // Using K_FOREVER makes the RTOS put this thread to sleep until a data packet arrives and needs dispatching, so saves CPU.
        k_msgq_get(&i2c_msgq, &frame_read, K_FOREVER);

        // print the data to console
        printk("I2C frame read -> Address: 0x%02X, Data: 0x%02X, Read:%d\n",
        frame_read.address, frame_read.data, frame_read.is_read_cmd);
    
        k_msleep(500);
    }

}

// Start the threads!
K_THREAD_DEFINE(parser_id, 1024, parser_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(dispatcher_id, 1024, dispatcher_thread, NULL, NULL, NULL, 7, 0, 0);
