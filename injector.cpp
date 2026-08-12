#define USE_NIMBLE
#include <WiFi.h>
#include <WebServer.h>
#include <BleKeyboard.h>

// --- KEY_ESC FALLBACK ---
#ifndef KEY_ESC
#define KEY_ESC 177
#endif

const char* AP_SSID = "Input_Controller";
const char* AP_PASS = "12345678"; 

// --- HARDWARE PANIC BUTTON ---
const int PANIC_PIN = 0; 

WebServer server(80);
BleKeyboard bleKeyboard("ESP32 Injector", "CustomCorp", 100);

String payload = "";
int payloadIndex = 0;
bool isInjecting = false;
bool atLineStart = true; 

// Settings variables
int targetWPM = 60;
int jitterPercent = 20;
int pauseChance = 5; 
int pauseDurationMs = 2000;
int pauseJitterPercent = 20;
bool useSmartBrackets = true; 
bool useEscKiller = true; 

unsigned long nextTypeTime = 0;

// --- NON-BLOCKING DELAY WITH KEY SAFETY ---
void smartDelay(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        server.handleClient();
        
        // Check Panic Pin or Cancellation
        if ((digitalRead(PANIC_PIN) == LOW && isInjecting) || !isInjecting) {
            isInjecting = false;
            bleKeyboard.releaseAll(); // Prevent stuck keys
            return;
        }
        delay(1); 
    }
}

