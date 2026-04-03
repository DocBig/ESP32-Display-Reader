#pragma once

static const char WIFI_SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Display Reader WiFi Setup</title>
<style>
body{font-family:Arial,sans-serif;background: #274441;color:#eee;max-width:700px;margin:0 auto;padding:20px}
input,button{width:100%;box-sizing:border-box;padding:10px;margin:8px 0;border-radius:8px;border:1px solid #444;background:#222;color:#eee}
button{cursor:pointer}
pre{background:#000;padding:10px;border-radius:8px;white-space:pre-wrap;word-break:break-word}
#wifiList{display:grid;gap:8px;margin:10px 0 14px 0}
.wifiItem{padding:10px;border:1px solid #444;border-radius:8px;background:#1a1a1a;cursor:pointer}
.wifiItem:hover{border-color:#777;background:#222}
.wifiName{font-weight:bold}
.wifiMeta{font-size:12px;color:#aaa;margin-top:4px}
</style>
</head>
<body>
<h2>Display Reader - WiFi Setup</h2>
<p>The device is running in setup AP mode. Please select a network or enter manually.</p>

<label>SSID</label>
<input id="ssid" placeholder="Network name">
<button type="button" onclick="scanWifi()">Scan networks</button>
<div id="wifiList"><div class="wifiMeta">Searching for networks...</div></div>

<label>Password</label>
<input id="password" type="password" placeholder="WiFi password">

<label>Hostname</label>
<input id="hostname" value="display-reader">

<button type="button" onclick="saveWifi()">Save WiFi</button>

<pre id="out">-</pre>

<script>
function wifiQualityText(rssi) {
  if (rssi >= -55) return 'excellent';
  if (rssi >= -67) return 'good';
  if (rssi >= -75) return 'fair';
  return 'weak';
}

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function renderWifiList(items) {
  const list = document.getElementById('wifiList');

  if (!items || !items.length) {
    list.innerHTML = '<div class="wifiMeta">No networks found. Please enter SSID manually.</div>';
    return;
  }

  const bySsid = new Map();
  for (const n of items) {
    if (!n.ssid) continue;
    const prev = bySsid.get(n.ssid);
    if (!prev || n.rssi > prev.rssi) bySsid.set(n.ssid, n);
  }

  const unique = Array.from(bySsid.values()).sort((a, b) => b.rssi - a.rssi);

  list.innerHTML = unique.map(n => {
    const safe = escapeHtml(n.ssid);
    return `
      <div class="wifiItem" data-ssid="${safe}" onclick="selectWifi(this.dataset.ssid)">
        <div class="wifiName">${safe}</div>
        <div class="wifiMeta">
          ${n.open ? 'open' : 'secured'} · ${n.rssi} dBm · ${wifiQualityText(n.rssi)}
        </div>
      </div>
    `;
  }).join('');
}

function selectWifi(ssid) {
  document.getElementById('ssid').value = ssid;
  document.getElementById('wifiList').style.display = 'none';
}

async function scanWifi() {
  const list = document.getElementById('wifiList');
  list.innerHTML = '<div class="wifiMeta">Searching for networks...</div>';

  try {
    const r = await fetch('/api/wifi_scan');
    const data = await r.json();

    if (!data.ok) {
      list.innerHTML = '<div class="wifiMeta">Network scan failed.</div>';
      return;
    }

    renderWifiList(data.networks || []);
  } catch (e) {
    list.innerHTML = '<div class="wifiMeta">Network scan error: ' + e + '</div>';
  }
}

async function saveWifi() {
  const out = document.getElementById('out');
  const payload = {
    ssid: document.getElementById('ssid').value,
    password: document.getElementById('password').value,
    hostname: document.getElementById('hostname').value
  };

  out.textContent = 'Saving WiFi settings and testing connection...';

  try {
    const r = await fetch('/api/wifi_setup', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload)
    });

    const data = await r.json();

    if (!data.ok || !data.connected) {
      out.textContent =
        'WiFi connection failed. The device stays in AP mode.\n' +
        'Please check SSID / password and try again.';
      return;
    }

    const host = data.hostname && data.hostname.length
      ? ('http://' + data.hostname + '.local')
      : '';

    const lines = [
      'WiFi connected successfully.',
      '',
      'Assigned IP address:',
      'http://' + data.ip,
      '',
      host ? ('mDNS (if supported):\n' + host + '\n') : '',
      'Important:',
      '1. Note down this IP address.',
      '2. Reconnect your phone / laptop to your regular WiFi.',
      '3. Then open the address shown above in your browser.',
      '',
      'The setup access point will close automatically in ~12 seconds.'
    ].filter(Boolean);

    out.textContent = lines.join('\n');
  } catch (e) {
    out.textContent = 'Unexpected error during WiFi setup: ' + e;
  }
}

window.addEventListener('load', () => {
  scanWifi();
});
</script>
</body>
</html>
)HTML";

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<link href="https://cdn.jsdelivr.net/npm/@mdi/font@7.4.47/css/materialdesignicons.min.css" rel="stylesheet">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Display Reader by Dr.Big</title>
<style>
:root{
  --bg:#274441;
  --panel:#171718;
  --panel2:#1d1d1f;
  --line:#34343a;
  --text:#f2f2f3;
  --muted:#a9a9b2;
  --green:#8cff4f;
  --cyan:#00d0ff;
  --orange:#ffaa00;
}
*{box-sizing:border-box}
body{
  margin:0;
  background:var(--bg);
  color:var(--text);
  font-family:Arial,sans-serif;
  font-size:16px;
}
header{
  padding:8px 16px 6px 16px;
  display:flex;
  align-items:center;
  justify-content:space-between;
}
h1{
  margin:0 0 4px 0;
  font-size:24px;
}
#btnToggleAll{
  width:24px;
  height:24px;
  padding:0;
  border-radius:6px;
  font-size:14px;
  line-height:1;
  display:flex;
  align-items:center;
  justify-content:center;
  flex-shrink:0;
  align-self:flex-start;
  margin-top:2px;
}
h3{
  margin:0 0 12px 0;
  font-size:16px;
}
.small{font-size:13px;color:var(--muted)}
.app{
  padding:0 14px 14px 14px;
}
.statusBar{
  display:grid;
  grid-template-columns:repeat(6,minmax(120px,1fr));
  gap:6px;
  margin-bottom:8px;
}
.statusItem{
  background:linear-gradient(180deg,#1b1b1d,#141416);
  border:1px solid var(--line);
  border-radius:8px;
  padding:6px 10px;
  min-height:46px;
}
.statusItem .k{
  color:var(--muted);
  font-size:12px;
  margin-bottom:2px;
}
.statusItem .v{
  font-size:16px;
  font-weight:700;
}
.statusLed{
  display:inline-block;
  width:10px;
  height:10px;
  border-radius:50%;
  margin-right:6px;
  background:#666;
}
.ledGreen{background:#4cff3a}
.ledRed{background:#ff4040}

.card{
  background:linear-gradient(180deg,var(--panel2),var(--panel));
  border:1px solid var(--line);
  border-radius:12px;
  padding:14px;
}
.two{
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:10px;
}
button,input,select{
  width:100%;
  box-sizing:border-box;
  padding:9px 10px;
  border-radius:9px;
  border:1px solid #47474f;
  background:#222327;
  color:#f2f2f3;
}
label{
  display:block;
  margin:8px 0;
}
input[type="checkbox"]{
  width:auto;
  margin-right:8px;
}
button{
  cursor:pointer;
  background:linear-gradient(180deg,#313238,#202127);
  box-shadow:0 2px 0 #101115, 0 4px 10px rgba(0,0,0,.25);
  transition:transform .06s ease, box-shadow .12s ease, border-color .12s ease;
}
button:hover{border-color:#777986}
button:active{
  transform:translateY(1px);
  box-shadow:0 1px 0 #101115, 0 2px 4px rgba(0,0,0,.25);
}
button.busy{
  opacity:.75;
  pointer-events:none;
}
button.ok{
  border-color:#2f9e44;
  color:#d3f9d8;
}
button.err{
  border-color:#c92a2a;
  color:#ffe3e3;
}
button.highlight{
  background:linear-gradient(180deg,#4a90e2,#357abd);
  border-color:#5ba0f2;
  font-weight:bold;
  color:#ffffff;
}

.configSections{
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:8px;
  margin-bottom:8px;
  align-items:start;
}

.mainGrid{
  display:grid;
  grid-template-columns:minmax(760px,1fr) 420px;
  gap:12px;
  align-items:start;
}
.workspace{
  background:linear-gradient(180deg,var(--panel2),var(--panel));
  border:1px solid var(--line);
  border-radius:12px;
  padding:12px;
  min-height:720px;
}
.toolbar{
  display:flex;
  gap:8px;
  flex-wrap:wrap;
  margin-bottom:10px;
}
.toolbar button{
  width:auto;
  padding:8px 12px;
}
.toolbarBreak{
  width:100%;
  height:0;
  margin:0;
}
.toolbarToggle{
  display:flex;
  align-items:center;
  gap:8px;
  margin:0;
  padding:8px 12px;
  border:1px solid #47474f;
  border-radius:9px;
  background:#222327;
  color:#f2f2f3;
  white-space:nowrap;
}

.toolbarToggle input[type="checkbox"]{
  margin:0;
}
#canvasWrap{
  position:relative;
  display:inline-block;
  overflow:auto;
  max-width:100%;
  background:#000;
  border:1px solid #444;
  border-radius:8px;
  margin-top:20px;
  scrollbar-width:none;
  -ms-overflow-style:none;
}
#canvasWrap::-webkit-scrollbar{display:none;}
#snap{
  display:block;
  image-rendering:pixelated;
  transform-origin:top left;
  user-select:none;
  -webkit-user-drag:none;
}
#overlay{
  position:absolute;
  left:0;
  top:0;
  cursor:crosshair;
}
.sidebar{
  display:flex;
  flex-direction:column;
  gap:8px;
}
#roiList{
  list-style:none;
  margin:0;
  padding:0;
  display:grid;
  grid-template-columns:repeat(auto-fit,minmax(120px,1fr));
  gap:6px;
}
#roiList li{
  padding:6px 8px;
  border:1px solid var(--line);
  border-radius:6px;
  cursor:pointer;
  background:#16171a;
  font-size:12px;
  text-align:center;
  overflow:hidden;
  text-overflow:ellipsis;
  white-space:nowrap;
}
#roiList li.active{
  background:#2a2d33;
}
pre{
  background:#0d0d0f;
  padding:10px;
  border-radius:8px;
  white-space:pre-wrap;
  word-break:break-word;
  min-height:56px;
}
progress{
  width:100%;
  height:18px;
}
.subtle{
  color:var(--muted);
  font-size:13px;
}
.help{
  margin-top:8px;
  font-size:12px;
  color:var(--muted);
  line-height:1.4;
}
.warn{
  margin-top:8px;
  padding:8px 10px;
  border:1px solid #8b2d2d;
  border-radius:8px;
  background:#2a1111;
  color:#ffb3b3;
  font-size:12px;
  line-height:1.35;
}
#haFields{
  display:none;
  margin-top:8px;
  padding-top:8px;
  border-top:1px solid #2f3136;
}
.collapsibleSection{
  border:1px solid var(--line);
  border-radius:10px;
  background:linear-gradient(180deg,var(--panel2),var(--panel));
  padding:8px 12px;
}
.collapsibleHeader{
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:8px;
  margin-bottom:0;
}
.collapsibleHeader h3{
  margin:0;
  font-size:16px;
}
.sectionTitle{
  display:flex;
  align-items:center;
  gap:6px;
}
.collapsibleToggle{
  display:flex;
  align-items:center;
  gap:6px;
  margin:0;
  white-space:nowrap;
  font-size:14px;
}
.collapsibleToggle input[type="checkbox"]{
  width:auto;
  margin:0;
}
.collapsibleContent{
  margin-top:8px;
  padding-top:8px;
  border-top:1px solid #2f3136;
}
.collapsibleContent.hidden{
  display:none;
}

#btnLive{
  font-size:12px;
  padding:3px 8px;
  width:auto;
  margin:0;
  white-space:nowrap;
}


