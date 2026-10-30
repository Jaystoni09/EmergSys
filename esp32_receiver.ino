#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>


const char* wifiSsid = "kylla";
const char* wifiPassword = "kylla123";
const char* statusUrl = "https://emergsystem.onrender.com/api/status";

const int greenLedPin = 25;
const int redLedPin = 26;
const int buzzerPin = 27;
const unsigned long pollInterval = 1000;
const unsigned long blinkInterval = 100;

unsigned long lastPoll = 0;
unsigned long lastBlink = 0;
bool blinkState = false;
String activeAlert = "";
volatile uint8_t activeAlertType = 0;

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
  activeAlertType = 0;
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
      activeAlertType = 1;
      Serial.println("Trauma alert received");
    } else if (medicalAlert) {
      activeAlert = "MEDICAL";
      activeAlertType = 2;
      Serial.println("Medical alert received");
    }
  } else {
    Serial.print("Status response: ");
    Serial.println(responseCode);
    if (responseCode > 0) {
      Serial.print("Server response: ");
      Serial.println(http.getString());
    } else {
      Serial.print("HTTP error: ");
      Serial.println(http.errorToString(responseCode));
    }
  }

  http.end();
}

void updateOutputs() {
  if (activeAlertType == 0) {
    setOutputs(false, false, false);
    return;
  }

  unsigned long now = millis();
  if (now - lastBlink >= blinkInterval) {
    lastBlink = now;
    blinkState = !blinkState;
  }

  if (activeAlertType == 1) {
    setOutputs(blinkState, false, blinkState);
  } else if (activeAlertType == 2) {
    setOutputs(false, blinkState, blinkState);
  }
}

void outputTask(void* parameter) {
  while (true) {
    updateOutputs();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  stopAlert();
  xTaskCreatePinnedToCore(outputTask, "Alert outputs", 2048, nullptr, 1, nullptr, 0);
  connectToWiFi();
  Serial.println("ESP32 alert receiver ready");
}

void loop() {
  unsigned long now = millis();

  if (now - lastPoll >= pollInterval) {
    lastPoll = now;
    pollAlertStatus();
  }

  delay(1);
}
