cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(protocol_analyzer)

target_sources(app PRIVATE src/main.c)

# Conditionally compile based on Kconfig
zephyr_sources_ifdef(CONFIG_ENABLE_UART_ANALYZER src/uart.c)
zephyr_sources_ifdef(CONFIG_ENABLE_I2C_ANALYZER src/i2c.c)