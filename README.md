# SignalWatch Emergency Alert System

A dependency-free receiver dashboard for a two-button ESP32 emergency system. The ESP32 is the sender; this web page displays incoming alerts.

## Run

Start the receiver server with `powershell -ExecutionPolicy Bypass -File .\receiver.ps1`, then open `http://localhost:8000`. The server accepts alerts from the ESP32 and serves the dashboard.

## Free permanent deployment with Render

1. Create a GitHub repository and upload this folder.
2. In Render, create a new **Web Service** from the repository.
3. Use the included `render.yaml`, or set the start command to `node server.js`.
4. Choose the free plan and deploy.
5. Set `receiverUrl` in `esp32_sender.ino` to the Render HTTPS URL plus `/api/alert`.
6. Open the Render URL in both receiver browsers.

The Node server listens on Render's `PORT` environment variable and exposes `/health`, `/api/alert`, `/api/status`, `/api/response`, and `/api/history/clear`. The current browser admin password is client-side demo authentication; use server-side authentication before treating the public deployment as production-secure.

## Receiver behavior

- The web page does not trigger alerts. It listens for alerts sent by the ESP32.
- `MILD` and `HEAVY` are the two ESP32 sender states.
- The receiver displays the room number, severity, symptoms, sender ID, and received time.

For browser-only testing, call `window.receiveAlert({ severity: 'HEAVY', room: '204', symptoms: 'Chest pain' })` in the browser console. The server provides `POST /api/alert` for the ESP32 and `GET /api/status` for the browser.

## ESP32 sender

Open `esp32_sender.ino` in Arduino IDE and set `wifiSsid`, `wifiPassword`, and `receiverUrl` to the computer's LAN IP. Wire both buttons between the GPIO pin and GND:


The sketch uses the ESP32 internal pull-ups, sends one JSON `POST` per button press, and waits for the button to be released before sending another alert. The computer and ESP32 must be on the same Wi-Fi network, and Windows Firewall must allow TCP port 8000.

## Second ESP32 receiver

Upload `esp32_receiver.ino` to a second ESP32 and set its Wi-Fi credentials. Wire a green LED to GPIO 25, a red LED to GPIO 26, and a buzzer to GPIO 27, using a resistor for each LED. The receiver polls `/api/status` every second:

- `TRAUMA` or `HEAVY`: green LED blinks and buzzer sounds
- `MEDICAL` or `MILD`: red LED blinks and buzzer sounds
- `CONFIRMED` or `DENIED`: both LEDs and buzzer stop
