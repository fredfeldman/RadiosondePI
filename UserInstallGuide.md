# RadiosondePI - User Installation & Setup Guide

This guide walks you through setting up, configuring, and operating **RadiosondePI** on a **Raspberry Pi** with an **RTL-SDR** dongle.

---

## 📋 Table of Contents

1. [Hardware Requirements](#1-hardware-requirements)
2. [Software Prerequisites](#2-software-prerequisites)
3. [Quick Automated Installation](#3-quick-automated-installation)
4. [Manual Step-by-Step Installation](#4-manual-step-by-step-installation)
5. [RTL-SDR Driver & Permissions Setup](#5-rtl-sdr-driver--permissions-setup)
6. [Configuration Guide (`config.json`)](#6-configuration-guide-configjson)
7. [Running the Application](#7-running-the-application)
   - [Running Interactively (CLI Mode)](#running-interactively-cli-mode)
   - [Running as a Background `systemd` Service](#running-as-a-background-systemd-service)
8. [Accessing the Live Web Dashboard & Map](#8-accessing-the-live-web-dashboard--map)
9. [Setting Up Cloud Uplinks & Aggregators (AeroHub, SondeHub & APRS-IS)](#9-setting-up-cloud-uplinks--aggregators-aerohub-sondehub--aprs-is)
10. [Updating an Existing Installation](#10-updating-an-existing-installation)
11. [Troubleshooting & FAQs](#11-troubleshooting--faqs)

---

## 1. Hardware Requirements

| Item | Recommendation | Notes |
| :--- | :--- | :--- |
| **SBC (Single Board Computer)** | Raspberry Pi 4B, 5, 3B+, or Zero 2W | 64-bit OS recommended. |
| **SDR Receiver** | **Nuand bladeRF 2.0 micro / Classic**, **SDRplay RSPdx-R2 / RSP**, or **RTL-SDR v3 / v4** | bladeRF provides multi-RX channel (RX1/RX2) and full hardware AGC; RSPdx-R2 provides 14-bit ADC and notch filters. |
| **Antenna** | 400–406 MHz tuned dipole, ground plane, or turnstile | Vertically polarized for standard radiosonde signals. |
| **Power Supply** | Official 5V 3A (Pi 4) / 5V 5A (Pi 5) / 5V 2.5A (Pi 3/Zero 2W) | Stable power prevents SDR USB dropouts. |
| **Optional Filter/LNA** | 403 MHz SAW Bandpass Filter + LNA (Bias-T powered) | Recommended in urban areas with high LTE/PMR interference. |

---

## 2. Software Prerequisites

* **Operating System**: **Raspberry Pi OS (64-bit)** Bullseye (Debian 11), Bookworm (Debian 12), or **Trixie (Debian 13)** (Lite or Desktop).
* **Compiler & Build Tools**: CMake $\ge 3.20$, GCC/G++ $\ge 11$ (GCC 12, 13, and 14 on Trixie fully supported).
* **System Libraries**: `librtlsdr-dev`, `libbladerf-dev`, `libfftw3-dev`, `libsqlite3-dev`, `libssl-dev`.

---

## 3. Quick Automated Installation

Run the automated setup script to install dependencies, blacklist interfering kernel DVB drivers, build the binaries, and register the system service:

```bash
git clone https://github.com/fredfeldman/RadiosondePI.git
cd RadiosondePI
chmod +x scripts/install.sh
./scripts/install.sh
```

---

## 4. Manual Step-by-Step Installation

If you prefer building manually without running the install script:

### Step 4.1: Update System and Install Build Dependencies
```bash
sudo apt-get update
sudo apt-get install -y git cmake g++ build-essential \
  librtlsdr-dev libbladerf-dev bladerf libfftw3-dev libsqlite3-dev libssl-dev rtl-sdr
```

### Step 4.2: Clone & Compile RadiosondePI
```bash
cd ~
git clone https://github.com/fredfeldman/RadiosondePI.git
cd RadiosondePI
mkdir build && cd build

# Configure CMake in Release mode with native compiler optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compile using all available CPU cores
make -j$(nproc)

# Run test suites to verify DSP and decoders
ctest --output-on-failure
```

---

## 5. SDR Driver & Permissions Setup

### A. RTL-SDR Setup
By default, Linux loads the kernel module `dvb_usb_rtl28xxu` (DVB-T TV tuner driver) when an RTL-SDR dongle is plugged in. This must be blacklisted so `librtlsdr` can access raw I/Q samples in user space.

```bash
# 1. Blacklist the Kernel DVB Module
echo "blacklist dvb_usb_rtl28xxu" | sudo tee /etc/modprobe.d/nortlsdr.conf

# 2. Configure Udev Rules (Non-root USB Access)
sudo tee /etc/udev/rules.d/20-rtlsdr.rules << 'EOF'
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2838", MODE:="0666"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2832", MODE:="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger
```

---

### B. SDRplay RSPdx-R2 / RSP Series Setup
SDRplay devices utilize the **SDRplay API v3** service (`sdrplay_api_service`).

```bash
# 1. Download SDRplay API v3 installer for ARM64 (Raspberry Pi OS 64-bit)
wget https://www.sdrplay.com/software/SDRplay_RSP_API-ARM64-3.15.2.run

# 2. Make executable and run the installer
chmod +x SDRplay_RSP_API-ARM64-3.15.2.run
./SDRplay_RSP_API-ARM64-3.15.2.run

# 3. Start and enable the SDRplay service
sudo systemctl daemon-reload
sudo systemctl enable sdrplay
sudo systemctl start sdrplay

# 4. Check that the service is active and detecting your RSPdx-R2
sudo systemctl status sdrplay
```

---

### C. Nuand bladeRF 2.0 micro / Classic Setup
Nuand bladeRF devices use `libbladeRF` and USB udev rules.

```bash
# 1. Install bladeRF CLI tools, library, and FPGA images
sudo apt-get update
sudo apt-get install -y bladerf libbladerf-dev bladerf-fpga-hostedxa4 bladerf-fpga-hostedxa9

# 2. Add user to plugdev group
sudo usermod -aG plugdev $USER

# 3. Check attached bladeRF device status
bladeRF-cli -p
```

---

## 6. Configuration Guide (`config.json`)

Create or edit your runtime configuration file (default location: `/etc/radiosondepi/config.json` or `config/config.example.json`):

```json
{
  "sdr": {
    "driver": "auto",
    "device_index": 0,
    "frequency_hz": 403000000,
    "sample_rate": 2400000,
    "ppm_error": 0,
    "gain": "auto",
    "bias_tee": false,
    "antenna_port": "AntennaA",
    "lna_state": 0,
    "broadcast_notch": false,
    "dab_notch": false,
    "rx_channel": 0,
    "bandwidth_hz": 1500000,
    "gain_mode": "manual"
  },
  "diversity": {
    "enabled": false,
    "mode": "MaximalRatioCombining",
    "secondary_device_index": 1
  },
  "scanner": {
    "enabled": true,
    "min_frequency_hz": 400050000,
    "max_frequency_hz": 406000000,
    "step_hz": 1800000,
    "dwell_time_ms": 250,
    "detection_threshold_db": 8.0
  },
  "station": {
    "callsign": "MYCALL",
    "latitude": 51.5074,
    "longitude": -0.1278,
    "altitude_m": 45.0,
    "antenna": "Turnstile 403MHz"
  },
  "web": {
    "enabled": true,
    "port": 8080,
    "bind_address": "0.0.0.0"
  },
  "uplink": {
    "aerohub": {
      "enabled": true,
      "endpoint_url": "http://localhost:8088/api/v1/telemetry",
      "api_key": "",
      "station_id": "RadiosondePI-01"
    },
    "sondehub": {
      "enabled": true,
      "uploader_position": true
    },
    "aprs_is": {
      "enabled": false,
      "server": "rotate.aprs2.net",
      "port": 14580,
      "passcode": -1
    },
    "mqtt": {
      "enabled": false,
      "host": "localhost",
      "port": 1883,
      "topic_prefix": "radiosonde"
    }
  }
}
```

### Key Parameter Explanations:
* `sdr.driver`: Choose `"auto"`, `"bladerf"`, `"sdrplay"` (for RSPdx-R2 / RSPdx / RSPduo / RSP1A), or `"rtlsdr"`.
* `sdr.rx_channel`: For bladeRF 2.0 micro, set to `0` for RX1 or `1` for RX2.
* `sdr.bandwidth_hz`: Analog LPF bandwidth (default: `1500000` = 1.5 MHz).
* `sdr.gain_mode`: For bladeRF, select `"manual"`, `"fast"` (AGC), `"slow"` (AGC), or `"hybrid"`.
* `sdr.antenna_port`: For RSPdx-R2, choose `"AntennaA"`, `"AntennaB"`, or `"AntennaC"` (BNC 50Ω ports).
* `sdr.lna_state`: LNA reduction state ($0 = \text{maximum LNA gain}$, higher integer = reduced gain for strong signals).
* `sdr.broadcast_notch`: Set to `true` to activate the built-in FM broadcast notch filter ($88\text{--}108\text{ MHz}$).
* `sdr.gain`: Set to `"auto"` or a manual gain value in tenths of a dB (e.g., `421` for $42.1\text{ dB}$).
* `sdr.ppm_error`: Frequency oscillator offset in PPM (TCXO on bladeRF and RSPdx-R2 has $<0.5\text{ PPM}$ drift).
* `sdr.bias_tee`: Set to `true` if powering an external 403 MHz LNA through the antenna port.
  }
}
```

### Key Parameter Explanations:
* `sdr.gain`: Set to `"auto"` or a manual gain value in tenths of a dB (e.g., `421` for $42.1\text{ dB}$).
* `sdr.ppm_error`: Frequency oscillator offset in PPM (usually $0$ to $+2$ on genuine RTL-SDR v3/v4).
* `sdr.bias_tee`: Set to `true` if powering an active LNA directly through the coax cable.
* `scanner.detection_threshold_db`: Peak signal-to-noise ratio required before locking onto a sonde.
* `station.callsign`: Your amateur radio callsign or tracking station identifier for SondeHub / APRS.

---

## 7. Running the Application

### Running Interactively (CLI Mode)

To start RadiosondePI in the foreground for debugging or field chasing:
```bash
# Using SDRplay RSPdx-R2 on Antenna Port A:
./build/radiosondepi --driver sdrplay --antenna AntennaA --freq 404800000

# Using RTL-SDR tuned to 404.8 MHz:
./build/radiosondepi --driver rtlsdr --freq 404800000 --config config/config.example.json

# Or run with auto-scanner enabled:
./build/radiosondepi --scan --config config/config.example.json
```

**Live Terminal Output Example:**
```text
=================================================
 RadiosondePI - RTL-SDR Radiosonde Core (v0.1.0) 
=================================================
[CONFIG] Tuned Frequency: 404.800 MHz | Driver: sdrplay | Antenna: AntennaA
[SDRplay] Connected to SDRplay Service (API v3.15)
[SDRplay] Initialized SDRplay RSPdx-R2 on AntennaA @ 404.800 MHz
[DSP] Zero-allocation multi-protocol pipeline running. Press Ctrl+C to terminate.

>>> [RS41 LOCKED @ 404.800 MHz] Frame #1420 | Serial: V3421882 <<<
    Position: 52.12481, -0.45192 | Alt: 18452.1 m | Climb: 5.4 m/s | Speed: 42.1 km/h | Heading: 84 deg
    Temp: -48.20 C | RH: 8.5 % | Battery: 2.94 V
```
    Position: 52.12481, -0.45192 | Alt: 18452.1 m | Climb: 5.4 m/s | Speed: 42.1 km/h | Heading: 84 deg
    Temp: -48.20 C | RH: 8.5 % | Battery: 2.94 V
```

---

### Running as a Background `systemd` Service

For headless fixed station setups (runs automatically on boot):

```bash
# Enable the service to launch on boot
sudo systemctl enable radiosondepi

# Start the service
sudo systemctl start radiosondepi

# Check live service status
sudo systemctl status radiosondepi

# View real-time log stream
journalctl -u radiosondepi -f -n 50

# Restart or stop the service
sudo systemctl restart radiosondepi
sudo systemctl stop radiosondepi
```

---

## 8. Accessing the Live Web Dashboard & Map

Once RadiosondePI is running, open a web browser on any device on your local network (phone, tablet, or PC):

```
http://192.168.50.154:8080
```
*(Or `http://<YOUR_RASPBERRY_PI_IP>:8080` / `http://raspberrypi.local:8080`)*

### Features Available in the Web UI:
- **Real-Time Live Map**: Tracks flight trajectories and coordinates on OpenStreetMap.
- **Flight Metrics**: Latitude, longitude, altitude, vertical ascent/descent rate, ground speed, and heading.
- **Atmospheric Sensor Cards**: Calibrated temperature ($^\circ\text{C}$), relative humidity ($\%$), and battery voltage.
- **Descent & Landing Predictor**: Visualizes the projected landing site once the balloon bursts and enters descent phase.

---

## 9. Setting Up Cloud Uplinks & Aggregators (AeroHub, SondeHub & APRS-IS)

### AeroHub Aggregator Uplink
RadiosondePI streams live real-time radiosonde targets, GPS trajectories, and atmospheric sensor telemetry directly to **AeroHub aggregator instances**:
1. In `config.json`, locate `"uplink" -> "aerohub"`.
2. Set `"enabled": true`.
3. Configure `"endpoint_url"` (e.g. `http://<aerohub-host>:8088/api/v1/telemetry` or cloud instance) and provide your `"api_key"` and `"station_id"`.

### SondeHub v2 Upload
To contribute telemetry to the global amateur tracking network at [https://sondehub.org](https://sondehub.org):
1. In `config.json`, locate `"uplink" -> "sondehub"`.
2. Set `"enabled": true`.
3. Fill in your station details under `"station"`: `callsign`, `latitude`, `longitude`, `altitude_m`, and `antenna`.

### APRS-IS Gateway
To beacon balloon positions into the APRS network:
1. In `config.json`, locate `"uplink" -> "aprs_is"`.
2. Set `"enabled": true`.
3. Supply your amateur callsign with an SSID (e.g., `N0CALL-11`) and corresponding APRS passcode.

---

## 10. Updating an Existing Installation

### Option A: Automated Update Script (Recommended)
From your `RadiosondePI` folder on the Raspberry Pi:
```bash
cd ~/RadiosondePI
chmod +x scripts/update.sh
./scripts/update.sh
```

### Option B: Manual Step-by-Step Update
```bash
cd ~/RadiosondePI

# 1. Stop background service
sudo systemctl stop radiosondepi

# 2. Pull latest code
git pull origin main

# 3. Build updated binaries
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 4. Verify tests
ctest --output-on-failure

# 5. Install updated executable and web dashboard assets
sudo cp radiosondepi /opt/radiosondepi/bin/
sudo cp -r ../web/* /opt/radiosondepi/web/

# 6. Restart background service
sudo systemctl daemon-reload
sudo systemctl restart radiosondepi

# 7. Check service status
sudo systemctl status radiosondepi
```

---

## 11. Troubleshooting & FAQs

### Q1: `(code=exited, status=217/USER)` when starting systemd service
- **Cause**: The `radiosondepi.service` file hardcodes `User=pi`, but your Raspberry Pi user account is different (e.g. `fred`, `admin`, or custom setup on Debian Bookworm / Trixie).
- **Fix**: Run `scripts/update.sh`, or update the service file with your actual username:
  ```bash
  sudo sed -i "s/^User=.*/User=$USER/" /etc/systemd/system/radiosondepi.service
  sudo sed -i "s/^Group=.*/Group=$(id -gn)/" /etc/systemd/system/radiosondepi.service
  sudo systemctl daemon-reload
  sudo systemctl restart radiosondepi
  ```

### Q2: `rtlsdr_open() failed` or `No RTL-SDR devices found`
- **Cause**: Kernel DVB driver was not unloaded or USB permissions are missing.
- **Fix**: Run `echo "blacklist dvb_usb_rtl28xxu" | sudo tee /etc/modprobe.d/nortlsdr.conf`, unplug the dongle, and plug it back in.

### Q3: High CPU usage on Raspberry Pi Zero 2W or Pi 3
- **Cause**: Debug build running without optimization.
- **Fix**: Ensure CMake was configured with `-DCMAKE_BUILD_TYPE=Release` to enable `-O3`, `-ffast-math`, and ARM NEON SIMD optimizations.

### Q4: No sonde decoded even with strong signal visible on SDR# / GQRX
- **Cause**: Frequency offset (PPM error) or tuner overload.
- **Fix**:
  1. Determine your dongle's PPM offset using `rtl_test -p` and set `"ppm_error"` in `config.json`.
  2. Switch gain from `"auto"` to a fixed manual gain like `38.6` or `42.1` dB.

### Q5: When are weather balloons typically launched?
- Synoptic meteorological radiosondes are routinely launched worldwide twice daily at **00:00 UTC** and **12:00 UTC** (typically released 45–60 minutes prior, around 23:15 UTC and 11:15 UTC).
