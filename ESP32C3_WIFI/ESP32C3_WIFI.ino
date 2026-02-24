/*
 *      HK Source Measure Unit
 *      Designed & Developed by Hiranya Keshan
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <HardwareSerial.h>
#include <ESPmDNS.h>

#define UART_BAUD 921600
#define RX_PIN 20
#define TX_PIN 21
#define RX_BUF_SIZE 512
#define LED_PIN 8

#define DATA_PIN   6   // DS
#define CLOCK_PIN  4   // SHCP
#define LATCH_PIN  0   // STCP

const int USART_FDAC = 0x0;
const int USART_CDAC = 0x1;
const int USART_IADC = 0x2;
const int USART_VADC = 0x3;
const int USART_ISTAGE = 0x4;
const int USART_VSTAGE = 0x5;
const int USART_MADDR = 0x6;
const int USART_MDATA = 0x7;

const int CMD_MUX_POS1 = 0x0;
const int CMD_MUX_POS2 = 0x1;
const int CMD_MUX_POS3 = 0x2;
const int CMD_MUX_POS4 = 0x3;

//Unassigned
const int CMD_CODE_4 = 0x4;
const int CMD_CODE_5 = 0x5;
const int CMD_CODE_6 = 0x6;
const int CMD_CODE_7 = 0x7;
const int CMD_CODE_8 = 0x8;
const int CMD_CODE_9 = 0x9;
const int CMD_CODE_A = 0xA;

const int CMD_POSITIVE = 0xB;
const int CMD_OUTPUT_ENABLE = 0xC;
const int CMD_SAMPLE_RATE = 0xD;
const int CMD_SLEEP_MODE = 0xE;
const int CMD_RESET = 0xF;

HardwareSerial MySerial(1);

volatile uint8_t rx_buffer[RX_BUF_SIZE];
volatile uint16_t rx_head = 0, rx_tail = 0;
volatile uint16_t latest_data[8] = {0};

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
hw_timer_t *timer = NULL;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

const char* ssid = "HiranyaKeshan";
const char* password = "20000922Hkk";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HK Source Measure Unit</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
:root{--bg:#0a0e17;--card:rgba(20,25,40,0.65);--border:rgba(100,220,180,0.12);--accent:#00ffc3;--accent-glow:#00ffc380;--text:#e0fff8;--text-muted:#88d9c9}
.theme-purple{--accent:#c678ff;--accent-glow:#c678ff80;--border:rgba(198,120,255,0.2)}
.theme-blue{--accent:#00d0ff;--accent-glow:#00d0ff80;--border:rgba(0,208,255,0.2)}
.theme-red{--accent:#ff6b9d;--accent-glow:#ff6b9d80;--border:rgba(255,107,157,0.2)}
*{margin:0;padding:0;box-sizing:border-box}
html,body{min-height:100vh;font-family:'SF Pro Display',system-ui,sans-serif;color:var(--text);overflow-y:auto;overflow-x:hidden;background:var(--bg)}
body{background:radial-gradient(circle at 30% 70%,rgba(0,255,195,0.08),transparent 50%),radial-gradient(circle at 70% 30%,rgba(0,255,100,0.06),transparent 50%),var(--bg);transition:background .6s}
.container{max-width:1400px;margin:0 auto;padding:24px 16px 100px}
.logo{position:fixed;top:16px;left:16px;width:56px;height:56px;border-radius:16px;background:url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAACXBIWXMAAAsTAAALEwEAmpwYAAAFhGlUWHRYTUw6Y29tLmFkb2JlLnhtcAAAAAAAPD94cGFja2V0IGJlZ2luPSLvu78iIGlkPSJXNU0wTXBDZWhpSHpyZVN6TlRjemtjOWQiPz4gPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0a2Q9IlhNUCBDb3JlIDYuMC4wIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIiByeG1sbnM6cGhvdG9zaG9wPSJodHRwOi8vbnMuYWRvYmUuY29tL3Bob3Rvc2hvcC8xLjAvIiByeG1sbnM6eG1wPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvIiByeG1sbnM6dGlmZj0iaHR0cDovL25zLmFkb2JlLmNvbS90aWZmLzEuMC8iIHhtbG5zOmV4aWY9Imh0dHA6Ly9ucy5hZG9iZS5jb20vZXhpZi8xLjAvIj4gPGRjOnRpdGxlPkhLIFNvdXJjZSBNZWFzdXJlIFVuaXQ8L2RjOnRpdGxlPiA8L3JkZjpEZXNjcmlwdGlvbj4gPC9yZGY6UkRGPiA8L3g6eG1wbWV0YT4gPD94cGFja2V0IGVuZD0iciI/PgAAAABJRU5ErkJggg==') center/cover;box-shadow:0 8px 32px rgba(0,0,0,0.6),0 0 30px var(--accent-glow);border:2px solid rgba(255,255,255,0.1);animation:logoPulse 4s infinite;z-index:1001}
@keyframes logoPulse{0%,100%{box-shadow:0 8px 32px rgba(0,0,0,0.6),0 0 30px var(--accent-glow)}50%{box-shadow:0 12px 48px rgba(0,0,0,0.7),0 0 50px var(--accent-glow)}}

.menu-btn{position:fixed;top:16px;right:16px;width:56px;height:56px;background:rgba(0,0,0,0.4);border-radius:16px;border:1px solid var(--border);backdrop-filter:blur(12px);cursor:pointer;z-index:1001;display:flex;align-items:center;justify-content:center;transition:.3s}
.menu-btn:hover{background:var(--accent);transform:scale(1.1)}
.menu-btn span,.menu-btn span::before,.menu-btn span::after{content:'';position:absolute;width:26px;height:3px;background:var(--text);border-radius:2px;transition:.4s}
.menu-btn span::before{top:-9px}
.menu-btn span::after{top:9px}
.menu-btn.open span{background:transparent}
.menu-btn.open span::before{transform:translateY(9px) rotate(45deg)}
.menu-btn.open span::after{transform:translateY(-9px) rotate(-45deg)}

.sidebar{position:fixed;top:0;right:-320px;width=300px;height:100vh;background:rgba(10,14,23,0.98);backdrop-filter:blur(20px);border-left:1px solid var(--border);padding:100px 24px 40px;transition:right .5s cubic-bezier(.2,.8,.4,1);z-index:1000;box-shadow:-20px 0 60px rgba(0,0,0,0.7)}
.sidebar.open{right:0}
.sidebar h2{font-size:1.4rem;margin-bottom:32px;color:var(--accent);text-align:center}
.menu-item{padding:16px 20px;margin:8px 0;border-radius:14px;background:rgba(255,255,255,0.04);cursor:pointer;transition:.3s;border:1px solid transparent}
.menu-item:hover,.menu-item.active{background:var(--accent);color:#000;border-color:var(--accent)}

.title{text-align:center;margin:80px 0 40px}
.title h1{font-size:2.8rem;font-weight:900;background:linear-gradient(90deg,var(--accent),#00d4ff);-webkit-background-clip:text;color:transparent;text-shadow:0 0 40px var(--accent-glow);animation:shimmer 8s infinite}
@keyframes shimmer{0%,100%{background-position:0%}50%{background-position:100%}}

.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:24px}
.card{background:var(--card);border-radius:20px;padding:20px;border:1px solid var(--border);backdrop-filter:blur(16px);box-shadow:0 8px 32px rgba(0,0,0,0.4);transition:all .4s;position:relative;overflow:hidden}
.card:hover{transform:translateY(-12px);box-shadow:0 24px 60px rgba(0,0,0,0.6),0 0 40px var(--accent-glow);border-color:var(--accent-glow)}
.card::before{content:'';position:absolute;top:0;left:-100%;width:100%;height:100%;background:linear-gradient(90deg,transparent,rgba(255,255,255,0.06),transparent);transition:.8s}
.card:hover::before{left:100%}
.card h3{font-size:0.95rem;color:var(--text-muted);margin-bottom:12px;letter-spacing:0.8px;text-transform:uppercase}
.gauge{height:120px;position:relative;margin:12px 0;border-radius:16px;overflow:hidden;background:#0d1726;box-shadow:inset 0 4px 16px rgba(0,0,0,0.6)}
.val{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);font-size:1.8rem;font-weight:800;text-shadow:0 4px 20px #000}
.chart-container{height:160px;margin-top:12px}
canvas{border-radius:12px}
.btn{margin-top:auto;width:100%;padding:12px;border:1px solid var(--border);border-radius:14px;background:linear-gradient(135deg,rgba(255,255,255,0.1),rgba(255,255,255,0.02));color:var(--accent);font-weight:700;cursor:pointer;transition:.3s}
.btn:hover{background:var(--accent);color:#000;box-shadow:0 8px 30px var(--accent-glow)}

.toggle-card{grid-column:1/-1;display:flex;flex-direction:column;align-items:center;gap:16px;padding:32px}
.slider{position:relative;width:80px;height:44px;background:#1a2338;border-radius:50px;cursor:pointer;box-shadow:inset 0 4px 12px rgba(0,0,0,0.6);transition:.4s}
.slider::before{content:'';position:absolute;width:36px;height:36px;left:4px;top:4px;background:linear-gradient(135deg,#fff,#e0e8ff);border-radius:50%;box-shadow:0 4px 16px rgba(0,0,0,0.5);transition:.4s}
#ledToggle:checked + .slider{background:var(--accent);box-shadow:0 0 30px var(--accent-glow)}
#ledToggle:checked + .slider::before{transform:translateX(36px)}

#polarityToggle:checked + .slider{background:var(--accent);box-shadow:0 0 30px var(--accent-glow)}
#polarityToggle:checked + .slider::before{transform:translateX(36px)}

.status{position:fixed;top:20px;right:80px;padding:10px 20px;background:rgba(0,0,0,0.5);border-radius:16px;border:1px solid var(--border);backdrop-filter:blur(12px);font-weight:600;z-index:1000}
.status.connected{background:rgba(0,255,150,0.12);border-color:var(--accent);color:var(--accent)}

#loadingScreen{position:fixed;inset:0;background:var(--bg);display:flex;flex-direction:column;justify-content:center;align-items:center;z-index:9999}
.loader{width:80px;height:80px;background:conic-gradient(var(--accent) 0%,transparent 30%);border-radius:50%;animation:spin 1.4s linear infinite}
.loader::before{content:'';position:absolute;inset:8px;background:var(--bg);border-radius:50%}
@keyframes spin{to{transform:rotate(360deg)}}

.numberpad{position:fixed;inset:0;background:rgba(0,10,30,0.94);display:none;align-items:center;justify-content:center;z-index:1000;backdrop-filter:blur(12px)}
.numpad{background:linear-gradient(135deg,rgba(15,25,50,0.95),rgba(10,15,35,0.95));padding:32px 24px;border-radius:28px;width:340px;max-width:90%;border:1px solid var(--border);box-shadow:0 40px 100px #000}
#numpadDisplay{width:100%;padding:18px 16px;margin-bottom:20px;background:rgba(0,0,0,0.4);border:none;border-radius:16px;color:var(--text);font-size:2rem;text-align:right;box-shadow:inset 0 4px 20px rgba(0,0,0,0.6)}
.numpad-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}
.numpad-btn{padding:20px;background:rgba(255,255,255,0.04);border:none;border-radius:18px;color:var(--text);font-size:1.4rem;font-weight:600;cursor:pointer;transition:.2s}
.numpad-btn:hover{background:var(--accent);color:#000}
.confirm-btn{grid-column:1/-1;background:var(--accent)!important;color:#000!important;font-weight:800}
</style>
</head><body>

<div class="logo"></div>
<div class="menu-btn" id="menuBtn"><span></span></div>

<div class="sidebar" id="sidebar">
  <h2>HK Control Panel</h2>
  <div class="menu-item active" id="connectBtn" onclick="toggleConnect()">Connect</div>
  <div class="menu-item" onclick="document.body.className=document.body.className.replace(/theme-\\w+/g,'')">Cyber Green (Default)</div>
  <div class="menu-item" onclick="document.body.classList.toggle('theme-purple')">Purple Nebula</div>
  <div class="menu-item" onclick="document.body.classList.toggle('theme-blue')">Ocean Blue</div>
  <div class="menu-item" onclick="document.body.classList.toggle('theme-red')">Sunset Red</div>
</div>

<div id="loadingScreen"><div class="loader"></div><h2 style="margin-top:24px;color:var(--accent)">HK Source Measure Unit</h2></div>

<div id="mainContent" style="display:none">
  <div class="status" id="status">Connecting...</div>
  <div class="container">
    <div class="title"><h1>HK Source Measure Unit</h1></div>
    <div class="grid">
      <div class="card"><h3>IADC</h3><div class="gauge" id="g2"><div class="val">0</div></div><div class="chart-container"><canvas id="c2"></canvas></div></div>
      <div class="card"><h3>VADC</h3><div class="gauge" id="g3"><div class="val">0</div></div><div class="chart-container"><canvas id="c3"></canvas></div></div>
      <div class="card"><h3>Istage</h3><div class="gauge" id="g4"><div class="val">0</div></div><div class="chart-container"><canvas id="c4"></canvas></div></div>
      <div class="card"><h3>Vstage</h3><div class="gauge" id="g5"><div class="val">0</div></div><div class="chart-container"><canvas id="c5"></canvas></div></div>
      <div class="card"><h3>MADDR</h3><div class="gauge" id="g6"><div class="val">0</div></div><div class="chart-container"><canvas id="c6"></canvas></div></div>
      <div class="card"><h3>MDATA</h3><div class="gauge" id="g7"><div class="val">0</div></div><div class="chart-container"><canvas id="c7"></canvas></div></div>
      <div class="card"><h3>FDAC</h3><div class="val" id="fdacValue">0</div><button class="btn" onclick="editDAC('fdac')">Set Value</button></div>
      <div class="card"><h3>CDAC</h3><div class="val" id="cdacValue">0</div><button class="btn" onclick="editDAC('cdac')">Set Value</button></div>
      <div class="card toggle-card">
        <h3>User Control</h3>
        <label class="toggle-wrapper">
          <input type="checkbox" id="ledToggle" onchange="toggleLED()">
          <span class="slider"></span>
        </label>
        <div id="ledStatus" style="margin-top:12px;font-size:1.1rem">LED: OFF</div>
        
        <div style="height:12px"></div>
        
        <label class="toggle-wrapper">
          <input type="checkbox" id="polarityToggle" onchange="togglePolarity()">
          <span class="slider"></span>
        </label>
        <div id="polarityStatus" style="margin-top:12px;font-size:1.1rem">Polarity: 0</div>
      </div>
    </div>
  </div>
</div>

<div id="numberpad" class="numberpad">
  <div class="numpad">
    <input type="text" id="numpadDisplay" readonly placeholder="0">
    <div class="numpad-grid">
      <button class="numpad-btn" onclick="appendNumber(7)">7</button><button class="numpad-btn" onclick="appendNumber(8)">8</button><button class="numpad-btn" onclick="appendNumber(9)">9</button>
      <button class="numpad-btn" onclick="appendNumber(4)">4</button><button class="numpad-btn" onclick="appendNumber(5)">5</button><button class="numpad-btn" onclick="appendNumber(6)">6</button>
      <button class="numpad-btn" onclick="appendNumber(1)">1</button><button class="numpad-btn" onclick="appendNumber(2)">2</button><button class="numpad-btn" onclick="appendNumber(3)">3</button>
      <button class="numpad-btn" onclick="backspace()">Backspace</button><button class="numpad-btn" onclick="appendNumber(0)">0</button>
      <button class="numpad-btn confirm-btn" onclick="confirmDAC()">CONFIRM</button>
    </div>
  </div>
</div>

<script>
let charts = {}, ws = null, currentDAC = '', numpadValue = '';

function hideLoading() {
  document.getElementById('loadingScreen').style.display = 'none';
  document.getElementById('mainContent').style.display = 'block';
}

// Auto-hide loading screen after 8 seconds even if WS fails
setTimeout(hideLoading, 8000);

function connectWS() {
  if (ws) ws.close();
  ws = new WebSocket("ws://" + location.hostname + "/ws");
  ws.onopen = () => {
    document.getElementById("status").textContent = "Connected";
    document.getElementById("status").classList.add("connected");
    document.getElementById("connectBtn").textContent = "Disconnect";
    document.getElementById("connectBtn").classList.add("active");
    hideLoading();
  };
  ws.onmessage = e => {
    JSON.parse(e.data).forEach(p => {
      if (p.id >= 2 && p.id <= 7) {
        const g = document.getElementById("g" + p.id);
        const deg = (p.data / 4095) * 360;
        const col = p.data < 1365 ? "var(--accent)" : p.data < 2730 ? "#ffe066" : "#ff6b9d";
        g.style.background = `conic-gradient(${col} 0deg ${deg}deg, #0d1726 ${deg}deg 360deg)`;
        g.querySelector(".val").textContent = p.data;

        charts[p.id].data.datasets[0].data.push(p.data);
        charts[p.id].data.labels.push('');
        if (charts[p.id].data.labels.length > 60) {
          charts[p.id].data.labels.shift();
          charts[p.id].data.datasets[0].data.shift();
        }

        // Dynamically set Y-axis max to 120% of the maximum value in dataset
        const maxValue = Math.max(...charts[p.id].data.datasets[0].data);
        const roundedMax = Math.ceil(maxValue * 1.2 / 100) * 100;
        charts[p.id].options.scales.y.max = roundedMax;

        charts[p.id].update('none');
      }
    });
  };
  ws.onclose = ws.onerror = () => {
    document.getElementById("status").textContent = "Disconnected";
    document.getElementById("status").classList.remove("connected");
    document.getElementById("connectBtn").textContent = "Connect";
    document.getElementById("connectBtn").classList.remove("active");
  };
}

function toggleConnect() {
  if (ws && ws.readyState === WebSocket.OPEN) ws.close();
  else connectWS();
}

function toggleLED() {
  const s = document.getElementById("ledToggle").checked;
  document.getElementById("ledStatus").textContent = "LED: " + (s ? "ON" : "OFF");
  if (ws) ws.send(JSON.stringify({led: s}));
}

function togglePolarity() {
  const s = document.getElementById("polarityToggle").checked;
  const val = s ? 1 : 0;
  document.getElementById("polarityStatus").textContent = "Polarity: " + val;
  if (ws) ws.send(JSON.stringify({polarity: val}));
}

function editDAC(t) { currentDAC = t; numpadValue = ''; document.getElementById('numpadDisplay').value = ''; document.getElementById('numberpad').style.display = 'flex'; }
function appendNumber(n) { numpadValue += n; document.getElementById('numpadDisplay').value = numpadValue; }
function backspace() { numpadValue = numpadValue.slice(0, -1); document.getElementById('numpadDisplay').value = numpadValue; }
function confirmDAC() {
  if (!numpadValue) return;
  const v = parseInt(numpadValue);
  if (currentDAC === 'fdac') { if (ws) ws.send(JSON.stringify({fdac: v})); document.getElementById('fdacValue').textContent = v; }
  else { if (ws) ws.send(JSON.stringify({cdac: v})); document.getElementById('cdacValue').textContent = v; }
  document.getElementById('numberpad').style.display = 'none';
}

window.onload = () => {
  [2,3,4,5,6,7].forEach(id => {
    charts[id] = new Chart(document.getElementById("c" + id), {
      type: "line",
      data: { 
        labels: [], 
        datasets: [{ 
          data: [], 
          borderColor: "rgba(0,255,195,0.7)", 
          backgroundColor: "rgba(0,255,195,0.1)", 
          tension: 0.4, 
          fill: true, 
          pointRadius: 0, 
          borderWidth: 2.5 
        }] 
      },
      options: { 
        animation: false, 
        scales: { 
          x: { display: false }, 
          y: { 
            display: true,
            beginAtZero: true,
            ticks: {
              color: "rgba(136,217,201,0.6)",
              font: { size: 11, weight: 500 },
              padding: 8,
              callback: function(value) { return value; }
            },
            grid: {
              color: "rgba(100,220,180,0.1)",
              drawBorder: false
            }
          } 
        }, 
        plugins: { legend: { display: false } }, 
        maintainAspectRatio: true,
        responsive: true
      } 
    });
  });

  // Auto-connect
  connectWS();

  // Menu toggle
  document.getElementById("menuBtn").onclick = () => {
    document.getElementById("sidebar").classList.toggle("open");
    document.getElementById("menuBtn").classList.toggle("open");
  };
};
</script>
</body></html>
)rawliteral";

void sendToCH32_12bit(uint8_t id, uint16_t data) {
  uint8_t hdr = (id & 0x07) << 4;
  MySerial.write(hdr | 0x80 | (data >> 8));           
  MySerial.write(hdr | 0x80 | ((data >> 4) & 0x0F));
  MySerial.write(hdr | (data & 0x0F));
}

void sendToCH32_4bit(uint8_t id, uint8_t data) {
  uint8_t hdr = (id & 0x07) << 4;
  MySerial.write(hdr | (data & 0x0F));
}

void IRAM_ATTR processPackets() {
  portENTER_CRITICAL_ISR(&mux);
  while ((rx_head - rx_tail + RX_BUF_SIZE) % RX_BUF_SIZE >= 1) {
    uint8_t b0 = rx_buffer[rx_tail];
    uint8_t id = (b0 >> 4) & 0x07;

    if ((rx_head - rx_tail + RX_BUF_SIZE) % RX_BUF_SIZE >= 3) {
      uint8_t b1 = rx_buffer[(rx_tail + 1) % RX_BUF_SIZE];
      uint8_t b2 = rx_buffer[(rx_tail + 2) % RX_BUF_SIZE];
      if ((b0 & 0x80) && (b1 & 0x80) && !(b2 & 0x80) && id == ((b1 >> 4) & 0x07) && id == ((b2 >> 4) & 0x07)) {
        latest_data[id] = ((b0 & 0x0F) << 8) | ((b1 & 0x0F) << 4) | (b2 & 0x0F);
        rx_tail = (rx_tail + 3) % RX_BUF_SIZE;
        continue;
      }
    }
    if (!(b0 & 0x80)) {
      latest_data[id] = b0 & 0x0F;
      rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
      continue;
    }
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
  }
  portEXIT_CRITICAL_ISR(&mux);
}

void IRAM_ATTR onTimer() { processPackets(); }

void wsTask(void *pv) {
  uint32_t last = 0;
  for (;;) {
    if (millis() - last >= 100 && ws.count() > 0) {
      String json = "[";
      for (int i = 0; i < 8; i++) {
        json += "{\"id\":" + String(i) + ",\"data\":" + String(latest_data[i]) + "}" + (i < 7 ? "," : "");
      }
      json += "]";
      ws.textAll(json);
      last = millis();
    }
    delay(20);
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;

    if (msg.indexOf("\"led\"") != -1) {
      bool state = msg.indexOf("true") != -1;
      digitalWrite(LED_PIN, !state);
      sendToCH32_4bit(USART_MDATA, state);
      sendToCH32_4bit(USART_MADDR, CMD_OUTPUT_ENABLE);
    }
    else if (msg.indexOf("\"fdac\"") != -1) {
      int val = msg.substring(msg.indexOf(":") + 1).toInt();
      sendToCH32_12bit(USART_FDAC, val);   // ID 0 → FDAC
      Serial.printf("→ FDAC = %d\n", val);
    }
    else if (msg.indexOf("\"cdac\"") != -1) {
      int val = msg.substring(msg.indexOf(":") + 1).toInt();
      sendToCH32_12bit(USART_CDAC, val);   // ID 1 → CDAC
      Serial.printf("→ CDAC = %d\n", val);
    }
    else if (msg.indexOf("\"polarity\"") != -1) {
      int val = msg.substring(msg.indexOf(":") + 1).toInt();
      val = val ? 1 : 0;
      sendToCH32_4bit(USART_MDATA, val);
      sendToCH32_4bit(USART_MADDR, CMD_POSITIVE);
      Serial.printf("→ Polarity = %d\n", val);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT)  Serial.printf("WS client connected\n");
  if (type == WS_EVT_DATA)     handleWebSocketMessage(arg, data, len);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  // Clear everything first
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0x00);
  digitalWrite(LATCH_PIN, HIGH);

  delay(100);

  // Set only Q3 HIGH (00001000)
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, 0x05);
  digitalWrite(LATCH_PIN, HIGH);

  MySerial.begin(UART_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

  timer = timerBegin(1000000);                // 1 MHz tick rate (1 µs per count)
  timerAttachInterrupt(timer, &onTimer);      // Attach ISR (no edge parameter)
  timerAlarm(timer, 4000, true, 0);           // Alarm every 4000 µs (4 ms), autoreload infinite

  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  //Serial.println("\nWiFi Connected → http://" + WiFi.localIP().toString());
  if (MDNS.begin("HKSMU")) {
    Serial.println("mDNS active → http://HKSMU.local");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();

  xTaskCreate(wsTask, "WS", 8192, NULL, 1, NULL);
}

void loop() {
  while (MySerial.available()) {
    uint16_t next = (rx_head + 1) % RX_BUF_SIZE;
    if (next != rx_tail) {
      rx_buffer[rx_head] = MySerial.read();
      rx_head = next;
    } else MySerial.read();
  }
  delay(1);
}