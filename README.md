# 📸 Custom Digital Photo Frame

A fully custom digital photo frame built around an **ESP32-S3**, with a cloud-backed photo library, automated image processing, and a web interface for managing photos.

The project combines **embedded C++**, **FreeRTOS**, **HTTPS networking**, **SD-card storage**, **image processing**, **cloud infrastructure**, and a **Next.js web application** into one end-to-end system.

> Upload a photo → process it for the display → store it in the cloud → automatically download it to the frame → display it.

---

## ✨ Features

- 🖼️ 800×480 TFT display
- 📶 Wi-Fi connectivity
- ☁️ Cloud-backed photo synchronization
- 💾 MicroSD local photo cache
- 🔒 HTTPS communication with certificate verification
- 🧹 Fault-tolerant downloads using temporary files
- 🖥️ Web interface for uploading and managing photos
- ⚙️ Automated image processing for constrained hardware
- 🧵 FreeRTOS-based embedded architecture
- 🐳 Containerized image-processing service
- 🔄 Automatic synchronization between the cloud and physical frame

---

## 🏗️ Architecture

The project is organized as a monorepo containing three major components:

```
                     ┌────────────────────┐
                     │      Web Client      │
                     │    Next.js / React    │
                     │  Upload & Manage       │
                     │       Photos           │
                     └──────────┬─────────────┘
                                │
                                ▼
                     ┌────────────────────┐
                     │      Supabase        │
                     │  Database + Storage    │
                     │      + API Layer       │
                     └───────┬────────┬───────┘
                             │        │
              ┌──────────────┘        └──────────────┐
              ▼                                       ▼
   ┌──────────────────┐                    ┌──────────────────┐
   │  Image Processor   │                    │     ESP32-S3       │
   │  Python + Docker    │                    │  Wi-Fi + HTTPS      │
   │  Resize / Convert    │                    │  SD Card + TFT       │
   └──────────────────┘                    └────────┬─────────┘
                                                       │
                                                       ▼
                                             ┌──────────────────┐
                                             │   Local Cache       │
                                             │    MicroSD            │
                                             └──────────────────┘
```

The core design principle is simple: **push expensive work away from the resource-constrained ESP32.** The ESP32 is responsible for networking, storage, rendering, and synchronization, while the server-side components handle image processing and cloud management.

---

## 🔄 Photo Pipeline

### 1. Upload
A user uploads a photo through the web application, built with **Next.js**, **React**, **TypeScript**, and **Supabase**. The web app provides the user-facing interface for managing the photo collection.

### 2. Cloud Storage
Uploaded photos and their metadata are stored using **Supabase**, which acts as the central communication point between the web application, image processor, and physical frame.

### 3. Image Processing
The uploaded image is processed by a dedicated **Python** image-processing service, containerized with **Docker**, that prepares images for the physical display before they reach the ESP32. This keeps expensive image manipulation off the microcontroller, where CPU time and memory are far more constrained.

### 4. Synchronization
The ESP32 periodically checks the cloud backend for images that aren't already on its SD card. When it finds a new one, it:

1. Establishes an HTTPS connection
2. Retrieves the image from its signed cloud URL
3. Streams the image into a temporary file on the SD card
4. Verifies the download completed successfully
5. Renames the temporary file to its final `.bmp` filename
6. Adds the image to the local cache

This prevents interrupted downloads from becoming corrupted images in the cache.

### 5. Display
The frame displays photos directly from its local SD-card cache, so it doesn't need to re-download an image every time it's shown — and it keeps working even when the network is unavailable.

---

## 🔧 Hardware

| Component | Specification |
|---|---|
| Microcontroller | ESP32-S3 |
| Display | 800×480 TFT |
| Storage | MicroSD |
| Connectivity | Wi-Fi |
| Framework | Arduino / PlatformIO |
| RTOS | FreeRTOS |

**Embedded stack:** C++ · Arduino · FreeRTOS · PlatformIO · LovyanGFX · ArduinoJson · WiFiClientSecure · HTTPClient · MicroSD

---

## 💻 Software Architecture

### ESP32 Firmware
Responsible for all functionality directly tied to the physical device:

- Display initialization and rendering
- Photo slideshow management
- Wi-Fi connection management
- HTTPS communication
- Cloud synchronization
- SD-card filesystem management
- Local photo caching
- Streaming images from SD storage
- Handling incomplete network transfers
- FreeRTOS task execution

Display, storage, and frame-management responsibilities are separated into individual components.

### Web Application
The interface for managing the photo library.

