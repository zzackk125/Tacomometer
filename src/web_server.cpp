/*
 * File: web_server.cpp
 * Description: Web Server Implementation with Embedded Material Design UI
 * Author: zzackk125
 * License: MIT
 */

#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>
#include "imu_driver.h" // For zeroIMU
#include "ui.h" // For hideToast, triggerCalibrationUI

WebServer server(80);
bool ap_mode_active = false;
uint32_t ap_start_time = 0;
bool toast_visible = false;
uint32_t connection_start_time = 0;
bool client_connected_flag = false;

// --- HTML Content ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Tacomometer Remote</title>
  <style>
    :root {
      --primary: #FF6D00;
      --bg: #121212;
      --surface: #1E1E1E;
      --on-surface: #E0E0E0;
      --ripple: rgba(255, 109, 0, 0.3);
      --error: #CF6679;
    }
    body {
      background-color: var(--bg);
      color: var(--on-surface);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      margin: 0;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100vh;
      -webkit-tap-highlight-color: transparent;
    }
    h1 { margin-bottom: 2rem; font-weight: 300; letter-spacing: 1px; }
    .card {
      background-color: var(--surface);
      border-radius: 12px;
      padding: 24px;
      width: 80%;
      max-width: 300px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.3);
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    button {
      background-color: var(--surface);
      color: var(--primary);
      border: 1px solid var(--primary);
      border-radius: 8px;
      padding: 16px;
      font-size: 16px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 1.25px;
      cursor: pointer;
      transition: background-color 0.2s, transform 0.1s;
      outline: none;
    }
    button:active {
      background-color: var(--ripple);
      transform: scale(0.98);
    }
    button.filled {
      background-color: var(--primary);
      color: #000;
      border: none;
    }
    button.danger {
      color: var(--error);
      border-color: var(--error);
    }
    #toast {
      visibility: hidden;
      min-width: 250px;
      /* background-color: #333; */
      background-color: var(--surface);
      color: var(--on-surface);
      border: 1px solid var(--primary);
      text-align: center;
      border-radius: 4px;
      padding: 16px;
      position: fixed;
      z-index: 100;
      bottom: 30px;
      font-size: 14px;
      left: 50%;
      transform: translateX(-50%);
    }
    #toast.show {
      visibility: visible;
      animation: fadein 0.5s, fadeout 0.5s 2.5s;
    }
    @keyframes fadein { from {bottom: 0; opacity: 0;} to {bottom: 30px; opacity: 1;} }
    @keyframes fadeout { from {bottom: 30px; opacity: 1;} to {bottom: 0; opacity: 0;} }
  </style>
