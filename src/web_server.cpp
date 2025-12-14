/*
 * File: web_server.cpp
 * Description: Web Server Implementation with Embedded Material Design UI
 * Author: zzackk125
 * License: MIT
 */

#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "imu_driver.h" // For zeroIMU
#include <Preferences.h> // For WiFi Timeout Logic
#include "ui.h" // For hideToast, triggerCalibrationUI, getters
#include "imu_driver.h" // For zeroIMU, smoothing getters

WebServer server(80);
// DNSServer dnsServer; Removed
bool ap_mode_active = false;
uint32_t ap_start_time = 0;
bool toast_visible = false;
uint32_t connection_start_time = 0;
bool client_connected_flag = false;

// Config
static Preferences web_prefs;
static int wifi_timeout_sec = 45; // Default 45s

// --- HTML Content ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Tacomometer</title>
  <style>
    :root {
      --primary: #FF6D00;
      --bg: #121212;
      --surface: #1E1E1E;
      --on-surface: #E0E0E0;
    }
    body {
      background-color: var(--bg);
      color: var(--on-surface);
      font-family: 'Roboto', sans-serif;
      margin: 0;
      padding: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
    }
    h1 { font-weight: 300; letter-spacing: 1px; margin-bottom: 20px; text-align: center; font-size: 24px; }
    .container { width: 100%; max-width: 480px; display: none; flex-direction: column; gap: 12px; }
    .container.active { display: flex; }
    
    .card {
      background-color: var(--surface);
      border-radius: 12px;
      padding: 16px;
      display: flex;
      flex-direction: column;
      gap: 12px;
    }
    .card-title { font-size: 13px; color: #888; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 4px; }
    
    button {
      background-color: var(--surface);
      color: var(--primary);
      border: 1px solid var(--primary);
      border-radius: 8px;
      padding: 14px;
      font-size: 16px;
      font-weight: 600;
      text-transform: uppercase;
      cursor: pointer;
      width: 100%;
    }
    button:active { transform: scale(0.98); opacity: 0.8; }
    button.filled { background-color: var(--primary); color: #000; border: none; }
    button.danger { color: #CF6679; border-color: #CF6679; }
    button.small { padding: 8px; font-size: 12px; }
    
    .row { display: flex; justify-content: space-between; align-items: center; }
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    
    input[type=range] { width: 100%; accent-color: var(--primary); }
    select { padding: 10px; background: #333; color: white; border: 1px solid #555; border-radius: 6px; width: 100%; font-size: 16px; }
    input[type=number] { padding: 10px; background: #333; color: white; border: 1px solid #555; border-radius: 6px; width: 100%; font-size: 16px; box-sizing: border-box; }
    
    /* Color Picker */
    .color-row { display: flex; gap: 12px; overflow-x: auto; padding-bottom: 8px; scrollbar-width: none; }
    .color-row::-webkit-scrollbar { display: none; }
    .color-btn { width: 48px; height: 48px; border-radius: 50%; border: 2px solid transparent; cursor: pointer; flex-shrink: 0; }
    .color-btn.selected { border-color: white; transform: scale(1.1); }
    
    /* Stats Table */
    .stat-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #333; }
    .stat-val { font-weight: bold; color: var(--primary); }
    
    #toast {
       visibility: hidden; min-width: 200px; background: #333; color: white;
       border: 1px solid var(--primary); text-align: center; border-radius: 8px; padding: 12px;
       position: fixed; bottom: 30px; left: 50%; transform: translateX(-50%); z-index: 100;
    }
    #toast.show { visibility: visible; }
  </style>
</head>
<body>
  <h1>Tacomometer</h1>
  
  <!-- HOME PAGE -->
  <div id="home" class="container active">
    <!-- Actions -->
    <div class="card">
        <span class="card-title">Live Controls</span>
         <button class="filled" onclick="doAction('calibrate')">Calibrate Zero</button>
    </div>
    
    <!-- Navigation -->
    <div class="card">
        <span class="card-title">Menu</span>
        <button onclick="nav('stats')">View Stats</button>
        <button onclick="nav('settings')">Device Settings</button>
    </div>
    
     <div class="card">
        <span class="card-title">Power</span>
        <button class="danger" onclick="doAction('disconnect')">Disconnect WiFi</button>
    </div>
  </div>

  <!-- STATS PAGE -->
  <div id="stats" class="container">
      <div class="card">
          <span class="card-title">System Status</span>
          <div class="stat-row"><span>Uptime</span><span id="st_uptime" class="stat-val">-</span></div>
          <div class="stat-row"><span>Clients</span><span id="st_clients" class="stat-val">-</span></div>
      </div>
      
      <div class="card">
          <span class="card-title">Session Max Angles</span>
          <div class="stat-row"><span>Roll (Left/Right)</span><span id="st_s_roll" class="stat-val">-</span></div>
          <div class="stat-row"><span>Pitch (Fwd/Back)</span><span id="st_s_pitch" class="stat-val">-</span></div>
      </div>
      
      <div class="card">
          <span class="card-title">All Time Max</span>
          <div class="stat-row"><span>Roll (Left/Right)</span><span id="st_at_roll" class="stat-val">-</span></div>
          <div class="stat-row"><span>Pitch (Fwd/Back)</span><span id="st_at_pitch" class="stat-val">-</span></div>
          <button class="small danger" style="margin-top:10px;" onclick="resetStats()">Reset All Time</button>
      </div>
      
      <button onclick="nav('home')">Back</button>
  </div>

  <!-- SETTINGS ROOT -->
  <div id="settings" class="container">
      <div class="card">
          <span class="card-title">Configuration</span>
          <button onclick="nav('customize')">Customize UI</button>
          <button onclick="nav('system')">System & Update</button>
      </div>
      <button onclick="nav('home')">Back</button>
  </div>

  <!-- CUSTOMIZE PAGE -->
  <div id="customize" class="container">
      <div class="card">
          <span class="card-title">Theme Color</span>
          <div class="color-row">
            <div class="color-btn" style="background:#FF6D00" onclick="setColor(0)" id="c0"></div>
            <div class="color-btn" style="background:#2196F3" onclick="setColor(1)" id="c1"></div>
            <div class="color-btn" style="background:#4CAF50" onclick="setColor(2)" id="c2"></div>
            <div class="color-btn" style="background:#F44336" onclick="setColor(3)" id="c3"></div>
            <div class="color-btn" style="background:#9C27B0" onclick="setColor(4)" id="c4"></div>
            <div class="color-btn" style="background:#FFEB3B" onclick="setColor(5)" id="c5"></div>
          </div>
      </div>

       <div class="card">
          <span class="card-title">Gauge Smoothing</span>
          <input type="range" id="smooth" min="1" max="100" oninput="updateSmoothUI(this.value)" onchange="saveSmooth(this.value)">
          <div class="row"><span style="font-size:12px">Slow</span> <span id="smooth_val" style="color:var(--primary)">100%</span> <span style="font-size:12px">Fast</span></div>
      </div>

      <div class="card">
          <span class="card-title">Critical Angles</span>
          <div class="grid-2">
            <div><label>Roll</label><input type="number" id="crit_r" onchange="saveCrit()"></div>
            <div><label>Pitch</label><input type="number" id="crit_p" onchange="saveCrit()"></div>
          </div>
      </div>
      
      <div class="card">
          <span class="card-title">Rotation</span>
          <select id="rot" onchange="setRot(this.value)">
              <option value="0">0&deg; (Portrait)</option>
              <option value="90">90&deg; (Landscape Right)</option>
              <option value="180">180&deg; (Inverted)</option>
              <option value="270">270&deg; (Landscape Left)</option>
          </select>
      </div>

      <button onclick="nav('settings')">Back</button>
  </div>

  <!-- SYSTEM PAGE -->
  <div id="system" class="container">
      <div class="card">
          <span class="card-title">Power Saving</span>
          <div class="row">
              <label>Burn-in Protection</label>
              <input type="checkbox" id="pshift" onchange="setPixelShift()">
          </div>
          <div style="margin-top:10px;">
              <label>WiFi Timeout</label>
              <select id="wto" onchange="setWifiTimeout(this.value)">
                  <option value="0">Always On</option>
                  <option value="30">30 Seconds</option>
                  <option value="45">45 Seconds</option>
                  <option value="60">1 Minute</option>
                  <option value="120">2 Minutes</option>
                  <option value="300">5 Minutes</option>
              </select>
          </div>
      </div>
      
      <div class="card">
          <span class="card-title">Firmware Update</span>
          <input type="file" id="fw_file" accept=".bin" style="display:none;" onchange="fileSelected()">
          <button class="small" onclick="document.getElementById('fw_file').click()">Select .bin File</button>
          <span id="file_name" style="text-align:center; font-size:12px; margin-top:5px;"></span>
          <button id="flash_btn" class="filled" style="display:none; margin-top:5px;" onclick="startFlash()">Flash Firmware</button>
      </div>

      <div class="card">
          <span class="card-title">Admin</span>
          <button class="danger" onclick="doAction('reboot')">Reboot Device</button>
          <button class="danger" style="margin-top:10px;" onclick="doAction('reset_settings')">Factory Reset</button>
      </div>
      
      <button onclick="nav('settings')">Back</button>
  </div>

  <div id="toast">Msg</div>

  <script>
    // Navigation
    var statsTimer = null;
    const COLORS = ['#FF6D00', '#2196F3', '#4CAF50', '#F44336', '#9C27B0', '#FFEB3B'];

    function nav(pageId) {
        if(statsTimer) { clearInterval(statsTimer); statsTimer=null; }
        
        document.querySelectorAll('.container').forEach(e => e.classList.remove('active'));
        document.getElementById(pageId).classList.add('active');
        
        if(pageId === 'stats') {
            loadStats();
            statsTimer = setInterval(loadStats, 3000);
        }
        if(pageId === 'customize' || pageId === 'system') loadSettings();
    }
    
    // Shared Utils
    function showToast(msg) {
        var x = document.getElementById("toast"); x.innerText = msg; x.className = "show";
        setTimeout(() => x.className = "", 3000);
    }
    function doAction(act) {
        if((act.includes('reboot') || act.includes('reset')) && !confirm("Are you sure?")) return;
        fetch('/'+act, {method:'POST'}).then(r => showToast(r.ok?"OK":"Error"));
    }

    // Data Loading
    function loadSettings() {
        fetch('/get_settings').then(r=>r.json()).then(d => {
            // Colors
            applyTheme(d.color);
            
            // Crit
            document.getElementById('crit_r').value = d.crit_r;
            document.getElementById('crit_p').value = d.crit_p;
            // Rot
            document.getElementById('rot').value = d.rot;
            // Smooth
            document.getElementById('smooth').value = d.smooth;
            document.getElementById('smooth_val').innerText = d.smooth + '%';
            // System
            document.getElementById('pshift').checked = (d.pshift == 1);
            document.getElementById('wto').value = d.wto;
        });
    }

    function loadStats() {
        fetch('/get_stats').then(r=>r.json()).then(d => {
            document.getElementById('st_uptime').innerText = formatTime(d.uptime);
            document.getElementById('st_clients').innerText = d.clients;
            document.getElementById('st_s_roll').innerText = d.s_rl + "° / " + d.s_rr + "°";
            document.getElementById('st_s_pitch').innerText = d.s_pf + "° / " + d.s_pb + "°";
            document.getElementById('st_at_roll').innerText = d.at_rl + "° / " + d.at_rr + "°";
            document.getElementById('st_at_pitch').innerText = d.at_pf + "° / " + d.at_pb + "°";
        });
    }
    
    function formatTime(s) {
        if(s < 60) return s + "s";
        let m = Math.floor(s/60);
        if(m < 60) return m + "m";
        let h = Math.floor(m/60);
        return h + "h " + (m%60) + "m";
    }

    // Setters
    function applyTheme(idx) {
        for(let i=0;i<6;i++) {
            let el = document.getElementById('c'+i);
            if(el) {
                el.classList.remove('selected');
                if(i == idx) el.classList.add('selected');
            }
        }
        if(COLORS[idx]) {
            document.documentElement.style.setProperty('--primary', COLORS[idx]);
        }
    }

    function setColor(idx) { 
        applyTheme(idx);
        fetch('/set_color?val='+idx, {method:'POST'}); 
    }
    
    function setRot(v) { fetch('/set_rotation?val='+v, {method:'POST'}); }
    
    function saveCrit() { 
        let r=document.getElementById('crit_r').value; 
        let p=document.getElementById('crit_p').value;
        fetch(`/set_critical?roll=${r}&pitch=${p}`, {method:'POST'});
    }
    
    function updateSmoothUI(v) {
        document.getElementById('smooth_val').innerText = v + '%';
    }
    
    function saveSmooth(v) {
        updateSmoothUI(v);
        fetch('/set_smoothing?val='+v, {method:'POST'});
    }
    function setPixelShift() {
        let v = document.getElementById('pshift').checked ? 1 : 0;
        fetch('/set_pixel_shift?val='+v, {method:'POST'});
    }
    function setWifiTimeout(v) {
        fetch('/set_wifi_timeout?val='+v, {method:'POST'});
    }
    function resetStats() {
        if(confirm("Reset All Time Stats?")) {
            fetch('/reset_stats', {method:'POST'}).then(loadStats);
        }
    }
    
    // OTA
    function fileSelected() {
        var i = document.getElementById('fw_file');
        if(i.files[0]) {
            document.getElementById('file_name').innerText = i.files[0].name;
            document.getElementById('flash_btn').style.display='block';
        }
    }
    function startFlash() {
         var f = document.getElementById('fw_file').files[0];
         if(!f || !confirm("Flash firmware?")) return;
         var fd = new FormData(); fd.append("update", f);
         showToast("Uploading...");
         var xhr = new XMLHttpRequest();
         xhr.open("POST", "/update");
         xhr.onload = function() { 
             if(xhr.status==200) { showToast("Success! Rebooting..."); setTimeout(()=>location.reload(),4000); } 
             else showToast("Failed");
         };
         xhr.send(fd);
    }
  </script>
</body>
</html>
)rawliteral";

// Use send_P for PROGMEM content to avoid RAM copy/issues
void handleRoot() {
    server.send_P(200, "text/html", index_html);
}

void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

void handleCalibrate() {
    triggerCalibrationUI(); // Use public UI function to show "CALIBRATING..."
    // zeroIMU is called by the UI loop when is_calibrating is true? 
    // UI logic: calibration_event_cb -> sets is_calibrating=true.
    // Loop: if(isCalibrating()) { zeroIMU(); }
    // So we DON'T need to call zeroIMU here directly if triggerCalibrationUI sets the flag.
    // Checking ui.cpp: triggerCalibrationUI sets is_calibrating = true.
    // Checking Tacomometer.ino: if (isCalibrating()) { zeroIMU(); }
    // So calling triggerCalibrationUI is sufficient and correct to match physical behavior.
    server.send(200, "text/plain", "OK");
}

void handleSettings() {
    server.send(200, "text/plain", "Use Web UI");
}

void handleGetSettings() {
    String json = "{";
    json += "\"rot\":" + String(getUIRotation()) + ",";
    json += "\"crit_r\":" + String(getCriticalRoll()) + ",";
    json += "\"crit_p\":" + String(getCriticalPitch()) + ",";
    json += "\"color\":" + String(getUIColor()) + ",";
    json += "\"ver\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"pshift\":" + String(getPixelShift() ? 1 : 0) + ",";
    json += "\"smooth\":" + String(getSmoothing()) + ",";
    json += "\"wto\":" + String(getWiFiTimeout());
    json += "}";
    server.send(200, "application/json", json);
}

void handleSetRotation() {
    if (server.hasArg("val")) {
        int val = server.arg("val").toInt();
        setUIRotation(val);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing val");
    }
}

void handleSetCritical() {
    bool updated = false;
    if (server.hasArg("roll")) {
        int r = server.arg("roll").toInt();
        setCriticalValues(r, getCriticalPitch());
        updated = true;
    }
    if (server.hasArg("pitch")) {
        int p = server.arg("pitch").toInt();
        setCriticalValues(getCriticalRoll(), p);
        updated = true;
    }
    
    if (updated) server.send(200, "text/plain", "OK");
    else server.send(400, "text/plain", "Missing roll or pitch");
}

void handleSetColor() {
    if (server.hasArg("val")) {
        int val = server.arg("val").toInt();
        setUIColor(val);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing val");
    }
}


void handleSetPixelShift() {
    if (server.hasArg("val")) {
        int val = server.arg("val").toInt();
        setPixelShift(val > 0);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing val");
    }
}

void handleSetSmoothing() {
    if (server.hasArg("val")) {
        int val = server.arg("val").toInt();
        setSmoothing(val);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing val");
    }
}

void handleSetWiFiTimeout() {
    if (server.hasArg("val")) {
        int val = server.arg("val").toInt();
        setWiFiTimeout(val);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing val");
    }
}

void handleResetSettings() {
    resetSettings();
    server.send(200, "text/plain", "OK");
}

void handleResetStats() {
    resetAllTimeStats();
    server.send(200, "text/plain", "OK");
}

void handleGetStats() {
    // Construct JSON
    String json = "{";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    
    // RSSI (Only valid if stations > 0, but we can try)
    // ESP32 AP doesn't easily give RSSI of clients without digging into low level.
    // We'll skip RSSI for now or just show station count.
    json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
    
    // Max Angles (All Time)
    float atl, atr, apf, apb;
    getAllTimeMax(&atl, &atr, &apf, &apb);
    json += "\"at_rl\":" + String((int)abs(atl)) + ",";
    json += "\"at_rr\":" + String((int)abs(atr)) + ",";
    json += "\"at_pf\":" + String((int)abs(apf)) + ",";
    json += "\"at_pb\":" + String((int)abs(apb)) + ",";
    
    // Max Angles (Session)
    float sl, sr, sf, sb;
    getSessionMax(&sl, &sr, &sf, &sb);
    json += "\"s_rl\":" + String((int)abs(sl)) + ",";
    json += "\"s_rr\":" + String((int)abs(sr)) + ",";
    json += "\"s_pf\":" + String((int)abs(sf)) + ",";
    json += "\"s_pb\":" + String((int)abs(sb));
    
    json += "}";
    server.send(200, "application/json", json);
}


bool should_disconnect = false;

// ... HTML Content ...

// ... Handlers ...

void startAPMode() {
    if (ap_mode_active) return;
    
    Serial.println("Starting AP Mode...");
    WiFi.mode(WIFI_AP);
    // Explicitly set IP to standard 192.168.4.1 to avoid auto-IP confusion
    WiFi.softAP("Tacomometer", NULL); 
    delay(500); // Give AP time to stabilize
    
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP); // Print to debug
    
    /* Captive Portal Removed
    if (myIP) {
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", myIP);
    }
    */

    server.on("/", handleRoot);
    
    // Captive Portal Handlers Removed
    
    server.onNotFound(handleNotFound);

    server.on("/calibrate", handleCalibrate);
    server.on("/get_settings", handleGetSettings);
    server.on("/set_rotation", HTTP_POST, handleSetRotation);
    server.on("/set_critical", HTTP_POST, handleSetCritical);
    server.on("/set_color", HTTP_POST, handleSetColor);
    server.on("/set_critical", HTTP_POST, handleSetCritical);
    server.on("/set_color", HTTP_POST, handleSetColor);
    server.on("/set_pixel_shift", HTTP_POST, handleSetPixelShift);
    server.on("/set_smoothing", HTTP_POST, handleSetSmoothing);
    server.on("/set_wifi_timeout", HTTP_POST, handleSetWiFiTimeout);
    server.on("/reset_settings", HTTP_POST, handleResetSettings);
    server.on("/reset_stats", HTTP_POST, handleResetStats);
    server.on("/get_stats", handleGetStats);
    server.on("/reboot", HTTP_POST, [](){
        server.send(200, "text/plain", "Rebooting...");
        delay(100);
        ESP.restart();
    });
    
    server.on("/disconnect", HTTP_POST, [](){
        server.send(200, "text/plain", "Bye");
        should_disconnect = true; // Trigger disconnect in main loop
    });
    
    // OTA Update Handler
    server.on("/update", HTTP_POST, []() {
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      delay(500);
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });
    
    server.begin();
    ap_mode_active = true;
    ap_start_time = millis();
    toast_visible = true;
    client_connected_flag = false;
    should_disconnect = false;
    
    // Load Timeout
    web_prefs.begin("web", false);
    wifi_timeout_sec = web_prefs.getInt("to", 45);
    Serial.printf("AP Started. Timeout: %ds\n", wifi_timeout_sec);
}

void stopAPMode() {
    if (!ap_mode_active) return;
    
    Serial.println("Stopping AP Mode...");
    delay(100); 
    Serial.println("Stopping AP Mode...");
    delay(100); 
    server.stop();
    // dnsServer.stop(); Removed
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    ap_mode_active = false;
    
    hideToast();
    toast_visible = false;
}

void initWebServer() {
    // Standby
}

void createWebServer() {
    // Alias if needed
}

void handleWebServer() {
    if (ap_mode_active) {
        // dnsServer.processNextRequest(); Removed
        server.handleClient();
        
        if (should_disconnect) {
            delay(100); // Allow response to flush
            stopAPMode();
            should_disconnect = false;
            return;
        }
        
        // Auto-Shutdown / Hide Toast Logic
        if (toast_visible) {
             int stations = WiFi.softAPgetStationNum();
             
             if (stations > 0) {
                 if (!client_connected_flag) {
                     client_connected_flag = true;
                     connection_start_time = millis();
                 }
                 
                 // If connected for > 10 seconds, hide toast
                 if (millis() - connection_start_time > 10000) {
                     hideToast();
                     toast_visible = false;
                 }
             } else {
                 client_connected_flag = false;
                 // If NO connection for > Timeout, Shutdown
                 // Timeout 0 means Disabled? Let's say 0 is Always On.
                 if (wifi_timeout_sec > 0 && (millis() - ap_start_time > (wifi_timeout_sec * 1000))) {
                     stopAPMode();
                 }
                 
                 // Note: We don't hide toast here unless we timeout.
                 // So if user disconnects (stations=0), toast stays visible until 45s timeout.
                 // This seems acceptable behavior "stay up... to allow user to see IP".
             }
        } else { 
            // Toast is hidden, but check if we need to shutdown due to inactivity?
            // "shutdown... if someone does not connect... for 45s".
            // If toast is gone, it means we either connected (and hid it) or timed out.
            // If we connected and then disconnected, should we shutdown?
            // Let's assume the 45s rule is mainly for the initial connection window.
            // Once hidden, we assume usage.
            // BUT, if we are active, we might want a general inactivity timeout?
            // For now, let's stick to the initial 45s rule.
        }
    }
}

bool isAPMode() {
    return ap_mode_active;
}

void setWiFiTimeout(int seconds) {
    if (seconds < 0) seconds = 0; // 0 = Always On
    wifi_timeout_sec = seconds;
    web_prefs.putInt("to", wifi_timeout_sec);
}

int getWiFiTimeout() {
    return wifi_timeout_sec;
}
