/*
 * Smart Kitchen Safety System - Program 2
 * Platform: Wokwi ESP32
 * Role: Smart Fridge Monitor + Cloud Gateway (MQTT)
 *
 * Components & Wiring (Wokwi):
 *   - DHT22 (humidity)       -> GPIO 5
 *   - DS18B20 x2 (fridge &   -> GPIO 15 (both on same OneWire bus)
 *     freezer temp)
 *   - OLED SSD1306 I2C       -> SDA=21, SCL=22
 *   - Buzzer                 -> GPIO 4
 *   - Push Button             -> GPIO 2 (INPUT_PULLUP)
 *   - Red LED (alert)        -> GPIO 13
 *
 * Cloud: Publishes to HiveMQ MQTT broker
 *
 * Wokwi: Create new ESP32 project, paste this code,
 *         paste diagram.json for auto-wiring
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- Pin Definitions ---
#define DHT_PIN        5
#define ONEWIRE_PIN    15
#define OLED_SDA       21
#define OLED_SCL       22
#define BUZZER_PIN     4
#define BUTTON_PIN     2
#define LED_PIN        13

// --- OLED ---
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Sensors ---
DHT dht(DHT_PIN, DHT22);
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature tempSensors(&oneWire);

// --- WiFi (Wokwi simulates this) ---
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// --- MQTT Broker (HiveMQ public) ---
const char* MQTT_SERVER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "kitchen/fridge";
const char* MQTT_ALERT    = "kitchen/alerts";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// --- Thresholds ---
#define FRIDGE_MAX_TEMP    8.0    // °C - fridge too warm
#define FREEZER_MAX_TEMP  -10.0   // °C - freezer too warm
#define FRIDGE_DANGER_TEMP 12.0   // °C - food unsafe
#define HUMIDITY_MAX       85.0   // % - too humid inside fridge
#define DOOR_OPEN_TIMEOUT  120000 // ms - 2 minutes door open alert

// --- State ---
float fridgeTemp    = 0.0;
float freezerTemp   = 0.0;
float humidity      = 0.0;
bool  alertActive   = false;
bool  buzzerMuted   = false;
unsigned long lastPublish  = 0;
unsigned long lastDisplay  = 0;
int   displayPage   = 0;

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  // Init OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Kitchen Safety");
  display.setCursor(10, 35);
  display.println("Fridge Monitor v1");
  display.display();

  // Init sensors
  dht.begin();
  tempSensors.begin();

  // Connect WiFi
  connectWiFi();

  // Connect MQTT
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  connectMQTT();

  delay(2000);
  display.clearDisplay();
}

void loop() {
  // Keep MQTT alive
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  // --- Read Sensors ---
  readSensors();

  // --- Evaluate Safety ---
  evaluateSafety();

  // --- Handle Button (mute) ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    buzzerMuted = !buzzerMuted;
    if (buzzerMuted) noTone(BUZZER_PIN);
    delay(300); // debounce
  }

  // --- Update OLED (every 1s) ---
  if (millis() - lastDisplay > 1000) {
    updateDisplay();
    lastDisplay = millis();
  }

  // --- Publish to MQTT (every 5s) ---
  if (millis() - lastPublish > 5000) {
    publishData();
    lastPublish = millis();
  }

  delay(100);
}

// --- WiFi ---
void connectWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    display.println("WiFi OK!");
    display.println(WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed - offline mode");
    display.println("WiFi FAILED");
    display.println("Running offline");
  }
  display.display();
  delay(1000);
}

// --- MQTT ---
void connectMQTT() {
  int attempts = 0;
  while (!mqtt.connected() && attempts < 3) {
    Serial.print("MQTT connecting...");
    String clientId = "KitchenESP-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected!");
      mqtt.subscribe("kitchen/commands");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
    attempts++;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("MQTT received: " + message);

  // Remote commands
  if (message == "mute")   { buzzerMuted = true;  noTone(BUZZER_PIN); }
  if (message == "unmute") { buzzerMuted = false; }
  if (message == "status") { publishData(); }
}

// --- Sensor Reading ---
void readSensors() {
  humidity = dht.readHumidity();
  if (isnan(humidity)) humidity = 0.0;

  tempSensors.requestTemperatures();
  fridgeTemp  = tempSensors.getTempCByIndex(0);
  freezerTemp = tempSensors.getTempCByIndex(1);

  // Handle disconnected sensors (returns -127)
  if (fridgeTemp < -100) fridgeTemp = 0.0;
  if (freezerTemp < -100) freezerTemp = -18.0;

  Serial.print("Fridge: "); Serial.print(fridgeTemp);
  Serial.print("°C  Freezer: "); Serial.print(freezerTemp);
  Serial.print("°C  Humidity: "); Serial.print(humidity);
  Serial.println("%");
}

// --- Safety Evaluation ---
void evaluateSafety() {
  alertActive = false;
  String alertMsg = "";

  // Fridge too warm
  if (fridgeTemp > FRIDGE_DANGER_TEMP) {
    alertActive = true;
    alertMsg = "FRIDGE UNSAFE!";
    Serial.println("ALERT: Fridge temp dangerously high!");
  } else if (fridgeTemp > FRIDGE_MAX_TEMP) {
    alertActive = true;
    alertMsg = "Fridge warm!";
  }

  // Freezer too warm
  if (freezerTemp > FREEZER_MAX_TEMP) {
    alertActive = true;
    alertMsg = "FREEZER WARM!";
    Serial.println("ALERT: Freezer temp too high!");
  }

  // Humidity too high
  if (humidity > HUMIDITY_MAX) {
    alertActive = true;
    alertMsg = "Humidity high!";
  }

  // Act on alert
  if (alertActive) {
    digitalWrite(LED_PIN, HIGH);
    if (!buzzerMuted) {
      if (fridgeTemp > FRIDGE_DANGER_TEMP) {
        tone(BUZZER_PIN, 2500); // Urgent
      } else {
        tone(BUZZER_PIN, 1500, 200); // Warning beep
      }
    }
    // Publish alert to MQTT
    mqtt.publish(MQTT_ALERT, alertMsg.c_str());
  } else {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}

// --- OLED Display ---
void updateDisplay() {
  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("KITCHEN SAFETY");
  display.setCursor(100, 0);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "OFF");

  // Divider line
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Fridge temperature
  display.setCursor(0, 14);
  display.print("Fridge : ");
  display.print(fridgeTemp, 1);
  display.print((char)247); // degree
  display.print("C");
  if (fridgeTemp > FRIDGE_MAX_TEMP) {
    display.print(" !");
  }

  // Freezer temperature
  display.setCursor(0, 26);
  display.print("Freezer: ");
  display.print(freezerTemp, 1);
  display.print((char)247);
  display.print("C");
  if (freezerTemp > FREEZER_MAX_TEMP) {
    display.print(" !");
  }

  // Humidity
  display.setCursor(0, 38);
  display.print("Humidity: ");
  display.print(humidity, 1);
  display.print("%");

  // Status bar at bottom
  display.drawLine(0, 50, 128, 50, SSD1306_WHITE);
  display.setCursor(0, 54);
  if (alertActive) {
    display.setTextSize(1);
    display.print("!! ALERT !!");
    if (buzzerMuted) display.print(" [MUTED]");
  } else {
    display.print("Status: ALL OK");
  }

  display.display();
}

// --- Publish to MQTT ---
void publishData() {
  if (!mqtt.connected()) return;

  // Build JSON payload
  String json = "{";
  json += "\"node\":\"fridge\",";
  json += "\"fridge_temp\":" + String(fridgeTemp, 1) + ",";
  json += "\"freezer_temp\":" + String(freezerTemp, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"alert\":" + String(alertActive ? "true" : "false") + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI());
  json += "}";

  mqtt.publish(MQTT_TOPIC, json.c_str());
  Serial.println("MQTT published: " + json);
}
