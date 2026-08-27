# Custom Digital Picture Frame

A fully custom digital picture frame built around an **ESP32-S3**, with a cloud-backed photo management system and an automated image-processing pipeline.

The system combines embedded firmware, a web application, cloud storage, and image processing into a single end-to-end photo pipeline.

## Overview

The project is split across three components:

```text
┌──────────────────────┐
│     Web Application  │
│      Next.js         │
│                      │
│ Upload / Manage      │
│ Photos               │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│       Supabase       │
│                      │
│ Storage + Database   │
│ Edge Functions       │
└──────────┬───────────┘
           │
           ├──────────────────────┐
           │                      │
           ▼                      ▼
┌──────────────────────┐  ┌──────────────────────┐
│  Image Processor     │  │    ESP32-S3 Frame    │
│                      │  │                      │
│ Resize / Convert     │  │ Download / Cache     │
│ Images for Display   │  │ Display Photos       │
└──────────────────────┘  └──────────────────────┘
```

### Repositories

| Component              | Repository                                                                                    | Purpose                                                                  |
| ---------------------- | --------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| 🖼️ **Frame Firmware** | [`custom-digital-picture-frame`](https://github.com/davidchen20/custom-digital-picture-frame) | ESP32-S3 firmware, display, SD storage, Wi-Fi, and cloud synchronization |
| ⚙️ **Image Processor** | [`photo-frame-image-processor`](https://github.com/davidchen20/photo-frame-image-processor)   | Processes uploaded images into formats suitable for the physical frame   |
| 🌐 **Web Application** | [`photo-frame-web`](https://github.com/davidchen20/photo-frame-web)                           | Web interface for uploading and managing photos                          |

## How It Works

The complete pipeline is:

```text
                    Upload
                      │
                      ▼
             ┌─────────────────┐
             │   Web App       │
             │   Next.js       │
             └────────┬────────┘
                      │
                      ▼
             ┌─────────────────┐
             │    Supabase     │
             │ Storage / DB    │
             └────────┬────────┘
                      │
                      ▼
             ┌─────────────────┐
             │ Image Processor │
             │                 │
             │ Resize / Format │
             └────────┬────────┘
                      │
                      ▼
             ┌─────────────────┐
             │    Supabase     │
             │ Processed Image │
             └────────┬────────┘
                      │
                  HTTPS / Wi-Fi
                      │
                      ▼
             ┌─────────────────┐
             │    ESP32-S3     │
             │                 │
             │ Download        │
             │ Cache to SD     │
             │ Display         │
             └─────────────────┘
```

### 1. Upload

Photos are uploaded through the web application.

The web frontend is built with **Next.js** and integrates with Supabase.

### 2. Image Processing

Uploaded images are passed through a dedicated image-processing service.

The processor is implemented as a Python application and includes a Docker configuration for deployment.

The purpose of this service is to prepare images for the constrained hardware rather than requiring the ESP32 to perform expensive image processing itself.

### 3. Cloud Storage

Processed images and their associated metadata are stored in Supabase.

The cloud layer acts as the interface between the web application, image processor, and physical frame.

### 4. Synchronization

The ESP32 periodically connects to the cloud backend and checks for photos that are not already present in its local cache.

New images are downloaded over HTTPS and stored on the SD card.

### 5. Display

The ESP32 displays photos from its local cache on the 800×480 display.

Keeping photos locally cached allows the frame to continue displaying images without repeatedly downloading the same files.

---

# Hardware

| Component       | Specification        |
| --------------- | -------------------- |
| Microcontroller | ESP32-S3             |
| Display         | 800×480 TFT          |
| Storage         | MicroSD              |
| Connectivity    | Wi-Fi                |
| Framework       | Arduino / PlatformIO |
| RTOS            | FreeRTOS             |

The ESP32 firmware uses LovyanGFX for display management and ArduinoJson for JSON processing.

---

# Software Architecture

The project intentionally separates responsibilities across multiple services.

### ESP32 Firmware

Responsible for everything directly related to the physical device:

* Display initialization and rendering
* SD-card storage
* Wi-Fi connectivity
* Cloud synchronization
* Local photo caching
* Slideshow management
* HTTPS downloads
* FreeRTOS task execution

The firmware is organized into separate display, storage, and photo-frame components.

### Image Processor

The image processor runs independently from the embedded device.

This allows computationally expensive image transformations to happen in the cloud/server environment instead of consuming ESP32 CPU time and memory.

**Stack:**

* Python
* Docker

### Web Application

The web application provides the user-facing interface for managing the photo collection.

**Stack:**

* Next.js
* TypeScript
* React
* Supabase

The current application is structured as a Next.js project and includes a Supabase integration.

---

# Why Build It This Way?

The project is designed around the constraints of the physical device.

The ESP32 has significantly less compute, memory, and storage than a typical server or desktop computer. Instead of making the microcontroller responsible for everything, the system pushes expensive work upstream:

```text
          More capable systems
                 ▲
                 │
       Image processing
       Web application
       Database / storage
                 │
                 ▼
        Resource-constrained
             ESP32-S3
                 │
          Display + cache
```

This separation keeps the embedded firmware relatively lightweight while allowing the rest of the system to scale independently.

---

# Key Engineering Challenges

### Embedded Storage

The frame maintains a local cache of downloaded photos on an SD card.

Images are downloaded to temporary files before being committed to the cache, preventing incomplete downloads from being treated as valid images.

### Unreliable Networks

The ESP32 has to handle real-world network behavior, including connection failures and incomplete reads during large image downloads.

### Resource Constraints

Image processing is deliberately moved away from the ESP32 so that the microcontroller primarily handles:

* Networking
* Storage
* Image rendering
* Frame scheduling

### Distributed Components

The final system spans:

* A browser
* A web server
* Cloud storage/database
* An image-processing service
* An embedded device

This makes the project as much a **distributed software system** as an embedded project.

---

# Technologies

### Embedded

* C++
* ESP32-S3
* Arduino
* FreeRTOS
* PlatformIO
* LovyanGFX
* ArduinoJson
* HTTPS
* MicroSD

### Backend / Cloud

* Supabase
* Docker
* Python

### Web

* TypeScript
* React
* Next.js

---

# Project Status

🚧 **Work in Progress**

### Implemented

* [x] ESP32-S3 photo frame
* [x] 800×480 display
* [x] SD-card photo cache
* [x] Wi-Fi connectivity
* [x] Cloud synchronization
* [x] HTTPS image downloads
* [x] Image processing service
* [x] Web-based photo management
* [x] Supabase backend
* [x] Distributed multi-component architecture

### Planned

* [ ] Physical enclosure
* [ ] Improved image-processing pipeline
* [ ] More robust synchronization
* [ ] Remote device configuration
* [ ] Power-management improvements
* [ ] Additional display/image optimizations

---

# What I'm Learning

This project is an exploration of **embedded systems, distributed systems, and full-stack software engineering**.

It provides hands-on experience with:

* Embedded C++
* FreeRTOS
* Resource-constrained networking
* SD-card filesystems
* Display drivers
* HTTPS
* Cloud storage
* Database-backed applications
* Image processing pipelines
* Docker
* Next.js
* Distributed system design

---