.collapseBtn{
  width:22px;
  height:22px;
  padding:0;
  border-radius:5px;
  font-size:12px;
  line-height:1;
  flex-shrink:0;
  display:flex;
  align-items:center;
  justify-content:center;
}

@media (max-width:1450px){
  .statusBar{grid-template-columns:repeat(4,minmax(120px,1fr))}
  .app{overflow-x:auto}
}

@media (max-width:1100px){
  .configSections{
    grid-template-columns:1fr;
  }
}
</style>
</head>
<body>
<header>
  <h1>Display Reader by Dr.Big</h1>
  <button type="button" id="btnToggleAll" class="collapseBtn" onclick="toggleAllSections()" title="Alle Sektionen auf-/zuklappen">▾</button>
</header>

<div class="app">
  <div class="statusBar">
    <div class="statusItem">
      <div class="k">WiFi</div>
      <div class="v">
        <span id="led_wifi" class="statusLed"></span>
        <span id="st_wifi">-</span>
      </div>
    </div>

    <div class="statusItem">
      <div class="k">IP Address</div>
      <div class="v" id="st_ip">-</div>
    </div>

    <div class="statusItem">
      <div class="k">MQTT</div>
      <div class="v">
        <span id="led_mqtt" class="statusLed"></span>
        <span id="st_mqtt">-</span>
      </div>
    </div>

    <div class="statusItem">
      <div class="k">ESP32 Temperature</div>
      <div class="v" id="st_temp">-</div>
    </div>

    <div class="statusItem">
      <div class="k">Free Heap</div>
      <div class="v" id="st_heap">-</div>
    </div>

    <div class="statusItem">
      <div class="k">WiFi Signal</div>
      <div class="v" id="st_rssi">-</div>
    </div>

    <div class="statusItem">
      <div class="k">Camera Sensor</div>
      <div class="v" id="st_cam_sensor">-</div>
    </div>

    <div class="statusItem">
      <div class="k">Frame Size</div>
      <div class="v" id="st_cam_frame">-</div>
    </div>

    <div class="statusItem">
      <div class="k">Camera Status</div>
      <div class="v">
        <span id="led_cam" class="statusLed"></span>
        <span id="st_cam">-</span>
      </div>
    </div>

    <div class="statusItem">
      <div class="k">ESP32 Board Model</div>
      <div class="v" id="st_cam_model">-</div>
    </div>

    <div class="statusItem">
      <div class="k">Firmware Version</div>
      <div class="v" id="st_fw_version">-</div>
    </div>

    <div class="statusItem">
      <div class="k">Uptime</div>
      <div class="v" id="st_uptime">-</div>
    </div>
  </div>

  <div class="configSections">
    <!-- Spalte 1, Zeile 1: Camera -->
    <div class="collapsibleSection">
      <div class="collapsibleHeader">
        <div class="sectionTitle">
          <button type="button" class="collapseBtn" id="camera_btn"
            onclick="toggleSection('camera_content','camera_btn')">▸</button>
          <h3>Camera</h3>
        </div>
        <button id="btnLive" onclick="openLiveStream()">🎥 Live</button>
      </div>

      <div id="camera_content" class="collapsibleContent hidden">
        <label>ESP32 Board Model
          <select id="cam_model">
            <option value="freenove_wrover">Freenove WROVER</option>
            <option value="ai_thinker">AI-Thinker ESP32-CAM</option>
          </select>
        </label>

        <label>Frame Size
          <select id="cam_frame">
            <option value="160x120">160x120 (QQVGA)</option>
            <option value="320x240">320x240 (QVGA)</option>
            <option value="640x480">640x480 (VGA)</option>
            <option value="800x600">800x600 (SVGA)</option>
          </select>
        </label>

        <label>JPEG Quality <input id="cam_jpeg_quality" type="number" min="0" max="63"></label>

        <div class="two">
          <label><input id="cam_auto_exposure" type="checkbox">Auto Exposure</label>
          <label><input id="cam_auto_gain" type="checkbox">Auto Gain</label>
        </div>
        <label><input id="cam_auto_whitebalance" type="checkbox">Auto White Balance</label>

        <div class="two">
          <label>Exposure Value <input id="cam_aec_value" type="number" min="0" max="1200"></label>
          <label>Gain <input id="cam_agc_gain" type="number" min="0" max="30"></label>
        </div>

        <div class="two">
          <label><input id="cam_vflip" type="checkbox">Vertical Flip</label>
          <label><input id="cam_hflip" type="checkbox">Horizontal Flip</label>
        </div>

        <label class="collapsibleToggle">
          <input id="stream_enabled" type="checkbox">
          <span>🎥 aktivate Live Stream</span>
        </label>
        <div class="subtle">Enable live preview popup. '🎥 Live' to open</div>
      </div>
    </div>

    <!-- Spalte 2, Zeile 1: Lighting (PWM) -->
    <div class="collapsibleSection">
      <div class="collapsibleHeader">
        <div class="sectionTitle">
          <button type="button" class="collapseBtn" id="lighting_btn"
            onclick="toggleSection('lighting_content','lighting_btn')">▸</button>
          <h3>Lighting (PWM)</h3>
        </div>

        <label class="collapsibleToggle">
          <input id="l_enabled" type="checkbox">
          <span>Enable Lighting</span>
        </label>
      </div>

      <div id="lighting_content" class="collapsibleContent hidden">
        <div class="two">
          <label>GPIO Pin <input id="l_gpio" type="number" min="0" max="39"></label>
          <label>Brightness (0-255) <input id="l_brightness" type="number" min="0" max="255"></label>
        </div>

        <div class="two">
          <label>Settle Time (ms) <input id="l_settle" type="number" min="0" max="3000"></label>
          <label><input id="l_capture_only" type="checkbox">Capture Only</label>
        </div>

        <label><input id="l_invert" type="checkbox">Invert Output</label>

        <button id="btnLightTest" onclick="testLight()">Test Light</button>
        <div class="subtle">Use a free GPIO pin. Avoid camera and boot pins.</div>
      </div>
    </div>

    <!-- Spalte 1, Zeile 2: MQTT -->
    <div class="collapsibleSection">
      <div class="collapsibleHeader">
        <div class="sectionTitle">
          <button type="button" class="collapseBtn" id="mqtt_btn"
            onclick="toggleSection('mqtt_content','mqtt_btn')">▸</button>
          <h3>MQTT</h3>
        </div>

        <label class="collapsibleToggle">
          <input id="m_enabled" type="checkbox">
          <span>Enable MQTT</span>
        </label>
      </div>

      <div id="mqtt_content" class="collapsibleContent hidden">
        <label>Broker Address <input id="m_host"></label>

        <div class="two">
          <label>Port <input id="m_port" type="number" min="1" max="65535"></label>
          <label>Publish Interval (s) <input id="m_interval" type="number" min="0"></label>
        </div>

        <label>Topic <input id="m_topic"></label>
        <label>Username <input id="m_user"></label>
        <label>Password <input id="m_pass" type="password"></label>
        <label><input id="m_image_enabled" type="checkbox">Publish Image to Home Assistant (MQTT Snapshot)</label>

        <div class="card">
          <h3>HA-Discovery</h3>
          <label>Discovery Prefix <input id="ha_prefix"></label>
          <label>Device Name <input id="d_name" placeholder="Display Reader"></label>
          <div class="small">Name des Geräts in Home Assistant. Wird beim Config-Import automatisch zurückgesetzt und kann hier angepasst werden.</div>
        </div>
      </div>
    </div>

    <!-- Spalte 2, Zeile 2: Lighting (WS2812B) -->
    <div class="collapsibleSection">
      <div class="collapsibleHeader">
        <div class="sectionTitle">
          <button type="button" class="collapseBtn" id="lighting2_btn"
            onclick="toggleSection('lighting2_content','lighting2_btn')">▸</button>
          <h3>Lighting (WS2812B)</h3>
        </div>

        <label class="collapsibleToggle">
          <input id="l2_enabled" type="checkbox">
          <span>Enable WS2812B</span>
        </label>
      </div>

      <div id="lighting2_content" class="collapsibleContent hidden">
        <div class="two">
          <label>GPIO Pin <input id="l2_gpio" type="number" min="0" max="39"></label>
          <label>LED Count <input id="l2_led_count" type="number" min="1" max="64"></label>
        </div>

        <div class="two">
          <label>Brightness (0-255) <input id="l2_brightness" type="number" min="0" max="255"></label>
          <label>Color <input id="l2_color" type="color" value="#ffffff"></label>
        </div>

        <div class="two">
          <label>Settle Time (ms) <input id="l2_settle" type="number" min="0" max="3000"></label>
          <label><input id="l2_capture_only" type="checkbox">Capture Only</label>
        </div>

        <button id="btnLight2Test" onclick="testLight2()">Test WS2812B</button>
        <div class="subtle">WS2812B data pin. Needs 5 V supply — do not power from ESP GPIO. Level shifter recommended.</div>
      </div>
    </div>

    <!-- Spalte 2, Zeile 3: Pre-Capture -->
    <div class="collapsibleSection" style="grid-column:2">
      <div class="collapsibleHeader">
        <div class="sectionTitle">
          <button type="button" class="collapseBtn" id="precapture_btn"
            onclick="toggleSection('precapture_content','precapture_btn')">▸</button>
          <h3>Pre-Capture Trigger</h3>
        </div>

        <label class="collapsibleToggle">
          <input id="pc_enabled" type="checkbox">
          <span>Enable Pre-Capture Trigger</span>
        </label>
      </div>

      <div id="precapture_content" class="collapsibleContent hidden">
        <div class="subtle">Send MQTT signal before capturing to trigger external actions (lighting, fingerbot, etc.)</div>

        <div class="two">
          <label>Delay Before Capture (ms) <input id="pc_delay" type="number" min="0" max="30000"></label>
          <label>MQTT Topic <input id="pc_topic"></label>
        </div>

        <div class="subtle">
          Topic: [base_topic]/[this_topic] → e.g., "display_reader_abc123/pre_capture"<br>
          Payload: <code>{"state":"on"}</code> (sent before capture), <code>{"state":"off"}</code> (sent after capture)
        </div>
      </div>
    </div>
  </div>

  <div class="mainGrid">
    <div class="workspace">
      <div class="toolbar">
        <button id="btnSnapshot" onclick="loadSnapshot()">Capture</button>
        <button id="btnSave" onclick="saveConfig()" class="highlight">SaveConfig</button>

        <label class="toolbarToggle">
          <input id="show_selected_only" type="checkbox" onchange="draw()">
          <span>Only sel. ROI</span>
        </label>

        <label class="toolbarToggle">
          <input id="show_seg_overlay" type="checkbox" onchange="draw()">
          <span>Segments</span>
        </label>

        <div class="toolbarBreak"></div>

        <button id="btnTest" onclick="testEval()">Test Detection</button>
        <button onclick="addSevenSeg()">+ Sevenseg</button>
        <button onclick="addSymbol()">+ Symbol</button>
        <button onclick="delSelected()">Delete ROI</button>
        <button onclick="setZoom(1)">100%</button>
        <button onclick="setZoom(2)">200%</button>
        <button onclick="setZoom(3)">300%</button>
        <button onclick="setZoom(4)">400%</button>
      </div>

      <div class="small" style="margin-bottom:8px">
        Zoom: <span id="zoomLabel">100%</span> | Camera: <span id="frameLabel">320x240</span>
      </div>

      <div id="canvasWrap">
        <img id="snap" alt="snapshot">
        <canvas id="overlay"></canvas>
      </div>

      <div class="help">
        Select an ROI and drag it to move.
        Drag the corners to resize.
      </div>
    </div>

    <div class="sidebar">
      <div class="card">
        <h3>Regions of Interest ROI</h3>
        <ul id="roiList"></ul>
      </div>

      <div class="collapsibleSection">
        <div class="collapsibleHeader">
          <div class="sectionTitle">
            <button type="button" class="collapseBtn" id="roi_btn"
              onclick="toggleSection('roi_content','roi_btn')">▸</button>
            <h3>ROI Settings</h3>
          </div>
        </div>

        <div id="roi_content" class="collapsibleContent hidden">
          <label>ID <input id="f_id"></label>
          <label>Label <input id="f_label"></label>

          <label>Type
            <select id="f_type" onchange="syncTypeFields()">
              <option value="sevenseg">sevenseg</option>
              <option value="symbol">symbol</option>
            </select>
          </label>

          <label><input id="f_invert_logic" type="checkbox">Invert Detection Logic (dark = ON)</label>
          <label><input id="f_auto_threshold" type="checkbox" onchange="syncAutoThreshold()">Auto Threshold</label>

          <div class="two">
            <label>X <input id="f_x" type="number" min="0"></label>
            <label>Y <input id="f_y" type="number" min="0"></label>
          </div>

          <div class="two">
            <label>W <input id="f_w" type="number" min="1"></label>
            <label>H <input id="f_h" type="number" min="1"></label>
          </div>

          <div id="sevensegFields">
            <div class="two">
              <label>Threshold <input id="f_threshold" type="number" min="0" max="255"></label>
              <label>Digit Spacing <input id="f_gap" type="number" min="0" oninput="updateSelectedFromForm();draw()"></label>
            </div>

            <div class="two">
              <label><input id="f_stretch_contrast" type="checkbox">Contrast Stretch (per digit)</label>
              <label>Confidence Margin <input id="f_confidence_margin" type="number" min="0" max="100" style="width:60px"></label>
            </div>

            <label>Digits
              <select id="f_digits" onchange="renderGapInputs();updateSelectedFromForm();draw()">
                <option value="1">1</option>
                <option value="2">2</option>
                <option value="3">3</option>
                <option value="4">4</option>
                <option value="5">5</option>
                <option value="6">6</option>
                <option value="7">7</option>
                <option value="8">8</option>
                <option value="9">9</option>
                <option value="10">10</option>
              </select>
            </label>

            <label>Decimal Places
              <select id="f_decimal_places">
                <option value="0">0 (none)</option>
                <option value="1">1</option>
                <option value="2">2</option>
                <option value="3">3</option>
                <option value="4">4</option>
                <option value="5">5</option>
              </select>
            </label>

            <label style="display:flex;align-items:center;gap:6px;">
              <input type="checkbox" id="f_single_gap" onchange="onSingleGapToggle()">
              Individual Digit Gaps
            </label>
            <div id="gap_inputs_wrap" style="display:none;margin-top:4px;"></div>

            <div id="sevensegGeometryWarn" class="warn" style="display:none"></div>

            <label>Segment Profile
              <select id="f_seg_profile" onchange="updateSelectedFromForm()"></select>
            </label>
          </div>

          <div id="symbolFields">
            <label>Threshold On <input id="f_on" type="number" min="0" max="255"></label>
            <label>Threshold Off <input id="f_off" type="number" min="0" max="255"></label>
          </div>

          <label><input id="f_ha_enabled" type="checkbox" onchange="syncHaFields()">HA Enabled</label>

          <div id="haFields">
            <label>HA Name <input id="f_ha_name"></label>
            <label>Icon
              <select id="f_ha_icon">
                <option value="">Auto</option>
                <option value="mdi:thermometer">🌡 Temperature</option>
                <option value="mdi:water-percent">💧 Humidity</option>
                <option value="mdi:gauge">📟 Pressure</option>
                <option value="mdi:flash">⚡ Power</option>
                <option value="mdi:lightning-bolt">⚡ Energy</option>
                <option value="mdi:sine-wave">🔌 Voltage</option>
                <option value="mdi:current-ac">🔋 Current</option>
                <option value="mdi:timer-outline">⏱ Runtime</option>
                <option value="mdi:cup-water">🥛 Volume / Liter</option>
                <option value="mdi:radiator">🔥 Heating</option>
                <option value="mdi:waves-arrow-right">🌊 Flow</option>
                <option value="mdi:alert">⚠️ Error / Alert</option>
                <option value="mdi:toggle-switch">🔘 Switch</option>
                <option value="mdi:counter">🔢 Counter</option>
                <option value="mdi:pump">🟢 Pump</option>
                <option value="mdi:water">💦 Level</option>
              </select>
            </label>

            <label>Unit
              <select id="f_ha_unit">
                <option value="">Auto</option>
                <option value="°C">°C</option>
                <option value="%">%</option>
                <option value="bar">bar</option>
                <option value="W">W</option>
                <option value="kWh">kWh</option>
                <option value="V">V</option>
                <option value="A">A</option>
                <option value="h">h</option>
                <option value="l">l</option>
              </select>
            </label>

            <label>Device Class
              <select id="f_ha_device_class">
                <option value="">Auto</option>
                <option value="temperature">temperature</option>
                <option value="humidity">humidity</option>
                <option value="pressure">pressure</option>
                <option value="power">power</option>
                <option value="energy">energy</option>
                <option value="voltage">voltage</option>
                <option value="current">current</option>
                <option value="duration">duration</option>
                <option value="heat">heat</option>
                <option value="running">running</option>
                <option value="problem">problem</option>
                <option value="connectivity">connectivity</option>
              </select>
            </label>

            <label>State Class
              <select id="f_ha_state_class">
                <option value="">Auto</option>
                <option value="measurement">measurement</option>
                <option value="total">total</option>
                <option value="total_increasing">total_increasing</option>
              </select>
            </label>
          </div>
        </div>
      </div>

      <div class="collapsibleSection">
        <div class="collapsibleHeader">
          <div class="sectionTitle">
            <button type="button" class="collapseBtn" id="segprof_btn"
              onclick="openSegProfSection()">▸</button>
            <h3>Segment/Profiles Editor</h3>
          </div>
        </div>

        <div id="segprof_content" class="collapsibleContent hidden">
          <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
            <select id="sp_list" onchange="onSegProfSelect()" style="flex:1"></select>
            <button type="button" onclick="segProfNew()" title="New profile">+</button>
            <button type="button" onclick="segProfDuplicate()" title="Duplicate">⧉</button>
            <button type="button" onclick="segProfDelete()" title="Delete" id="sp_del_btn">✕</button>
          </div>

          <div id="sp_editor">
            <label>Name <input id="sp_name"
              onblur="segProfRename()"
              onkeydown="if(event.key==='Enter'){this.blur();}"></label>

            <div style="overflow-x:auto;margin-top:8px">
              <table style="width:100%;border-collapse:collapse;font-size:12px">
                <thead>
                  <tr>
                    <th style="text-align:left;padding:2px 4px">Seg</th>
                    <th style="padding:2px 4px">rx %</th>
                    <th style="padding:2px 4px">ry %</th>
                    <th style="padding:2px 4px">rw %</th>
                    <th style="padding:2px 4px">rh %</th>
                  </tr>
                </thead>
                <tbody id="sp_segs_body"></tbody>
              </table>
            </div>
            <div class="small" style="margin-top:6px">Coordinates relative to the digit bounding box (0–100%). Order: a = top, b = top-right, c = bottom-right, d = bottom, e = bottom-left, f = top-left, g = middle.</div>
          </div>
        </div>
      </div>

      <div class="collapsibleSection">
        <div class="collapsibleHeader">
          <div class="sectionTitle">
            <button type="button" class="collapseBtn" id="result_btn"
              onclick="toggleSection('result_content','result_btn')">▸</button>
            <h3>Status & Results</h3>
          </div>
        </div>

        <div id="result_content" class="collapsibleContent hidden">
          <pre id="out">-</pre>
        </div>
      </div>

      <div class="collapsibleSection">
        <div class="collapsibleHeader">
          <div class="sectionTitle">
            <button type="button" class="collapseBtn" id="config_btn"
              onclick="toggleSection('config_content','config_btn')">▸</button>
            <h3>Configuration</h3>
          </div>
        </div>

        <div id="config_content" class="collapsibleContent hidden">
          <button id="btnExport" onclick="exportConfig()" style="width:100%">Export Configuration</button>
          <button id="btnImport" onclick="triggerImport()" style="width:100%">Import Configuration</button>
          <input type="file" id="importFile" accept=".json" style="display:none" onchange="importConfig()">
          <div class="small" style="margin-top:8px">Backup and restore your configuration. Useful for reinstalls or multi-device setups.</div>
        </div>
      </div>

      <div class="collapsibleSection">
        <div class="collapsibleHeader">
          <div class="sectionTitle">
            <button type="button" class="collapseBtn" id="ota_btn"
              onclick="toggleSection('ota_content','ota_btn')">▸</button>
            <h3>OTA Update</h3>
          </div>
        </div>

        <div id="ota_content" class="collapsibleContent hidden">
          <div class="small" style="margin-bottom:8px">Only available in STA mode. Upload the compiled <code>.bin</code> file here.</div>
          <input id="ota_file" type="file" accept=".bin,application/octet-stream">
          <button id="btnOta" onclick="uploadFirmware()">Upload Firmware</button>
          <progress id="ota_progress" value="0" max="100"></progress>
          <div class="small" id="ota_text">No Upload</div>
        </div>
      </div>

      <div class="collapsibleSection">
        <div class="collapsibleHeader">
          <div class="sectionTitle">
            <button type="button" class="collapseBtn" id="debug_btn"
              onclick="toggleSection('debug_content','debug_btn')">▸</button>
            <h3>Debug</h3>
          </div>
        </div>

        <div id="debug_content" class="collapsibleContent hidden">
          <label>Debug Level
            <select id="dbg_level">
              <option value="0">0 - Errors</option>
              <option value="1">1 - Info</option>
              <option value="2">2 - Debug</option>
            </select>
          </label>
          <button id="btnReboot" onclick="rebootDevice()">Reboot Device</button>
          <div class="subtle">Restarts the ESP32 after confirmation.</div>
        </div>
      </div>
    </div>
  </div>