</head>
<body>
  <h1>Tacomometer</h1>
  
  <div class="card">
    <button class="filled" onclick="doAction('calibrate')">Calibrate</button>
    <button onclick="openSettings()">Settings</button>
    <button class="danger" onclick="doAction('disconnect')">Disconnect</button>
  </div>

  <!-- Settings Modal -->
  <div id="settingsModal" style="display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.8); z-index:10; align-items:center; justify-content:center;">
    <div class="card" style="background:var(--surface); min-width:300px;">
      <h2 style="margin-top:0;">Settings</h2>
      
      <label>UI Rotation: <span id="rotVal">0</span>°</label>
      <div style="display:flex; gap:10px; margin-bottom:15px;">
        <button class="filled" onclick="updateRotation(0)">0°</button>
        <button class="filled" onclick="updateRotation(90)">90°</button>
        <button class="filled" onclick="updateRotation(180)">180°</button>
        <button class="filled" onclick="updateRotation(270)">270°</button>
      </div>
      
      <label>Critical Angle: <span id="critVal">50</span>°</label>
      <input type="number" id="critInput" min="10" max="90" onchange="updateCritical(this.value)">
      
      <hr style="width:100%; border:0; border-top:1px solid #444; margin: 10px 0;">
      
      <button onclick="resetSettings()">Return to Default</button>
      <button class="filled" onclick="closeSettings()">Close</button>
    </div>
  </div>

  <div id="toast">Command Sent</div>

  <script>

    function doAction(action) {
      if (action === 'disconnect') {
          fetch('/disconnect', { method: 'POST' })
            .then(() => showToast("Disconnecting..."))
            .catch(() => showToast("Disconnecting..."));
          return;
      }
      
      fetch('/' + action)
        .then(response => {
          if (response.ok) showToast(action + " executed");
          else showToast("Error executing " + action);
        })
        .catch(error => showToast("Connection failed"));
    }

    function showToast(msg) {
      var x = document.getElementById("toast");
      x.innerText = msg;
      x.className = "show";
      setTimeout(function(){ x.className = x.className.replace("show", ""); }, 3000);
    }
    
    // Settings Logic
    function openSettings() {
        document.getElementById('settingsModal').style.display = 'flex';
        fetch('/get_settings')
            .then(r => r.json())
            .then(data => {
                // document.getElementById('rotSlider').value = data.rot; // Removed
                document.getElementById('rotVal').innerText = data.rot;
                document.getElementById('critInput').value = data.crit;
                document.getElementById('critVal').innerText = data.crit;
            });
    }
    
    function closeSettings() {
        document.getElementById('settingsModal').style.display = 'none';
    }
    
    var rotTimeout;
    var critTimeout;
    var lastRotVal = -1;
    
    function updateRotation(val) {
        document.getElementById('rotVal').innerText = val;
        fetch('/set_rotation?val=' + val, {method: 'POST'});
    }
    
    function updateCritical(val) {
        document.getElementById('critVal').innerText = val;
        clearTimeout(critTimeout);
        critTimeout = setTimeout(function() {
             fetch('/set_critical?val=' + val, {method: 'POST'});
        }, 100);
    }
    
    function resetSettings() {
        if(confirm("Reset all settings?")) {
            fetch('/reset_settings', {method: 'POST'})
                .then(() => {
                    // Update UI to defaults
                    // document.getElementById('rotSlider').value = 0; // Removed
                    document.getElementById('rotVal').innerText = 0;
                    document.getElementById('critInput').value = 50;
                    document.getElementById('critVal').innerText = 50;
                    showToast("Settings Reset");
                });
        }
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
    json += "\"rot\":0,"; // Rotation hardcoded 0
    json += "\"crit\":" + String(getCriticalAngle());
    json += "}";
    server.send(200, "application/json", json);
}

void handleSetRotation() {
    if (server.hasArg("val")) {
        int val = server.arg("val").toInt();
        // setUIRotation(val); // Removed
        (void)val; // Avoid unused warning
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing val");
    }
}

void handleSetCritical() {
    if (server.hasArg("val")) {
        int val = server.arg("val").toInt();
        setCriticalAngle(val);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing val");
    }
}

void handleResetSettings() {
    resetSettings();
    server.send(200, "text/plain", "OK");
}


bool should_disconnect = false;

// ... HTML Content ...

// ... Handlers ...

void startAPMode() {
    if (ap_mode_active) return;
    
    Serial.println("Starting AP Mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Tacomometer", NULL); 
    
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    
    server.on("/", handleRoot);
    server.on("/calibrate", handleCalibrate);
    // server.on("/settings", handleSettings);
    
    server.on("/get_settings", handleGetSettings);
    server.on("/set_rotation", HTTP_POST, handleSetRotation);
    server.on("/set_critical", HTTP_POST, handleSetCritical);
    server.on("/reset_settings", HTTP_POST, handleResetSettings);
    
    server.on("/disconnect", HTTP_POST, [](){
        server.send(200, "text/plain", "Bye");
        should_disconnect = true; // Trigger disconnect in main loop
    });
    
    server.onNotFound(handleNotFound);
    
    server.begin();
    ap_mode_active = true;
    ap_start_time = millis();
    toast_visible = true;
    client_connected_flag = false;
    should_disconnect = false;
}

void stopAPMode() {
    if (!ap_mode_active) return;
    
    Serial.println("Stopping AP Mode...");
    delay(100); 
    server.stop();
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
                 // If NO connection for > 45 seconds, Shutdown
                 if (millis() - ap_start_time > 45000) {
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
