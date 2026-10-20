#include <WiFi.h>
#include <HTTPClient.h>

const char* wifiSsid = "Binsoi_Home";
const char* wifiPassword = "VinceAdrien17!";
const char* statusUrl = "http://192.168.1.8:8000/api/status";

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

String readJsonString(const String& json, const String& key) {
  int keyStart = json.indexOf(key);
  if (keyStart < 0) return "";
  int valueStart = keyStart + key.length();
  int valueEnd = json.indexOf('"', valueStart);
  if (valueEnd < 0) return "";
  return json.substring(valueStart, valueEnd);
}

void pollAlertStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    return;
  }

  WiFiClient client;
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
    String currentSeverity = readJsonString(responseBody, "\"severity\":\"");
    bool traumaAlert = currentSeverity == "TRAUMA" || currentSeverity == "HEAVY" || currentSeverity == "Trauma Emergencies";
    bool medicalAlert = currentSeverity == "MEDICAL" || currentSeverity == "MILD" || currentSeverity == "Medical Emergencies";

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