</div>

<script>
const state = {
  config:null,
  selected:null,
  zoom:1,
  selectedSegIdx:-1,
  drag:{
    active:false,
    mode:null,
    roiId:null,
    startMouseX:0,
    startMouseY:0,
    startX:0,
    startY:0,
    startW:0,
    startH:0
  },
  pan:{
    active:false,
    startClientX:0,
    startClientY:0,
    startScrollLeft:0,
    startScrollTop:0
  }
};

const HANDLE_SIZE = 8;
const MIN_SEVENSEG_DIGIT_WIDTH_PX = 8;
const MIN_SEVENSEG_DIGIT_HEIGHT_PX = 20;

async function api(url, opt) {
  const r = await fetch(url, opt);
  const ct = r.headers.get('content-type') || '';
  if (ct.includes('application/json')) return await r.json();
  return await r.text();
}

function setButtonBusy(id, busy, textBusy='...') {
  const btn = document.getElementById(id);
  if (!btn) return;
  if (!btn.dataset.origText) btn.dataset.origText = btn.textContent;

  if (busy) {
    btn.classList.add('busy');
    btn.textContent = textBusy;
  } else {
    btn.classList.remove('busy');
    btn.textContent = btn.dataset.origText;
  }
}

function flashButtonState(id, ok, okText='✓ OK', errText='✕ Error') {
  const btn = document.getElementById(id);
  if (!btn) return;
  if (!btn.dataset.origText) btn.dataset.origText = btn.textContent;

  btn.classList.remove('ok','err');
  btn.classList.add(ok ? 'ok' : 'err');
  btn.textContent = ok ? okText : errText;

  setTimeout(() => {
    btn.classList.remove('ok','err');
    btn.textContent = btn.dataset.origText;
  }, 1200);
}

