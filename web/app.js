// Initialize Leaflet Map
const map = L.map('map').setView([0, 0], 2);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
  attribution: '&copy; OpenStreetMap contributors',
  maxZoom: 19
}).addTo(map);

let sondeMarker = null;
let flightPolyline = L.polyline([], { color: '#38bdf8', weight: 3 }).addTo(map);
let hasCentered = false;

async function fetchTelemetry() {
  try {
    const res = await fetch('/api/telemetry');
    if (!res.ok) return;
    const data = await res.json();

    if (!data.serial) return;

    // Update Header
    document.getElementById('sonde-title').innerText = `${data.type} [${data.serial}]`;
    document.getElementById('sonde-freq').innerText = `${(data.freq_hz / 1e6).toFixed(3)} MHz`;
    const badge = document.getElementById('status-badge');
    badge.innerText = 'LOCKED';
    badge.classList.add('active');

    // Update Metrics
    document.getElementById('val-lat').innerText = data.lat.toFixed(5);
    document.getElementById('val-lon').innerText = data.lon.toFixed(5);
    document.getElementById('val-alt').innerText = `${data.alt.toFixed(1)} m`;
    document.getElementById('val-climb').innerText = `${data.climb.toFixed(1)} m/s`;
    document.getElementById('val-speed').innerText = `${data.speed.toFixed(1)} km/h`;
    document.getElementById('val-heading').innerText = `${data.heading.toFixed(0)}°`;

    document.getElementById('val-temp').innerText = data.temp !== null ? `${data.temp} °C` : '--';
    document.getElementById('val-rh').innerText = data.rh !== null ? `${data.rh} %` : '--';
    document.getElementById('val-batt').innerText = data.batt !== null ? `${data.batt} V` : '--';
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
  } catch (err) {
    console.error('Error fetching telemetry:', err);
  }
}

// Poll every 1000ms
setInterval(fetchTelemetry, 1000);
fetchTelemetry();
