// ==================== ESP8266 WIFI MODULE CODE (FIXED v3) ====================
// Updated for: 3 Voltage Sensors, 3 Current Sensors, 4 Relays
// Data format: DATA:V1,V2,V3,C1,C2,C3,R1,R2,R3,R4

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// WiFi Credentials
const char* ssid     = "YOUR_WIFI_SSID";      // Change this
const char* password = "YOUR_WIFI_PASSWORD";  // Change this

ESP8266WebServer server(80);

// Sensor Data Variables — 3 channels for voltage/current, 4 for relays
float voltage[3]      = {0, 0, 0};
float current[3]      = {0, 0, 0};
bool relayStates[4]   = {false, false, false, false};

// Data history for plots (last 50 readings, 3 channels)
const int historySize = 50;
float voltageHistory[3][historySize];
float currentHistory[3][historySize];
int historyIndex = 0;

unsigned long lastDataReceived = 0;

void setup() {
  Serial.begin(115200);
  delay(10);

  // Initialize history arrays
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < historySize; j++) {
      voltageHistory[i][j] = 0;
      currentHistory[i][j] = 0;
    }
  }

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Web server routes
  server.on("/",            handleRoot);
  server.on("/plots",       handlePlots);
  server.on("/api/data",    handleAPIData);
  server.on("/api/relay",   HTTP_POST, handleRelayControl);
  server.on("/api/history", handleHistoryData);

  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  server.handleClient();

  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    processArduinoData(data);
  }
}

void processArduinoData(String data) {
  data.trim();

  if (data.startsWith("DATA:")) {
    lastDataReceived = millis();
    String values  = data.substring(5);
    int lastIndex  = 0;
    int commaIndex = 0;

    // Parse 3 voltages
    for (int i = 0; i < 3; i++) {
      commaIndex = values.indexOf(',', lastIndex);
      voltage[i] = values.substring(lastIndex, commaIndex).toFloat();
      lastIndex  = commaIndex + 1;
    }

    // Parse 3 currents
    for (int i = 0; i < 3; i++) {
      commaIndex = values.indexOf(',', lastIndex);
      current[i] = values.substring(lastIndex, commaIndex).toFloat();
      lastIndex  = commaIndex + 1;
    }

    // Parse 4 relay states
    for (int i = 0; i < 4; i++) {
      if (i < 3) {
        commaIndex      = values.indexOf(',', lastIndex);
        relayStates[i]  = values.substring(lastIndex, commaIndex).toInt();
        lastIndex       = commaIndex + 1;
      } else {
        relayStates[i]  = values.substring(lastIndex).toInt();
      }
    }

    updateHistory();
  }
}

void updateHistory() {
  for (int i = 0; i < 3; i++) {
    voltageHistory[i][historyIndex] = voltage[i];
    currentHistory[i][historyIndex] = current[i];
  }
  historyIndex = (historyIndex + 1) % historySize;
}

// ============= API HANDLERS =============

void handleAPIData() {
  String json = "{";

  json += "\"voltage\":[";
  for (int i = 0; i < 3; i++) {
    json += String(voltage[i], 2);
    if (i < 2) json += ",";
  }

  json += "],\"current\":[";
  for (int i = 0; i < 3; i++) {
    json += String(current[i], 2);
    if (i < 2) json += ",";
  }

  json += "],\"relays\":[";
  for (int i = 0; i < 4; i++) {
    json += relayStates[i] ? "true" : "false";
    if (i < 3) json += ",";
  }

  json += "]}";
  server.send(200, "application/json", json);
}

