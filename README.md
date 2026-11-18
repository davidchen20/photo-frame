# Custom Digital Photo Frame

A Wi-Fi photo frame built with an ESP32-S3 and an 800×480 TFT display. Photos are uploaded through a web app, prepared for the display, and downloaded to a microSD card so the frame can show them without a constant network connection.

## Features

- 800×480 touchscreen display with a local photo slideshow
- On-device slideshow interval settings, saved across restarts
- Web interface for uploading, cropping, and managing multiple photos
- Automatic image processing for the display
- Cloud synchronization over Wi-Fi and HTTPS
- On-device Wi-Fi selection and phone-based network setup
- MicroSD photo cache for offline viewing
- Streamed downloads with temporary files to protect against incomplete transfers
- FreeRTOS-based firmware and a containerized image-processing service

## Architecture

The project has three main components: a web app for managing photos, an image-processing service, and the frame itself. Supabase provides storage, a database, and APIs shared by the components.

```text
    Web app
  Next.js / React
        |
        v
     Supabase <----> Image processor
  Database + storage    Python / Docker
        |
        v
    ESP32-S3
   Wi-Fi / HTTPS
        |
        v
  MicroSD cache ---> TFT display
```

The frame handles synchronization, storage, and display. Image processing runs separately, before photos reach the ESP32.

## How it works

1. Upload one or more photos in the web app, and optionally crop each one.
2. A Python service processes it for the display and stores it in Supabase.
3. The frame checks for new photos and downloads them to its SD card over HTTPS.
4. The frame displays photos from its local cache.

Image processing happens off the ESP32 to save memory and CPU time. Downloads are streamed to a temporary file and renamed only after they finish, so an interrupted transfer doesn't leave a partial image in the cache. The frame scans its SD card on startup and downloads photos it doesn't already have.

## Wi-Fi setup

When the frame starts, use the touchscreen to connect to saved Wi-Fi, set up a new network, or stay offline. Offline mode displays photos already cached on the SD card.

To set up a network:

1. Tap **Set up new Wi-Fi** on the frame.
2. On your phone, connect to `PhotoFrame-Setup` using the temporary password shown on the frame.
3. Open `http://192.168.4.1` in your phone's browser, enter the network name and password, and select **Connect**.
4. Check the frame for the connection result. After a successful connection, the frame saves the credentials for future use.

The ESP32-S3 uses 2.4 GHz Wi-Fi. Networks that require a separate browser sign-in page or device registration may need additional setup; the frame's setup page doesn't handle those sign-ins. Saved credentials are stored in the ESP32's internal flash using `Preferences`, while photos are stored on the SD card.

## Slideshow settings

Tap the screen during the slideshow to choose an interval from 5 seconds to 1 hour. The frame saves the choice in internal flash.

## Engineering decisions

**Process images before sending them to the frame.** Resizing and format conversion run in the Python service, where there is more memory and processing power available. The ESP32 can then focus on downloading and displaying the prepared images.

**Keep a local SD-card cache.** The frame doesn't need to download a photo every time it displays it, and cached photos remain available when the network is down. On startup, it scans the card so it can skip photos it already has during sync.

**Write downloads to temporary files.** A connection can drop partway through a transfer. The frame writes to a temporary file first and renames it to its final `.bmp` name only after the download completes, keeping partial images out of the cache.

**Stream downloads in small chunks.** A processed image can be larger than the memory available to the ESP32. Streaming it to the SD card through a small buffer avoids holding the entire file in RAM.

## Hardware and software

- **Frame:** Elecrow CrowPanel 7-inch ESP32-S3, 800×480 touchscreen, microSD card, and Wi-Fi
- **Firmware:** C++ with Arduino, FreeRTOS, PlatformIO, and LovyanGFX
- **Web app:** Next.js, React, TypeScript, and Supabase
- **Image processor:** Python service packaged with Docker
- **Backend:** Supabase database, storage, and APIs

The frame verifies TLS certificates when connecting to the backend. It also handles network errors and incomplete downloads during synchronization.

## Repository

```text
firmware/          ESP32 firmware and PlatformIO configuration
image-processor/   Python image-processing service and Dockerfile
web/               Next.js photo management app
```

The components can be deployed independently. The web app is deployed on Vercel, the image processor runs in Docker, and the frame connects to Supabase over HTTPS. See [web/README.md](web/README.md) for web app configuration.

## Status

This is a work in progress. The frame supports touchscreen Wi-Fi setup, syncs and caches photos, and displays them from the SD card. The web app supports multiple uploads, photo management, and manual cropping, and the image-processing service is containerized.

Still planned:

- Physical enclosure
- More robust sync and retry logic
- Remote device configuration
- Power and display performance improvements
- Further image-processing optimizations
- Cache eviction when the SD card fills up