function formatUptime(sec){
  sec = Math.floor(sec || 0);
  const d = Math.floor(sec / 86400);
  sec %= 86400;
  const h = Math.floor(sec / 3600);
  sec %= 3600;
  const m = Math.floor(sec / 60);
  const s = sec % 60;

  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

function clampInt(v, min, max){
  return Math.max(min, Math.min(max, Math.round(v)));
}

function frameValueFromWidthHeight(width, height) {
  const key = `${width}x${height}`;
  const allowed = ['160x120', '320x240', '640x480', '800x600'];

  if (allowed.includes(key)) return key;
  return '320x240';
}

function selectedRoi(){
  return state.config?.rois?.find(r => r.id === state.selected) || null;
}

function setLed(id, state){
  const el = document.getElementById(id);
  if (!el) return;
  el.classList.remove("ledGreen", "ledRed");
  if (state) el.classList.add("ledGreen");
  else el.classList.add("ledRed");
}

function setSectionVisible(toggleId, contentId){
  const toggle = document.getElementById(toggleId);
  const content = document.getElementById(contentId);
  if (!toggle || !content) return;

  if (toggle.checked) content.classList.remove('hidden');
  else content.classList.add('hidden');
}

function setSectionExpanded(contentId, buttonId, expanded){
  const content = document.getElementById(contentId);
  const btn = document.getElementById(buttonId);
  if (!content || !btn) return;

  if (expanded) {
    content.classList.remove('hidden');
    btn.textContent = '▾';
  } else {
    content.classList.add('hidden');
    btn.textContent = '▸';
  }
}

function toggleSection(contentId, buttonId){
  const content = document.getElementById(contentId);
  const btn = document.getElementById(buttonId);
  if (!content || !btn) return;

  const hidden = content.classList.toggle('hidden');
  btn.textContent = hidden ? '▸' : '▾';
}

function toggleAllSections(){
  const contents = document.querySelectorAll('.collapsibleContent');
  const btns = document.querySelectorAll('.collapseBtn:not(#btnToggleAll)');
  const globalBtn = document.getElementById('btnToggleAll');
  // Wenn irgendeine Sektion offen ist → alle zuklappen, sonst alle aufklappen
  const anyOpen = Array.from(contents).some(c => !c.classList.contains('hidden'));
  contents.forEach(c => {
    if (anyOpen) c.classList.add('hidden'); else c.classList.remove('hidden');
  });
  btns.forEach(b => { b.textContent = anyOpen ? '▸' : '▾'; });
  if (globalBtn) globalBtn.textContent = anyOpen ? '▾' : '▸';
}

function openStatusResults(){
  [
    ['camera_content','camera_btn'],
    ['lighting_content','lighting_btn'],
    ['lighting2_content','lighting2_btn'],
    ['mqtt_content','mqtt_btn'],
    ['precapture_content','precapture_btn'],
    ['roi_content','roi_btn'],
    ['segprof_content','segprof_btn'],
    ['config_content','config_btn'],
    ['ota_content','ota_btn'],
    ['debug_content','debug_btn'],
  ].forEach(([c,b]) => setSectionExpanded(c, b, false));
  setSectionExpanded('result_content', 'result_btn', true);
}

function openSegProfSection(){
  const isOpen = !document.getElementById('segprof_content').classList.contains('hidden');
  if (isOpen) {
    // already open → just toggle closed
    setSectionExpanded('segprof_content','segprof_btn', false);
    return;
  }
  // close all other sections, open segment profiles
  const allSections = [
    ['camera_content','camera_btn'],
    ['mqtt_content','mqtt_btn'],
    ['lighting_content','lighting_btn'],
    ['precapture_content','precapture_btn'],
    ['lighting2_content','lighting2_btn'],
    ['roi_content','roi_btn'],
    ['result_content','result_btn'],
    ['config_content','config_btn'],
    ['ota_content','ota_btn'],
    ['debug_content','debug_btn'],
  ];
  allSections.forEach(([c,b]) => setSectionExpanded(c, b, false));
  setSectionExpanded('segprof_content','segprof_btn', true);
}

function setOutText(text){
  document.getElementById('out').textContent = text;
  openStatusResults();
}

function appendOutText(text){
  document.getElementById('out').textContent += text;
  openStatusResults();
}


function refreshCollapsibleSections(){
}

function syncHaFields(){
  const enabled = document.getElementById('f_ha_enabled').checked;
  document.getElementById('haFields').style.display = enabled ? 'block' : 'none';
}

async function loadStatus(){
  const s = await api('/api/status');

  document.getElementById('st_wifi').textContent =
    s.wifi.connected ? 'Connected' : 'Disconnected';
  document.getElementById('st_ip').textContent =
    s.wifi.ip || '-';
  document.getElementById('st_mqtt').textContent =
    s.mqtt.connected ? 'Connected' : 'Disconnected';
  document.getElementById('st_temp').textContent =
    (s.system?.temperature != null ? s.system.temperature + ' °C' : '-');
  document.getElementById('st_heap').textContent =
    (s.system?.heap_free ?? '-') + '';
  document.getElementById('st_rssi').textContent =
    (s.wifi?.rssi ?? '-') + ' dBm';
  document.getElementById('st_cam').textContent =
    s.camera?.ok ? 'OK' : 'Fault';
  document.getElementById('st_cam_model').textContent =
    s.camera?.model || '-';
  document.getElementById('st_cam_sensor').textContent =
    s.camera?.sensor || '-';
  document.getElementById('st_cam_frame').textContent =
    s.camera?.frame || '-';
  document.getElementById('st_fw_version').textContent =
    s.system?.fw_version || '-';
  document.getElementById('st_uptime').textContent =
    formatUptime(s.system?.uptime_s ?? 0);

  setLed("led_wifi", !!s.wifi.connected);
  setLed("led_mqtt", !!s.mqtt.connected);
  setLed("led_cam", !!s.camera?.ok);

  const progress = document.getElementById('ota_progress');
  const otaText = document.getElementById('ota_text');

  if (s.ota?.running) {
    const pct = s.ota.total > 0
      ? Math.floor((s.ota.written * 100) / s.ota.total)
      : 0;
    progress.value = pct;
    otaText.textContent = `Upload in progress: ${pct}%`;
  } else if (s.ota?.ok) {
    progress.value = 100;
    otaText.textContent = 'Update successful, restarting...';
  } else if (s.ota?.error) {
    otaText.textContent = `OTA error: ${s.ota.error}`;
  } else {
    progress.value = 0;
    otaText.textContent = 'No Upload';
  }
}

async function loadConfig(){
  state.config = await api('/api/config');
  fillPanels();
  renderList();
}

async function loadSnapshot(){
  const img = document.getElementById('snap');

  setButtonBusy('btnSnapshot', true, 'Loading...');

  try {
    img.src = '/api/snapshot?t=' + Date.now();

    await new Promise((res, rej) => {
      img.onload = res;
      img.onerror = rej;
    });

    applyZoom();
    draw();

    setButtonBusy('btnSnapshot', false);
    flashButtonState('btnSnapshot', true, '✓ Capture');
  } catch (e) {
    setButtonBusy('btnSnapshot', false);
    flashButtonState('btnSnapshot', false);
    setOutText(String(e));
  }
}

async function waitForDevice() {
  for (let i = 0; i < 20; i++) {
    try {
      const r = await fetch('/api/status');
      if (r.ok) {
        location.reload();
        return;
      }
    } catch(e) {}
    await new Promise(r => setTimeout(r, 1000));
  }
}

function openLiveStream(){
  if (!document.getElementById('stream_enabled').checked) {
    setOutText('Live Stream ist deaktiviert.\nEnable "🎥 Live Stream" in der Camera-Sektion und Config speichern.');
    return;
  }
  window.open('/live', 'live-stream',
    'width=860,height=700,resizable=yes,menubar=no,toolbar=no,location=no,status=no');
}

async function rebootDevice(){
  const confirmed = confirm('Device wirklich neu starten? Laufende Messung wird kurz unterbrochen.');
  if (!confirmed) return;

  setButtonBusy('btnReboot', true, 'Rebooting...');

  try {
    const res = await api('/api/reboot', {
      method:'POST'
    });

    document.getElementById('out').textContent = JSON.stringify(res, null, 2) + "\\n\\nDevice restarting...";
    setButtonBusy('btnReboot', false);
    flashButtonState('btnReboot', true, '✓ Restarting');
    setTimeout(waitForDevice, 2500);
  } catch (e) {
    setButtonBusy('btnReboot', false);
    flashButtonState('btnReboot', false);
    setOutText(String(e));
  }
}

async function uploadFirmware(){
  const fileInput = document.getElementById('ota_file');
  const file = fileInput.files[0];

  if (!file) {
    document.getElementById('ota_text').textContent = 'Please select a .bin file first';
    return;
  }

  // Magic-Byte Prüfung: ESP32 App-Binary muss mit 0xE9 beginnen
  const header = await file.slice(0, 4).arrayBuffer();
  const bytes = new Uint8Array(header);
  if (bytes[0] !== 0xE9) {
    document.getElementById('ota_text').textContent =
      `Wrong file: first byte is 0x${bytes[0].toString(16).toUpperCase().padStart(2,'0')}, expected 0xE9. Use firmware-ota.bin, not firmware.bin!`;
    return;
  }

  setButtonBusy('btnOta', true, 'Uploading...');
  document.getElementById('ota_progress').value = 0;
  document.getElementById('ota_text').textContent = 'Upload started...';

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/update', true);

  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      const pct = Math.floor((e.loaded * 100) / e.total);
      document.getElementById('ota_progress').value = pct;
      document.getElementById('ota_text').textContent = `Upload: ${pct}%`;
    }
  };

  xhr.onload = function() {
    setButtonBusy('btnOta', false);

    if (xhr.status >= 200 && xhr.status < 300) {
      flashButtonState('btnOta', true, '✓ OTA OK');
      document.getElementById('ota_text').textContent =
        'Firmware uploaded. Device restarting...';
      setTimeout(waitForDevice, 3000);
    } else {
      flashButtonState('btnOta', false);
      document.getElementById('ota_text').textContent =
        `Upload failed (${xhr.status})`;
    }
  };

  xhr.onerror = function() {
    setButtonBusy('btnOta', false);
    flashButtonState('btnOta', false);
    document.getElementById('ota_text').textContent =
      'Network error during OTA upload';
  };

  const formData = new FormData();
  formData.append('update', file, file.name);

  xhr.send(formData);
}

async function testLight(){
  updateSelectedFromForm();
  updatePanelsFromForm();

  setButtonBusy('btnLightTest', true, 'Testing...');

  try {
    const res = await api('/api/light_test', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify(state.config)
    });

    setOutText(JSON.stringify(res, null, 2));
    setButtonBusy('btnLightTest', false);
    flashButtonState('btnLightTest', !!res.ok, '✓ Light OK', '✕ Error');
  } catch (e) {
    setButtonBusy('btnLightTest', false);
    flashButtonState('btnLightTest', false);
    setOutText(String(e));
  }
}

async function testLight2(){
  updateSelectedFromForm();
  updatePanelsFromForm();

  setButtonBusy('btnLight2Test', true, 'Testing...');

  try {
    const res = await api('/api/light2_test', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify(state.config)
    });

    setOutText(JSON.stringify(res, null, 2));
    setButtonBusy('btnLight2Test', false);
    flashButtonState('btnLight2Test', !!res.ok, '✓ Light OK', '✕ Error');
  } catch (e) {
    setButtonBusy('btnLight2Test', false);
    flashButtonState('btnLight2Test', false);
    setOutText(String(e));
  }
}

function setZoom(z){
  state.zoom = z;
  updateZoomAndFrameLabel();
  applyZoom();
  draw();
}

