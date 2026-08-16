# ⌨️ Linear Human Injector (ESP32 BLE)

A stealthy, web-controlled Bluetooth Low Energy (BLE) keystroke injector built for the standard ESP32.

Unlike standard macro tools that dump text as fast as possible, the Linear Human Injector is designed to flawlessly mimic human typing patterns. It respects physical Bluetooth connection intervals for 100% accuracy and features "Smart Line Logic" to navigate the auto-formatting behaviors of modern IDEs (like VS Code or Arduino IDE) without breaking the code structure.

## ✨ Key Features

* **100% BLE Accuracy Engine:** Employs strict 40ms hardware hold times and 80ms minimum gaps to prevent host OS buffer bursting, "Shift-bleed", and dropped characters caused by BLE connection interval limits.
* **IDE-Aware Smart Logic:**
* Eats leading spaces to defeat the "staircase effect" caused by IDE auto-indentation.
* Detects auto-completed closing brackets (`}`) and executes smart arrow-key navigation (`Down` -> `End`) to escape them without throwing syntax errors.


* **Advanced Humanization Engine:**
* **WPM Control:** Mathematically capped at 120 WPM to ensure flawless wireless packet delivery.
* **RNG Jitter:** Applies randomized millisecond deviations to inter-key delays.
* **Organic Pausing:** Configurable % chance to simulate a human pausing to think when pressing the spacebar, complete with its own pause-duration jitter.


* **Air-Gapped Web Interface:** Hosts its own standalone Wi-Fi Access Point (AP). No external network required.
* **Client-Side Auto-Formatter:** The web UI automatically strips block/line comments and blank lines *before* transmission, saving the ESP32's limited RAM for massive payloads.

## 🛠️ Hardware Requirements

* Any standard **ESP32 Development Board** (e.g., DOIT DevKit V1, NodeMCU-32S).
* A target device with Bluetooth (Windows, macOS, Linux, Android, iOS).

## 📦 Dependencies & Installation

This project utilizes the lightweight **NimBLE** stack to save RAM and prevent crashes when handling large text payloads.

### 1. Install Required Libraries

In the Arduino IDE, go to **Sketch > Include Library > Manage Libraries**:

1. Install **NimBLE-Arduino** by h2zero.
* ⚠️ **CRITICAL:** You *must* install version **1.4.2** (or 1.4.x). Do not install version 2.0+, as it contains breaking changes for the keyboard library.


2. Install **ESP32-BLE-Keyboard** by T-vK. (Download the `.zip` from GitHub and add it via **Sketch > Include Library > Add .ZIP Library**).

### 2. Configure Arduino IDE Settings

Because the Wi-Fi and Bluetooth radio stacks are large, you must allocate more memory to the application partition:

1. Go to **Tools > Board** and select **ESP32 Dev Module**.
2. Go to **Tools > Partition Scheme**.
3. Select **Huge APP (3MB No OTA/1MB SPIFFS)**.

### 3. Flash the Board

Upload the `.ino` sketch to your ESP32.

## 🚀 Usage

1. **Power the ESP32:** Plug it into a wall adapter or battery bank.
2. **Connect to Target:** On your computer/target device, turn on Bluetooth and pair with the device named **ESP32 Injector**.
3. **Open the Controller:** On your phone or laptop, connect to the ESP32's Wi-Fi network:
* **SSID:** `Input_Controller`
* **Password:** `12345678`


4. **Inject:** Open a web browser and navigate to `[http://192.168.4.1](http://192.168.4.1)`. Paste your code, adjust your humanization settings, and click **INJECT CODE**.

## 🧠 Why the 40ms / 80ms Limits?

If you attempt to push a standard ESP32 BLE keyboard past 150+ WPM, the target OS will start dropping `Shift` modifiers (turning `)` into `0`) or rendering placeholder pipes (`|||`).

This happens because BLE transmits in intervals (typically every 15–45ms). If the ESP32 sends a "Key Press" and "Key Release" inside the same interval window, the host driver receives contradictory states at the exact same microsecond and drops the packet.

The Linear Human Injector solves this physics limitation by enforcing a **40ms hardware-level key hold**, followed by an **80ms absolute floor** before the next key. This guarantees that every press and release spans across separate radio intervals, achieving perfect 1-to-1 accuracy on any operating system.

## ⚠️ Disclaimer

This tool is provided for educational purposes, embedded systems learning, and authorized penetration testing only. Do not use this device on systems you do not own or do not have explicit permission to test.
