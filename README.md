# Protocol analyzer

This is an open-source hardware tool that reads and displays embedded communication protocols. It gives developers a cheap way to test hardware. I test this firmware against a Saleae logic analyzer to make sure the bytes match exactly on the physical wire.

## Assembled Cypher
<img width="342" height="512" alt="image" src="https://github.com/user-attachments/assets/5a24ad59-41bc-45e7-9e92-424feee6e0dd" />

*And image showing the assembled Cypher PCB (Printed Circuit Board)*


## Current status

The project uses an 8-bit matrix architecture running on Zephyr RTOS.

* DMA capture records data to a continuous ring buffer without using the CPU.
* The firmware supports SPI, I2C, UART, and CAN.
* The ESP32-C6 can route up to 8 pins at the same time to read multiple protocols on one bus.
* A Python app controls the hardware over a virtual USB CDC connection.

## How it works

The firmware processes data in five steps.

1. You send a one-byte ASCII command from the Python app over USB.
2. The Zephyr USB interrupt catches the command and tells the ESP32-C6 GPIO matrix to route the physical pins into an internal 8-bit parallel bus.
3. The hardware DMA engine continuously copies data from that bus into a 100 KB SRAM ring buffer. The CPU does not do any of this work.
4. An RTOS thread reads the ring buffer right behind the DMA write head. It uses bitmasks to separate the 8-bit chunks into specific slots based on the mode you chose.
5. The isolated bits go into C state machines. These decoders track timing and edges to read the bytes, handle CAN bit-stuffing, calculate CRCs, and send text back to your computer.

## Project roadmap

## Custom hardware
I designed a custom printed circuit board (PCB) in Altium Designer to run this logic analyzer. I call it Cypher!

<img width="236" height="314" alt="image" src="https://github.com/user-attachments/assets/c648626c-3d74-43cc-a0d7-1440084c12c6" />
<img width="257" height="323" alt="image" src="https://github.com/user-attachments/assets/0844408f-6a54-41da-a868-507184612aad" />
<img width="245" height="308" alt="image" src="https://github.com/user-attachments/assets/326d12cb-f893-461a-8281-2c52773697d6" />


**Microcontroller**

The board uses the ESP32-C6. I chose this chip for three reasons:

* Parallel IO (PARLIO): The chip can sample 8 pins at the exact same time and dump that data straight into the DMA ring buffer; Current Data Bus is 8 Bit Wide.
* Native USB: The chip has built-in USB routing. This kept the schematic simple because I did not need an external USB-to-serial converter.
* Wireless radios: It includes native Wi-Fi and Bluetooth for future wireless sniffing.

**Level shifter**

The ESP32-C6 operates strictly at 3.3V. If you probe a 5V target board, you will destroy the microcontroller. To prevent this, the board includes an SN74LVC8T245 8-bit level shifter. It reads the reference voltage from the target board and safely drops the incoming logic signals down to 3.3V before they reach the ESP32-C6 parallel bus.

**CAN transceiver**

A microcontroller cannot read raw CAN bus signals directly because the differential voltages are too high. The board includes a TCAN1051HV transceiver. It sits between the external probe connector and the ESP32-C6 to convert the raw CAN bus voltages into safe, standard digital logic.

### Protocol expansion

* [x] UART / Serial: Asynchronous time-driven decoding.
* [x] I2C: Synchronous edge-driven decoding with start and stop conditions.
* [x] SPI: High-speed synchronous data capture.
* [x] CAN 2.0: Asynchronous decoding with bit-stuffing removal and CRC-15 verification.
* [ ] Wireless: Using the ESP32 radio to sniff wireless packets.

## Testbench setup

* Target: Embedded hardware broadcasting test patterns.
* Sniffer: ESP32-C6 DevKit running Zephyr RTOS.
* Interface: Python desktop application.
* Ground truth: Saleae logic analyzer.

*Note: This repository is a work in progress. I am making updates to this weekly!*