function updateZoomAndFrameLabel(){
  document.getElementById('zoomLabel').textContent = (state.zoom * 100) + '%';
  const w = state.config?.camera?.width || 320;
  const h = state.config?.camera?.height || 240;
  document.getElementById('frameLabel').textContent = `${w}x${h}`;
}

function applyZoom(){
  const img = document.getElementById('snap');
  const cv = document.getElementById('overlay');
  if (!img.naturalWidth || !state.config) return;

  const w = state.config.camera.width * state.zoom;
  const h = state.config.camera.height * state.zoom;

  img.style.width = w + 'px';
  img.style.height = h + 'px';

  cv.width = w;
  cv.height = h;
  cv.style.width = w + 'px';
  cv.style.height = h + 'px';

  const wrap = document.getElementById('canvasWrap');
  wrap.style.width   = w + 'px';
  wrap.style.height  = h + 'px';
  wrap.style.maxHeight = Math.min(h, window.innerHeight - 220) + 'px';
}

function renderList(){
  const ul = document.getElementById('roiList');
  ul.innerHTML = '';

  state.config.rois.forEach(roi => {
    const li = document.createElement('li');
    li.textContent = `${roi.id} (${roi.type}${roi.type === 'sevenseg' ? ':' + roi.digits + (roi.decimal_places > 0 ? ',dp' + roi.decimal_places : '') : ''})`;
    if (roi.id === state.selected) li.classList.add('active');
    li.onclick = () => {
      state.selected = roi.id;
      fillForm(roi);
      renderList();
      draw();
      setSectionExpanded('roi_content','roi_btn', true);
    };
    ul.appendChild(li);
  });
}

function fillForm(roi){
  document.getElementById('f_id').value = roi.id || '';
  document.getElementById('f_label').value = roi.label || '';
  document.getElementById('f_ha_enabled').checked = !!roi.ha_enabled;
  document.getElementById('f_ha_name').value = roi.ha_name || '';
  document.getElementById('f_ha_unit').value = roi.ha_unit || '';
  document.getElementById('f_ha_icon').value = roi.ha_icon || '';
  document.getElementById('f_ha_device_class').value = roi.ha_device_class || '';
  document.getElementById('f_ha_state_class').value = roi.ha_state_class || '';
  document.getElementById('f_type').value = roi.type || 'sevenseg';
  document.getElementById('f_invert_logic').checked = !!roi.invert_logic;
  document.getElementById('f_auto_threshold').checked = !!roi.auto_threshold;
  document.getElementById('f_x').value = roi.x || 0;
  document.getElementById('f_y').value = roi.y || 0;
  document.getElementById('f_w').value = roi.w || 1;
  document.getElementById('f_h').value = roi.h || 1;
  document.getElementById('f_threshold').value = roi.threshold || 105;
  document.getElementById('f_stretch_contrast').checked = !!roi.stretch_contrast;
  document.getElementById('f_confidence_margin').value = roi.confidence_margin || 0;
  document.getElementById('f_gap').value = roi.digit_gap_px || 8;
  document.getElementById('f_digits').value = roi.digits || 2;
  document.getElementById('f_decimal_places').value = roi.decimal_places || 0;
  // SingleGap
  const hasIndividual = Array.isArray(roi.gaps) && roi.gaps.length > 0;
  document.getElementById('f_single_gap').checked = hasIndividual;
  renderGapInputs(hasIndividual ? roi.gaps : null);
  document.getElementById('f_on').value = roi.threshold_on || 125;
  document.getElementById('f_off').value = roi.threshold_off || 105;
  refreshSegProfDropdown();
  document.getElementById('f_seg_profile').value = roi.seg_profile || 'standard';
  syncHaFields();
  syncTypeFields();
  updateSevenSegGeometryWarning();
}

function fillPanels(){
  document.getElementById('cam_model').value = state.config.camera.model || 'ai_thinker';
  document.getElementById('cam_frame').value =
    frameValueFromWidthHeight(state.config.camera.width || 320, state.config.camera.height || 240);
  document.getElementById('cam_jpeg_quality').value = state.config.camera.jpeg_quality || 12;
  document.getElementById('cam_auto_exposure').checked = !!state.config.camera.auto_exposure;
  document.getElementById('cam_auto_gain').checked = !!state.config.camera.auto_gain;
  document.getElementById('cam_auto_whitebalance').checked = !!state.config.camera.auto_whitebalance;
  document.getElementById('cam_vflip').checked = !!state.config.camera.vflip;
  document.getElementById('cam_hflip').checked = !!state.config.camera.hflip;
  document.getElementById('cam_aec_value').value = state.config.camera.aec_value ?? 300;
  document.getElementById('cam_agc_gain').value = state.config.camera.agc_gain ?? 0;

  document.getElementById('l_enabled').checked = !!state.config.light.enabled;
  document.getElementById('l_gpio').value = state.config.light.gpio ?? -1;
  document.getElementById('l_brightness').value = state.config.light.brightness ?? 255;
  document.getElementById('l_settle').value = state.config.light.settle_ms ?? 250;
  document.getElementById('l_capture_only').checked = !!state.config.light.capture_only;
  document.getElementById('l_invert').checked = !!state.config.light.invert;

  if (!state.config.light2) state.config.light2 = {};
  document.getElementById('l2_enabled').checked = !!state.config.light2.enabled;
  document.getElementById('l2_gpio').value = state.config.light2.gpio ?? -1;
  document.getElementById('l2_led_count').value = state.config.light2.led_count ?? 16;
  document.getElementById('l2_brightness').value = state.config.light2.brightness ?? 200;
  document.getElementById('l2_settle').value = state.config.light2.settle_ms ?? 250;
  document.getElementById('l2_capture_only').checked = !!state.config.light2.capture_only;
  {
    const r = state.config.light2.color_r ?? 255;
    const g = state.config.light2.color_g ?? 255;
    const b = state.config.light2.color_b ?? 255;
    document.getElementById('l2_color').value = '#' + [r,g,b].map(v=>v.toString(16).padStart(2,'0')).join('');
  }

  if (!state.config.pre_capture) state.config.pre_capture = {};
  document.getElementById('pc_enabled').checked = !!state.config.pre_capture.enabled;
  document.getElementById('pc_delay').value = state.config.pre_capture.delay_ms ?? 500;
  document.getElementById('pc_topic').value = state.config.pre_capture.mqtt_topic ?? 'pre_capture';

  document.getElementById('m_enabled').checked = !!state.config.mqtt.enabled;
  document.getElementById('m_image_enabled').checked = !!state.config.mqtt.image_enabled;
  document.getElementById('m_host').value = state.config.mqtt.host || '';
  document.getElementById('m_port').value = state.config.mqtt.port || 1883;
  document.getElementById('m_topic').value = state.config.mqtt.topic || '';
  document.getElementById('m_user').value = state.config.mqtt.username || '';
  document.getElementById('m_pass').value = state.config.mqtt.password || '';
  document.getElementById('m_interval').value = state.config.timing?.capture_interval_sec ?? 60;

  document.getElementById('ha_prefix').value = state.config.ha?.discovery_prefix || 'homeassistant';
  document.getElementById('d_name').value = state.config.device?.name || '';
  document.getElementById('stream_enabled').checked = !!state.config.stream?.enabled;

  document.getElementById('dbg_level').value = String(state.config.debug?.level ?? 2);
  updateZoomAndFrameLabel();

  renderSegProfList();
  refreshCollapsibleSections();

  // Camera section stays collapsed by default
  setSectionExpanded('mqtt_content','mqtt_btn', false);
  setSectionExpanded('lighting_content','lighting_btn', !!state.config.light.enabled);
  setSectionExpanded('lighting2_content','lighting2_btn', !!state.config.light2?.enabled);
  setSectionExpanded('precapture_content','precapture_btn', false);
  setSectionExpanded('segprof_content','segprof_btn', false);
  setSectionExpanded('roi_content','roi_btn', false);
  setSectionExpanded('result_content','result_btn', false);
  setSectionExpanded('config_content','config_btn', false);
  setSectionExpanded('ota_content','ota_btn', false);
  setSectionExpanded('debug_content','debug_btn', false);
}

function updatePanelsFromForm(){
  state.config.camera.model = document.getElementById('cam_model').value;
  const frameValue = document.getElementById('cam_frame').value || '320x240';
  const parts = frameValue.split('x');
  state.config.camera.width = parseInt(parts[0] || '320', 10);
  state.config.camera.height = parseInt(parts[1] || '240', 10);
  state.config.camera.jpeg_quality = parseInt(document.getElementById('cam_jpeg_quality').value || '12', 10);
  state.config.camera.auto_exposure = document.getElementById('cam_auto_exposure').checked;
  state.config.camera.auto_gain = document.getElementById('cam_auto_gain').checked;
  state.config.camera.auto_whitebalance = document.getElementById('cam_auto_whitebalance').checked;
  state.config.camera.vflip = document.getElementById('cam_vflip').checked;
  state.config.camera.hflip = document.getElementById('cam_hflip').checked;
  state.config.camera.aec_value = parseInt(document.getElementById('cam_aec_value').value || '300', 10);
  state.config.camera.agc_gain = parseInt(document.getElementById('cam_agc_gain').value || '0', 10);

  state.config.light.enabled = document.getElementById('l_enabled').checked;
  state.config.light.gpio = parseInt(document.getElementById('l_gpio').value || '-1', 10);
  state.config.light.brightness = parseInt(document.getElementById('l_brightness').value || '255', 10);
  state.config.light.settle_ms = parseInt(document.getElementById('l_settle').value || '250', 10);
  state.config.light.capture_only = document.getElementById('l_capture_only').checked;
  state.config.light.invert = document.getElementById('l_invert').checked;

  if (!state.config.light2) state.config.light2 = {};
  state.config.light2.enabled = document.getElementById('l2_enabled').checked;
  state.config.light2.gpio = parseInt(document.getElementById('l2_gpio').value || '-1', 10);
  state.config.light2.led_count = parseInt(document.getElementById('l2_led_count').value || '16', 10);
  state.config.light2.brightness = parseInt(document.getElementById('l2_brightness').value || '200', 10);
  state.config.light2.settle_ms = parseInt(document.getElementById('l2_settle').value || '250', 10);
  state.config.light2.capture_only = document.getElementById('l2_capture_only').checked;
  {
    const hex = document.getElementById('l2_color').value;
    state.config.light2.color_r = parseInt(hex.slice(1,3), 16);
    state.config.light2.color_g = parseInt(hex.slice(3,5), 16);
    state.config.light2.color_b = parseInt(hex.slice(5,7), 16);
  }

  if (!state.config.pre_capture) state.config.pre_capture = {};
  state.config.pre_capture.enabled = document.getElementById('pc_enabled').checked;
  state.config.pre_capture.delay_ms = parseInt(document.getElementById('pc_delay').value || '500', 10);
  state.config.pre_capture.mqtt_topic = document.getElementById('pc_topic').value || 'pre_capture';

  state.config.mqtt.enabled = document.getElementById('m_enabled').checked;
  state.config.mqtt.image_enabled = document.getElementById('m_image_enabled').checked;
  state.config.mqtt.host = document.getElementById('m_host').value;
  state.config.mqtt.port = parseInt(document.getElementById('m_port').value || '1883', 10);
  state.config.mqtt.topic = document.getElementById('m_topic').value;
  state.config.mqtt.username = document.getElementById('m_user').value;
  state.config.mqtt.password = document.getElementById('m_pass').value;

  if (!state.config.timing) state.config.timing = {};
  state.config.timing.capture_interval_sec = parseInt(document.getElementById('m_interval').value || '60', 10);

  if (!state.config.ha) state.config.ha = {};
  state.config.ha.discovery_prefix = document.getElementById('ha_prefix').value;
  if (!state.config.device) state.config.device = {};
  state.config.device.name = document.getElementById('d_name').value;

  if (!state.config.stream) state.config.stream = {};
  state.config.stream.enabled = document.getElementById('stream_enabled').checked;

  if (!state.config.debug) state.config.debug = {};
  state.config.debug.level = parseInt(document.getElementById('dbg_level').value || '2', 10);
  updateZoomAndFrameLabel();
  refreshCollapsibleSections();
}

