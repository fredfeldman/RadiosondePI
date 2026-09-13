// Initialize Leaflet Map
const map = L.map('map').setView([0, 0], 2);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
  attribution: '&copy; OpenStreetMap contributors',
  maxZoom: 19
}).addTo(map);

let sondeMarker = null;
let flightPolyline = L.polyline([], { color: '#38bdf8', weight: 3 }).addTo(map);
let hasCentered = false;
let wsConnected = false;

function applyTelemetryData(data) {
  if (!data || !data.serial) return;

  // Update Header
  document.getElementById('sonde-title').innerText = `${data.type} [${data.serial}]`;
  document.getElementById('sonde-freq').innerText = `${(data.freq_hz / 1e6).toFixed(3)} MHz`;
  const badge = document.getElementById('status-badge');
  badge.innerText = wsConnected ? 'LIVE (WS)' : 'LOCKED (HTTP)';
  badge.classList.add('active');

  // Update Metrics
  document.getElementById('val-lat').innerText = data.lat.toFixed(5);
  document.getElementById('val-lon').innerText = data.lon.toFixed(5);
  document.getElementById('val-alt').innerText = `${data.alt.toFixed(1)} m`;
  document.getElementById('val-climb').innerText = `${data.climb.toFixed(1)} m/s`;
  document.getElementById('val-speed').innerText = `${data.speed.toFixed(1)} km/h`;
  document.getElementById('val-heading').innerText = `${data.heading.toFixed(0)}°`;

  document.getElementById('val-temp').innerText = data.temp !== null && data.temp !== undefined ? `${data.temp} °C` : '--';
  document.getElementById('val-rh').innerText = data.rh !== null && data.rh !== undefined ? `${data.rh} %` : '--';
  document.getElementById('val-batt').innerText = data.batt !== null && data.batt !== undefined ? `${data.batt} V` : '--';
  document.getElementById('val-frame').innerText = data.frame || '--';

  // Update Map Marker
  if (data.gps_valid) {
    const pos = [data.lat, data.lon];
    if (!sondeMarker) {
      sondeMarker = L.marker(pos).addTo(map)
        .bindPopup(`<b>${data.type} ${data.serial}</b><br>Alt: ${data.alt}m`);
    } else {
      sondeMarker.setLatLng(pos);
    }

    flightPolyline.addLatLng(pos);

    if (!hasCentered) {
      map.setView(pos, 11);
      hasCentered = true;
    }
  }
}

async function fetchTelemetryHttp() {
  if (wsConnected) return;
  try {
    const res = await fetch('/api/telemetry');
    if (!res.ok) return;
    const data = await res.json();
    applyTelemetryData(data);
  } catch (err) {
    console.error('Error fetching telemetry HTTP:', err);
  }
}

function initWebSocket() {
  const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const wsUrl = `${wsProtocol}//${window.location.host}/ws/telemetry`;
  
  try {
    const ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      console.log('[WebSocket] Connected to telemetry stream');
      wsConnected = true;
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        applyTelemetryData(data);
      } catch (e) {
        console.error('Error parsing WS message:', e);
      }
    };

    ws.onclose = () => {
      wsConnected = false;
      console.log('[WebSocket] Disconnected, retrying in 3s...');
      setTimeout(initWebSocket, 3000);
    };

    ws.onerror = (err) => {
      wsConnected = false;
      ws.close();
    };
  } catch (err) {
    wsConnected = false;
    setTimeout(initWebSocket, 5000);
  }
}

async function tuneFrequency() {
  const val = document.getElementById('tune-input').value;
  if (!val) return;
  try {
    const res = await fetch(`/api/tune?freq=${encodeURIComponent(val)}`);
    const data = await res.json();
    if (data.status === 'ok') {
      document.getElementById('sonde-freq').innerText = `${(data.frequency_hz / 1e6).toFixed(3)} MHz`;
    }
  } catch (err) {
    console.error('Error tuning frequency:', err);
  }
}

// Start WebSocket connection with HTTP polling fallback
initWebSocket();
setInterval(fetchTelemetryHttp, 1500);
fetchTelemetryHttp();