void handleRelayControl() {
  if (server.hasArg("relay") && server.hasArg("state")) {
    String relay = server.arg("relay");
    String state = server.arg("state");
    Serial.println("RELAY:" + relay + ":" + state);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleHistoryData() {
  String json = "{\"voltage\":[";

  for (int ch = 0; ch < 3; ch++) {
    json += "[";
    for (int i = 0; i < historySize; i++) {
      int index = (historyIndex + i) % historySize;
      json += String(voltageHistory[ch][index], 2);
      if (i < historySize - 1) json += ",";
    }
    json += "]";
    if (ch < 2) json += ",";
  }

  json += "],\"current\":[";

  for (int ch = 0; ch < 3; ch++) {
    json += "[";
    for (int i = 0; i < historySize; i++) {
      int index = (historyIndex + i) % historySize;
      json += String(currentHistory[ch][index], 2);
      if (i < historySize - 1) json += ",";
    }
    json += "]";
    if (ch < 2) json += ",";
  }

  json += "]}";
  server.send(200, "application/json", json);
}

// ============= DASHBOARD PAGE =============

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Power Monitor</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }

        .container { max-width: 1400px; margin: 0 auto; }

        header { text-align: center; color: white; margin-bottom: 30px; }
        h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
        .subtitle { font-size: 1.1em; opacity: 0.9; }

        .nav-buttons { display: flex; justify-content: center; gap: 15px; margin-bottom: 30px; }
        .nav-btn {
            padding: 12px 30px; background: white; color: #667eea;
            border: none; border-radius: 25px; cursor: pointer;
            font-size: 16px; font-weight: bold; transition: all 0.3s;
            text-decoration: none; display: inline-block;
        }
        .nav-btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.3); }

        .connection-status {
            text-align: center; padding: 15px; background: white;
            border-radius: 10px; margin-bottom: 20px;
        }
        .connected    { color: #27ae60; }
        .disconnected { color: #e74c3c; }

        /* 3 sensor cards */
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }

        .card {
            background: white; border-radius: 15px; padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2); transition: transform 0.3s;
        }
        .card:hover { transform: translateY(-5px); }

        .card-header {
            display: flex; justify-content: space-between; align-items: center;
            margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid #f0f0f0;
        }
        .card-title { font-size: 1.3em; color: #333; font-weight: bold; }
        .channel-number {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white; width: 35px; height: 35px; border-radius: 50%;
            display: flex; align-items: center; justify-content: center; font-weight: bold;
        }

        .reading-row {
            display: flex; justify-content: space-between; align-items: center;
            padding: 12px; margin-bottom: 10px; background: #f8f9fa; border-radius: 8px;
        }
        .reading-label  { color: #666; font-weight: 500; }
        .reading-value  { font-size: 1.4em; font-weight: bold; color: #333; }
        .voltage-value  { color: #e74c3c; }
        .current-value  { color: #3498db; }
        .power-value    { color: #27ae60; }
        .unit           { font-size: 0.8em; color: #999; margin-left: 5px; }

        /* 4 relay controls in one card */
        .relay-card {
            background: white; border-radius: 15px; padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2); margin-bottom: 30px;
        }
        .relay-card-title {
            font-size: 1.3em; font-weight: bold; color: #333;
            margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid #f0f0f0;
        }
        .relay-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .relay-control {
            display: flex; align-items: center; justify-content: space-between;
            padding: 15px; background: #f8f9fa; border-radius: 8px;
        }
        .relay-label { font-weight: 500; color: #333; }

        .switch { position: relative; display: inline-block; width: 60px; height: 34px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider {
            position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
            background-color: #ccc; transition: .4s; border-radius: 34px;
        }
        .slider:before {
            position: absolute; content: ""; height: 26px; width: 26px;
            left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%;
        }
        input:checked + .slider { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
        input:checked + .slider:before { transform: translateX(26px); }

        .status-indicator {
            display: inline-block; width: 12px; height: 12px; border-radius: 50%;
            margin-left: 10px; animation: pulse 2s infinite;
        }
        .status-on  { background: #27ae60; }
        .status-off { background: #e74c3c; }

        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }

        @media (max-width: 768px) {
            h1 { font-size: 1.8em; }
            .grid { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
<div class="container">
    <header>
        <h1>&#9889; Smart Power Monitor</h1>
        <p class="subtitle">Real-time Voltage, Current &amp; Power Monitoring</p>
    </header>

    <div class="nav-buttons">
        <a href="/" class="nav-btn">Dashboard</a>
        <a href="/plots" class="nav-btn">&#128202; View Plots</a>
    </div>

    <div class="connection-status">
        <span id="status">&#128260; Connecting...</span>
    </div>

    <!-- 3 Sensor Cards -->
    <div class="grid" id="channelsGrid"></div>

    <!-- 4 Relay Controls -->
    <div class="relay-card">
        <div class="relay-card-title">&#128268; Relay Controls</div>
        <div class="relay-grid" id="relayGrid"></div>
    </div>
</div>

<script>
    // Build 3 sensor cards
    function createChannels() {
        const grid = document.getElementById('channelsGrid');
        for (let i = 0; i < 3; i++) {
            const card = document.createElement('div');
            card.className = 'card';
            card.innerHTML = `
                <div class="card-header">
                    <div class="card-title">Channel ${i + 1}</div>
                    <div class="channel-number">${i + 1}</div>
                </div>
                <div class="readings">
                    <div class="reading-row">
                        <span class="reading-label">&#9889; Voltage:</span>
                        <span class="reading-value voltage-value">
                            <span id="voltage${i}">0.00</span><span class="unit">V</span>
                        </span>
                    </div>
                    <div class="reading-row">
                        <span class="reading-label">&#128268; Current:</span>
                        <span class="reading-value current-value">
                            <span id="current${i}">0.00</span><span class="unit">A</span>
                        </span>
                    </div>
                    <div class="reading-row">
                        <span class="reading-label">&#128161; Power:</span>
                        <span class="reading-value power-value">
                            <span id="power${i}">0.00</span><span class="unit">W</span>
                        </span>
                    </div>
                </div>`;
            grid.appendChild(card);
        }
    }

    // Build 4 relay controls
    function createRelays() {
        const grid = document.getElementById('relayGrid');
        for (let i = 0; i < 4; i++) {
            const div = document.createElement('div');
            div.className = 'relay-control';
            div.innerHTML = `
                <span class="relay-label">
                    Relay ${i + 1}
                    <span class="status-indicator status-off" id="rstatus${i}"></span>
                </span>
                <label class="switch">
                    <input type="checkbox" id="relay${i}" onchange="toggleRelay(${i})">
                    <span class="slider"></span>
                </label>`;
            grid.appendChild(div);
        }
    }

    function updateData() {
        fetch('/api/data')
            .then(r => r.json())
            .then(data => {
                document.getElementById('status').innerHTML = '<span class="connected">&#9989; Connected</span>';

                // Update 3 sensor channels
                for (let i = 0; i < 3; i++) {
                    document.getElementById(`voltage${i}`).textContent = data.voltage[i].toFixed(2);
                    document.getElementById(`current${i}`).textContent = data.current[i].toFixed(2);
                    document.getElementById(`power${i}`).textContent   = (data.voltage[i] * data.current[i]).toFixed(2);
                }

                // Update 4 relay states
                for (let i = 0; i < 4; i++) {
                    document.getElementById(`relay${i}`).checked = data.relays[i];
                    const dot = document.getElementById(`rstatus${i}`);
                    dot.className = 'status-indicator ' + (data.relays[i] ? 'status-on' : 'status-off');
                }
            })
            .catch(() => {
                document.getElementById('status').innerHTML = '<span class="disconnected">&#10060; Disconnected</span>';
            });
    }

    function toggleRelay(channel) {
        const checkbox = document.getElementById(`relay${channel}`);
        const state    = checkbox.checked ? 'ON' : 'OFF';
        fetch('/api/relay', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `relay=${channel + 1}&state=${state}`
        }).catch(() => { checkbox.checked = !checkbox.checked; });
    }

    createChannels();
    createRelays();
    updateData();
    setInterval(updateData, 1000);
</script>
</body>
</html>
)=====";
  server.send(200, "text/html", html);
}

// ============= PLOTS PAGE =============

void handlePlots() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Power Monitor - Plots</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh; padding: 20px;
        }
        .container { max-width: 1600px; margin: 0 auto; }
        header { text-align: center; color: white; margin-bottom: 30px; }
        h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
        .nav-buttons { display: flex; justify-content: center; gap: 15px; margin-bottom: 30px; }
        .nav-btn {
            padding: 12px 30px; background: white; color: #667eea;
            border: none; border-radius: 25px; cursor: pointer;
            font-size: 16px; font-weight: bold; transition: all 0.3s; text-decoration: none;
        }
        .nav-btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.3); }
        .charts-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(700px, 1fr));
            gap: 25px;
        }
        .chart-card { background: white; border-radius: 15px; padding: 25px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }
        .chart-title { font-size: 1.5em; color: #333; margin-bottom: 20px; text-align: center; font-weight: bold; }
        .chart-container { position: relative; height: 350px; }
        @media (max-width: 768px) { .charts-grid { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
<div class="container">
    <header>
        <h1>&#128202; Real-Time Monitoring Plots</h1>
        <p>Historical Data Visualization</p>
    </header>
    <div class="nav-buttons">
        <a href="/" class="nav-btn">&#127968; Dashboard</a>
        <a href="/plots" class="nav-btn">Current Page</a>
    </div>
    <div class="charts-grid">
        <div class="chart-card">
            <div class="chart-title">&#9889; Voltage (All Channels)</div>
            <div class="chart-container"><canvas id="voltageChart"></canvas></div>
        </div>
        <div class="chart-card">
            <div class="chart-title">&#128268; Current (All Channels)</div>
            <div class="chart-container"><canvas id="currentChart"></canvas></div>
        </div>
        <div class="chart-card">
            <div class="chart-title">&#128161; Power (All Channels)</div>
            <div class="chart-container"><canvas id="powerChart"></canvas></div>
        </div>
        <div class="chart-card">
            <div class="chart-title">&#128200; Energy Consumption</div>
            <div class="chart-container"><canvas id="energyChart"></canvas></div>
        </div>
    </div>
</div>

<script>
    const maxDataPoints = 50;

    // 3 channel colors
    const channelColors = [
        'rgba(231, 76,  60,  1)',
        'rgba(52,  152, 219, 1)',
        'rgba(46,  204, 113, 1)'
    ];
    const channelColorsFaded = [
        'rgba(231, 76,  60,  0.2)',
        'rgba(52,  152, 219, 0.2)',
        'rgba(46,  204, 113, 0.2)'
    ];

    function createDataset(label, index) {
        return {
            label: label,
            data: Array(maxDataPoints).fill(0),
            borderColor:     channelColors[index],
            backgroundColor: channelColorsFaded[index],
            borderWidth: 2, fill: true, tension: 0.4,
            pointRadius: 0, pointHoverRadius: 5
        };
    }

    const commonOptions = {
        responsive: true, maintainAspectRatio: false,
        plugins: { legend: { display: true, position: 'top' } },
        scales: {
            y: { beginAtZero: true, grid: { color: 'rgba(0,0,0,0.05)' } },
            x: { grid: { color: 'rgba(0,0,0,0.05)' } }
        },
        animation: { duration: 300 }
    };

    // Build datasets for 3 channels
    function makeDatasets() {
        return [
            createDataset('Channel 1', 0),
            createDataset('Channel 2', 1),
            createDataset('Channel 3', 2)
        ];
    }

    const voltageChart = new Chart(
        document.getElementById('voltageChart').getContext('2d'), {
        type: 'line',
        data: { labels: Array(maxDataPoints).fill(''), datasets: makeDatasets() },
        options: { ...commonOptions, scales: { ...commonOptions.scales,
            y: { ...commonOptions.scales.y, title: { display: true, text: 'Voltage (V)' } }
        }}
    });

    const currentChart = new Chart(
        document.getElementById('currentChart').getContext('2d'), {
        type: 'line',
        data: { labels: Array(maxDataPoints).fill(''), datasets: makeDatasets() },
        options: { ...commonOptions, scales: { ...commonOptions.scales,
            y: { ...commonOptions.scales.y, title: { display: true, text: 'Current (A)' } }
        }}
    });

    const powerChart = new Chart(
        document.getElementById('powerChart').getContext('2d'), {
        type: 'line',
        data: { labels: Array(maxDataPoints).fill(''), datasets: makeDatasets() },
        options: { ...commonOptions, scales: { ...commonOptions.scales,
            y: { ...commonOptions.scales.y, title: { display: true, text: 'Power (W)' } }
        }}
    });

    const energyChart = new Chart(
        document.getElementById('energyChart').getContext('2d'), {
        type: 'bar',
        data: {
            labels: ['Channel 1', 'Channel 2', 'Channel 3'],
            datasets: [{ label: 'Total Energy (Wh)', data: [0, 0, 0],
                backgroundColor: channelColorsFaded, borderColor: channelColors, borderWidth: 2 }]
        },
        options: { ...commonOptions, scales: { y: { beginAtZero: true,
            title: { display: true, text: 'Energy (Wh)' } }}}
    });

    let energyAccumulator = [0, 0, 0];
    let lastUpdateTime    = Date.now();

    function updateCharts() {
        fetch('/api/data')
            .then(r => r.json())
            .then(data => {
                const now       = Date.now();
                const timeDelta = (now - lastUpdateTime) / 3600000; // hours
                lastUpdateTime  = now;

                for (let i = 0; i < 3; i++) {
                    voltageChart.data.datasets[i].data.shift();
                    voltageChart.data.datasets[i].data.push(data.voltage[i]);

                    currentChart.data.datasets[i].data.shift();
                    currentChart.data.datasets[i].data.push(data.current[i]);

                    const power = data.voltage[i] * data.current[i];
                    powerChart.data.datasets[i].data.shift();
                    powerChart.data.datasets[i].data.push(power);

                    energyAccumulator[i] += power * timeDelta;
                    energyChart.data.datasets[0].data[i] = energyAccumulator[i];
                }

                voltageChart.update('none');
                currentChart.update('none');
                powerChart.update('none');
                energyChart.update('none');
            })
            .catch(err => console.error('Fetch error:', err));
    }

    updateCharts();
    setInterval(updateCharts, 1000);
</script>
</body>
</html>
)=====";
  server.send(200, "text/html", html);
}