function updateSelectedFromForm(){
  const roi = selectedRoi();
  if (!roi) return;

  const oldId = roi.id;

  roi.id = document.getElementById('f_id').value;
  roi.label = document.getElementById('f_label').value;
  roi.ha_enabled = document.getElementById('f_ha_enabled').checked;
  roi.ha_name = document.getElementById('f_ha_name').value;
  roi.ha_unit = document.getElementById('f_ha_unit').value;
  roi.ha_icon = document.getElementById('f_ha_icon').value;
  roi.ha_device_class = document.getElementById('f_ha_device_class').value;
  roi.ha_state_class = document.getElementById('f_ha_state_class').value;
  roi.type = document.getElementById('f_type').value;
  roi.invert_logic = document.getElementById('f_invert_logic').checked;
  roi.auto_threshold = document.getElementById('f_auto_threshold').checked;
  roi.x = parseInt(document.getElementById('f_x').value || '0', 10);
  roi.y = parseInt(document.getElementById('f_y').value || '0', 10);
  roi.w = Math.max(1, parseInt(document.getElementById('f_w').value || '1', 10));
  roi.h = Math.max(1, parseInt(document.getElementById('f_h').value || '1', 10));
  roi.threshold = parseInt(document.getElementById('f_threshold').value || '105', 10);
  roi.stretch_contrast = document.getElementById('f_stretch_contrast').checked;
  roi.confidence_margin = Math.max(0, parseInt(document.getElementById('f_confidence_margin').value || '0', 10));
  roi.digit_gap_px = parseInt(document.getElementById('f_gap').value || '8', 10);
  roi.digits = Math.max(1, Math.min(10, parseInt(document.getElementById('f_digits').value || '2', 10)));
  roi.decimal_places = parseInt(document.getElementById('f_decimal_places').value || '0', 10);
  if (roi.decimal_places < 0) roi.decimal_places = 0;
  if (roi.decimal_places >= roi.digits) roi.decimal_places = roi.digits - 1;
  // SingleGap
  if (document.getElementById('f_single_gap').checked) {
    roi.gaps = readGapInputs();
  } else {
    roi.gaps = [];
  }
  roi.threshold_on = parseInt(document.getElementById('f_on').value || '125', 10);
  roi.threshold_off = parseInt(document.getElementById('f_off').value || '105', 10);
  roi.seg_profile = document.getElementById('f_seg_profile').value || 'standard';

  if (state.selected === oldId) state.selected = roi.id;

  syncHaFields();
  updateSevenSegGeometryWarning();
  renderList();
  draw();
}

function syncTypeFields(){
  const type = document.getElementById('f_type').value;
  document.getElementById('sevensegFields').style.display = type === 'sevenseg' ? 'block' : 'none';
  document.getElementById('symbolFields').style.display = type === 'symbol' ? 'block' : 'none';
  syncAutoThreshold();
  updateSevenSegGeometryWarning();
}

function syncAutoThreshold(){
  const auto = document.getElementById('f_auto_threshold').checked;
  const type = document.getElementById('f_type').value;
  const thr = document.getElementById('f_threshold');
  const on  = document.getElementById('f_on');
  const off = document.getElementById('f_off');
  if (type === 'sevenseg') {
    thr.disabled = auto;
    thr.style.opacity = auto ? '0.4' : '';
  } else {
    on.disabled = auto;
    off.disabled = auto;
    on.style.opacity = auto ? '0.4' : '';
    off.style.opacity = auto ? '0.4' : '';
  }
}

function renderGapInputs(prefill) {
  const wrap = document.getElementById('gap_inputs_wrap');
  const enabled = document.getElementById('f_single_gap').checked;
  if (!enabled) { wrap.style.display = 'none'; wrap.innerHTML = ''; return; }
  const digits = Math.max(1, parseInt(document.getElementById('f_digits').value || '2', 10));
  const n = digits - 1;
  if (n < 1) { wrap.style.display = 'none'; wrap.innerHTML = ''; return; }
  wrap.style.display = 'flex';
  wrap.style.flexWrap = 'wrap';
  wrap.style.gap = '6px';
  // Keep existing values if count matches
  const existingInputs = wrap.querySelectorAll('input');
  const existingVals = Array.from(existingInputs).map(el => el.value);
  wrap.innerHTML = '';
  const defaultGap = parseInt(document.getElementById('f_gap').value || '8', 10);
  for (let i = 0; i < n; i++) {
    const lbl = document.createElement('label');
    lbl.style.cssText = 'display:flex;flex-direction:column;align-items:center;font-size:12px;gap:2px;';
    const span = document.createElement('span');
    span.textContent = i + '\u2192' + (i+1);
    const inp = document.createElement('input');
    inp.type = 'number';
    inp.min = '0';
    inp.style.width = '48px';
    let val;
    if (prefill && prefill.length === n) val = prefill[i];
    else if (existingVals.length === n) val = parseInt(existingVals[i], 10) || defaultGap;
    else val = defaultGap;
    inp.value = val;
    inp.oninput = () => { updateSelectedFromForm(); draw(); };
    lbl.appendChild(span);
    lbl.appendChild(inp);
    wrap.appendChild(lbl);
  }
}

function readGapInputs() {
  const wrap = document.getElementById('gap_inputs_wrap');
  return Array.from(wrap.querySelectorAll('input')).map(el => parseInt(el.value || '0', 10));
}

function onSingleGapToggle() {
  const enabled = document.getElementById('f_single_gap').checked;
  if (enabled) {
    renderGapInputs(null); // pre-fill with digit_gap_px
  } else {
    renderGapInputs(null);
  }
  updateSelectedFromForm();
  draw();
}

function updateSevenSegGeometryWarning(){
  const warn = document.getElementById('sevensegGeometryWarn');
  if (!warn) return;

  const roi = selectedRoi();
  if (!roi || roi.type !== 'sevenseg') {
    warn.style.display = 'none';
    warn.textContent = '';
    return;
  }

  const digits = Math.max(1, Number(roi.digits || 1));
  const gap = Math.max(0, Number(roi.digit_gap_px || 0));
  const w = Math.max(1, Number(roi.w || 1));
  const h = Math.max(1, Number(roi.h || 1));

  const minW = digits * MIN_SEVENSEG_DIGIT_WIDTH_PX + (digits - 1) * gap;
  const okW = w >= minW;
  const okH = h >= MIN_SEVENSEG_DIGIT_HEIGHT_PX;

  if (okW && okH) {
    warn.style.display = 'none';
    warn.textContent = '';
    return;
  }

  let msg = 'Geometry warning: ';
  if (!okW && !okH) {
    msg += `W and H too small. Need at least W=${minW}, H=${MIN_SEVENSEG_DIGIT_HEIGHT_PX}.`;
  } else if (!okW) {
    msg += `W too small. Need at least W=${minW} (digits=${digits}, gap=${gap}).`;
  } else {
    msg += `H too small. Need at least H=${MIN_SEVENSEG_DIGIT_HEIGHT_PX}.`;
  }

  warn.textContent = msg;
  warn.style.display = 'block';
}

// ── Segment Profile Editor ───────────────────────────────────────────────────

const SEG_NAMES = ['a','b','c','d','e','f','g'];
const SEG_STANDARD = [
  [0.25,0.05,0.50,0.08],[0.78,0.18,0.10,0.26],[0.78,0.56,0.10,0.26],
  [0.25,0.88,0.50,0.08],[0.12,0.56,0.10,0.26],[0.12,0.18,0.10,0.26],
  [0.25,0.46,0.50,0.08]
];

function segProfiles() {
  if (!state.config.seg_profiles) state.config.seg_profiles = [];
  // Ensure "standard" always exists as first entry
  if (!state.config.seg_profiles.find(p => p.name === 'standard')) {
    state.config.seg_profiles.unshift({ name:'standard', segs: SEG_STANDARD.map(s=>[...s]) });
  }
  return state.config.seg_profiles;
}

function renderSegProfList() {
  const sel = document.getElementById('sp_list');
  const prev = sel.value;
  sel.innerHTML = '';
  segProfiles().forEach(p => {
    const o = document.createElement('option');
    o.value = p.name;
    o.textContent = p.name + (p.name === 'standard' ? ' (schreibgeschützt)' : '');
    sel.appendChild(o);
  });
  if (prev && [...sel.options].some(o => o.value === prev)) sel.value = prev;
  onSegProfSelect();

  // also refresh the ROI profile dropdown
  refreshSegProfDropdown();
}

function refreshSegProfDropdown() {
  const dd = document.getElementById('f_seg_profile');
  const prev = dd.value;
  dd.innerHTML = '';
  segProfiles().forEach(p => {
    const o = document.createElement('option');
    o.value = p.name;
    o.textContent = p.name;
    dd.appendChild(o);
  });
  if (prev && [...dd.options].some(o => o.value === prev)) dd.value = prev;
}

function onSegProfSelect() {
  const name = document.getElementById('sp_list').value;
  const prof = segProfiles().find(p => p.name === name);
  if (!prof) return;

  state.selectedSegIdx = -1;

  const isStd = name === 'standard';
  if (document.activeElement?.id !== 'sp_name') {
    document.getElementById('sp_name').value = name;
  }
  document.getElementById('sp_name').disabled = isStd;
  document.getElementById('sp_del_btn').disabled = isStd;

  const tbody = document.getElementById('sp_segs_body');
  tbody.innerHTML = '';
  SEG_NAMES.forEach((sn, i) => {
    const s = prof.segs[i] || [0,0,0.1,0.1];
    const tr = document.createElement('tr');
    tr.style.cursor = 'pointer';
    tr.onclick = (e) => {
      if (e.target.tagName === 'INPUT') return;
      state.selectedSegIdx = (state.selectedSegIdx === i) ? -1 : i;
      // highlight active row
      tbody.querySelectorAll('tr').forEach((r, ri) => {
        r.style.background = ri === state.selectedSegIdx ? '#2a3a55' : '';
      });
      draw();
    };
    tr.innerHTML = `<td style="padding:2px 4px;font-weight:bold">${sn}</td>` +
      ['rx','ry','rw','rh'].map((_, j) =>
        `<td><input type="number" step="0.1" min="0" max="100" style="width:52px"
          value="${Math.round(s[j]*1000)/10}"
          ${isStd ? 'disabled' : ''}
          oninput="segProfUpdateSeg(${i},${j},this.value)"></td>`
      ).join('');
    tbody.appendChild(tr);
  });

  draw();
}

function segProfUpdateSeg(segIdx, valIdx, rawVal) {
  const name = document.getElementById('sp_list').value;
  const prof = segProfiles().find(p => p.name === name);
  if (!prof || name === 'standard') return;
  const v = parseFloat(rawVal) / 100;
  if (!isNaN(v)) {
    prof.segs[segIdx][valIdx] = Math.max(0, Math.min(1, v));
    state.selectedSegIdx = segIdx;
    // sync row highlight
    const rows = document.getElementById('sp_segs_body')?.querySelectorAll('tr');
    if (rows) rows.forEach((r, ri) => { r.style.background = ri === segIdx ? '#2a3a55' : ''; });
    draw();
  }
}

function segProfRename() {
  const name = document.getElementById('sp_list').value;
  if (name === 'standard') return;
  const prof = segProfiles().find(p => p.name === name);
  if (!prof) return;
  const newName = document.getElementById('sp_name').value.trim();
  if (!newName || newName === name) return;
  if (segProfiles().find(p => p.name === newName)) return; // duplicate

  // update references in ROIs
  state.config.rois.forEach(r => { if (r.seg_profile === name) r.seg_profile = newName; });
  prof.name = newName;
  renderSegProfList();
  document.getElementById('sp_list').value = newName;
}

