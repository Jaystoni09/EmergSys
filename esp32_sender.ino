#include <WiFi.h>
#include <HTTPClient.h>

const char* wifiSsid = "Binsoi_Home";
const char* wifiPassword = "VinceAdrien17!";
const char* receiverUrl = "http://127.0.0.1:8000/api/alert";

const char* roomNumber = "204";
const int mildButtonPin = 18;
const int heavyButtonPin = 19;
const unsigned long debounceDelay = 250;

bool buttonsReady = true;
unsigned long lastButtonEvent = 0;
unsigned long lastStatusPrint = 0;
bool previousMildPressed = false;
bool previousHeavyPressed = false;

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid, wifiPassword);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed");
  }
}

void sendAlert(const char* severity, const char* symptoms) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected; reconnecting");
    connectToWiFi();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Alert not sent: ESP32 is offline");
    return;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(client, receiverUrl)) {
    Serial.println("Alert not sent: invalid receiver URL");
    return;
  }
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"active\":true,\"severity\":\"";
  payload += severity;
  payload += "\",\"room\":\"";
  payload += roomNumber;
  payload += "\",\"symptoms\":\"";
  payload += symptoms;
  payload += "\"}";

  int responseCode = http.POST(payload);
  Serial.print("Alert response: ");
  Serial.println(responseCode);
  if (responseCode > 0) {
    Serial.print("Receiver response: ");
    Serial.println(http.getString());
  } else {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(responseCode));
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(mildButtonPin, INPUT_PULLUP);
  pinMode(heavyButtonPin, INPUT_PULLUP);
  previousMildPressed = digitalRead(mildButtonPin) == LOW;
  previousHeavyPressed = digitalRead(heavyButtonPin) == LOW;
  buttonsReady = !previousMildPressed && !previousHeavyPressed;
  connectToWiFi();
  Serial.println("Button wiring: GPIO 18 = MILD, GPIO 19 = HEAVY, button side goes to GND");
  Serial.print("Initial GPIO states: ");
  Serial.print(digitalRead(mildButtonPin));
  Serial.print(", ");
  Serial.println(digitalRead(heavyButtonPin));
  Serial.println("ESP32 emergency sender ready");
}

void loop() {
    int mildPressed = digitalRead(mildButtonPin);
    int heavyPressed = digitalRead(heavyButtonPin);
  
 if (mildPressed == HIGH) {
    Serial.println("Mild symptoms button pressed");
      sendAlert("MILD", "Headache, nausea, mild pain");
 }
 else if (heavyPressed == HIGH) {
    Serial.println("Heavy symptoms button pressed");
      sendAlert("HEAVY", "Chest pain, shortness of breath, severe pain");
 }
  

  delay(20);
}