const char* htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Advanced BLE Input Controller</title>
    <style>
        :root { --bg: #121212; --card: #1e1e1e; --text: #e0e0e0; --accent: #00d2ff; --danger: #ff5252; --warn: #ffaa00; --success: #4CAF50; }
        body { font-family: system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; justify-content: center; }
        .container { background: var(--card); max-width: 600px; width: 100%; padding: 20px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); }
        h2 { margin-top: 0; color: var(--accent); }
        label { display: block; margin: 15px 0 5px; font-size: 0.9em; font-weight: bold; }
        
        textarea { width: 100%; height: 180px; background: #2c2c2c; color: var(--text); border: 1px solid #444; border-radius: 8px; padding: 10px; font-family: monospace; resize: vertical; box-sizing: border-box; }
        
        .stats-bar { display: flex; justify-content: space-between; font-size: 0.85em; color: #888; margin-top: 5px; padding: 0 5px; }
        .slider-container { margin-bottom: 10px; background: #2a2a2a; padding: 10px 15px; border-radius: 8px; margin-top: 15px; }
        .val-display { color: var(--warn); font-size: 1.1em; }
        input[type="range"] { width: 100%; accent-color: var(--accent); }
        
        .input-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 10px; }
        .input-box { background: #2a2a2a; padding: 10px 15px; border-radius: 8px; }
        .input-box label { margin-top: 0; font-size: 0.85em; color: var(--warn); margin-bottom: 8px; }
        .input-box input[type="number"] { width: 100%; background: #1e1e1e; color: #fff; border: 1px solid #444; padding: 8px; border-radius: 4px; font-size: 1em; box-sizing: border-box; font-family: monospace; }
        
        .button-row { display: flex; gap: 10px; margin-top: 20px; }
        button { flex: 1; border: none; padding: 15px; font-size: 1.1em; font-weight: bold; border-radius: 8px; cursor: pointer; transition: opacity 0.2s; color: #000; }
        button:disabled { opacity: 0.5; cursor: not-allowed; }
        button:active:not(:disabled) { opacity: 0.8; }
        
        #formatBtn { background: var(--success); color: white; margin-top: 10px; width: 100%; margin-bottom: 15px; }
        #injectBtn { background: var(--accent); }
        #cancelBtn { background: var(--danger); color: white; max-width: 120px; }
        .toggle-group { display: flex; align-items: center; gap: 10px; margin-top: 8px; }
        .status { margin-bottom: 15px; font-size: 0.9em; color: #aaa; }
        .alert-text { color: var(--warn); font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <h2>⌨️ Smart Java Injector</h2>
        <div class="status" id="connStatus">Checking hardware status...</div>
        
        <textarea id="textPayload" placeholder="Paste limitless raw Java code here..." oninput="updateStats()"></textarea>
        <div class="stats-bar">
            <span id="charCount">0 characters</span>
            <span id="estTime">Est. Time: 00:00</span>
        </div>

        <button id="formatBtn" onclick="formatCode()">1. FORMAT & PREVIEW CODE</button>
        
        <div class="toggle-group">
            <input type="checkbox" id="smartBrackets" checked onchange="saveSettings()">
            <label for="smartBrackets" style="margin:0; color: var(--accent);">IDE Smart Brackets (Down-Arrow Bypass)</label>
        </div>

        <div class="toggle-group">
            <input type="checkbox" id="escKiller" checked onchange="updateStats(); saveSettings();">
            <label for="escKiller" style="margin:0; color: var(--warn);">IDE Popup Killer (Press ESC before Enter)</label>
        </div>

        <div class="toggle-group">
            <input type="checkbox" id="soundToggle" onchange="saveSettings()">
            <label for="soundToggle" style="margin:0; color: var(--success);">Play Sound on Phone when finished</label>
        </div>
        
        <div class="slider-container">
            <label>Speed: <span id="wpmVal" class="val-display">120</span> WPM</label>
            <input type="range" id="wpmSlider" min="0" max="120" value="120" oninput="updateStats(); saveSettings();">
        </div>
        
        <div class="input-grid">
            <div class="input-box">
                <label>Typing Jitter (%)</label>
                <input type="number" id="jitterVal" min="0" max="1000" value="15" oninput="updateStats(); saveSettings();">
            </div>
            <div class="input-box">
                <label>Pause Freq (%)</label>
                <input type="number" id="pauseFreq" min="0" max="100" value="3" oninput="updateStats(); saveSettings();">
            </div>
            <div class="input-box">
                <label>Pause Duration (ms)</label>
                <input type="number" id="pauseDur" min="0" value="1500" oninput="updateStats(); saveSettings();">
            </div>
            <div class="input-box">
                <label>Pause Jitter (%)</label>
                <input type="number" id="pauseJit" min="0" max="100" value="20" oninput="saveSettings();">
            </div>
        </div>

        <div class="button-row">
            <button id="injectBtn" onclick="startInjectionSequence()" disabled>2. INJECT CODE</button>
            <button id="cancelBtn" onclick="sendCancel()">CANCEL</button>
        </div>
    </div>

    <script>
        const textArea = document.getElementById('textPayload');
        const wpmSlider = document.getElementById('wpmSlider');
        const statusText = document.getElementById('connStatus');
        
        let audioCtx; 
        
        let payloadChunks = [];
        let currentChunkIndex = 0;
        let pollingInterval = null;
        let finishTimer = null;
        const MAX_CHUNK_SIZE = 12000;
        let consecutivePollFailures = 0;
        const MAX_POLL_FAILURES = 5;

        // --- BLE HANDSHAKE SLIDER CAP LOGIC ---
        function updateSliderCap(isConnected) {
            if (isConnected) {
                wpmSlider.max = 150;
                document.getElementById('injectBtn').disabled = false;
                if (!pollingInterval) statusText.innerHTML = `<span style="color: var(--success);">BLE Handshake Verified. 150 WPM Unlocked.</span>`;
            } else {
                if (parseInt(wpmSlider.value) > 120) {
                    wpmSlider.value = 120;
                }
                wpmSlider.max = 120;
                document.getElementById('injectBtn').disabled = true;
                if (!pollingInterval) statusText.innerHTML = `<span style="color: var(--danger);">Target PC Disconnected. WPM Capped.</span>`;
            }
            document.getElementById('wpmVal').innerText = wpmSlider.value;
        }

        // Background poller to monitor BLE handshake state while idle
        setInterval(() => {
            if (!pollingInterval) {
                fetch('/status')
                .then(res => res.text())
                .then(state => {
                    if (state === "ready") updateSliderCap(true);
                    else if (state === "disconnected") updateSliderCap(false);
                }).catch(e => updateSliderCap(false));
            }
        }, 3000);

        function unlockAudio() {
            if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            if (audioCtx.state === 'suspended') audioCtx.resume();
            const buffer = audioCtx.createBuffer(1, 1, 22050);
            const source = audioCtx.createBufferSource();
            source.buffer = buffer;
            source.connect(audioCtx.destination);
            source.start();
        }

        function playFinishSound() {
            if (!document.getElementById('soundToggle').checked) return;
            try {
                if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
                const playNote = (freq, time, dur) => {
                    const osc = audioCtx.createOscillator();
                    const gain = audioCtx.createGain();
                    osc.connect(gain); gain.connect(audioCtx.destination);
                    osc.type = 'sine'; osc.frequency.value = freq;
                    gain.gain.setValueAtTime(0.1, audioCtx.currentTime + time);
                    gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + time + dur);
                    osc.start(audioCtx.currentTime + time); osc.stop(audioCtx.currentTime + time + dur);
                };
                playNote(523.25, 0.0, 0.2); 
                playNote(659.25, 0.2, 0.2); 
                playNote(783.99, 0.4, 0.4); 
            } catch(e) {}
        }

        window.onload = () => {
            const savedWPM = localStorage.getItem('wpm');
            if (savedWPM && savedWPM <= 120) wpmSlider.value = savedWPM; 
            
            ['jitterVal', 'pauseFreq', 'pauseDur', 'pauseJit'].forEach(id => {
                if (localStorage.getItem(id)) document.getElementById(id).value = localStorage.getItem(id);
            });
            if (localStorage.getItem('smartBrackets') !== null) document.getElementById('smartBrackets').checked = localStorage.getItem('smartBrackets') === 'true';
            if (localStorage.getItem('escKiller') !== null) document.getElementById('escKiller').checked = localStorage.getItem('escKiller') === 'true';
            const savedSound = localStorage.getItem('soundToggle');
            document.getElementById('soundToggle').checked = savedSound === 'true';
            document.getElementById('wpmVal').innerText = wpmSlider.value;
            updateStats();
        };

        function saveSettings() {
            localStorage.setItem('wpm', wpmSlider.value);
            ['jitterVal', 'pauseFreq', 'pauseDur', 'pauseJit'].forEach(id => {
                localStorage.setItem(id, document.getElementById(id).value);
            });
            localStorage.setItem('smartBrackets', document.getElementById('smartBrackets').checked);
            localStorage.setItem('escKiller', document.getElementById('escKiller').checked);
            localStorage.setItem('soundToggle', document.getElementById('soundToggle').checked);
        }

        function calculateTotalMs(text) {
            if (text.length === 0) return 0;
            const wpm = parseInt(wpmSlider.value) || 120;
            const msPerChar = 60000.0 / (wpm * 5.0); 
            const pauseFreq = parseInt(document.getElementById('pauseFreq').value) || 0;
            const pauseDur = parseInt(document.getElementById('pauseDur').value) || 0;
            
            const spaces = (text.match(/ /g) || []).length;
            const newlines = (text.match(/\n/g) || []).length;
            const estimatedPauses = spaces * (pauseFreq / 100.0);
            
            const escPenalty = document.getElementById('escKiller').checked ? 50 : 0;
            return (text.length * msPerChar) + (estimatedPauses * pauseDur) + (newlines * (150 + escPenalty));
        }

        function updateStats() {
            const text = textArea.value;
            document.getElementById('wpmVal').innerText = wpmSlider.value;
            document.getElementById('charCount').innerText = `${text.length} chars`;
            const totalMs = calculateTotalMs(text);
            if (totalMs === 0) {
                document.getElementById('estTime').innerText = "Est. Time: 00:00";
                return;
            }
            const mins = Math.floor(totalMs / 60000);
            const secs = Math.floor((totalMs % 60000) / 1000);
            document.getElementById('estTime').innerText = `Est. Time: ${mins}m ${secs.toString().padStart(2, '0')}s`;
        }

        // --- THE SMART FLATTENER ---
        function formatCode() {
            let text = textArea.value;
            if(!text) return alert("Paste code first.");
            
            // 1. Strip comments
            text = text.replace(/\/\*[\s\S]*?\*\//g, '');
            text = text.replace(/(?<!:)\/\/.*/g, '');
            
            // 2. Strip indents and blanks
            let lines = text.split('\n').map(line => line.trim()).filter(line => line.length > 0);
            
            // 3. Glues multi-line statements (like massive 'if' blocks) into a single line
            let flattened = [];
            let buffer = "";
            
            for (let line of lines) {
                if (buffer.length > 0) buffer += " ";
                buffer += line;
                
                // If the line ends in a structural terminator, it's safe to press ENTER.
                if (buffer.match(/[{};:]$/)) {
                    flattened.push(buffer);
                    buffer = "";
                }
            }
            // Push anything left over
            if (buffer.length > 0) flattened.push(buffer);
            
            textArea.value = flattened.join('\n');
            updateStats();
        }

        function splitIntoChunks(text) {
            const lines = text.split('\n');
            const result = [];
            let currentBlock = "";
            
            for (let i = 0; i < lines.length; i++) {
                let lineStr = lines[i] + (i === lines.length - 1 ? "" : "\n");
                
                while (lineStr.length > MAX_CHUNK_SIZE) {
                    if (currentBlock.length > 0) {
                        result.push(currentBlock);
                        currentBlock = "";
                    }
                    result.push(lineStr.substring(0, MAX_CHUNK_SIZE));
                    lineStr = lineStr.substring(MAX_CHUNK_SIZE);
                }

                if (currentBlock.length + lineStr.length > MAX_CHUNK_SIZE) {
                    if (currentBlock.length > 0) result.push(currentBlock);
                    currentBlock = lineStr;
                } else {
                    currentBlock += lineStr;
                }
            }
            if (currentBlock.length > 0) result.push(currentBlock);
            return result;
        }

        function startInjectionSequence() {
            let text = textArea.value;
            if(!text) return alert("Please enter some text first.");
            
            unlockAudio();
            
            payloadChunks = splitIntoChunks(text);
            currentChunkIndex = 0;
            document.getElementById('injectBtn').disabled = true;
            
            if (payloadChunks.length > 1) {
                alert(`Massive Payload Detected! Splitting into ${payloadChunks.length} parts.\n\nPLEASE KEEP YOUR PHONE AWAKE ON THIS SCREEN UNTIL IT FINISHES.`);
                sendChunkToESP(true); 
            } else {
                sendChunkToESP(false); 
            }
        }

        function sendChunkToESP(isMultiPart) {
            if (currentChunkIndex >= payloadChunks.length) {
                statusText.innerHTML = `<span class="alert-text" style="color: var(--success);">Injection Complete!</span>`;
                document.getElementById('injectBtn').disabled = false;
                playFinishSound();
                return;
            }

            if (isMultiPart) {
                statusText.innerHTML = `<span class="alert-text">Injecting Part ${currentChunkIndex + 1} of ${payloadChunks.length}... Do not close browser.</span>`;
            } else {
                statusText.innerHTML = `<span class="alert-text" style="color: var(--success);">Injecting... You may lock your phone screen.</span>`;
            }
            
            const chunkText = payloadChunks[currentChunkIndex];
            
            const wpm = parseInt(wpmSlider.value) || 120;
            const jitter = parseInt(document.getElementById('jitterVal').value) || 0;
            const pFreq = parseInt(document.getElementById('pauseFreq').value) || 0;
            const pDur = parseInt(document.getElementById('pauseDur').value) || 0;
            const pJit = parseInt(document.getElementById('pauseJit').value) || 0;
            const smart = document.getElementById('smartBrackets').checked ? 1 : 0;
            const escKill = document.getElementById('escKiller').checked ? 1 : 0;

            const url = `/inject?wpm=${wpm}&jitter=${jitter}&pfreq=${pFreq}&pdur=${pDur}&pjit=${pJit}&smart=${smart}&esc=${escKill}&chunk=${currentChunkIndex}`;

            fetch(url, {
                method: 'POST',
                headers: { 'Content-Type': 'text/plain' },
                body: chunkText
            }).then(async res => {
                if(res.ok) {
                    if (!isMultiPart && currentChunkIndex === 0) {
                        alert(`Injection Started! Expected Time: ` + document.getElementById('estTime').innerText + `\n\nIt is safe to turn off your phone screen. You will hear a chime when finished.`);
                    }
                    if (isMultiPart) {
                        consecutivePollFailures = 0;
                        pollingInterval = setInterval(checkESPStatus, 2000);
                    } else {
                        if(finishTimer) clearTimeout(finishTimer);
                        const waitTime = calculateTotalMs(chunkText) + 2500;
                        
                        finishTimer = setTimeout(() => {
                            playFinishSound();
                            statusText.innerHTML = `<span class="alert-text" style="color: var(--success);">Injection Complete!</span>`;
                            document.getElementById('injectBtn').disabled = false;
                        }, waitTime);
                    }
                } else {
                    const errText = await res.text();
                    if (errText === "SYSTEM_BUSY") {
                        alert("ESP32 is already injecting! Please wait.");
                    } else if (errText === "BLE_DISCONNECTED") {
                        updateSliderCap(false);
                        alert("Target PC not connected via Bluetooth! Ensure the host is paired.");
                    } else {
                        alert("Injection failed. ESP32 error.");
                    }
                    document.getElementById('injectBtn').disabled = false;
                }
            }).catch(err => {
                alert("Network Error during injection.");
                document.getElementById('injectBtn').disabled = false;
                statusText.innerHTML = `<span style="color: var(--danger);">Connection lost to ESP32 AP.</span>`;
            });
        }

        function checkESPStatus() {
            fetch('/status')
            .then(res => res.text())
            .then(state => {
                consecutivePollFailures = 0; 
                
                if (state === "disconnected") {
                    stopPolling();
                    updateSliderCap(false);
                    alert("Hardware disconnected! Injection aborted.");
                } else if (state === "ready") {
                    stopPolling();
                    currentChunkIndex++;
                    sendChunkToESP(true); 
                }
            }).catch(e => {
                consecutivePollFailures++;
                if (consecutivePollFailures >= MAX_POLL_FAILURES) {
                    stopPolling();
                    document.getElementById('injectBtn').disabled = false;
                    statusText.innerHTML = `<span class="alert-text" style="color: var(--danger);">Connection lost to ESP32 AP.</span>`;
                    alert("Lost communication with ESP32 Access Point.");
                }
            });
        }

        function stopPolling() {
            if (pollingInterval) {
                clearInterval(pollingInterval);
                pollingInterval = null;
            }
        }

        function sendCancel() {
            payloadChunks = []; 
            stopPolling();
            if (finishTimer) clearTimeout(finishTimer); 
            document.getElementById('injectBtn').disabled = false;
            
            fetch('/cancel', { method: 'POST' })
            .then(res => { 
                if(res.ok) {
                    statusText.innerHTML = `<span class="alert-text" style="color: var(--warn);">Action Cancelled!</span>`;
                    alert("Action Cancelled!"); 
                }
            });
        }
    </script>
</body>
</html>
)rawliteral";

void handleRoot() { 
    server.send_P(200, "text/html", htmlPage); 
}

void handleInject() {
    if (isInjecting) {
        server.send(409, "text/plain", "SYSTEM_BUSY");
        return;
    }

    if (!bleKeyboard.isConnected()) {
        server.send(400, "text/plain", "BLE_DISCONNECTED");
        return;
    }

    if (server.hasArg("plain")) {
        payload = server.arg("plain");
        
        targetWPM = server.hasArg("wpm") ? server.arg("wpm").toInt() : 120;
        jitterPercent = server.hasArg("jitter") ? server.arg("jitter").toInt() : 15;
        pauseChance = server.hasArg("pfreq") ? server.arg("pfreq").toInt() : 3;
        pauseDurationMs = server.hasArg("pdur") ? server.arg("pdur").toInt() : 1500;
        pauseJitterPercent = server.hasArg("pjit") ? server.arg("pjit").toInt() : 20;
        
        useSmartBrackets = server.hasArg("smart") ? (server.arg("smart") == "1") : true;
        useEscKiller = server.hasArg("esc") ? (server.arg("esc") == "1") : true;
        
        if (server.hasArg("chunk") && server.arg("chunk").toInt() == 0) {
            atLineStart = true;
        } else if (!server.hasArg("chunk")) {
            atLineStart = true; 
        }
        
        payloadIndex = 0;
        isInjecting = true;
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad Request");
    }
}

void handleCancel() {
    isInjecting = false;
    payloadIndex = 0;
    payload = ""; 
    bleKeyboard.releaseAll(); 
    server.send(200, "text/plain", "Cancelled");
}

void handleStatus() {
    if (!bleKeyboard.isConnected()) {
        server.send(200, "text/plain", "disconnected");
    } else if (isInjecting) {
        server.send(200, "text/plain", "busy");
    } else {
        server.send(200, "text/plain", "ready");
    }
}

void setup() {
    Serial.begin(115200);
    
    pinMode(PANIC_PIN, INPUT_PULLUP);
    
    // Prevent heap fragmentation by reserving a large continuous block of RAM
    payload.reserve(13000); 
    
    bleKeyboard.begin();
    WiFi.softAP(AP_SSID, AP_PASS);
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus); 
    server.on("/inject", HTTP_POST, handleInject);
    server.on("/cancel", HTTP_POST, handleCancel);
    server.begin();
}

void loop() {
    // --- NON-BLOCKING HARDWARE PANIC CHECK ---
    if (digitalRead(PANIC_PIN) == LOW) {
        isInjecting = false;
        bleKeyboard.releaseAll();
        Serial.println("HARDWARE PANIC BUTTON PRESSED!");
        
        unsigned long debounceStart = millis();
        while (millis() - debounceStart < 1000) {
            server.handleClient();
            delay(1);
        }
    }

    // --- INFINITE BUSY TRAP FAILSAFE ---
    if (!bleKeyboard.isConnected() && isInjecting) {
        isInjecting = false;
        payloadIndex = 0;
        payload = "";
        bleKeyboard.releaseAll();
        Serial.println("BLE Disconnected mid-injection. Failsafe triggered.");
    }

    server.handleClient();

    if (bleKeyboard.isConnected() && isInjecting) {
        if (millis() >= nextTypeTime) {
            
            // Bounds check evaluated before payload memory extraction
            if (payloadIndex >= payload.length()) {
                isInjecting = false;
                payload = ""; 
                return;
            }
            
            char c = payload[payloadIndex];
            long delayMs = 80;

            if (atLineStart) {
                // Instantly consume bad leading whitespace
                if (c == ' ' || c == '\t' || c == '\r') {
                    payloadIndex++;
                    return; 
                }
                
                if (c == '}' && useSmartBrackets) {
                    bleKeyboard.press(KEY_DOWN_ARROW); smartDelay(40); bleKeyboard.releaseAll(); 
                    if (!isInjecting) return; 
                    smartDelay(40);
                    bleKeyboard.press(KEY_END); smartDelay(40); bleKeyboard.releaseAll();
                    if (!isInjecting) return;
                    
                    payloadIndex++; 
                    atLineStart = false; 
                    nextTypeTime = millis() + 60; 
                    return; 
                }
                atLineStart = false; 
            }

            if (c == '\n') {
                if (useEscKiller) {
                    bleKeyboard.press(KEY_ESC); smartDelay(30); bleKeyboard.releaseAll();
                    if (!isInjecting) return;
                    smartDelay(20);
                    if (!isInjecting) return; 
                }
                
                bleKeyboard.press(KEY_RETURN); smartDelay(40); bleKeyboard.releaseAll();
                atLineStart = true; 
                delayMs = 150; 
            } 
            else if (c != '\r') {
                bleKeyboard.press(c); 
                smartDelay(40); 
                if (!isInjecting) return; // Safe array progression interrupt
                bleKeyboard.releaseAll();
                
                float msPerChar = 60000.0 / (targetWPM * 5.0);
                delayMs = (long)msPerChar;
                
                if (jitterPercent > 0) {
                    long jitterAmount = (delayMs * jitterPercent) / 100;
                    delayMs += random(-jitterAmount, jitterAmount + 1);
                }
                
                // Floor dropped to 50ms to physically allow 150 WPM execution
                if (delayMs < 50) delayMs = 50; 
                
                if (c == ' ' && pauseChance > 0) {
                    if (random(0, 100) < pauseChance) {
                        long currentPauseMs = pauseDurationMs;
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
            
            // Double-check termination to clear memory exactly on the last char
            if (payloadIndex >= payload.length()) {
                isInjecting = false;
                payload = ""; 
            }
            
            long nextDelay = delayMs - 40;
            if (nextDelay < 5) nextDelay = 5; 
            
            nextTypeTime = millis() + nextDelay;
        }
    }
}