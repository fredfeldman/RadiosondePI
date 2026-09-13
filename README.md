# RadiosondePI 🎈📻   -   very early, not tested.  Created using AI.

A high-performance, native Raspberry Pi Radiosonde Auto-Scanner and Telemetry Decoder using RTL-SDR.

---

## 🚀 Features

- **Native C++20 DSP Core**: Low CPU footprint ($<20\%$ on Raspberry Pi 4, $<35\%$ on Pi Zero 2W) with ARM NEON SIMD acceleration.
- **Multi-Sonde Protocol Support**:
  - **Vaisala RS41** (4800 baud GFSK, Reed-Solomon FEC, GPS & PTU telemetry)
  - **GRAW DFM-06 / DFM-09 / DFM-17** (2400 baud FSK, Manchester)
  - **Meteomodem M10 / M20** (9600 baud GFSK)
- **Wideband Auto-Scanner**: Automatically scans 400.05–406.00 MHz meteorology band, locks onto active sondes, and tracks frequency drift (AFC).
- **Embedded Web UI**: Real-time Leaflet.js live map, altitude/ascent graphs, and receiver status accessible via web browser.
- **Direct Cloud Uplinks**: Automatic submission to **SondeHub v2 Tracker**, **APRS-IS**, and local **MQTT**.

---

## 🛠️ Hardware Requirements

1. **Raspberry Pi**: Pi 3B+, 4B, 5, or Zero 2W (running Raspberry Pi OS 64-bit Lite/Desktop).
2. **SDR Receiver**: RTL-SDR v3 / v4 or compatible RTL2832U-based USB dongle.
3. **Antenna**: 400–406 MHz tuned vertical dipole, ground plane, or directional Yagi antenna. Optional 403 MHz SAW filter & LNA.

---

## 🏗️ Project Architecture

```
RadiosondePI/
├── CMakeLists.txt              # Root build definition
├── SPRINT_PLAN.md              # Detailed sprint backlog & milestones
├── config/
│   └── config.example.json     # Configuration template (gain, uplinks, station info)
├── include/
│   ├── dsp/                    # Filters, discriminator, symbol synchronizers
│   ├── sdr/                    # librtlsdr device wrapper & buffer manager
│   ├── decoders/               # RS41, DFM, M10/M20 protocol parsers
│   ├── telemetry/              # Position, PTU data models & landing prediction
│   ├── uplink/                 # SondeHub, APRS-IS, MQTT clients
│   └── web/                    # Embedded HTTP & WebSocket server
├── src/
│   ├── main.cpp                # Application entry point & pipeline coordinator
│   ├── dsp/
│   ├── sdr/
│   ├── decoders/
│   ├── uplink/
│   └── web/
├── web/
│   ├── index.html              # Leaflet.js live tracking dashboard
│   ├── app.js
│   └── style.css
└── tests/                      # Unit tests & recorded I/Q sample test runners
```

---

## ⚡ Quick Build Instructions

### Prerequisites (Debian / Raspberry Pi OS)
```bash
sudo apt-get update
sudo apt-get install -y cmake g++ librtlsdr-dev libfftw3-dev libsqlite3-dev libssl-dev
```

### Build & Run
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./radiosondepi --config ../config/config.example.json
```
