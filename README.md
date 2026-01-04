# ESP32 Business Card Web Server

![PCB Business Card](https://kevinsun.dev/blog/img/business_card.jpg)

**A 0.8mm thick PCB business card that hosts its own website.**

This repository contains the KiCad hardware design files and the PlatformIO firmware source code for a custom PCB business card. The card features a built-in ESP32-C3 microcontroller, a PCB-edge USB-C connector, and enough flash memory to host a compressed version of my personal portfolio website.

**[Read the full write-up here.](https://kevinsun.dev/blog/pcb-business-card)**

## ⚡ Features

* **Self-Hosting:** Runs a full HTTP web server directly on the card.
* **Ultra-Thin:** Designed on a **0.8mm** PCB to meet USB-C thickness specifications.
* **Paperless:** Integrates directly into a USB port to power up and serve content.
* **Optimized:** Uses **LittleFS** and **GZIP compression** to serve a <14kB website with minimal latency.
* **Cost-Effective:** BOM target under **$5** using the ESP32-C3-FH4.

## 🛠 Hardware Design

The hardware is designed around the **Espressif ESP32-C3-FH4**, a RISC-V microcontroller with built-in 4MB flash, chosen for its low component count and ease of use.

### Key Specs

* **MCU:** ESP32-C3FH4 (RISC-V, 160MHz, WiFi/BLE)
* **Connector:** [PCB-Edge USB-C](https://github.com/AnasMalas/pcb-edge-usb-c) (Reversible)
* **PCB Thickness:** 0.8mm (Critical for USB-C fit)
* **Finish:** ENIG (Electroless Nickel Immersion Gold) for durability and aesthetics.

## 💻 Firmware

The firmware is built using **PlatformIO** with the **Arduino Framework** atop **ESP-IDF**.

### Key Capabilities

* **LittleFS Filesystem:** Decouples the website content from the C++ firmware logic.
* **GZIP Compression:** All web assets are pre-compressed (gzip) to reduce transmission size (~5kB) and load times.
* **OTA Updates:** The website and firmware can be updated wirelessly without plugging the card back into a programmer.
* **Dynamic Routing:** Automatically maps URLs (e.g., `/about`) to their corresponding compressed files (`/about.html.gz`).

### Directory Structure

```
├── hardware/        # KiCad schematics and PCB layout
├── src/             # C++ source code (main.cpp)
├── data/            # Website files (HTML, CSS, JS) to be uploaded to LittleFS
├── platformio.ini   # Project configuration
└── README.md

```

## 🚀 Getting Started

### 1. Build the Hardware

Order the PCB using the Gerbers in `hardware/gerbers` and assemble the components listed in the BOM.

### 2. Flash the Firmware

1. Install [PlatformIO](https://platformio.org/).
2. Clone this repository.
3. Connect the board via USB (hold the BOOT button while plugging in to enter bootloader mode).
4. Upload the firmware:
```bash
pio run --target upload
```



### 3. Upload the Website

The website files live in the `data/` folder. You must upload the filesystem image separately from the code.

```bash
# Compress your site and place in data/
# Then run:
pio run --target uploadfs
```

## 🌐 Live Demo

I've set up an ESP32 webserver hosting a mirror of my website 24/7.

* **Card Site:** [https://card.kevinsun.dev](https://card.kevinsun.dev)
* **Card Status:** [https://card.kevinsun.dev/status](https://www.google.com/url?sa=E&source=gmail&q=https://card.kevinsun.dev/status) (Tunneled via Cloudflare)

## 📄 License

* **Hardware:** Creative Commons Attribution-ShareAlike 4.0 (CC BY-SA 4.0)
* **Firmware:** MIT License

## 🙌 Credits & Inspiration

* **[mitxela](https://mitxela.com/projects/stylocard):** For the original StyloCard inspiration.
* **[Anas Malas](https://github.com/AnasMalas/pcb-edge-usb-c):** For the PCB-edge USB-C footprint design.
* **Kevin Sun:** Project Creator.