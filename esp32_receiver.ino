#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char* wifiSsid = "Binsoi_Home";
const char* wifiPassword = "VinceAdrien17!";
const char* statusUrl = "https://emergsystem.onrender.com/api/status";

const int greenLedPin = 25;
const int redLedPin = 26;
const int buzzerPin = 27;
const unsigned long pollInterval = 1000;
const unsigned long blinkInterval = 500;

unsigned long lastPoll = 0;
unsigned long lastBlink = 0;
bool blinkState = false;
String activeAlert = "";

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid, wifiPassword);
  Serial.print("Connecting receiver to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Receiver ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void setOutputs(bool greenOn, bool redOn, bool buzzerOn) {
  digitalWrite(greenLedPin, greenOn ? HIGH : LOW);
  digitalWrite(redLedPin, redOn ? HIGH : LOW);
  if (buzzerOn) {
    tone(buzzerPin, 2200);
  } else {
    noTone(buzzerPin);
  }
}

void stopAlert() {
  activeAlert = "";
  blinkState = false;
  setOutputs(false, false, false);
}

void pollAlertStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);

  if (!http.begin(client, statusUrl)) {
    Serial.println("Unable to open receiver status URL");
    return;
  }

  int responseCode = http.GET();
  if (responseCode == 200) {
    String responseBody = http.getString();
    bool alertActive = responseBody.indexOf("\"active\":true") >= 0;
    bool responsePending = responseBody.indexOf("\"response\":\"PENDING\"") >= 0;
    bool traumaAlert = responseBody.indexOf("\"severity\":\"TRAUMA\"") >= 0 ||
                       responseBody.indexOf("\"severity\":\"HEAVY\"") >= 0;
    bool medicalAlert = responseBody.indexOf("\"severity\":\"MEDICAL\"") >= 0 ||
                        responseBody.indexOf("\"severity\":\"MILD\"") >= 0;

    if (!alertActive || !responsePending) {
      if (activeAlert.length() > 0) {
        Serial.println("Alert completed; receiver outputs off");
      }
      stopAlert();
    } else if (traumaAlert) {
      activeAlert = "TRAUMA";
      Serial.println("Trauma alert received");
    } else if (medicalAlert) {
      activeAlert = "MEDICAL";
      Serial.println("Medical alert received");
    }
  } else {
    Serial.print("Status response: ");
    Serial.println(responseCode);
  }

  http.end();
}

void updateOutputs() {
  if (activeAlert.length() == 0) {
    setOutputs(false, false, false);
    return;
  }

  unsigned long now = millis();
  if (now - lastBlink >= blinkInterval) {
    lastBlink = now;
    blinkState = !blinkState;
  }

  if (activeAlert == "TRAUMA") {
    setOutputs(blinkState, false, blinkState);
  } else if (activeAlert == "MEDICAL") {
    setOutputs(false, blinkState, blinkState);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  stopAlert();
  connectToWiFi();
  Serial.println("ESP32 alert receiver ready");
}

void loop() {
  unsigned long now = millis();

  if (now - lastPoll >= pollInterval) {
    lastPoll = now;
    pollAlertStatus();
  }

  updateOutputs();
  delay(10);
}
