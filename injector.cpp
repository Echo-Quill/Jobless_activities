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

// Settings variables
int targetWPM = 60;
int jitterPercent = 20;
int pauseChance = 5; 
int pauseDurationMs = 2000;
int pauseJitterPercent = 20;

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
        
        .slider-container { margin-bottom: 10px; background: #2a2a2a; padding: 10px 15px; border-radius: 8px; }
        .val-display { color: var(--warn); font-size: 1.1em; }
        input[type="range"] { width: 100%; accent-color: var(--accent); }
        
        /* New Grid for Number Boxes */
        .input-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 10px; }
        .input-box { background: #2a2a2a; padding: 10px 15px; border-radius: 8px; }
        .input-box label { margin-top: 0; font-size: 0.85em; color: var(--warn); margin-bottom: 8px; }
        .input-box input[type="number"] { width: 100%; background: #1e1e1e; color: #fff; border: 1px solid #444; padding: 8px; border-radius: 4px; font-size: 1em; box-sizing: border-box; font-family: monospace; }
        
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
        
        <div class="input-grid">
            <div class="input-box">
                <label>Typing Jitter (%) [0-1000]</label>
                <input type="number" id="jitterVal" min="0" max="1000" value="20">
            </div>
            <div class="input-box">
                <label>Pause Freq (% chance)</label>
                <input type="number" id="pauseFreq" min="0" max="100" value="5">
            </div>
            <div class="input-box">
                <label>Pause Duration (ms)</label>
                <input type="number" id="pauseDur" min="0" value="2000">
            </div>
            <div class="input-box">
                <label>Pause Jitter (+/- %)</label>
                <input type="number" id="pauseJit" min="0" max="100" value="20">
            </div>
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
        
        document.getElementById('wpmSlider').addEventListener('input', (e) => {
            document.getElementById('wpmVal').innerText = Math.round(calculateWPM(e.target.value));
        });

        function sendAction(endpoint) {
            if(liveMode.checked) { alert("Disable Live Mode to inject full payload."); return; }
            const text = textArea.value;
            if(!text) return alert("Please enter some text first.");
            
            const wpm = Math.round(calculateWPM(document.getElementById('wpmSlider').value));
            
            // Get values from the new number boxes
            const jitter = parseInt(document.getElementById('jitterVal').value) || 0;
            const pFreq = parseInt(document.getElementById('pauseFreq').value) || 0;
            const pDur = parseInt(document.getElementById('pauseDur').value) || 0;
            const pJit = parseInt(document.getElementById('pauseJit').value) || 0;

            const url = `${endpoint}?wpm=${wpm}&jitter=${jitter}&pfreq=${pFreq}&pdur=${pDur}&pjit=${pJit}`;

            fetch(url, {
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
        pauseChance = server.hasArg("pfreq") ? server.arg("pfreq").toInt() : 5;
        pauseDurationMs = server.hasArg("pdur") ? server.arg("pdur").toInt() : 2000;
        pauseJitterPercent = server.hasArg("pjit") ? server.arg("pjit").toInt() : 20;
        
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
                // Eat any leading spaces or tabs to prevent bad copy-pastes
                if (c == ' ' || c == '\t' || c == '\r') {
                    payloadIndex++;
                    return; 
                }
                
                // If the line starts with a closing bracket, jump to IDE's bracket!
                if (c == '}') {
                    bleKeyboard.press(KEY_DOWN_ARROW); delay(40); bleKeyboard.releaseAll(); delay(40);
                    bleKeyboard.press(KEY_END); delay(40); bleKeyboard.releaseAll();
                    
                    payloadIndex++; 
                    atLineStart = false; 
                    nextTypeTime = millis() + 60; 
                    return; 
                }
                
                atLineStart = false; 
            }

            // --- NORMAL TYPING LOGIC ---
            if (c == '\n') {
                bleKeyboard.press(KEY_RETURN); delay(40); bleKeyboard.releaseAll();
                atLineStart = true; 
                delayMs = 150; 
            } 
            else if (c != '\r') {
                bleKeyboard.press(c); delay(40); bleKeyboard.releaseAll();
                
                // Calculate human jitter (Removed the 1000ms hard cap)
                float msPerChar = 60000.0 / (targetWPM * 5.0);
                delayMs = (long)msPerChar;
                
                if (jitterPercent > 0) {
                    long jitterAmount = (delayMs * jitterPercent) / 100;
                    delayMs += random(-jitterAmount, jitterAmount + 1);
                }
                if (delayMs < 80) delayMs = 80; // Safety floor
                
                // --- RANDOM PAUSE LOGIC ---
                // Pauses only trigger when typing a Space (' ') to look human
                if (c == ' ' && pauseChance > 0) {
                    if (random(0, 100) < pauseChance) {
                        long currentPauseMs = pauseDurationMs;
                        
                        // Apply Pause Jitter
                        if (pauseJitterPercent > 0) {
                            long pJitterAmt = (currentPauseMs * pauseJitterPercent) / 100;
                            currentPauseMs += random(-pJitterAmt, pJitterAmt + 1);
                        }
                        
                        if (currentPauseMs > 0) {
                            delayMs += currentPauseMs;
                        }
                    }
                }
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