**Stack:** TypeScript · React · Next.js · Supabase

Deployed independently from the firmware and image-processing service.

### Image Processor
An independent Python service responsible for preparing uploaded images for the physical display.

**Stack:** Python · Docker

Kept separate from the ESP32 so computationally expensive image transformations never consume embedded CPU time or memory.

---

## 🧠 Engineering Decisions

**Why process images before sending them to the ESP32?**
The ESP32-S3 is dramatically more constrained than the machines running the cloud services. Rather than have the frame perform resizing and format conversion, those operations run upstream, keeping the firmware focused only on what it actually needs to do: download, cache, and render.

**Why use a local SD-card cache?**
Repeatedly downloading images would waste bandwidth and make the frame dependent on a reliable connection. Instead, the ESP32 scans the SD card on boot, tracks which images are already local, and only downloads what's missing during sync.

**Why download to a temporary file?**
A network connection can fail mid-download. Writing directly to the final `.bmp` file could leave a corrupted image that looks valid on next boot. Instead, data streams to `image.tmp` and is only renamed to `image.bmp` once the download completes — incomplete `.tmp` files can be cleaned up during cache maintenance.

**Why stream images instead of loading them into memory?**
A processed image can be over a megabyte, while the ESP32 has far less memory than a desktop or server. The firmware streams image data through a small RAM buffer straight to the SD card rather than loading the whole file into memory, substantially reducing memory pressure during downloads.

---

## 🔐 Networking

The frame communicates with the cloud backend over HTTPS, with TLS certificate verification enabled using the appropriate root CA rather than relying on an insecure connection.

The firmware also handles real-world network conditions, including:

- Wi-Fi connection failures
- DNS failures
- HTTP errors
- Interrupted downloads
- Partial network reads
- Read timeouts
- Incomplete files
- Retry/synchronization cycles

This matters for an embedded device that may stay powered on for long stretches without user intervention.

---

## 🧩 Repository Structure

```
photo-frame/
│
├── firmware/
│   ├── include/
│   ├── lib/
│   ├── src/
│   ├── test/
│   └── platformio.ini
│
├── image-processor/
│   ├── app/
│   │   ├── core/
│   │   ├── models/
│   │   ├── services/
│   │   ├── config.py
│   │   └── main.py
│   ├── Dockerfile
│   └── requirements.txt
│
├── web/
│   ├── app/
│   ├── lib/
│   │   └── supabase/
│   ├── public/
│   ├── package.json
│   └── ...
│
└── README.md
```

The three components live in a single repository while remaining independently deployable.

---

## 🚀 Deployment

Each component is designed to be deployed independently:

- **Web** — Next.js app deployed on Vercel
- **Backend** — Supabase (database + storage + API layer)
- **Image Processor** — Python service in Docker, deployed to the cloud
- **Firmware** — ESP32-S3, communicating with the backend over HTTPS

None of the components need to run on the same machine.

---

## 📚 What I Learned

This project started as a digital photo frame, but turned into a hands-on exploration of several areas of software engineering:

**Embedded Systems** — Embedded C++, FreeRTOS, SPI peripherals, SD-card filesystems, display drivers, memory-constrained programming

**Networking** — Wi-Fi on embedded devices, TCP/TLS, HTTPS, DNS, streaming network data, connection failures and timeouts, certificate validation

**Distributed Systems** — Service separation, cloud-backed synchronization, client-side caching, failure handling, communication between independently deployed components

**Backend / Cloud** — Supabase, cloud storage, database-backed APIs, Docker, Python services

**Web Development** — Next.js, React, TypeScript, Supabase integration

---

## 🚧 Project Status

**Work in progress**

**Implemented**
- [x] ESP32-S3 photo frame
- [x] 800×480 display
- [x] SD-card storage
- [x] Local photo caching
- [x] Wi-Fi connectivity
- [x] HTTPS communication
- [x] Cloud synchronization
- [x] Streaming image downloads
- [x] Temporary-file download system
- [x] Image-processing service
- [x] Dockerized image processor
- [x] Web-based photo management
- [x] Supabase backend
- [x] Monorepo architecture

**Planned**
- [ ] Physical enclosure
- [ ] More robust synchronization/retry logic
- [ ] Remote device configuration
- [ ] Power-management improvements
- [ ] Additional image-processing optimizations
- [ ] Display/image performance improvements
- [ ] Eviction logic for when SD card runs out of storage
- [ ] Uploading multiple images at once
- [ ] User GUI to manually crop/process images

---