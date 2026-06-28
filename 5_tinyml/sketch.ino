/*
 * Smart Kitchen Safety System - Program 5
 * Platform: Wokwi ESP32 + TinyML
 * Role: Kitchen Sound Classifier (simulated audio via potentiometers)
 *
 * Components:
 *   - POT1 (Frequency)     -> GPIO 34
 *   - POT2 (Energy)        -> GPIO 35
 *   - POT3 (Pattern)       -> GPIO 32
 *   - LCD 16x2 I2C         -> SDA=21, SCL=22
 *   - Green LED (Safe)     -> GPIO 25
 *   - Yellow LED (Warning) -> GPIO 26
 *   - Red LED (Danger)     -> GPIO 27
 *   - Buzzer               -> GPIO 14
 *
 * The 3 pots simulate MFCC audio features:
 *   POT1 = dominant frequency (low->high)
 *   POT2 = energy level (quiet->loud)
 *   POT3 = pattern type (steady->burst)
 *
 * Upload kitchen_model.h to Wokwi project files
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "kitchen_model.h"
#include <TensorFlowLite_ESP32.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

// --- Pins ---
#define POT_FREQ    34
#define POT_ENERGY  35
#define POT_PATTERN 32
#define LED_GREEN   25
#define LED_YELLOW  26
#define LED_RED     27
#define BUZZER_PIN  14

// --- LCD I2C ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- TFLite Micro ---
const int kTensorArenaSize = 8 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

tflite::AllOpsResolver resolver;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// --- Sound classes ---
const char* CLASS_NAMES[] = {
  "Normal", "Boiling Over", "Gas Hissing", "Smoke Alarm", "Glass Break"
};
const char* CLASS_SHORT[] = {
  "NORMAL", "BOILING", "GAS HISS", "ALARM", "GLASS"
};
const int NUM_CLASSES = 5;

// --- State ---
int lastPrediction = -1;
float lastConfidence = 0;
unsigned long lastInference = 0;

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Init LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("TinyML Kitchen");
  lcd.setCursor(0, 1);
  lcd.print("Sound Classify");

  // Load TFLite model from .h file
  model = tflite::GetModel(kitchen_model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model version mismatch!");
    lcd.clear();
    lcd.print("MODEL ERROR!");
    while (1);
  }

  // Build interpreter
  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, kTensorArenaSize
  );
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors failed!");
    lcd.clear();
    lcd.print("ALLOC ERROR!");
    while (1);
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("TinyML model loaded!");
  Serial.print("Input shape: ");
  for (int i = 0; i < input->dims->size; i++) {
    Serial.print(input->dims->data[i]);
    if (i < input->dims->size - 1) Serial.print(" x ");
  }
  Serial.println();

  delay(2000);
  lcd.clear();
}

void loop() {
  // --- Read pots (simulated audio features) ---
  int freqRaw    = analogRead(POT_FREQ);
  int energyRaw  = analogRead(POT_ENERGY);
  int patternRaw = analogRead(POT_PATTERN);

  // Normalize to 0.0 - 1.0
  float freq    = freqRaw / 4095.0;
  float energy  = energyRaw / 4095.0;
  float pattern = patternRaw / 4095.0;

  // --- Run inference every 500ms ---
  if (millis() - lastInference > 500) {
    // Fill input tensor with simulated MFCC features
    // Input shape: (1, 32 frames, 13 MFCCs)
    int input_size = input->bytes / sizeof(float);
    float* input_data = input->data.f;

    for (int frame = 0; frame < 32; frame++) {
      float t = (float)frame / 32.0;
      for (int mfcc = 0; mfcc < 13; mfcc++) {
        int idx = frame * 13 + mfcc;
        if (idx < input_size) {
          // Map pot values to MFCC-like features
          float val = 0;

          if (mfcc == 0) {
            // Energy coefficient
            val = (energy - 0.5) * 20.0;
          } else if (mfcc <= 4) {
            // Low-mid frequency features
            val = (freq - 0.5) * 10.0;
            // Add pattern modulation
            if (pattern > 0.5) {
              val += sin(t * pattern * 8.0 * 3.14159) * 5.0;
            }
          } else {
            // High frequency features
            val = (freq * 2.0 - 1.0) * (float)mfcc;
            if (pattern > 0.7) {
              val += (1.0 - t) * energy * 15.0; // transient burst
            }
          }

          input_data[idx] = val;
        }
      }
    }

    // Run model
    if (interpreter->Invoke() != kTfLiteOk) {
      Serial.println("Invoke failed!");
      return;
    }

    // Get results
    float maxProb = 0;
    int maxIdx = 0;
    float probs[NUM_CLASSES];

    for (int i = 0; i < NUM_CLASSES; i++) {
      probs[i] = output->data.f[i];
      if (probs[i] > maxProb) {
        maxProb = probs[i];
        maxIdx = i;
      }
    }

    lastPrediction = maxIdx;
    lastConfidence = maxProb;
    lastInference = millis();

    // --- Serial output ---
    Serial.print("{\"node\":\"tinyml\",\"class\":\"");
    Serial.print(CLASS_NAMES[maxIdx]);
    Serial.print("\",\"confidence\":");
    Serial.print(maxProb * 100, 1);
    Serial.print(",\"probs\":[");
    for (int i = 0; i < NUM_CLASSES; i++) {
      Serial.print(probs[i], 3);
      if (i < NUM_CLASSES - 1) Serial.print(",");
    }
    Serial.println("]}");

    // --- Update LCD ---
    lcd.setCursor(0, 0);
    lcd.print("Snd:");
    lcd.print(CLASS_SHORT[maxIdx]);
    // Pad to fill line
    for (int i = strlen(CLASS_SHORT[maxIdx]) + 4; i < 16; i++) lcd.print(" ");

    lcd.setCursor(0, 1);
    lcd.print("Conf:");
    lcd.print((int)(maxProb * 100));
    lcd.print("% ");

    // Show danger level
    if (maxIdx == 0) {
      lcd.print("SAFE    ");
    } else if (maxIdx == 1 || maxIdx == 4) {
      lcd.print("WARNING ");
    } else {
      lcd.print("DANGER! ");
    }

    // --- Update LEDs ---
    updateLEDs(maxIdx);

    // --- Buzzer ---
    updateBuzzer(maxIdx);
  }

  delay(50);
}

void updateLEDs(int prediction) {
  switch (prediction) {
    case 0: // Normal
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, LOW);
      break;
    case 1: // Boiling over
    case 4: // Glass breaking
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, HIGH);
      digitalWrite(LED_RED, LOW);
      break;
    case 2: // Gas hissing
    case 3: // Smoke alarm
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_YELLOW, LOW);
      digitalWrite(LED_RED, HIGH);
      break;
  }
}

void updateBuzzer(int prediction) {
  switch (prediction) {
    case 0: // Normal
      noTone(BUZZER_PIN);
      break;
    case 1: // Boiling over
      tone(BUZZER_PIN, 1000, 200);
      break;
    case 2: // Gas hissing
      tone(BUZZER_PIN, 2500);
      break;
    case 3: // Smoke alarm
      tone(BUZZER_PIN, 3000);
      break;
    case 4: // Glass breaking
      tone(BUZZER_PIN, 1500, 300);
      break;
  }
}
