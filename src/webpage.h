#pragma once
#include <pgmspace.h>

const char WEBPAGE_HTML[] PROGMEM = R"RAWPAGE(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>LiDAR Scanner</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{
  --bg:#eef6f5;
  --panel:#ffffffcc;
  --panel-solid:#ffffff;
  --panel-border:#b7d7d4;
  --text:#153636;
  --muted:#53706e;
  --accent:#0f9d8a;
  --accent-soft:#dff5f1;
  --sweep:#2dc4a9;
  --grid:#8dc7bd;
  --grid-soft:#d6ebe7;
  --point:#0f9d8a;
  --point-glow:rgba(15,157,138,.18);
  --warn:#f28f3b;
  --warn-glow:rgba(242,143,59,.20);
  --danger:#e15554;
  --danger-glow:rgba(225,85,84,.22);
  --radar-bg:#f7fcfb;
  --shadow:0 18px 40px rgba(47,102,98,.12);
  --alert-bg:#fff1ec;
  --alert-border:#f0c0b2;
  --alert-text:#8a3b2f;
  background:
    radial-gradient(circle at top left,#ffffff 0%,#eef6f5 45%,#d9ebe8 100%);
  color:var(--text);
  font-family:Georgia,"Avenir Next",serif;
  min-height:100vh;
  display:flex;
  flex-direction:column;
  align-items:center;
  padding:18px 12px 28px;
  gap:14px;
}
body[data-theme="dark"]{
  --bg:#071517;
  --panel:#0f2427cc;
  --panel-solid:#102629;
  --panel-border:#21484d;
  --text:#d4f1eb;
  --muted:#86aca8;
  --accent:#5ae0c1;
  --accent-soft:#12343a;
  --sweep:#4fe4c4;
  --grid:#2f6e69;
  --grid-soft:#173a3f;
  --point:#61e6c8;
  --point-glow:rgba(97,230,200,.16);
  --warn:#ffb25b;
  --warn-glow:rgba(255,178,91,.20);
  --danger:#ff7575;
  --danger-glow:rgba(255,117,117,.22);
  --radar-bg:#081719;
  --shadow:0 18px 40px rgba(0,0,0,.28);
  --alert-bg:#31181c;
  --alert-border:#693138;
  --alert-text:#ffc7c7;
  background:
    radial-gradient(circle at top left,#16343a 0%,#081719 52%,#051113 100%);
}
.topbar{
  width:min(1180px,100%);
  display:flex;
  justify-content:space-between;
  align-items:center;
  gap:12px;
}
.brand{display:flex;align-items:center;gap:12px}
.brand-mark{
  width:14px;height:14px;border-radius:50%;background:var(--accent);
  box-shadow:0 0 0 7px rgba(15,157,138,.18),0 0 14px var(--accent)
}
.brand h1{font-size:1.1rem;letter-spacing:.16em;font-weight:700}
.brand p{font-size:.78rem;color:var(--muted)}
.top-actions{display:flex;gap:10px;align-items:center}
button,input,select{font:inherit}
.btn,.save-btn{
  border:none;border-radius:999px;padding:10px 14px;cursor:pointer;
  background:var(--accent);color:#fff;font-weight:700;box-shadow:var(--shadow)
}
.btn.secondary{
  background:var(--panel-solid);color:var(--text);border:1px solid var(--panel-border);box-shadow:none
}
#alertBar{
  width:min(1180px,100%);background:var(--alert-bg);border:1px solid var(--alert-border);
  border-radius:14px;padding:10px 16px;font-size:.86rem;color:var(--alert-text);
  opacity:0;transform:translateY(-6px);transition:opacity .25s,transform .25s
}
#alertBar.show{opacity:1;transform:translateY(0)}
#layout{
  width:min(1180px,100%);display:grid;grid-template-columns:minmax(320px,1fr) 330px;gap:18px;align-items:start
}
@media (max-width:980px){#layout{grid-template-columns:1fr}}
.radar-wrap,.card{
  background:var(--panel);backdrop-filter:blur(10px);border:1px solid var(--panel-border);
  border-radius:24px;box-shadow:var(--shadow)
}
.radar-wrap{padding:18px}
canvas{
  display:block;width:100%;max-width:780px;aspect-ratio:1/1;height:auto;margin:auto;
  border-radius:50%;background:var(--radar-bg)
}
#panel{display:grid;gap:12px}
.card{padding:16px}
.card-hd{font-size:.75rem;letter-spacing:.18em;color:var(--muted);margin-bottom:10px}
.row{display:flex;justify-content:space-between;gap:12px;padding:4px 0;font-size:.85rem}
.lbl{color:var(--muted)}
.val{color:var(--text);font-weight:700}
#conn{display:flex;align-items:center;gap:10px;font-size:.88rem}
.dot{width:10px;height:10px;border-radius:50%;background:#c76969;flex-shrink:0}
.dot.live{background:var(--accent);box-shadow:0 0 10px var(--accent)}
.legend{display:grid;gap:8px}
.leg{display:flex;align-items:center;gap:10px;font-size:.8rem}
.legdot{width:10px;height:10px;border-radius:50%}
.settings-form{display:grid;gap:12px}
.field{display:grid;gap:6px}
.field label{font-size:.78rem;color:var(--muted)}
.field input,.field select{
  width:100%;padding:10px 12px;border-radius:12px;border:1px solid var(--panel-border);
  background:var(--panel-solid);color:var(--text)
}
.field input[type="range"]{padding:0;border:none;background:transparent;accent-color:var(--accent)}
.field-note{font-size:.73rem;color:var(--muted)}
.settings-actions{display:flex;gap:10px;align-items:center;flex-wrap:wrap}
#saveStatus{font-size:.76rem;color:var(--muted)}
</style>
</head>
<body data-theme="light">
  <div class="topbar">
    <div class="brand">
      <div class="brand-mark"></div>
      <div>
        <h1>LIDAR ROOM SCANNER</h1>
        <p>Live radar view with adjustable gearing and scan tuning</p>
      </div>
    </div>
    <div class="top-actions">
      <button class="btn secondary" id="themeBtn" type="button">Dark Mode</button>
    </div>
  </div>

  <div id="alertBar">Motion detected in multiple sectors.</div>

  <div id="layout">
    <div class="radar-wrap">
      <canvas id="cv" width="720" height="720"></canvas>
    </div>

    <div id="panel">
      <div class="card">
        <div class="card-hd">CONNECTION</div>
        <div id="conn"><div class="dot" id="dot"></div><span id="connTxt">Connecting...</span></div>
      </div>

      <div class="card">
        <div class="card-hd">SCAN STATUS</div>
        <div class="row"><span class="lbl">Rate</span><span class="val" id="sRate">- Hz</span></div>
        <div class="row"><span class="lbl">Angle</span><span class="val" id="sAngle">0.0 deg</span></div>
        <div class="row"><span class="lbl">Points</span><span class="val" id="sPts">-</span></div>
        <div class="row"><span class="lbl">Nearest</span><span class="val" id="sNear">- cm</span></div>
        <div class="row"><span class="lbl">Farthest</span><span class="val" id="sFar">- cm</span></div>
        <div class="row"><span class="lbl">Motion</span><span class="val" id="sMotion">0</span></div>
      </div>

      <div class="card">
        <div class="card-hd">SYSTEM</div>
        <div class="row"><span class="lbl">Driver</span><span class="val" id="sDriver">-</span></div>
        <div class="row"><span class="lbl">Gear</span><span class="val" id="sGear">-</span></div>
        <div class="row"><span class="lbl">Steps / Rev</span><span class="val" id="sSteps">-</span></div>
        <div class="row"><span class="lbl">Sweep Speed</span><span class="val" id="sSweep">-</span></div>
        <div class="row"><span class="lbl">Smoothing</span><span class="val" id="sSmooth">-</span></div>
        <div class="row"><span class="lbl">MQTT</span><span class="val" id="sMqtt">-</span></div>
      </div>

      <div class="card">
        <div class="card-hd">SETTINGS</div>
        <form class="settings-form" id="settingsForm">
          <div class="field">
            <label for="drivePulleyTeeth">Drive pulley teeth</label>
            <input id="drivePulleyTeeth" name="drivePulleyTeeth" type="number" min="8" max="400" step="1">
          </div>
          <div class="field">
            <label for="lidarPulleyTeeth">LiDAR pulley teeth</label>
            <input id="lidarPulleyTeeth" name="lidarPulleyTeeth" type="number" min="8" max="400" step="1">
          </div>
          <div class="field">
            <label for="microsteps">Microsteps</label>
            <input id="microsteps" name="microsteps" type="number" min="1" max="256" step="1">
          </div>
          <div class="field">
            <label for="lidarRevPerSec">LiDAR revolutions per second</label>
            <input id="lidarRevPerSec" name="lidarRevPerSec" type="number" min="0.10" max="3.00" step="0.05">
          </div>
          <div class="field">
            <label for="smoothingRadius">Smoothing radius</label>
            <select id="smoothingRadius" name="smoothingRadius">
              <option value="0">0 - raw</option>
              <option value="1">1 - light</option>
              <option value="2">2 - medium</option>
              <option value="3">3 - strong</option>
            </select>
            <div class="field-note">Higher smoothing reduces jitter but softens fine edges.</div>
          </div>
          <div class="field">
            <label><input id="mqttEnabled" name="mqttEnabled" type="checkbox" value="1"> Enable MQTT publishing</label>
          </div>
          <div class="field">
            <label for="mqttHost">MQTT broker host</label>
            <input id="mqttHost" name="mqttHost" type="text" maxlength="64" placeholder="192.168.1.10">
          </div>
          <div class="field">
            <label for="mqttPort">MQTT broker port</label>
            <input id="mqttPort" name="mqttPort" type="number" min="1" max="65535" step="1" placeholder="1883">
          </div>
          <div class="field">
            <label for="mqttBaseTopic">MQTT base topic</label>
            <input id="mqttBaseTopic" name="mqttBaseTopic" type="text" maxlength="96" placeholder="lidar/room_scanner">
            <div class="field-note">Published topics: /config, /status, /angle, /scan, /telemetry</div>
          </div>
          <div class="field">
            <label for="mqttUsername">MQTT username</label>
            <input id="mqttUsername" name="mqttUsername" type="text" maxlength="64" placeholder="optional">
          </div>
          <div class="field">
            <label for="mqttPassword">MQTT password</label>
            <input id="mqttPassword" name="mqttPassword" type="password" maxlength="64" placeholder="optional">
          </div>
          <div class="field">
            <label for="rangeSlider">Display range</label>
            <input id="rangeSlider" type="range" min="100" max="1200" step="50" value="600">
            <div class="field-note">Current display max: <span id="sMaxDist">600</span> cm</div>
          </div>
          <div class="settings-actions">
            <button class="save-btn" type="submit">Save To ESP32</button>
            <span id="saveStatus">Idle</span>
          </div>
        </form>
      </div>

      <div class="card">
        <div class="card-hd">LEGEND</div>
        <div class="legend">
          <div class="leg"><div class="legdot" style="background:var(--point)"></div><span>Static object</span></div>
          <div class="leg"><div class="legdot" style="background:var(--warn)"></div><span>Slow motion (&gt; 20 cm)</span></div>
          <div class="leg"><div class="legdot" style="background:var(--danger)"></div><span>Fast motion (&gt; 60 cm)</span></div>
        </div>
      </div>
    </div>
  </div>

<script>
(function(){
'use strict';

const cv = document.getElementById('cv');
const ctx = cv.getContext('2d');
const W = cv.width, H = cv.height, CX = W / 2, CY = H / 2, R = W / 2 - 18;
const HIST = 6, MOTION_LO = 20, MOTION_HI = 60;
const state = {
  maxDist: 600,
  serverAngle: 0,
  serverAt: performance.now() / 1000,
  sweepDegPerSec: 360,
  scanBins: 360,
  history: [],
  prevScanTs: 0
};

const els = {
  dot: document.getElementById('dot'),
  connTxt: document.getElementById('connTxt'),
  sRate: document.getElementById('sRate'),
  sAngle: document.getElementById('sAngle'),
  sPts: document.getElementById('sPts'),
  sNear: document.getElementById('sNear'),
  sFar: document.getElementById('sFar'),
  sMotion: document.getElementById('sMotion'),
  sDriver: document.getElementById('sDriver'),
  sGear: document.getElementById('sGear'),
  sSteps: document.getElementById('sSteps'),
  sSweep: document.getElementById('sSweep'),
  sSmooth: document.getElementById('sSmooth'),
  sMqtt: document.getElementById('sMqtt'),
  sMaxDist: document.getElementById('sMaxDist'),
  alertBar: document.getElementById('alertBar'),
  settingsForm: document.getElementById('settingsForm'),
  saveStatus: document.getElementById('saveStatus'),
  themeBtn: document.getElementById('themeBtn')
};

function cssVar(name) {
  return getComputedStyle(document.body).getPropertyValue(name).trim();
}

function applyTheme(theme) {
  document.body.dataset.theme = theme;
  localStorage.setItem('lidar-theme', theme);
  els.themeBtn.textContent = theme === 'dark' ? 'Light Mode' : 'Dark Mode';
}

applyTheme(localStorage.getItem('lidar-theme') || 'light');
els.themeBtn.addEventListener('click', function() {
  applyTheme(document.body.dataset.theme === 'dark' ? 'light' : 'dark');
});

function toXY(deg, cm) {
  const r = (cm / state.maxDist) * R;
  const a = (deg - 90) * Math.PI / 180;
  return [CX + r * Math.cos(a), CY + r * Math.sin(a)];
}

function sRad(deg) {
  return (deg - 90) * Math.PI / 180;
}

function drawGrid() {
  ctx.beginPath();
  ctx.arc(CX, CY, R, 0, Math.PI * 2);
  ctx.fillStyle = cssVar('--radar-bg');
  ctx.fill();

  for (let i = 1; i <= 5; i++) {
    const r = R * i / 5;
    const d = Math.round(state.maxDist * i / 5);
    ctx.beginPath();
    ctx.arc(CX, CY, r, 0, Math.PI * 2);
    ctx.strokeStyle = cssVar('--grid-soft');
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.fillStyle = cssVar('--muted');
    ctx.font = '12px Georgia';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText(d + ' cm', CX + r + 6, CY - 5);
  }

  for (let a = 0; a < 360; a += 30) {
    const rad = sRad(a);
    ctx.beginPath();
    ctx.moveTo(CX, CY);
    ctx.lineTo(CX + R * Math.cos(rad), CY + R * Math.sin(rad));
    ctx.strokeStyle = cssVar('--grid');
    ctx.globalAlpha = 0.35;
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.globalAlpha = 1;

    const lr = R + 20;
    ctx.fillStyle = cssVar('--muted');
    ctx.font = '12px Georgia';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(a + ' deg', CX + lr * Math.cos(rad), CY + lr * Math.sin(rad));
  }

  ctx.beginPath();
  ctx.arc(CX, CY, R, 0, Math.PI * 2);
  ctx.strokeStyle = cssVar('--grid');
  ctx.lineWidth = 1.5;
  ctx.stroke();
}

function drawSweep(angleDeg) {
  const tail = 50;
  const rad = sRad(angleDeg);

  ctx.beginPath();
  ctx.moveTo(CX, CY);
  ctx.arc(CX, CY, R, sRad(angleDeg - tail), rad);
  ctx.closePath();
  ctx.fillStyle = document.body.dataset.theme === 'dark' ? 'rgba(79,228,196,.08)' : 'rgba(45,196,169,.10)';
  ctx.fill();

  const ex = CX + R * Math.cos(rad);
  const ey = CY + R * Math.sin(rad);
  const g = ctx.createLinearGradient(CX, CY, ex, ey);
  g.addColorStop(0, 'rgba(0,0,0,0)');
  g.addColorStop(0.6, cssVar('--sweep'));
  g.addColorStop(1, cssVar('--accent'));
  ctx.beginPath();
  ctx.moveTo(CX, CY);
  ctx.lineTo(ex, ey);
  ctx.strokeStyle = g;
  ctx.lineWidth = 2.4;
  ctx.stroke();
}

function motionScore(dist, bin, prevHist) {
  let m = 0;
  for (const h of prevHist) {
    if (!h) continue;
    const prevDist = h[bin];
    if (!dist || !prevDist) continue;
    const delta = Math.abs(dist - prevDist);
    if (delta > m) m = delta;
  }
  return m;
}

function drawPoints(cur, prevHist) {
  for (let i = 0; i < state.scanBins; i++) {
    const d = cur[i];
    if (!d || d > state.maxDist) continue;

    const score = motionScore(d, i, prevHist);
    let color = cssVar('--point');
    let glow = cssVar('--point-glow');
    let size = 2.8;
    if (score >= MOTION_HI) {
      color = cssVar('--danger');
      glow = cssVar('--danger-glow');
      size = 4;
    } else if (score >= MOTION_LO) {
      color = cssVar('--warn');
      glow = cssVar('--warn-glow');
      size = 3.4;
    }

    const pointAngle = (i * 360) / state.scanBins;
    const xy = toXY(pointAngle, d);
    ctx.beginPath();
    ctx.arc(xy[0], xy[1], size + 5, 0, Math.PI * 2);
    ctx.fillStyle = glow;
    ctx.fill();
    ctx.beginPath();
    ctx.arc(xy[0], xy[1], size, 0, Math.PI * 2);
    ctx.fillStyle = color;
    ctx.fill();
  }
}

function drawCenter() {
  ctx.beginPath();
  ctx.arc(CX, CY, 5, 0, Math.PI * 2);
  ctx.fillStyle = cssVar('--accent');
  ctx.shadowColor = cssVar('--accent');
  ctx.shadowBlur = 14;
  ctx.fill();
  ctx.shadowBlur = 0;
}

function render(sweepAngle) {
  ctx.clearRect(0, 0, W, H);
  drawGrid();
  drawSweep(sweepAngle);
  if (state.history.length > 0) {
    const cur = state.history[state.history.length - 1];
    const prevHist = state.history.slice(0, -1).reverse();
    drawPoints(cur, prevHist);
  }
  drawCenter();
}

function updateConfig(cfg) {
  if (typeof cfg.sweepDegPerSec === 'number') state.sweepDegPerSec = cfg.sweepDegPerSec;
  if (typeof cfg.scanBins === 'number') state.scanBins = cfg.scanBins;

  els.sDriver.textContent = cfg.driver || '-';
  els.sGear.textContent = (cfg.drivePulleyTeeth || '-') + ':' + (cfg.lidarPulleyTeeth || '-');
  els.sSteps.textContent = (cfg.stepsPerLidarRev || 0) + ' / rev';
  els.sSweep.textContent = Number(cfg.lidarRevPerSec || 0).toFixed(2) + ' rev/s';
  els.sSmooth.textContent = 'radius ' + (cfg.smoothingRadius || 0);
  els.sMqtt.textContent = cfg.mqttEnabled ? ((cfg.mqttConnected ? 'connected ' : 'enabled ') + (cfg.mqttHost || '')) : 'disabled';

  if (cfg.drivePulleyTeeth != null) document.getElementById('drivePulleyTeeth').value = cfg.drivePulleyTeeth;
  if (cfg.lidarPulleyTeeth != null) document.getElementById('lidarPulleyTeeth').value = cfg.lidarPulleyTeeth;
  if (cfg.motorMicrosteps != null) document.getElementById('microsteps').value = cfg.motorMicrosteps;
  if (cfg.lidarRevPerSec != null) document.getElementById('lidarRevPerSec').value = Number(cfg.lidarRevPerSec).toFixed(2);
  if (cfg.smoothingRadius != null) document.getElementById('smoothingRadius').value = cfg.smoothingRadius;
  document.getElementById('mqttEnabled').checked = !!cfg.mqttEnabled;
  if (cfg.mqttHost != null) document.getElementById('mqttHost').value = cfg.mqttHost;
  if (cfg.mqttPort != null) document.getElementById('mqttPort').value = cfg.mqttPort;
  if (cfg.mqttBaseTopic != null) document.getElementById('mqttBaseTopic').value = cfg.mqttBaseTopic;
  if (cfg.mqttUsername != null) document.getElementById('mqttUsername').value = cfg.mqttUsername;
}

async function fetchConfig() {
  try {
    const response = await fetch('/api/config');
    if (!response.ok) throw new Error('config load failed');
    updateConfig(await response.json());
  } catch (err) {
    els.saveStatus.textContent = 'Could not load config';
  }
}

async function saveSettings(event) {
  event.preventDefault();
  els.saveStatus.textContent = 'Saving...';
  const body = new URLSearchParams(new FormData(els.settingsForm));

  try {
    const response = await fetch('/api/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: body.toString()
    });
    if (!response.ok) throw new Error('save failed');
    updateConfig(await response.json());
    state.history = [];
    els.saveStatus.textContent = 'Saved to ESP32';
  } catch (err) {
    els.saveStatus.textContent = 'Save failed';
  }
}

els.settingsForm.addEventListener('submit', saveSettings);
document.getElementById('rangeSlider').addEventListener('input', function() {
  state.maxDist = parseInt(this.value, 10);
  els.sMaxDist.textContent = state.maxDist;
});

function handleScan(scan) {
  state.scanBins = Array.isArray(scan.d) ? scan.d.length : state.scanBins;
  const now = Date.now();
  if (state.prevScanTs) {
    els.sRate.textContent = (1000 / (now - state.prevScanTs)).toFixed(2) + ' Hz';
  }
  state.prevScanTs = now;

  state.history.push(scan.d);
  if (state.history.length > HIST) state.history.shift();

  const valid = scan.d.filter(function(v){ return v > 0 && v <= 1200; });
  els.sPts.textContent = valid.length;
  if (valid.length) {
    els.sNear.textContent = Math.min.apply(null, valid) + ' cm';
    els.sFar.textContent = Math.max.apply(null, valid) + ' cm';
  }

  let motionPoints = 0;
  if (state.history.length >= 2) {
    const prev = state.history[state.history.length - 2];
    for (let i = 0; i < state.scanBins; i++) {
      if (scan.d[i] && prev[i] && Math.abs(scan.d[i] - prev[i]) >= MOTION_LO) motionPoints++;
    }
  }

  els.sMotion.textContent = motionPoints;
  if (motionPoints > 5) {
    els.alertBar.textContent = 'Motion detected: ' + motionPoints + ' sectors changed';
    els.alertBar.classList.add('show');
  } else {
    els.alertBar.classList.remove('show');
  }
}

function connect() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const socket = new WebSocket(proto + '//' + location.host + '/ws');

  socket.onopen = function() {
    els.dot.classList.add('live');
    els.connTxt.textContent = 'Connected';
  };

  socket.onclose = function() {
    els.dot.classList.remove('live');
    els.connTxt.textContent = 'Reconnecting...';
    setTimeout(connect, 2000);
  };

  socket.onmessage = function(event) {
    let msg;
    try { msg = JSON.parse(event.data); } catch (err) { return; }

    if (msg.type === 'config') {
      updateConfig(msg);
      return;
    }
    if (msg.type === 'angle') {
      state.serverAngle = msg.a;
      state.serverAt = performance.now() / 1000;
      return;
    }
    if (msg.type === 'scan') {
      handleScan(msg);
    }
  };
}

function frame(ts) {
  requestAnimationFrame(frame);
  const elapsed = (ts / 1000) - state.serverAt;
  const sweepNow = ((state.serverAngle + state.sweepDegPerSec * elapsed) % 360 + 360) % 360;
  els.sAngle.textContent = sweepNow.toFixed(1) + ' deg';
  render(sweepNow);
}

fetchConfig();
connect();
requestAnimationFrame(frame);
})();
</script>
</body>
</html>
)RAWPAGE";