function segProfNew() {
  const base = SEG_STANDARD.map(s => [...s]);
  let name = 'profile1';
  let n = 1;
  while (segProfiles().find(p => p.name === name)) name = 'profile' + (++n);
  segProfiles().push({ name, segs: base });
  renderSegProfList();
  document.getElementById('sp_list').value = name;
  onSegProfSelect();
  setSectionExpanded('segprof_content','segprof_btn', true);
}

function segProfDuplicate() {
  const name = document.getElementById('sp_list').value;
  const prof = segProfiles().find(p => p.name === name);
  if (!prof) return;
  let newName = name + '_copy';
  let n = 1;
  while (segProfiles().find(p => p.name === newName)) newName = name + '_copy' + (++n);
  segProfiles().push({ name: newName, segs: prof.segs.map(s => [...s]) });
  renderSegProfList();
  document.getElementById('sp_list').value = newName;
  onSegProfSelect();
}

function segProfDelete() {
  const name = document.getElementById('sp_list').value;
  if (name === 'standard') return;
  if (!confirm(`Profil "${name}" löschen?`)) return;
  state.config.seg_profiles = state.config.seg_profiles.filter(p => p.name !== name);
  // reset ROIs referencing this profile to "standard"
  state.config.rois.forEach(r => { if (r.seg_profile === name) r.seg_profile = 'standard'; });
  renderSegProfList();
  refreshSegProfDropdown();
}

function drawSegProfOverlay(prof) {
  draw(); // draw() enthält am Ende _drawSegOverlayOnCtx wenn Checkbox aktiv
}

function _drawSegOverlayOnCtx(ctx, prof) {
  const roi = selectedRoi();
  if (!roi || roi.type !== 'sevenseg') return;

  const z = state.zoom;
  const digits = Math.max(1, roi.digits || 1);
  const digitW = roiDigitWidth(roi);
  if (digitW < 1) return;

  const activeIdx = state.selectedSegIdx;

  for (let di = 0; di < digits; di++) {
    const dx = roiDigitXPos(roi, digitW, di);
    const dy = roi.y;
    const dw = digitW;
    const dh = roi.h;

    prof.segs.forEach((s, i) => {
      const isActive = i === activeIdx;
      const sx = (dx + dw * s[0]) * z;
      const sy = (dy + dh * s[1]) * z;
      const sw = Math.max(1, dw * s[2] * z);
      const sh = Math.max(1, dh * s[3] * z);

      if (isActive) {
        ctx.fillStyle = 'rgba(255,34,0,0.35)';
        ctx.strokeStyle = '#ff2200';
        ctx.lineWidth = 2;
      } else {
        ctx.fillStyle = 'rgba(68,136,204,0.40)';
        ctx.strokeStyle = 'rgba(68,160,255,0.95)';
        ctx.lineWidth = 1.5;
      }
      ctx.fillRect(sx, sy, sw, sh);
      ctx.strokeRect(sx, sy, sw, sh);

      // Label nur beim ersten Digit
      if (di === 0) {
        ctx.fillStyle = isActive ? '#ff2200' : 'rgba(68,136,204,0.7)';
        ctx.font = isActive ? 'bold 9px monospace' : '9px monospace';
        ctx.fillText(SEG_NAMES[i], sx + 2, sy + 9);
      }
    });
  }
}

// ── end Segment Profile Editor ────────────────────────────────────────────────

// Returns the x position (in ROI pixels, not zoomed) of digit i, respecting gaps[].
function roiDigitXPos(roi, digitW, i) {
  const hasInd = Array.isArray(roi.gaps) && roi.gaps.length === roi.digits - 1;
  const normalGap = Math.max(0, roi.digit_gap_px || 0);
  let x = roi.x;
  for (let j = 0; j < i; j++) {
    x += digitW;
    x += hasInd ? (roi.gaps[j] || 0) : normalGap;
  }
  return x;
}

// Returns the effective digitW accounting for gaps[].
function roiDigitWidth(roi) {
  const digits = Math.max(1, roi.digits || 1);
  const hasInd = Array.isArray(roi.gaps) && roi.gaps.length === digits - 1;
  const normalGap = Math.max(0, roi.digit_gap_px || 0);
  let totalGap = 0;
  for (let j = 0; j < digits - 1; j++) totalGap += hasInd ? (roi.gaps[j] || 0) : normalGap;
  return (roi.w - totalGap) / digits;
}

function roiScreenRect(roi){
  return {
    x: roi.x * state.zoom,
    y: roi.y * state.zoom,
    w: roi.w * state.zoom,
    h: roi.h * state.zoom
  };
}

function hitTestRoi(px, py){
  if (!state.config?.rois) return null;

  for (let i = state.config.rois.length - 1; i >= 0; i--) {
    const roi = state.config.rois[i];
    const r = roiScreenRect(roi);
    if (px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h) {
      return roi;
    }
  }
  return null;
}

function detectHandle(roi, px, py){
  const r = roiScreenRect(roi);
  const pts = {
    nw:{x:r.x, y:r.y},
    ne:{x:r.x+r.w, y:r.y},
    sw:{x:r.x, y:r.y+r.h},
    se:{x:r.x+r.w, y:r.y+r.h}
  };

  for (const [name,p] of Object.entries(pts)) {
    if (Math.abs(px - p.x) <= HANDLE_SIZE && Math.abs(py - p.y) <= HANDLE_SIZE) {
      return name;
    }
  }
  return null;
}

function updateFormFromSelected(){
  const roi = selectedRoi();
  if (!roi) return;
  fillForm(roi);
}

function canvasPointFromEvent(ev){
  const canvas = document.getElementById('overlay');
  const rect = canvas.getBoundingClientRect();
  return {
    x: ev.clientX - rect.left,
    y: ev.clientY - rect.top
  };
}

function hitTestSegment(px, py) {
  if (!document.getElementById('show_seg_overlay')?.checked) return -1;
  const profEditorOpen = !document.getElementById('segprof_content')?.classList.contains('hidden');
  if (!profEditorOpen) return -1;

  const roi = selectedRoi();
  if (!roi || roi.type !== 'sevenseg') return -1;

  const name = document.getElementById('sp_list')?.value;
  if (!name) return -1;
  const prof = (state.config.seg_profiles || []).find(p => p.name === name);
  if (!prof) return -1;

  const z = state.zoom;
  const digitW = roiDigitWidth(roi);
  if (digitW < 1) return -1;

  const digits = Math.max(1, roi.digits || 1);
  for (let di = 0; di < digits; di++) {
    const dx = roiDigitXPos(roi, digitW, di);
    const dy = roi.y;
    const dw = digitW;
    const dh = roi.h;
    for (let i = 0; i < prof.segs.length; i++) {
      const s = prof.segs[i];
      const sx = (dx + dw * s[0]) * z;
      const sy = (dy + dh * s[1]) * z;
      const sw = Math.max(4, dw * s[2] * z);
      const sh = Math.max(4, dh * s[3] * z);
      if (px >= sx && px <= sx + sw && py >= sy && py <= sy + sh) return i;
    }
  }
  return -1;
}

function beginDrag(ev){
  if (!state.config) return;

  const p = canvasPointFromEvent(ev);

  // Segment-Auswahl per Klick im Preview (nur wenn Segment-Overlay und Editor offen)
  const segHit = hitTestSegment(p.x, p.y);
  if (segHit >= 0) {
    state.selectedSegIdx = (state.selectedSegIdx === segHit) ? -1 : segHit;
    const rows = document.getElementById('sp_segs_body')?.querySelectorAll('tr');
    if (rows) {
      rows.forEach((r, ri) => { r.style.background = ri === state.selectedSegIdx ? '#2a3a55' : ''; });
      if (state.selectedSegIdx >= 0 && rows[state.selectedSegIdx]) {
        rows[state.selectedSegIdx].scrollIntoView({ block: 'nearest' });
      }
    }
    draw();
    ev.preventDefault();
    return;
  }

  let roi = selectedRoi();

  if (roi) {
    const handle = detectHandle(roi, p.x, p.y);
    if (handle) {
      state.drag.active = true;
      state.drag.mode = handle;
      state.drag.roiId = roi.id;
      state.drag.startMouseX = p.x;
      state.drag.startMouseY = p.y;
      state.drag.startX = roi.x;
      state.drag.startY = roi.y;
      state.drag.startW = roi.w;
      state.drag.startH = roi.h;
      ev.preventDefault();
      return;
    }
  }

  roi = hitTestRoi(p.x, p.y);
  if (!roi) {
    // Leere Fläche → Pan starten
    const wrap = document.getElementById('canvasWrap');
    state.pan.active = true;
    state.pan.startClientX = ev.clientX;
    state.pan.startClientY = ev.clientY;
    state.pan.startScrollLeft = wrap.scrollLeft;
    state.pan.startScrollTop  = wrap.scrollTop;
    document.getElementById('overlay').style.cursor = 'grabbing';
    ev.preventDefault();
    return;
  }

  state.selected = roi.id;
  fillForm(roi);
  renderList();
  draw();
  setSectionExpanded('roi_content','roi_btn', true);

  state.drag.active = true;
  state.drag.mode = 'move';
  state.drag.roiId = roi.id;
  state.drag.startMouseX = p.x;
  state.drag.startMouseY = p.y;
  state.drag.startX = roi.x;
  state.drag.startY = roi.y;
  state.drag.startW = roi.w;
  state.drag.startH = roi.h;

  ev.preventDefault();
}

function moveDrag(ev){
  const canvas = document.getElementById('overlay');

  if (state.pan.active) {
    const wrap = document.getElementById('canvasWrap');
    wrap.scrollLeft = state.pan.startScrollLeft - (ev.clientX - state.pan.startClientX);
    wrap.scrollTop  = state.pan.startScrollTop  - (ev.clientY - state.pan.startClientY);
    ev.preventDefault();
    return;
  }

  const p = canvasPointFromEvent(ev);

  if (!state.drag.active) {
    const roi = selectedRoi();
    if (!roi) {
      canvas.style.cursor = 'grab';
      return;
    }

    const handle = detectHandle(roi, p.x, p.y);
    if (handle === 'nw' || handle === 'se') canvas.style.cursor = 'nwse-resize';
    else if (handle === 'ne' || handle === 'sw') canvas.style.cursor = 'nesw-resize';
    else {
      const hit = hitTestRoi(p.x, p.y);
      canvas.style.cursor = hit ? 'move' : 'grab';
    }
    return;
  }

  const roi = selectedRoi();
  if (!roi || roi.id !== state.drag.roiId) return;

  const dx = (p.x - state.drag.startMouseX) / state.zoom;
  const dy = (p.y - state.drag.startMouseY) / state.zoom;

  if (state.drag.mode === 'move') {
    roi.x = clampInt(state.drag.startX + dx, 0, state.config.camera.width - 1);
    roi.y = clampInt(state.drag.startY + dy, 0, state.config.camera.height - 1);
  } else {
    let x = state.drag.startX;
    let y = state.drag.startY;
    let w = state.drag.startW;
    let h = state.drag.startH;

    if (state.drag.mode === 'se') {
      w = state.drag.startW + dx;
      h = state.drag.startH + dy;
    } else if (state.drag.mode === 'nw') {
      x = state.drag.startX + dx;
      y = state.drag.startY + dy;
      w = state.drag.startW - dx;
      h = state.drag.startH - dy;
    } else if (state.drag.mode === 'ne') {
      y = state.drag.startY + dy;
      w = state.drag.startW + dx;
      h = state.drag.startH - dy;
    } else if (state.drag.mode === 'sw') {
      x = state.drag.startX + dx;
      w = state.drag.startW - dx;
      h = state.drag.startH + dy;
    }

    w = Math.max(1, w);
    h = Math.max(1, h);

    x = clampInt(x, 0, state.config.camera.width - 1);
    y = clampInt(y, 0, state.config.camera.height - 1);

    if (x + w > state.config.camera.width) w = state.config.camera.width - x;
    if (y + h > state.config.camera.height) h = state.config.camera.height - y;

    roi.x = Math.round(x);
    roi.y = Math.round(y);
    roi.w = Math.max(1, Math.round(w));
    roi.h = Math.max(1, Math.round(h));
  }

  updateFormFromSelected();
  renderList();
  draw();
  ev.preventDefault();
}

