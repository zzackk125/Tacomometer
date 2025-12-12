/*
 * File: web_server.cpp
 * Description: Web Server Implementation with Embedded Material Design UI
 * Author: zzackk125
 * License: MIT
 */

#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "imu_driver.h" // For zeroIMU
#include "ui.h" // For hideToast, triggerCalibrationUI

WebServer server(80);
DNSServer dnsServer;
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
  <title>Tacomometer</title>
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
      font-family: 'Roboto', sans-serif;
      margin: 0;
      padding: 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
    }
    h1 { font-weight: 300; letter-spacing: 1px; margin-bottom: 24px; text-align: center; }
    .container {
      width: 100%;
      max-width: 480px;
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    .card {
      background-color: var(--surface);
      border-radius: 12px;
      padding: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.3);
      display: flex;
      flex-direction: column;
      gap: 12px;
    }
    .card-title { font-size: 14px; color: #888; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
    
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
      transition: all 0.2s;
    }
    button:active { transform: scale(0.98); background: var(--ripple); }
    button.filled { background-color: var(--primary); color: #000; border: none; }
    button.danger { color: var(--error); border-color: var(--error); }
    button.danger:active { background: rgba(207, 102, 121, 0.2); }
    
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .grid-4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
    
    input {
      background: #2C2C2C; border: 1px solid #444; color: white;
      padding: 12px; border-radius: 6px; font-size: 16px; width: 100%; box-sizing: border-box;
    }
    
    /* Color Picker */
    .color-row { display: flex; justify-content: space-between; margin-top: 10px; }
    .color-btn { width: 40px; height: 40px; border-radius: 50%; border: 2px solid transparent; cursor: pointer; }
    .color-btn.selected { border-color: white; transform: scale(1.1); }
    
    .modal {
        display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%;
        background: rgba(0,0,0,0.8); z-index: 100; align-items: center; justify-content: center;
        padding: 20px; box-sizing: border-box;
    }
    
    #toast {
       visibility: hidden; min-width: 250px; background-color: var(--surface); color: white;
       border: 1px solid var(--primary); text-align: center; border-radius: 8px; padding: 16px;
       position: fixed; z-index: 200; bottom: 30px; left: 50%; transform: translateX(-50%);
       box-shadow: 0 4px 12px rgba(0,0,0,0.5);
    }
    #toast.show { visibility: visible; animation: fadein 0.5s, fadeout 0.5s 2.5s; }
    @keyframes fadein { from {bottom: 0; opacity: 0;} to {bottom: 30px; opacity: 1;} }
    @keyframes fadeout { from {bottom: 30px; opacity: 1;} to {bottom: 0; opacity: 0;} }
  /* Responsive fixes */
    @media (max-width: 360px) {
      body { padding: 10px; }
      .container { width: 100%; }
      .card { padding: 16px; }
      h1 { margin-bottom: 16px; font-size: 24px; }
      button { padding: 12px; font-size: 14px; }
      .grid-4 { grid-template-columns: repeat(2, 1fr); } 
    }
  </style>
