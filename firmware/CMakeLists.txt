cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(Protocol-Analyzer)

zephyr_include_directories(
    ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/hal/include
    ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/hal/esp32/include
    ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/soc/esp32/include
    ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/soc/include
)
# Legacy / Global approach (Not recommended)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


target_sources(app PRIVATE 
    src/main.c 
    src/hardware_matrix.c 
    src/dma_ring_buffer.c 
    src/parser.c 
    src/decode_spi.c 
    src/decode_i2c.c
    src/decode_can.c 
    src/decode_uart.c  
    src/serial.c
)