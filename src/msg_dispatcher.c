#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "protocols_supported.h"

// Register a logging module 'dispatcher' on the zephyr logging subsystem;
// Severity level 'INF'means information, so system processes inf, warn and err logs but ignores dbg logs
LOG_MODULE_REGISTER(dispatcher, LOG_LEVEL_INF);

// Function to dispatch data packets from the queue

void dispatcher_thread(void){
    data_packet packet;

    LOG_INF("Dispatcher waiting for data.");

    // Print which protocol data is from
    switch(packet.name){
        case I2C:
        LOG_INF("[%u us] [I2C] Data: 0x%02X", packet.timestamp_us, packet.payload);

        case SPI:
        LOG_INF("[%u us] [SPI] Data: 0x%02X", packet.timestamp_us, packet.payload);

        case CAN:
        LOG_INF("[%u us] [CAN] Data: 0x%02X", packet.timestamp_us, packet.payload);

        case UART:
        LOG_INF("[%u us] [UART] Data: 0x%02X", packet.timestamp_us, packet.payload);

        default:
        LOG_INF("Unknown Packet!");
    }
}

// Define the thread with a 2048-byte stack and priority 3
K_THREAD_DEFINE(dispatcher_id, 2048, dispatcher_thread, NULL, NULL, NULL, 3, 0, 0);

/*
    Parameters for K_THREAD_DEFINE:
    1. name: The variable name for the thread ID.
    2. stack_size: Size of the stack in bytes.
    3. entry: The entry function name.
    4, 5, 6. p1, p2, p3: Up to three argument values passed to the entry function.
    7. prio: Scheduling priority.
    8. options: Thread options (e.g., K_ESSENTIAL).
    9. delay: Time in milliseconds to wait before starting.
*/