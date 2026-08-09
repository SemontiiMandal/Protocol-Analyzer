## Cypher logic and protocol analyzer

Cypher is an open-source hardware logic analyzer for embedded systems. It captures digital signals, decodes common communication protocols in real time, and displays the results in a Python desktop application.

I designed the hardware and firmware from the ground up and validate Cypher against a Saleae logic analyzer to compare decoded data directly against the physical wire.

## Assembled Cypher
<img width="342" height="512" alt="image" src="https://github.com/user-attachments/assets/5a24ad59-41bc-45e7-9e92-424feee6e0dd" />

*An image showing the assembled Cypher PCB (Printed Circuit Board); ESP32 slots into the female headers along the edges. Male headers on the top are the logic analyzer input channels for probing target communication protocols*

## Interface and visualization

<img width="1600" height="467" alt="image" src="https://github.com/user-attachments/assets/335a177a-5c86-487f-8c5d-9ef38061cfaa" />

*The Cypher desktop interface displaying live hardware controls alongside toggling digital waveforms.*

<img width="1600" height="846" alt="image" src="https://github.com/user-attachments/assets/e42b4fb8-c683-498c-b3f2-8628cee19bfe" />

*Real-time terminal data stream printing the decoded protocol packets directly from the hardware.*

## Architecture

Cypher uses an 8-bit parallel capture architecture:

```text
Target Bus
    │
    ▼
Input Protection / Level Shifting
    │
    ▼
ESP32 GPIO Matrix
    │
    ▼
8-bit Parallel Capture
    │
    ▼
I2S + DMA
    │
    ▼
SRAM Ring Buffer
    │
    ▼
Zephyr Parser Thread
    │
    ├── SPI Decoder
    ├── I2C Decoder
    ├── UART Decoder
    └── CAN Decoder
            │
            ▼
      Serial Stream
            │
            ▼
    Python Desktop App

```

## Data pipeline

To eliminate CPU bottlenecks during high-speed sampling, Cypher uses a hardware-accelerated data pipeline running on Zephyr RTOS.

The ESP32 I2S peripheral runs in 8-bit parallel mode. It captures the physical pin states and pushes them directly into a 100 KB SRAM circular buffer using DMA. This process uses zero CPU cycles.

A high-priority Zephyr RTOS thread polls the hardware DMA write pointer. When new data is available, the thread reads the raw 8-bit snapshots, applies a bitwise XOR to reverse the byte order (handling an ESP32 I2S memory alignment quirk), and uses bitmasks to extract the individual pin states. It feeds these isolated 1s and 0s into the protocol decoders one tick at a time.

This separation of signal acquisition and protocol decoding allows the hardware to capture multiple protocols from the exact same sample stream.

## Decoder state machines

The decoders are independent C state machines. They do not use external libraries. They process the raw bitstream tick-by-tick to reconstruct the original data frames.

* **SPI:** Synchronous and edge-driven. The state machine waits for the Chip Select (CS) pin to go low. It monitors the clock pin (CLK) and shifts the MOSI and MISO bits into a temporary buffer on the active clock edge. After 8 bits, it outputs the completed byte and waits for the next edge.
* **I2C:** Edge and level-driven. It monitors the data line (SDA) while the clock (SCL) is high to catch START and STOP conditions. Once started, it shifts 8 data bits on the clock edges and captures the 9th bit to evaluate the ACK/NACK handshake.
* **UART:** Asynchronous and time-driven. It relies on an oversampling counter. When it detects a start bit (a falling edge on the RX pin), it waits half a bit period to center the sample. It then reads the bus at fixed intervals based on the configured baud rate to extract the frame and stop bit.
* **CAN 2.0:** Asynchronous and edge-synchronized. The decoder watches for a Start of Frame (SOF) dominant bit. It tracks consecutive identical bits to identify and discard hardware-injected stuffing bits. It parses the Arbitration ID and Data Length Code (DLC), shifts the payload bytes, and computes a running CRC-15. It only forwards the packet to the Python app if the calculated CRC matches the checksum read from the physical wire.

## Custom hardware

I designed the Cypher PCB in Altium Designer.

<img width="236" height="314" alt="image" src="https://github.com/user-attachments/assets/c648626c-3d74-43cc-a0d7-1440084c12c6" />
<img width="257" height="323" alt="image" src="https://github.com/user-attachments/assets/0844408f-6a54-41da-a868-507184612aad" />
<img width="245" height="308" alt="image" src="https://github.com/user-attachments/assets/326d12cb-f893-461a-8281-2c52773697d6" />

The main components are:

* ESP32: Handles parallel digital capture, GPIO routing, DMA, and future wireless capabilities.
* SN74LVC8T245: An 8-bit level translator that interfaces with higher-voltage target hardware.
* TCAN1051HV: The CAN physical-layer transceiver.

## Protocol support

* [x] SPI: Clock-edge synchronous decoder.
* [x] I2C: Edge-driven decoder with start and stop detection.
* [x] UART: Asynchronous state-machine decoder.
* [x] CAN 2.0: Bit-stuffing removal and CRC-15 verification.
* [x] Multi-protocol capture: Up to 8 simultaneous digital lanes.
* [ ] Wireless: Planned.

## Desktop application

The Python application provides:

* Hardware connection and capture control
* Protocol configuration
* Live decoded packet log
* Digital waveform visualization
* Capture statistics and error tracking
* Raw serial console
* CSV export

---

## Timing and synchronization

Previously, the Python app timestamped packets using `time.time()`. This caused timing inaccuracies because OS buffers and USB polling group serial data into batches, hiding the true bus timing.

Timekeeping now happens on the hardware. The Zephyr RTOS cycle counter generates a microsecond timestamp the moment a frame is decoded. The ESP32 prepends this time to the serial payload (e.g., `1450233,[SPI] MOSI: 0xAA | MISO: 0xBB`). This gives the desktop app the exact physical timing and ignores USB latency.

<img width="959" height="242" alt="image" src="https://github.com/user-attachments/assets/16ca4407-057a-4957-8d6f-333ff9530344" />

*Data logged with timestamps.*

## Validation

Cypher is tested against a Saleae logic analyzer using the same physical signals.

```text
                 Target Hardware
                  /          \
                 /            \
            Cypher           Saleae
               │                │
               ▼                ▼
          Cypher Decode    Saleae Decode
               │                │
               └───────┬────────┘
                       ▼
                 Compare Results

```

Test patterns verify decoded bytes, bit ordering, protocol timing, frame boundaries, and error detection.

## Roadmap

* [x] Custom hardware
* [x] DMA-based 8-bit capture
* [x] SPI, I2C, UART, and CAN decoding
* [x] Multi-protocol capture
* [x] Python desktop application
* [x] Saleae validation
* [ ] Hardware timestamping
* [ ] Wireless protocol capture

## Tech stack

* Firmware: C, Zephyr RTOS, ESP32, I2S, DMA, GPIO Matrix
* Hardware: Altium Designer, custom PCB, SN74LVC8T245, TCAN1051HV
* Desktop: Python, Tkinter, PySerial
