#include <Arduino.h>

const int LDR_PIN = 34;
const int LED_PIN = 25;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  int valorSensorRaw = analogRead(LDR_PIN);

  // Por usar ESP32, o valor lido é em 12 bits (0-4095), converte para 10 bits.
  int valorSensor = map(valorSensorRaw, 0, 4095, 0, 1023);

  int valorLed = 0;

  if (valorSensor >= 800) {
    valorLed = 255;
  } else if (valorSensor >= 600 && valorSensor < 800) {
    valorLed = 192;
  } else if (valorSensor >= 400 && valorSensor < 600) {
    valorLed = 128;
  } else if (valorSensor >= 200 && valorSensor < 400) {
    valorLed = 64;
  } else { // valorSensor < 200
    valorLed = 0;
  }

  analogWrite(LED_PIN, valorLed);

  Serial.print("Valor de entrada do sensor: ");
  Serial.print(valorSensor);
  Serial.print(" | Saida do LED: ");
  Serial.println(valorLed);

  delay(500); 
}