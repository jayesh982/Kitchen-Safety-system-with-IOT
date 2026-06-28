#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial Bluetooth(10,11);
char Data;

void _delay(float seconds) {
  long endTime = millis() + seconds * 1000;
  while(millis() < endTime) _loop();
}

void setup() {
  Bluetooth.begin(9600);
  pinMode(5,OUTPUT);
  pinMode(6,OUTPUT);
  pinMode(7,OUTPUT);

  while(1) {
      if(Bluetooth.available()) {
          Data=Bluetooth.read();
          Serial.print(Data);
          // LED 1 on
          if (Data==('5')) {
              digitalWrite(5,1);
              Serial.print("Light 1 ON");

          }
          // LED 1 off
          if (Data==('1')) {
              digitalWrite(5,0);
              Serial.print("Light 2 OFF");

          }
          // LED 2 on
          if (Data==('6')) {
              digitalWrite(6,1);

          }
          // LED 2 off
          if (Data==('2')) {
              digitalWrite(6,0);

          }
          // LED 3 on
          if (Data==('7')) {
              digitalWrite(7,1);

          }
          // LED 3 off
          if (Data==('3')) {
              digitalWrite(7,0);

          }
          // ALL LEDs ON
          if (Data==('9')) {
              digitalWrite(5,1);
              digitalWrite(6,1);
              digitalWrite(7,1);

          }
          // ALL LEDs OFF
          if (Data==('0')) {
              digitalWrite(5,0);
              digitalWrite(6,0);
              digitalWrite(7,0);

          }

      }

      _loop();
  }

}

void _loop() {
}

void loop() {
  _loop();
}
