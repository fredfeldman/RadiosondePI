#!/usr/bin/env bash
set -e

echo "=== Installing RadiosondePI on Raspberry Pi ==="

# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y cmake g++ librtlsdr-dev libbladerf-dev bladerf libfftw3-dev libsqlite3-dev libssl-dev rtl-sdr

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

# 4. Install systemd service with current user
CURRENT_USER="${SUDO_USER:-$USER}"
CURRENT_GROUP=$(id -gn "$CURRENT_USER" 2>/dev/null || echo "$CURRENT_USER")

# Ensure proper permissions on /opt/radiosondepi
sudo chown -R "$CURRENT_USER:$CURRENT_GROUP" /opt/radiosondepi

# Ensure user is in plugdev group for SDR access
sudo usermod -aG plugdev "$CURRENT_USER" 2>/dev/null || true

# Generate systemd service with actual username
sudo tee /etc/systemd/system/radiosondepi.service > /dev/null << EOF
[Unit]
Description=RadiosondePI RTL-SDR Auto-Scanner & Telemetry Decoder Service
After=network.target

[Service]
Type=simple
User=$CURRENT_USER
Group=$CURRENT_GROUP
WorkingDirectory=/opt/radiosondepi
ExecStart=/opt/radiosondepi/bin/radiosondepi --config /etc/radiosondepi/config.json
Restart=always
RestartSec=5s
TimeoutStopSec=5s
KillSignal=SIGTERM
SendSIGKILL=yes
StandardOutput=journal
StandardError=journal
Nice=-10

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable radiosondepi.service

echo "=== Installation completed successfully ==="
echo "Start service with: sudo systemctl start radiosondepi"
echo "View logs with:    journalctl -u radiosondepi -f"
echo "Web UI at:         http://<raspberry-pi-ip>:8080"
