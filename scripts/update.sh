#!/usr/bin/env bash
set -e

echo "=== Updating RadiosondePI on Raspberry Pi ==="

# 1. Stop active background service if running
if systemctl is-active --quiet radiosondepi.service; then
    echo "[SERVICE] Stopping radiosondepi.service..."
    sudo systemctl stop radiosondepi.service
fi

# 2. Pull latest code from GitHub
echo "[GIT] Pulling latest repository updates..."
git pull origin main

# 3. Rebuild binary in Release mode
echo "[BUILD] Compiling updated release binaries..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 4. Run test suites to verify build integrity
echo "[TEST] Running verification test suites..."
ctest --output-on-failure

# 5. Copy updated binaries and web assets
echo "[INSTALL] Installing updated binaries and web assets to /opt/radiosondepi..."
sudo mkdir -p /opt/radiosondepi/bin /opt/radiosondepi/web
sudo cp radiosondepi /opt/radiosondepi/bin/
sudo cp -r ../web/* /opt/radiosondepi/web/

# 6. Update systemd service user if needed and restart
CURRENT_USER="${SUDO_USER:-$USER}"
CURRENT_GROUP=$(id -gn "$CURRENT_USER" 2>/dev/null || echo "$CURRENT_USER")

sudo chown -R "$CURRENT_USER:$CURRENT_GROUP" /opt/radiosondepi 2>/dev/null || true

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

echo "[SERVICE] Restarting radiosondepi.service..."
sudo systemctl daemon-reload
sudo systemctl restart radiosondepi.service

echo "=== RadiosondePI successfully updated! ==="
echo "Check status: sudo systemctl status radiosondepi"
echo "View live log: journalctl -u radiosondepi -f -n 50"
echo "Open Web UI:   http://$(hostname -I | awk '{print $1}'):8080"
