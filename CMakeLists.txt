cmake_minimum_required(VERSION 3.20.0)

# Pull in the Zephyr build system
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(ble_keyboard)

# Tell CMake to compile your main.c file
target_sources(app PRIVATE 
    src/main.c 
)