#include <Arduino.h>

const int PIR_PIN = 13;
const int LED_PIN = 14;
const int BUZZER_PIN = 12;
const int BUTTON_PIN = 23;

const unsigned long ALARM_DURATION_MS = 10000; // 10 segundos
const unsigned long PAUSE_DURATION_MS = 5000;  // 5 segundos
const int LED_BLINK_INTERVAL_MS = 250;       // Intervalo do piscar do LED

// Enum para gerir o estado do sistema (máquina de estados)
enum SystemState {
  DISARMED,
  TRIGGERED,
  ALARM_SOUNDING,
  ALARM_PAUSED
};
SystemState currentState = DISARMED;

unsigned long stateChangeTimestamp = 0;
unsigned long lastBlinkTimestamp = 0;
bool ledState = false;

void setup() {
  Serial.begin(115200);
  
  // Configuração dos pinos
  pinMode(PIR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Garante que o alarme começa desligado
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW); // Para buzzers passivos, seria noTone()

  Serial.println("Sistema de Seguranca Armado.");
  Serial.println("A aguardar movimento...");
}

void loop() {
  // A verificação do botão é feita a cada ciclo para ser sempre responsiva
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (currentState != DISARMED) {
      disarmAlarm();
    }
    // Adiciona um pequeno debounce para o botão
    delay(50); 
  }

  // --- Máquina de Estados ---
  switch (currentState) {
    case DISARMED:
      // Se o sistema está desarmado, verifica se há movimento
      if (digitalRead(PIR_PIN) == HIGH) {
        Serial.println("!!! MOVIMENTO DETETADO !!!");
        currentState = TRIGGERED; // Muda para o estado de alarme disparado
      }
      break;

    case TRIGGERED:
      // Este estado inicia o ciclo de alarme
      Serial.println("Alarme ATIVADO! A tocar por 10 segundos...");
      digitalWrite(BUZZER_PIN, HIGH); // Liga o buzzer
      stateChangeTimestamp = millis(); // Guarda o momento em que o alarme começou
      currentState = ALARM_SOUNDING;
      break;

    case ALARM_SOUNDING:
      // Mantém o LED a piscar
      handleBlinking();

      // Verifica se os 10 segundos do alarme já passaram
      if (millis() - stateChangeTimestamp >= ALARM_DURATION_MS) {
        Serial.println("Pausa de 5 segundos...");
        digitalWrite(BUZZER_PIN, LOW); // Desliga o buzzer
        stateChangeTimestamp = millis(); // Guarda o momento em que a pausa começou
        currentState = ALARM_PAUSED;
      }
      break;

    case ALARM_PAUSED:
      // Mantém o LED a piscar para indicar que o sistema ainda está em alerta
      handleBlinking();

      // Verifica se os 5 segundos de pausa já passaram
      if (millis() - stateChangeTimestamp >= PAUSE_DURATION_MS) {
        // Volta a disparar o alarme para um novo ciclo de 10s
        currentState = TRIGGERED; 
      }
      break;
  }
}

// Função para controlar o piscar do LED de forma não-bloqueante
void handleBlinking() {
  if (millis() - lastBlinkTimestamp > LED_BLINK_INTERVAL_MS) {
    lastBlinkTimestamp = millis();
    ledState = !ledState; // Inverte o estado do LED
    digitalWrite(LED_PIN, ledState);
  }
}

// Função para desarmar o alarme e voltar ao estado inicial
void disarmAlarm() {
  Serial.println("--- Sistema Desarmado pelo Utilizador ---");
  Serial.println("A aguardar movimento...");
  
  // Desliga os atuadores
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  ledState = false; // Garante que o LED começa desligado no próximo ciclo
  
  // Reinicia o estado do sistema
  currentState = DISARMED;
}