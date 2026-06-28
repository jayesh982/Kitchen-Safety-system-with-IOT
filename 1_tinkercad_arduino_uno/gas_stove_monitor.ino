int const GAS_SENSOR = A1;
int LED_GREEN  = 7;
int LED_YELLOW = 6;
int LED_RED1   = 5;
int LED_RED2   = 4;
int BUZZER     = 8;

void setup() {
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED1, OUTPUT);
    pinMode(LED_RED2, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    Serial.begin(9600);
}

void loop() {
    int raw = analogRead(GAS_SENSOR);
    int level = map(raw, 300, 750, 0, 100);
    level = constrain(level, 0, 100);

    Serial.print("Raw: ");
    Serial.print(raw);
    Serial.print(" | Level: ");
    Serial.print(level);
    Serial.println("%");

    // All off first
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED1, LOW);
    digitalWrite(LED_RED2, LOW);
    noTone(BUZZER);

    if (level >= 0 && level < 25) {
        // SAFE: only green
        digitalWrite(LED_GREEN, HIGH);
        delay(250);

    } else if (level >= 25 && level < 50) {
        // LOW WARNING: only yellow
        digitalWrite(LED_YELLOW, HIGH);
        delay(250);

    } else if (level >= 50 && level < 75) {
        // DANGER: only red1 + buzzer beep
        digitalWrite(LED_RED1, HIGH);
        tone(BUZZER, 1500);
        delay(150);
        noTone(BUZZER);
        delay(100);

    } else if (level >= 75 && level <= 100) {
        // EXTREME: only red2 + continuous loud buzzer
        digitalWrite(LED_RED2, HIGH);
        tone(BUZZER, 3000);
        delay(250);
    }
}
