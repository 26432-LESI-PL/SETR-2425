#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <LiquidCrystal_I2C.h>

const int RELAY_PIN = 27;
const int LED_VERMELHO_PIN = 26;
const int LED_VERDE_PIN = 25;

const float TEMP_MAXIMA_LIGAR = 24.0;
const float TEMP_MINIMA_DESLIGAR = 20.0;

LiquidCrystal_I2C lcd(0x27, 16, 2); 
Adafruit_BMP280 bmp;

bool ventoinhaLigada = false;

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_VERMELHO_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);

  // Desliga o relay por padrão (nível ALTO para muitos módulos de relay)
  // IMPORTANTE: Alguns relays ativam com LOW e outros com HIGH.
  // Se a ventoinha ligar quando devia desligar, mude HIGH para LOW e vice-versa.
  digitalWrite(RELAY_PIN, HIGH);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("A iniciar...");

  // Inicializar o sensor BMP280
  if (!bmp.begin()) {
    Serial.println("Nao foi possivel encontrar o sensor BMP280");
    lcd.clear();
    lcd.print("Erro no Sensor BMP280!");
    while (1);
  }

  Serial.println("Sistema de Climatizacao Iniciado");
  delay(1000); // Pequena pausa para mostrar a mensagem de início
  lcd.clear();
}

void loop() {
  // Lê a temperatura do sensor BMP280
  float temperatura = bmp.readTemperature();

  if (temperatura > TEMP_MAXIMA_LIGAR) {
    ventoinhaLigada = true;
  } else if (temperatura < TEMP_MINIMA_DESLIGAR) {
    ventoinhaLigada = false;
  }

  if (ventoinhaLigada) {
    digitalWrite(RELAY_PIN, LOW);      // Liga a ventoinha (ativa o relay)
    digitalWrite(LED_VERMELHO_PIN, HIGH); // Liga o LED vermelho (arrefecimento)
    digitalWrite(LED_VERDE_PIN, LOW);   // Desliga o LED verde
  } else {
    digitalWrite(RELAY_PIN, HIGH);     // Desliga a ventoinha (desativa o relay)
    digitalWrite(LED_VERMELHO_PIN, LOW);    // Desliga o LED vermelho
    digitalWrite(LED_VERDE_PIN, HIGH);  // Liga o LED verde (estabilizado)
  }


  lcd.setCursor(0, 0);
  if (ventoinhaLigada) {
    lcd.print("Fan ON          "); // Espaços para limpar a linha anterior
  } else {
    lcd.print("Fan OFF         ");
  }

  // Linha 2: Temperatura Atual
  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(temperatura, 1); // Mostra a temperatura com 1 casa decimal
  lcd.print((char)223); // Caractere de grau (°)
  lcd.print("C   "); // Espaços para limpar a linha

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.print("C, Estado da Ventoinha: ");
  Serial.println(ventoinhaLigada ? "ON" : "OFF");

  // Aguarda 1 segundo antes da próxima leitura
  delay(1000);
}