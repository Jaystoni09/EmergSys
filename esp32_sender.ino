#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char* wifiSsid = "kylla";
const char* wifiPassword = "kylla123";
const char* receiverUrl = "https://emergsystem.onrender.com/api/alert";
const int blueLed = 23;
const char* roomNumber = "1";
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

  WiFiClientSecure client;
  client.setInsecure();
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
  pinMode(blueLed, OUTPUT);
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
        
      sendAlert("Trauma Emergencies", "Headache, nausea, mild pain and etc.");
      digitalWrite(blueLed, HIGH);   // Turn the LED on (HIGH voltage level)
      delay(1000);
      digitalWrite(blueLed, LOW);   // Turn the LED on (HIGH voltage level)
   
 }
 else if (heavyPressed == HIGH) {
    Serial.println("Heavy symptoms button pressed");
      sendAlert("Medical Emergencies", "Chest pain, shortness of breath, severe pain and etc.");
      digitalWrite(blueLed, HIGH);   // Turn the LED on (HIGH voltage level)
      delay(1000);
      digitalWrite(blueLed, LOW);
 }

  delay(20);
}