function endDrag(){
  if (state.pan.active) {
    state.pan.active = false;
    document.getElementById('overlay').style.cursor = 'grab';
  }
  state.drag.active = false;
  state.drag.mode = null;
  state.drag.roiId = null;
}

function drawHandle(ctx, x, y, color){
  ctx.fillStyle = color;
  ctx.fillRect(x - 4, y - 4, 8, 8);
  ctx.strokeStyle = '#000';
  ctx.lineWidth = 1;
  ctx.strokeRect(x - 4, y - 4, 8, 8);
}

function draw(){
  const img = document.getElementById('snap');
  const cv = document.getElementById('overlay');
  const ctx = cv.getContext('2d');

  ctx.clearRect(0, 0, cv.width, cv.height);

  if (!img.naturalWidth || !state.config) return;

  const showSelectedOnly = document.getElementById('show_selected_only')?.checked;
  const hasSelection = !!state.selected;

  state.config.rois.forEach(roi => {
    if (showSelectedOnly && hasSelection && roi.id !== state.selected) return;

    const x = roi.x * state.zoom;
    const y = roi.y * state.zoom;
    const w = roi.w * state.zoom;
    const h = roi.h * state.zoom;

    const selected = roi.id === state.selected;
    ctx.lineWidth = selected ? 3 : 2;
    ctx.strokeStyle = roi.type === 'symbol' ? '#00d0ff' : '#8cff4f';
    ctx.strokeRect(x, y, w, h);

    ctx.fillStyle = 'rgba(0,0,0,0.72)';
    ctx.font = '12px sans-serif';

    const suffix = roi.type === 'sevenseg'
      ? ` (${roi.digits}d${(roi.decimal_places || 0) > 0 ? `,${roi.decimal_places}dp` : ''})`
      : '';
    const labelText = roi.id + suffix;
    const labelW = ctx.measureText(labelText).width + 8;

    ctx.fillRect(x, Math.max(0, y - 18), labelW, 16);

    ctx.fillStyle = '#fff';
    ctx.fillText(labelText, x + 4, Math.max(12, y - 5));

    if (roi.type === 'sevenseg') {
      const digits = roi.digits || 2;
      const digitW = roiDigitWidth(roi);

      for (let i = 0; i < digits; i++) {
        const dxRaw = roiDigitXPos(roi, digitW, i);
        const dwRaw = digitW;

        const dx = dxRaw * state.zoom;
        const dy = roi.y * state.zoom;
        const dw = dwRaw * state.zoom;
        const dh = roi.h * state.zoom;

        ctx.fillStyle = i % 2 === 0 ? 'rgba(255,170,0,0.10)' : 'rgba(255,255,0,0.08)';
        ctx.fillRect(dx, dy, dw, dh);

        ctx.lineWidth = 1;
        ctx.strokeStyle = '#ffaa00';
        ctx.strokeRect(dx, dy, dw, dh);

        ctx.fillStyle = 'rgba(0,0,0,0.75)';
        ctx.fillRect(dx + 2, dy + 2, 24, 14);

        ctx.fillStyle = '#ffaa00';
        ctx.font = '10px monospace';
        ctx.fillText('d' + i, dx + 5, dy + 12);
      }

      for (let i = 0; i < digits - 1; i++) {
        const thisEnd = roiDigitXPos(roi, digitW, i) + digitW;
        const nextStart = roiDigitXPos(roi, digitW, i + 1);
        const gapW = nextStart - thisEnd;
        if (gapW > 0) {
          const gapX = thisEnd * state.zoom;
          ctx.fillStyle = 'rgba(255,0,0,0.08)';
          ctx.fillRect(gapX, y, gapW * state.zoom, h);
          ctx.strokeStyle = 'rgba(255,80,80,0.35)';
          ctx.lineWidth = 1;
          ctx.strokeRect(gapX, y, gapW * state.zoom, h);
        }
      }
    }

    if (selected) {
      drawHandle(ctx, x, y, '#ffffff');
      drawHandle(ctx, x + w, y, '#ffffff');
      drawHandle(ctx, x, y + h, '#ffffff');
      drawHandle(ctx, x + w, y + h, '#ffffff');
    }
  });

  // segment overlay wenn Checkbox aktiv
  if (document.getElementById('show_seg_overlay')?.checked) {
    const profEditorOpen = !document.getElementById('segprof_content')?.classList.contains('hidden');
    const roi = selectedRoi();
    const name = (profEditorOpen ? document.getElementById('sp_list')?.value : null)
               || roi?.seg_profile
               || document.getElementById('sp_list')?.value;
    if (name) {
      const prof = (state.config.seg_profiles || []).find(p => p.name === name);
      if (prof) _drawSegOverlayOnCtx(ctx, prof);
    }
  }
}


function addSevenSeg(){
  const roi = {
    id:'value_' + Date.now(),
    label:'Value',
    type:'sevenseg',
    x:50, y:50,
    w:180,
    h:100,
    threshold:105,
    digit_gap_px:8,
    gaps:[],
    digits:4,
    decimal_places:0,
    threshold_on:125,
    threshold_off:105,
    invert_logic:false,
    auto_threshold:false,
    stretch_contrast:false,
    confidence_margin:0,
    ha_enabled:false,
    ha_name:'',
    ha_unit:'',
    ha_icon:'',
    ha_device_class:'',
    ha_state_class:'',
    seg_profile:'standard'
  };
  state.config.rois.push(roi);
  state.selected = roi.id;
  fillForm(roi);
  renderList();
  draw();
  setSectionExpanded('roi_content','roi_btn', true);
}

function addSymbol(){
  const roi = {
    id:'symbol_' + Date.now(),
    label:'Symbol',
    type:'symbol',
    x:50, y:50,
    w:30,
    h:30,
    threshold:105,
    digit_gap_px:8,
    gaps:[],
    digits:2,
    decimal_places:0,
    threshold_on:125,
    threshold_off:105,
    invert_logic:false,
    auto_threshold:false,
    stretch_contrast:false,
    confidence_margin:0,
    ha_enabled:false,
    ha_name:'',
    ha_unit:'',
    ha_icon:'',
    ha_device_class:'',
    ha_state_class:'',
    seg_profile:'standard'
  };
  state.config.rois.push(roi);
  state.selected = roi.id;
  fillForm(roi);
  renderList();
  draw();
  setSectionExpanded('roi_content','roi_btn', true);
}

function delSelected(){
  if (!state.selected) return;
  state.config.rois = state.config.rois.filter(r => r.id !== state.selected);
  state.selected = null;
  updateSevenSegGeometryWarning();
  renderList();
  draw();
}

async function saveConfig(){
  updateSelectedFromForm();
  updatePanelsFromForm();

  setButtonBusy('btnSave', true, 'Saving...');

  try {
    const res = await api('/api/config', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify(state.config)
    });

    setOutText(JSON.stringify(res, null, 2));

    setButtonBusy('btnSave', false);
    flashButtonState('btnSave', !!res.ok, '✓ Saved', '✕ Error');

    if (res.restart) {
      appendOutText("\n\nDevice restarting...");  
    }

    await loadStatus();
  } catch (e) {
    setButtonBusy('btnSave', false);
    flashButtonState('btnSave', false);
    setOutText(String(e));
  }
}

async function exportConfig(){
  setButtonBusy('btnExport', true, 'Exporting...');
  try {
    const response = await fetch('/api/config/export');
    if (!response.ok) {
      throw new Error('Export failed: ' + response.statusText);
    }
    const blob = await response.blob();
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'display-reader-config.json';
    document.body.appendChild(a);
    a.click();
    window.URL.revokeObjectURL(url);
    document.body.removeChild(a);

    flashButtonState('btnExport', true, '✓ Exported');
    setOutText('Configuration exported successfully.');
  } catch (e) {
    flashButtonState('btnExport', false, '✕ Error');
    setOutText('Export error: ' + String(e));
  } finally {
    setButtonBusy('btnExport', false);
  }
}

function triggerImport(){
  document.getElementById('importFile').click();
}

async function importConfig(){
  const fileInput = document.getElementById('importFile');
  const file = fileInput.files[0];

  if (!file) return;

  setButtonBusy('btnImport', true, 'Importing...');

  try {
    const text = await file.text();

    const res = await api('/api/config/import', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: text
    });

    setOutText(JSON.stringify(res, null, 2));

    if (res.ok) {
      flashButtonState('btnImport', true, '✓ Imported');
      appendOutText('\n\nPlease restart your device for changes to take effect.');
      setTimeout(() => location.reload(), 2000);
    } else {
      flashButtonState('btnImport', false, '✕ Error: ' + (res.error || 'Unknown error'));
    }
  } catch (e) {
    flashButtonState('btnImport', false, '✕ Error');
    setOutText('Import error: ' + String(e));
  } finally {
    setButtonBusy('btnImport', false);
    fileInput.value = '';
  }
}

async function testEval(){
  updateSelectedFromForm();
  updatePanelsFromForm();

  setButtonBusy('btnTest', true, 'Testing...');

  try {
    const res = await api('/api/evaluate', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify(state.config)
    });

    setOutText(JSON.stringify(res, null, 2));

    setButtonBusy('btnTest', false);
    flashButtonState('btnTest', true, '✓ Done');
  } catch (e) {
    setButtonBusy('btnTest', false);
    flashButtonState('btnTest', false);
    setOutText(String(e));
  }
}

[
  'f_id','f_label','f_ha_enabled','f_ha_name','f_ha_unit','f_ha_icon','f_ha_device_class','f_ha_state_class',
  'f_type','f_invert_logic','f_auto_threshold','f_stretch_contrast','f_confidence_margin','f_x','f_y','f_w','f_h','f_threshold','f_gap','f_digits','f_decimal_places','f_on','f_off',
  'cam_model','cam_frame','cam_jpeg_quality',
  'cam_auto_exposure','cam_auto_gain','cam_auto_whitebalance','cam_vflip','cam_hflip','cam_aec_value','cam_agc_gain',
  'm_image_enabled',
  'stream_enabled',
  'l_enabled','l_gpio','l_brightness','l_settle','l_capture_only','l_invert',
  'l2_enabled','l2_gpio','l2_led_count','l2_brightness','l2_color','l2_settle','l2_capture_only',
  'pc_enabled','pc_delay','pc_topic',
  'm_enabled','m_host','m_port','m_topic','m_user','m_pass','m_interval',
  'ha_prefix','d_name',
  'dbg_level',
].forEach(id => {
  document.addEventListener('change', ev => {
    if (ev.target && ev.target.id === id) {
      updateSelectedFromForm();
      updatePanelsFromForm();
    }
  });
});

function installMouseEditing(){
  const canvas = document.getElementById('overlay');
  canvas.addEventListener('mousedown', beginDrag);
  window.addEventListener('mousemove', moveDrag);
  window.addEventListener('mouseup', endDrag);
  canvas.addEventListener('mouseleave', () => {
    if (!state.drag.active && !state.pan.active) canvas.style.cursor = 'grab';
  });
}

(async function init(){
  installMouseEditing();
  await loadStatus();
  await loadConfig();
  await loadSnapshot();
  setInterval(loadStatus, 3000);

})();
</script>
</body>
</html>
)HTML";
