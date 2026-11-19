#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TinyGPS++.h>
#define WIFI_NAME "YOUR_WIFI"
#define WIFI_PASS "YOUR_PASSWORD"
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"
#define GPS_RX 16
#define GPS_TX 17
#define SENSOR_PIN 27
#define BUZZER_PIN 25
#define LED_PIN 26
#define SENSOR_ON HIGH
#define ALERT_TIME 30000UL
#define BUZZER_TIME 3000UL
TinyGPSPlus gps;
bool lastSensor = false;
bool buzzerOn = false;
bool needSend = false;
bool sentBefore = false;

unsigned long buzzerStart = 0;
unsigned long lastAlert = 0;
unsigned long lastTry = 0;
unsigned long lastWiFi = 0;

bool sendTelegram() {

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = "https://api.telegram.org/bot";
  url += BOT_TOKEN;
  url += "/sendMessage";

  if (!http.begin(client, url)) {
    return false;
  }

  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");

  String msg = "CAR ALERT!\\n";
  msg += "Vibration detected!";

  if (gps.location.isValid() &&
      gps.location.age() < 60000) {

    msg += "\\n\\nGoogle Maps:\\n";
    msg += "https://www.google.com/maps?q=";
    msg += String(gps.location.lat(), 6);
    msg += ",";
    msg += String(gps.location.lng(), 6);

  } else {
    msg += "\\n\\nGPS not ready";
  }

  String data = "{\"chat_id\":\"";
  data += CHAT_ID;
  data += "\",\"text\":\"";
  data += msg;
  data += "\"}";

  int result = http.POST(data);

  Serial.print("Telegram: ");
  Serial.println(result);

  http.end();

  return result == 200;
}

void setup() {

  Serial.begin(115200);

  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  Serial2.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  lastSensor = (digitalRead(SENSOR_PIN) == SENSOR_ON);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NAME, WIFI_PASS);

  lastWiFi = millis();

  Serial.println("SYSTEM START");
}

void loop() {

  unsigned long now = millis();

  while (Serial2.available()) {
    gps.encode(Serial2.read());
  }

  if (WiFi.status() != WL_CONNECTED &&
      now - lastWiFi >= 10000) {

    lastWiFi = now;

    WiFi.disconnect();
    WiFi.begin(WIFI_NAME, WIFI_PASS);

    Serial.println("WiFi Connecting...");
  }

  bool sensor = digitalRead(SENSOR_PIN) == SENSOR_ON;

  if (sensor && !lastSensor) {

    Serial.println("VIBRATION DETECTED");

    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);

    buzzerOn = true;
    buzzerStart = now;
    needSend = true;
  }

  lastSensor = sensor;

  if (buzzerOn && now - buzzerStart >= BUZZER_TIME) {

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    buzzerOn = false;
  }

  if (needSend && WiFi.status() == WL_CONNECTED) {

    if ((!sentBefore || now - lastAlert >= ALERT_TIME) &&
        (lastTry == 0 || now - lastTry >= 5000)) {

      lastTry = now;

      if (sendTelegram()) {

        Serial.println("MESSAGE SENT");

        lastAlert = millis();
        sentBefore = true;
        needSend = false;
      }
    }
  }

  delay(20);
}
