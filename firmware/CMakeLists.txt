cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(Protocol-Analyzer)

target_sources(app PRIVATE 
    src/main.c 
    src/hardware_matrix.c 
    src/dma_ring_buffer.c 
    src/parser_engine.c 
    src/decode_spi.c 
    src/decode_i2c.c
    src/decode_can.c 
    src/decode_uart.c  
    src/usb.c
)