#pragma once

// Self-contained control page: no external requests, since the AP has no
// internet uplink. Served for "/" and as the captive-portal landing page.
const char WEB_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Light Beacon Control</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    padding: 24px 16px 48px;
    background: #14161a;
    color: #eef0f2;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    display: flex;
    justify-content: center;
  }
  main { width: 100%; max-width: 420px; }
  h1 { font-size: 1.25rem; margin: 0 0 4px; }
  p.sub { margin: 0 0 24px; color: #9aa1ab; font-size: 0.9rem; }
  section { margin-bottom: 24px; }
  h2 { font-size: 0.8rem; text-transform: uppercase; letter-spacing: 0.06em; color: #9aa1ab; margin: 0 0 10px; }
  .row { display: flex; gap: 10px; }
  button.choice {
    flex: 1;
    padding: 14px 8px;
    border-radius: 10px;
    border: 2px solid #2a2e35;
    background: #1c1f25;
    color: #eef0f2;
    font-size: 0.95rem;
    font-weight: 600;
    cursor: pointer;
  }
  button.choice.active { border-color: #6ea8fe; background: #202840; }
  button.color-red.active { border-color: #ff5c5c; background: #3a1f1f; }
  button.color-green.active { border-color: #4ade80; background: #1c331f; }
  button.color-blue.active { border-color: #6ea8fe; background: #1b2540; }
  .slider-row { display: flex; align-items: center; gap: 14px; }
  input[type=range] { flex: 1; accent-color: #6ea8fe; }
  #brightnessValue { width: 3em; text-align: right; font-variant-numeric: tabular-nums; }
  .toggle-row { display: flex; align-items: center; justify-content: space-between; }
  .switch { position: relative; width: 50px; height: 28px; flex: none; }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider-track {
    position: absolute; inset: 0; background: #2a2e35; border-radius: 28px; cursor: pointer; transition: 0.15s;
  }
  .slider-track::before {
    content: ""; position: absolute; height: 22px; width: 22px; left: 3px; top: 3px;
    background: #eef0f2; border-radius: 50%; transition: 0.15s;
  }
  input:checked + .slider-track { background: #3f6bd6; }
  input:checked + .slider-track::before { transform: translateX(22px); }
  #status { font-size: 0.8rem; color: #6b7280; text-align: center; margin-top: 8px; }
  #tempReadout { font-size: 0.95rem; color: #cdd2d9; }
  input[type=file] { color: #9aa1ab; font-size: 0.85rem; }
  .fw-row { display: flex; flex-direction: column; gap: 10px; }
  .progress-track { background: #1c1f25; border-radius: 8px; overflow: hidden; height: 10px; margin-top: 10px; display: none; }
  .progress-track.visible { display: block; }
  .progress-bar { background: #6ea8fe; height: 100%; width: 0%; transition: width 0.1s linear; }
  #fwStatus, #hwStatus { font-size: 0.85rem; color: #9aa1ab; margin-top: 8px; }
  .field-row { display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; }
  .field-row label { font-size: 0.9rem; color: #cdd2d9; }
  .field-row input[type=number] {
    width: 90px;
    padding: 8px 10px;
    border-radius: 8px;
    border: 2px solid #2a2e35;
    background: #1c1f25;
    color: #eef0f2;
    font-size: 0.95rem;
    text-align: right;
  }
  #hwSave { margin-top: 4px; }
  .field-row select {
    padding: 8px 10px;
    border-radius: 8px;
    border: 2px solid #2a2e35;
    background: #1c1f25;
    color: #eef0f2;
    font-size: 0.95rem;
  }
  .hint { font-size: 0.8rem; color: #6b7280; margin: -4px 0 10px; }
</style>
</head>
<body>
<main>
  <h1>Light Beacon Control</h1>
  <p class="sub">Top and side never light up together &mdash; switching one on turns the other off.</p>

  <section>
    <h2>Status</h2>
    <div id="tempReadout">Internal temp: &ndash; &deg;C</div>
  </section>

  <section>
    <h2>Output</h2>
    <div class="row" id="outputRow">
      <button class="choice" data-value="off">Off</button>
      <button class="choice" data-value="top">Top</button>
      <button class="choice" data-value="side">Side</button>
    </div>
  </section>

  <section>
    <h2>Color</h2>
    <div class="row" id="colorRow">
      <button class="choice color-red" data-value="red">Red</button>
      <button class="choice color-green" data-value="green">Green</button>
      <button class="choice color-blue" data-value="blue">Blue</button>
    </div>
  </section>

  <section>
    <h2>Brightness</h2>
    <div class="slider-row">
      <input type="range" id="brightness" min="0" max="255" value="128">
      <span id="brightnessValue">128</span>
    </div>
  </section>

  <section>
    <h2>Blink (0.5 Hz)</h2>
    <div class="toggle-row">
      <span>Blink on/off once per second</span>
      <label class="switch">
        <input type="checkbox" id="blink">
        <span class="slider-track"></span>
      </label>
    </div>
  </section>

  <section>
    <h2>Hardware</h2>
    <div class="field-row">
      <label for="hwPin">Data pin (GPIO)</label>
      <input type="number" id="hwPin" min="0" max="48">
    </div>
    <div class="field-row">
      <label for="hwTopCount">Top LED count</label>
      <input type="number" id="hwTopCount" min="1" max="500">
    </div>
    <div class="field-row">
      <label for="hwSideCount">Side LED count</label>
      <input type="number" id="hwSideCount" min="1" max="500">
    </div>
    <div class="field-row">
      <label>Chain order</label>
    </div>
    <div class="row" id="orderRow">
      <button class="choice" data-value="false">Top first</button>
      <button class="choice" data-value="true">Side first</button>
    </div>
    <div class="field-row" style="margin-top: 10px;">
      <label for="hwColorOrder">Color order</label>
      <select id="hwColorOrder">
        <option value="rgb">RGB</option>
        <option value="rbg">RBG</option>
        <option value="grb">GRB</option>
        <option value="gbr">GBR</option>
        <option value="brg">BRG</option>
        <option value="bgr">BGR</option>
      </select>
    </div>
    <div class="hint">Most WS2812/WS2812B is GRB. WS2815 varies &mdash; if red/green/blue look swapped, try another order.</div>
    <button class="choice" id="hwSave">Save &amp; Apply</button>
    <div id="hwStatus">&nbsp;</div>
  </section>

  <section>
    <h2>Firmware Update</h2>
    <div class="fw-row">
      <input type="file" id="fwFile" accept=".bin">
      <button class="choice" id="fwUpload">Upload &amp; Flash</button>
    </div>
    <div class="progress-track" id="fwProgressWrap">
      <div class="progress-bar" id="fwProgressBar"></div>
    </div>
    <div id="fwStatus">&nbsp;</div>
  </section>

  <div id="status">&nbsp;</div>
</main>

<script>
const outputRow = document.getElementById('outputRow');
const colorRow = document.getElementById('colorRow');
const brightness = document.getElementById('brightness');
const brightnessValue = document.getElementById('brightnessValue');
const blink = document.getElementById('blink');
const statusEl = document.getElementById('status');
const tempReadout = document.getElementById('tempReadout');
const hwPin = document.getElementById('hwPin');
const hwTopCount = document.getElementById('hwTopCount');
const hwSideCount = document.getElementById('hwSideCount');
const orderRow = document.getElementById('orderRow');
const hwColorOrder = document.getElementById('hwColorOrder');
const hwSave = document.getElementById('hwSave');
const hwStatus = document.getElementById('hwStatus');
const fwFile = document.getElementById('fwFile');
const fwUpload = document.getElementById('fwUpload');
const fwProgressWrap = document.getElementById('fwProgressWrap');
const fwProgressBar = document.getElementById('fwProgressBar');
const fwStatus = document.getElementById('fwStatus');

let applying = false;

function setActive(row, value) {
  [...row.children].forEach(btn => btn.classList.toggle('active', btn.dataset.value === value));
}

function renderState(state) {
  setActive(outputRow, state.output);
  setActive(colorRow, state.color);
  brightness.value = state.brightness;
  brightnessValue.textContent = state.brightness;
  blink.checked = state.blink;
  if (typeof state.tempC === 'number') {
    tempReadout.textContent = 'Internal temp: ' + state.tempC.toFixed(1) + ' °C';
  }
}

function renderConfig(cfg) {
  hwPin.value = cfg.pin;
  hwTopCount.value = cfg.topCount;
  hwSideCount.value = cfg.sideCount;
  setActive(orderRow, String(cfg.sideFirst));
  hwColorOrder.value = cfg.colorOrder;
}

async function fetchState() {
  const res = await fetch('/api/state');
  renderState(await res.json());
}

async function fetchConfig() {
  const res = await fetch('/api/config');
  renderConfig(await res.json());
}

async function applyState(partial) {
  if (applying) return;
  applying = true;
  statusEl.textContent = 'Applying...';
  try {
    const res = await fetch('/api/set', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(partial)
    });
    if (!res.ok) throw new Error(await res.text());
    renderState(await res.json());
    statusEl.textContent = '';
  } catch (err) {
    statusEl.textContent = 'Failed to apply: ' + err.message;
  } finally {
    applying = false;
  }
}

outputRow.addEventListener('click', e => {
  const btn = e.target.closest('button.choice');
  if (btn) applyState({ output: btn.dataset.value });
});

colorRow.addEventListener('click', e => {
  const btn = e.target.closest('button.choice');
  if (btn) applyState({ color: btn.dataset.value });
});

brightness.addEventListener('input', () => {
  brightnessValue.textContent = brightness.value;
});
brightness.addEventListener('change', () => {
  applyState({ brightness: parseInt(brightness.value, 10) });
});

blink.addEventListener('change', () => {
  applyState({ blink: blink.checked });
});

orderRow.addEventListener('click', e => {
  const btn = e.target.closest('button.choice');
  if (btn) setActive(orderRow, btn.dataset.value);
});

hwSave.addEventListener('click', async () => {
  hwSave.disabled = true;
  hwStatus.textContent = 'Saving...';
  try {
    const orderBtn = orderRow.querySelector('button.active');
    const body = {
      pin: parseInt(hwPin.value, 10),
      topCount: parseInt(hwTopCount.value, 10),
      sideCount: parseInt(hwSideCount.value, 10),
      sideFirst: orderBtn ? orderBtn.dataset.value === 'true' : false,
      colorOrder: hwColorOrder.value
    };
    const res = await fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    if (!res.ok) throw new Error(await res.text());
    renderConfig(await res.json());
    hwStatus.textContent = 'Saved and applied.';
  } catch (err) {
    hwStatus.textContent = 'Failed to save: ' + err.message;
  } finally {
    hwSave.disabled = false;
  }
});

fwUpload.addEventListener('click', () => {
  const file = fwFile.files[0];
  if (!file) {
    fwStatus.textContent = 'Choose a .bin file first.';
    return;
  }

  const form = new FormData();
  form.append('firmware', file, file.name);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  fwUpload.disabled = true;
  fwProgressWrap.classList.add('visible');
  fwProgressBar.style.width = '0%';
  fwStatus.textContent = 'Uploading...';

  xhr.upload.addEventListener('progress', e => {
    if (e.lengthComputable) {
      fwProgressBar.style.width = Math.round((e.loaded / e.total) * 100) + '%';
    }
  });

  xhr.onload = () => {
    if (xhr.status === 200) {
      fwStatus.textContent = 'Flashed OK, device is rebooting...';
      waitForReboot();
    } else {
      fwStatus.textContent = 'Update failed: ' + xhr.responseText;
      fwUpload.disabled = false;
    }
  };
  xhr.onerror = () => {
    fwStatus.textContent = 'Upload connection lost (normal if the device already rebooted).';
    fwUpload.disabled = false;
  };
  xhr.send(form);
});

function waitForReboot() {
  let attempts = 0;
  const timer = setInterval(async () => {
    attempts++;
    try {
      await fetch('/api/state', { cache: 'no-store' });
      clearInterval(timer);
      location.reload();
    } catch (err) {
      if (attempts > 40) {
        clearInterval(timer);
        fwStatus.textContent = 'Still waiting on the device - reconnect to the Wi-Fi network and reload.';
      }
    }
  }, 1500);
}

fetchState();
fetchConfig();
setInterval(fetchState, 4000);
</script>
</body>
</html>
)rawliteral";
