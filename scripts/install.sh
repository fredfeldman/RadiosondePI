#!/usr/bin/env bash
set -e

echo "=== Installing RadiosondePI on Raspberry Pi ==="

# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y cmake g++ librtlsdr-dev libfftw3-dev libsqlite3-dev libssl-dev rtl-sdr

# 2. Blacklist default DVB-T driver for RTL-SDR
echo "blacklist dvb_usb_rtl28xxu" | sudo tee /etc/modprobe.d/nortlsdr.conf

# 3. Build & Install binary
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

sudo mkdir -p /opt/radiosondepi/bin /opt/radiosondepi/web /etc/radiosondepi
sudo cp radiosondepi /opt/radiosondepi/bin/
sudo cp -r ../web/* /opt/radiosondepi/web/
if [ ! -f /etc/radiosondepi/config.json ]; then
    sudo cp ../config/config.example.json /etc/radiosondepi/config.json
fi

# 4. Install systemd service
sudo cp ../systemd/radiosondepi.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable radiosondepi.service

echo "=== Installation completed successfully ==="
echo "Start service with: sudo systemctl start radiosondepi"
echo "View logs with:    journalctl -u radiosondepi -f"
echo "Web UI at:         http://<raspberry-pi-ip>:8080"