</head>
<body>
  <h1>Tacomometer</h1>
  
  <div id="mainPage" class="container">
    <div class="card">
        <span class="card-title">Actions</span>
        <button class="filled" onclick="doAction('calibrate')">Calibrate Zero</button>
        <button onclick="alert('Stats Feature Coming Soon!')">View Stats</button>
        <button onclick="showSettings()">Device Settings</button>
    </div>
    
    <div class="card">
        <span class="card-title">System</span>
        <button class="danger" onclick="doAction('reboot')">Reboot Device</button>
        <button class="danger" onclick="doAction('disconnect')">Disconnect WiFi</button>
    </div>
  </div>

  <!-- Settings Page (Hidden by default, same container style) -->
  <div id="settingsPage" class="container" style="display:none;">
    <div class="card">
      <h2 style="margin-top:0; border-bottom: 1px solid #333; padding-bottom: 10px;">Settings</h2>
      
      <span class="card-title">Screen Rotation</span>
      <div class="grid-4">
        <button id="rot0" onclick="setRot(0)">0&deg;</button>
        <button id="rot90" onclick="setRot(90)">90&deg;</button>
        <button id="rot180" onclick="setRot(180)">180&deg;</button>
        <button id="rot270" onclick="setRot(270)">270&deg;</button>
      </div>
      
      <span class="card-title" style="margin-top: 16px;">Critical Angles (Deg)</span>
      <div class="grid-2">
        <div>
            <label style="font-size:12px; color:#888;">Roll Limit</label>
            <input type="number" id="crit_r" onchange="saveCrit()">
        </div>
        <div>
            <label style="font-size:12px; color:#888;">Pitch Limit</label>
            <input type="number" id="crit_p" onchange="saveCrit()">
        </div>
      </div>
      <button style="margin-top:8px; padding:10px; font-size:12px;" onclick="resetCrit()">Reset to Default (50&deg;)</button>
      
      <span class="card-title" style="margin-top: 16px;">Display Theme</span>
      <div class="color-row">
        <!-- Orange, Blue, Green, Red, Purple, Yellow -->
        <div class="color-btn" style="background:#FF6D00" onclick="setColor(0)" id="c0"></div>
        <div class="color-btn" style="background:#2196F3" onclick="setColor(1)" id="c1"></div>
        <div class="color-btn" style="background:#4CAF50" onclick="setColor(2)" id="c2"></div>
        <div class="color-btn" style="background:#F44336" onclick="setColor(3)" id="c3"></div>
        <div class="color-btn" style="background:#9C27B0" onclick="setColor(4)" id="c4"></div>
        <div class="color-btn" style="background:#FFEB3B" onclick="setColor(5)" id="c5"></div>
      </div>
      
      <div style="margin-top: 24px; display:flex; gap:10px;">
        <button class="filled" style="flex:1;" onclick="showMain()">Main Menu</button>
      </div>
    </div>
  </div>



  <div id="toast">Command Sent</div>

  <script>
    // Constants
    const colors = [0,1,2,3,4,5];
    
    function doAction(action) {
      if(action === 'reboot' && !confirm("Reboot Device?")) return;
      if(action === 'disconnect') {
          // Special case for disconnect to prevent hang
          fetch('/disconnect', {method:'POST'}).then(showToast("Disconnecting..."));
          return;
      }
      
      fetch('/' + action, {method: 'POST'})
        .then(r => r.ok ? showToast("Success") : showToast("Error"))
        .catch(e => showToast("Connection Failed"));
    }

    function showToast(msg) {
      var x = document.getElementById("toast");
      x.innerText = msg;
      x.className = "show";
      setTimeout(() => x.className = x.className.replace("show", ""), 3000);
    }
    
    function showSettings() {
        document.getElementById('mainPage').style.display = 'none';
        document.getElementById('settingsPage').style.display = 'flex';
        refreshSettings();
    }
    
    function showMain() {
        document.getElementById('settingsPage').style.display = 'none';
        document.getElementById('mainPage').style.display = 'flex';
    }
    
    function refreshSettings() {
        fetch('/get_settings').then(r => r.json()).then(data => {
            // Update ROT
            [0,90,180,270].forEach(d => {
                document.getElementById('rot'+d).classList.remove('filled');
                if(data.rot == d) document.getElementById('rot'+d).classList.add('filled');
            });
            
            // Update CRIT
            document.getElementById('crit_r').value = data.crit_r;
            document.getElementById('crit_p').value = data.crit_p;
            
            // Update COLOR
            colors.forEach(c => {
                 document.getElementById('c'+c).classList.remove('selected');
                 if(data.color == c) document.getElementById('c'+c).classList.add('selected');
            });
        });
    }
    
    function setRot(deg) {
        fetch('/set_rotation?val=' + deg, {method:'POST'}).then(refreshSettings);
    }
    
    function saveCrit() {
        let r = document.getElementById('crit_r').value;
        let p = document.getElementById('crit_p').value;
        fetch(`/set_critical?roll=${r}&pitch=${p}`, {method:'POST'});
    }
    
    function resetCrit() {
        document.getElementById('crit_r').value = 50;
        document.getElementById('crit_p').value = 50;
        saveCrit();
    }
    
    function setColor(idx) {
        fetch('/set_color?val=' + idx, {method:'POST'}).then(refreshSettings);
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
    json += "\"color\":" + String(getUIColor());
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
    // Explicitly set IP to standard 192.168.4.1 to avoid auto-IP confusion
    WiFi.softAP("Tacomometer", NULL); 
    delay(500); // Give AP time to stabilize
    
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP); // Print to debug
    
    if (myIP) {
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", myIP);
    }

    server.on("/", handleRoot);
    
    // Captive Portal Redirection
    // Captive Portal: Serve Content Directly (200 OK) instead of Redirect
    // This stops "Redirect Loop" errors and works better on Android/iOS
    // Intelligent Captive Redirect
    // If request Host is NOT our IP, redirect to IP.
    // Captive Portal: Serve Content Directly (200 OK) - Per Tutorial
    // https://iotespresso.com/create-captive-portal-using-esp32/
    // We serve the index page for ANY request that isn't a specific API command.
    auto servePortal = []() {
        server.send_P(200, "text/html", index_html);
    };

    server.on("/generate_204", servePortal);
    server.on("/fwlink", servePortal);
    server.on("/hotspot-detect.html", servePortal);
    server.on("/canonical.html", servePortal);
    server.on("/success.txt", servePortal);
    server.on("/ncsi.txt", servePortal);
    
    // Wildcard - effectively acts as the 'canHandle -> true' from the tutorial
    server.onNotFound([servePortal]() {
        if (!isAPMode()) return;
        servePortal();
    });

    server.on("/calibrate", handleCalibrate);
    server.on("/get_settings", handleGetSettings);
    server.on("/set_rotation", HTTP_POST, handleSetRotation);
    server.on("/set_critical", HTTP_POST, handleSetCritical);
    server.on("/set_color", HTTP_POST, handleSetColor);
    server.on("/reset_settings", HTTP_POST, handleResetSettings);
    server.on("/reboot", HTTP_POST, [](){
        server.send(200, "text/plain", "Rebooting...");
        delay(100);
        ESP.restart();
    });
    
    server.on("/disconnect", HTTP_POST, [](){
        server.send(200, "text/plain", "Bye");
        should_disconnect = true; // Trigger disconnect in main loop
    });
    
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
    Serial.println("Stopping AP Mode...");
    delay(100); 
    server.stop();
    dnsServer.stop();
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
        dnsServer.processNextRequest();
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
