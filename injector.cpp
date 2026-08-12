#define USE_NIMBLE
#include <WiFi.h>
#include <WebServer.h>
#include <BleKeyboard.h>

const char* AP_SSID = "Input_Controller";
const char* AP_PASS = "12345678"; 

WebServer server(80);
BleKeyboard bleKeyboard("ESP32 Injector", "CustomCorp", 100);

String payload = "";
int payloadIndex = 0;
bool isInjecting = false;
bool atLineStart = true; // Tracks if we are at the beginning of a new line

int targetWPM = 60;
int jitterPercent = 20;
bool usePauses = false;
int pauseIntervalMs = 2000; 
int pauseChance = 5; 
unsigned long nextTypeTime = 0;

const char* htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Advanced BLE Input Controller</title>
    <style>
        :root { --bg: #121212; --card: #1e1e1e; --text: #e0e0e0; --accent: #00d2ff; --danger: #ff5252; --warn: #ffaa00; }
        body { font-family: system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; justify-content: center; }
        .container { background: var(--card); max-width: 600px; width: 100%; padding: 20px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); }
        h2 { margin-top: 0; color: var(--accent); }
        label { display: block; margin: 15px 0 5px; font-size: 0.9em; font-weight: bold; }
        textarea { width: 100%; height: 150px; background: #2c2c2c; color: var(--text); border: 1px solid #444; border-radius: 8px; padding: 10px; font-family: monospace; resize: vertical; box-sizing: border-box; }
        .row { display: flex; gap: 15px; align-items: center; }
        .row > div { flex: 1; }
        input[type="range"] { width: 100%; accent-color: var(--accent); }
        .slider-container { margin-bottom: 10px; background: #2a2a2a; padding: 10px 15px; border-radius: 8px; }
        .val-display { color: var(--warn); font-size: 1.1em; }
        .button-row { display: flex; gap: 10px; margin-top: 20px; }
        button { flex: 1; border: none; padding: 15px; font-size: 1.1em; font-weight: bold; border-radius: 8px; cursor: pointer; transition: opacity 0.2s; color: #000; }
        button:active { opacity: 0.8; }
        #injectBtn { background: var(--accent); }
        #cancelBtn { background: var(--danger); color: white; max-width: 120px; }
        .toggle-group { display: flex; align-items: center; gap: 10px; margin-top: 15px; margin-bottom: 10px; }
        .status { margin-bottom: 15px; font-size: 0.9em; color: #aaa; }
    </style>
</head>
<body>
    <div class="container">
        <h2>⌨️ Linear Human Injector</h2>
        <div class="status">Connect Target PC to "ESP32 Injector" via Bluetooth.</div>
        <textarea id="textPayload" placeholder="Paste formatted flush-left code here..."></textarea>
        <div class="toggle-group">
            <input type="checkbox" id="liveMode">
            <label for="liveMode" style="margin:0;">Live Mode (Type as you go)</label>
        </div>
        <div class="slider-container">
            <label>Speed: <span id="wpmVal" class="val-display">60</span> WPM</label>
            <input type="range" id="wpmSlider" min="0" max="100" value="45">
        </div>
        <div class="slider-container">
            <label>Jitter: <span id="jitterVal" class="val-display">20</span>%</label>
            <input type="range" id="jitterSlider" min="0" max="100" value="10">
        </div>
        <div class="button-row">
            <button id="injectBtn" onclick="sendAction('/inject')">INJECT CODE</button>
            <button id="cancelBtn" onclick="sendCancel()">CANCEL</button>
        </div>
    </div>
    <script>
        const textArea = document.getElementById('textPayload');
        const liveMode = document.getElementById('liveMode');
        function calculateWPM(val) { return 10 + (val / 100) * 110; }
        function calculateJitter(val) {
            if (val <= 50) return val * 2;
            else return 100 * Math.exp(0.046051 * (val - 50));
        }
        document.getElementById('wpmSlider').addEventListener('input', (e) => {
            document.getElementById('wpmVal').innerText = Math.round(calculateWPM(e.target.value));
        });
        document.getElementById('jitterSlider').addEventListener('input', (e) => {
            document.getElementById('jitterVal').innerText = Math.round(calculateJitter(e.target.value));
        });
        function sendAction(endpoint) {
            if(liveMode.checked) { alert("Disable Live Mode to inject full payload."); return; }
            const text = textArea.value;
            if(!text) return alert("Please enter some text first.");
            const wpm = Math.round(calculateWPM(document.getElementById('wpmSlider').value));
            const jitter = Math.round(calculateJitter(document.getElementById('jitterSlider').value));
            fetch(`${endpoint}?wpm=${wpm}&jitter=${jitter}`, {
                method: 'POST',
                headers: { 'Content-Type': 'text/plain' },
                body: text
            }).then(res => {
                if(res.ok) alert(`Injection Started! Hands off the keyboard.`);
            }).catch(err => alert("Network Error."));
        }
        function sendCancel() {
            fetch('/cancel', { method: 'POST' })
            .then(res => { if(res.ok) alert("Action Cancelled!"); });
        }
    </script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", htmlPage); }

void handleInject() {
    if (server.hasArg("plain")) {
        payload = server.arg("plain");
        targetWPM = server.hasArg("wpm") ? server.arg("wpm").toInt() : 60;
        jitterPercent = server.hasArg("jitter") ? server.arg("jitter").toInt() : 20;
        
        payloadIndex = 0;
        atLineStart = true;
        isInjecting = true;
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad Request");
    }
}

void handleCancel() {
    isInjecting = false;
    payloadIndex = 0;
    server.send(200, "text/plain", "Cancelled");
}

void setup() {
    Serial.begin(115200);
    bleKeyboard.begin();
    WiFi.softAP(AP_SSID, AP_PASS);
    server.on("/", HTTP_GET, handleRoot);
    server.on("/inject", HTTP_POST, handleInject);
    server.on("/cancel", HTTP_POST, handleCancel);
    server.begin();
}

void loop() {
    server.handleClient();

    if (bleKeyboard.isConnected() && isInjecting) {
        if (millis() >= nextTypeTime) {
            char c = payload[payloadIndex];
            long delayMs = 80;

            // --- SMART LINE START LOGIC ---
            if (atLineStart) {
                // 1. Eat any leading spaces or tabs to prevent bad copy-pastes
                if (c == ' ' || c == '\t' || c == '\r') {
                    payloadIndex++;
                    return; // Cycle loop instantly
                }
                
                // 2. If the line starts with a closing bracket, jump to IDE's bracket!
                if (c == '}') {
                    bleKeyboard.press(KEY_DOWN_ARROW); delay(40); bleKeyboard.releaseAll(); delay(40);
                    bleKeyboard.press(KEY_END); delay(40); bleKeyboard.releaseAll();
                    
                    payloadIndex++; // Move past the '}' in memory
                    atLineStart = false; // We handled the start
                    nextTypeTime = millis() + 60; 
                    return; // Cycle loop to type the rest of the line (like ' else {')
                }
                
                atLineStart = false; // Normal character starts the line
            }

            // --- NORMAL TYPING LOGIC ---
            if (c == '\n') {
                bleKeyboard.press(KEY_RETURN); delay(40); bleKeyboard.releaseAll();
                atLineStart = true; // Mark that next char is start of new line
                delayMs = 150; // Give IDE time to auto-indent
            } 
            else if (c != '\r') {
                bleKeyboard.press(c); delay(40); bleKeyboard.releaseAll();
                
                // Calculate human jitter
                float msPerChar = 60000.0 / (targetWPM * 5.0);
                delayMs = (long)msPerChar;
                if (jitterPercent > 0) {
                    long jitterAmount = (delayMs * jitterPercent) / 100;
                    if (jitterAmount > 1000) jitterAmount = 1000;
                    delayMs += random(-jitterAmount, jitterAmount + 1);
                }
                if (delayMs < 80) delayMs = 80;
            }

            payloadIndex++;
            
            // Finish check
            if (payloadIndex >= payload.length()) {
                isInjecting = false;
            }
            
            nextTypeTime = millis() + (delayMs - 40);
        }
    }
}