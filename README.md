## Protocol Analyzer
An open-source hardware tool designed to sniff, capture, and display embedded communication protocols in real time. The goal of this project is to create a quick, reliable testing tool for developers who don't have access to expensive commercial protocol analyzers. To ensure maximum accuracy, this firmware is developed and verified in parallel with a professional Salae logic analyzer, matching byte-for-byte on the physical wire.

## Current Status
This project is still a  **Work in Progress**. 

* **UART/Serial support added!** The firmware successfully intercepts asynchronous serial data via hardware interrupts and streams the captured data to a local terminal.

---

## Project Roadmap

### Protocol Expansion
* [x] **UART / Serial:** Hardware interrupts verified against a commercial analyzer.
* [ ] **I2C:** Implementing passive eavesdropping (bypassing target address restrictions).
* [ ] **SPI:** High-speed synchronous data capturing.
* [ ] **Wireless:** Leveraging the ESP32's native radio capabilities for quick wireless packet sniffing.

---

## Current Testbench Setup
* **The Target:** An STM32 Bluepill board broadcasting continuous test patterns.
* **The Sniffer:** An ESP32 DevKitC running custom Zephyr RTOS firmware to intercept the lines.
* **The Ground Truth:** A commercial Saleae Logic Analyzer running in parallel to validate data integrity and timing.
---

*Note: This repository is a strict Work in Progress (WIP). Firmware architectures, device tree overlays, and schematic files are updated regularly as milestones are achieved.*
