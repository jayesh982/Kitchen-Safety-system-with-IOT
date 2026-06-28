/*
 * Smart Kitchen Safety System - Program 3
 * Platform: Wokwi STM32 Blue Pill (STM32F103C8)
 * Role: Food Freshness Sensor Hub (Electronic Nose)
 *
 * Simulates gas sensor array using potentiometers:
 *   - POT1 (VOC level)      -> PA0
 *   - POT2 (H2S / rot level)-> PA1
 *   - POT3 (Ammonia level)  -> PA2
 *
 * Outputs:
 *   - Green LED  (Fresh)    -> PB0
 *   - Yellow LED (Warning)  -> PB1
 *   - Red LED    (Spoiled)  -> PA8
 *   - Buzzer                -> PA9
 *
 * Serial output: JSON data for ESP32 gateway
 *
 * Wokwi: New STM32 Blue Pill project -> paste code + diagram.json
 */

// --- Pin Definitions ---
#define VOC_PIN      PA0
#define H2S_PIN      PA1
#define AMMONIA_PIN  PA2

#define LED_GREEN    PB0
#define LED_YELLOW   PB1
#define LED_RED      PA8
#define BUZZER_PIN   PA9

// --- Thresholds (0-4095 for 12-bit ADC) ---
#define LEVEL_FRESH     1000   // Below this = fresh
#define LEVEL_WARNING   2500   // Above this = warning
#define LEVEL_SPOILED   3500   // Above this = spoiled

// --- State ---
enum FoodState { FRESH, STALE, WARNING, SPOILED };
FoodState currentState = FRESH;

unsigned long lastSerial = 0;

void setup() {
  Serial.begin(9600);

  pinMode(VOC_PIN, INPUT_ANALOG);
  pinMode(H2S_PIN, INPUT_ANALOG);
  pinMode(AMMONIA_PIN, INPUT_ANALOG);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Startup indication
  digitalWrite(LED_GREEN, HIGH);
  delay(300);
  digitalWrite(LED_YELLOW, HIGH);
  delay(300);
  digitalWrite(LED_RED, HIGH);
  delay(300);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  Serial.println("Food Freshness Monitor Online");
}

void loop() {
  // --- Read simulated gas sensors ---
  int voc     = analogRead(VOC_PIN);
  int h2s     = analogRead(H2S_PIN);
  int ammonia = analogRead(AMMONIA_PIN);

  // --- Compute freshness score ---
  // Weighted average: H2S is strongest spoilage indicator
  int score = (voc * 0.3) + (h2s * 0.5) + (ammonia * 0.2);

  // --- Classify food state ---
  currentState = classifyFood(score);

  // --- Drive LEDs and buzzer ---
  updateOutputs(currentState);

  // --- Send data over Serial every 1 second ---
  if (millis() - lastSerial > 1000) {
    sendData(voc, h2s, ammonia, score);
    lastSerial = millis();
  }

  delay(100);
}

FoodState classifyFood(int score) {
  if (score < LEVEL_FRESH) {
    return FRESH;
  } else if (score < LEVEL_WARNING) {
    return STALE;
  } else if (score < LEVEL_SPOILED) {
    return WARNING;
  } else {
    return SPOILED;
  }
}

void updateOutputs(FoodState state) {
  switch (state) {
    case FRESH:
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);
      noTone(BUZZER_PIN);
      break;

    case STALE:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, HIGH);
      digitalWrite(LED_RED, LOW);
      noTone(BUZZER_PIN);
      break;

    case WARNING:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, HIGH);
      // Blink red
      digitalWrite(LED_RED, (millis() / 500) % 2);
      tone(BUZZER_PIN, 1000, 200);
      break;

    case SPOILED:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, HIGH);
      tone(BUZZER_PIN, 2500);
      break;
  }
}

void sendData(int voc, int h2s, int ammonia, int score) {
  // JSON for ESP32 gateway
  Serial.print("{\"node\":\"freshness\",\"voc\":");
  Serial.print(voc);
  Serial.print(",\"h2s\":");
  Serial.print(h2s);
  Serial.print(",\"ammonia\":");
  Serial.print(ammonia);
  Serial.print(",\"score\":");
  Serial.print(score);
  Serial.print(",\"state\":\"");
  switch (currentState) {
    case FRESH:   Serial.print("fresh");   break;
    case STALE:   Serial.print("stale");   break;
    case WARNING: Serial.print("warning"); break;
    case SPOILED: Serial.print("spoiled"); break;
  }
  Serial.println("\"}");
}
