#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

/*
 *  Sistema D - Controlo de Acessos Inteligente (SETR)
 * 
 *  Este projeto integra:
 *  - Leitor RFID (MFRC522) para acesso por cartão.
 *  - Servidor Web num Access Point para controlo remoto (utilizador/password).
 *  - Servo motor para simular uma cancela.
 *  - LEDs e Buzzer para feedback ao utilizador.
 *
 *  CUMPRE OS REQUISITOS AVANÇADOS:
 *  - Multitasking: Usa FreeRTOS para gerir 3 tarefas (RFID, Controlo do Sistema, Servidor Web).
 *  - GUI Remota: Página web com monitorização em tempo real (AJAX) e controlo.
 *  - Interrupts: Um botão de pressão para acionamento manual/emergência.
 */


// --- DEFINIÇÕES DE HARDWARE ---
// RFID MFRC522
#define RST_PIN   4
#define SS_PIN    5
// Buzzer
#define BUZZER_PIN 25
// LEDs
#define LED_VERDE_PIN 32
#define LED_VERMELHO_PIN 33
// Servo
#define SERVO_PIN 15
// Botão para Interrupt
#define BUTTON_PIN 26

// --- CONFIGURAÇÕES DO SISTEMA ---
const char* ap_ssid = "Cancela_SmartCity_SETR";
const char* ap_password = "password123";
const char* web_user = "admin";
const char* web_pass = "admin";

// UID autorizado (coloque aqui o UID do seu cartão)
String authorizedUID = "XX:XX:XX:XX"; 

// Ângulos do Servo
#define SERVO_POS_FECHADA 0
#define SERVO_POS_ABERTA  90
// Tempo que a cancela fica aberta (em milissegundos)
#define TEMPO_ABERTA_MS 5000 

// --- OBJETOS GLOBAIS ---
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo cancelaServo;
AsyncWebServer server(80);

// --- VARIÁVEIS DE ESTADO (volatile para acesso seguro entre tarefas e ISRs) ---
enum EstadoCancela { FECHADA, ABRINDO, ABERTA, FECHANDO };
volatile EstadoCancela estadoCancela = FECHADA;
volatile bool botaoPressionadoFlag = false; // Flag para a ISR
unsigned long tempoAbertura = 0;

// --- FUNÇÃO DA INTERRUPÇÃO (ISR) ---
// Deve ser o mais rápida possível. Apenas define uma flag.
void IRAM_ATTR onBotaoPressionado() {
  botaoPressionadoFlag = true;
}

// =================================================================
// TAREFA 1: CONTROLO DA CANCELA, LEDS E ESTADO (MÁQUINA DE ESTADOS)
// =================================================================
void taskControloSistema(void * parameter) {
  Serial.println("Task de Controlo do Sistema iniciada.");
  for(;;) { // Loop infinito da tarefa

    // Verifica a flag do interrupt do botão
    if (botaoPressionadoFlag) {
      if (estadoCancela == FECHADA) {
        Serial.println(">>> Override manual pelo botao! Abrindo cancela...");
        estadoCancela = ABRINDO;
      }
      botaoPressionadoFlag = false; // Reset da flag
    }

    // Máquina de Estados principal
    switch (estadoCancela) {
      case ABRINDO:
        Serial.println("Estado: ABRINDO");
        digitalWrite(LED_VERMELHO_PIN, LOW);
        digitalWrite(LED_VERDE_PIN, HIGH);
        tone(BUZZER_PIN, 1200, 150); // Bip de sucesso
        cancelaServo.write(SERVO_POS_ABERTA);
        tempoAbertura = millis();
        estadoCancela = ABERTA;
        break;

      case ABERTA:
        Serial.println("Estado: ABERTA");
        // Se já passou o tempo, inicia o fecho
        if (millis() - tempoAbertura > TEMPO_ABERTA_MS) {
          estadoCancela = FECHANDO;
        }
        break;

      case FECHANDO:
        Serial.println("Estado: FECHANDO");
        cancelaServo.write(SERVO_POS_FECHADA);
        digitalWrite(LED_VERDE_PIN, LOW);
        digitalWrite(LED_VERMELHO_PIN, HIGH);
        estadoCancela = FECHADA;
        break;

      case FECHADA:
        Serial.println("Estado: FECHADA");
        // Estado de repouso, não faz nada
        break;
    }
    
    // Pequena pausa para permitir que outras tarefas executem
    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}

// ===============================================
// TAREFA 2: LEITURA DO CARTÃO RFID
// ===============================================
void taskLeitorRFID(void * parameter) {
  Serial.println("Task do Leitor RFID iniciada.");
  for(;;) {
    // Só tenta ler se a cancela estiver fechada
    if (estadoCancela == FECHADA) {
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          uid += (mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
          uid += String(mfrc522.uid.uidByte[i], HEX);
          if (i < mfrc522.uid.size - 1) uid += ":";
        }
        uid.toUpperCase();

        Serial.print("Cartao detectado. UID: ");
        Serial.println(uid);

        if (uid.equals(authorizedUID)) {
          Serial.println("Acesso AUTORIZADO.");
          estadoCancela = ABRINDO; // Dispara a máquina de estados
        } else {
          Serial.println("Acesso NEGADO.");
          // Feedback de erro
          tone(BUZZER_PIN, 500, 500);
          digitalWrite(LED_VERMELHO_PIN, LOW);
          delay(200);
          digitalWrite(LED_VERMELHO_PIN, HIGH);
          delay(200);
          digitalWrite(LED_VERMELHO_PIN, LOW);
          delay(200);
          digitalWrite(LED_VERMELHO_PIN, HIGH);
        }

        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
    }
    // Pausa para não sobrecarregar o CPU
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// --- HTML E CSS PARA A INTERFACE GRÁFICA ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Controlo Cancela SETR</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" type="text/css" href="/style.css">
</head>
<body>
    <div class="container">
        <h1>Sistema D - Controlo de Acessos</h1>
        <p>Estado da Cancela: <span id="estado">A carregar...</span></p>
        <div class="card">
            <h2>Desbloqueio Remoto</h2>
            <form id="unlockForm">
                <input type="text" id="user" name="user" placeholder="Utilizador" required>
                <input type="password" id="pass" name="pass" placeholder="Password" required>
                <button type="submit">Desbloquear</button>
            </form>
            <p id="response"></p>
        </div>
    </div>
    <script>
        // Função para atualizar o estado da cancela
        function getStatus() {
            fetch('/estado')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('estado').textContent = data.estado;
                })
                .catch(error => console.error('Erro ao buscar estado:', error));
        }

        // Atualiza o estado a cada 2 segundos
        setInterval(getStatus, 2000);
        window.onload = getStatus;

        // Função para tratar o envio do formulário
        document.getElementById('unlockForm').addEventListener('submit', function(event) {
            event.preventDefault();
            const user = document.getElementById('user').value;
            const pass = document.getElementById('pass').value;
            const responseP = document.getElementById('response');

            fetch(`/unlock?user=${user}&pass=${pass}`)
                .then(response => response.text())
                .then(data => {
                    responseP.textContent = data;
                    setTimeout(() => responseP.textContent = '', 3000); // Limpa a mensagem
                })
                .catch(error => console.error('Erro ao desbloquear:', error));
        });
    </script>
</body>
</html>
)rawliteral";

const char style_css[] PROGMEM = R"rawliteral(
body { font-family: Arial, sans-serif; background-color: #f0f2f5; margin: 0; padding: 20px; text-align: center; }
.container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
h1 { color: #333; }
p { font-size: 1.2em; }
#estado { font-weight: bold; color: #007bff; }
.card { background: #f9f9f9; border: 1px solid #ddd; padding: 15px; margin-top: 20px; border-radius: 5px; }
input[type="text"], input[type="password"] { width: calc(100% - 22px); padding: 10px; margin: 5px 0; border: 1px solid #ccc; border-radius: 4px; }
button { width: 100%; padding: 10px; background-color: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 1em; }
button:hover { background-color: #0056b3; }
#response { margin-top: 10px; font-weight: bold; }
)rawliteral";


// --- SETUP ---
void setup() {
  Serial.begin(115200);

  // Inicializa Hardware
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);
  pinMode(LED_VERMELHO_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Botão com resistor interno
  digitalWrite(LED_VERMELHO_PIN, HIGH); // Começa com LED vermelho ligado
  
  cancelaServo.attach(SERVO_PIN);
  cancelaServo.write(SERVO_POS_FECHADA);
  
  SPI.begin();
  mfrc522.PCD_Init();
  
  Serial.println("\nHardware inicializado.");

  // Configura a Interrupção
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onBotaoPressionado, FALLING);
  
  // Configura o Access Point
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // --- Rotas do Servidor Web ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });
  
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/css", style_css);
  });
  
  server.on("/estado", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"estado\": \"";
    switch(estadoCancela){
      case FECHADA: json += "Fechada"; break;
      case ABRINDO: json += "Abrindo..."; break;
      case ABERTA: json += "Aberta"; break;
      case FECHANDO: json += "Fechando..."; break;
    }
    json += "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/unlock", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("user") && request->hasParam("pass")) {
      String user = request->getParam("user")->value();
      String pass = request->getParam("pass")->value();
      if (user.equals(web_user) && pass.equals(web_pass)) {
        if (estadoCancela == FECHADA) {
          estadoCancela = ABRINDO; // Dispara a máquina de estados
          request->send(200, "text/plain", "Desbloqueio autorizado!");
        } else {
          request->send(200, "text/plain", "Acao ja em curso.");
        }
      } else {
        request->send(401, "text/plain", "Utilizador ou password invalidos.");
      }
    } else {
      request->send(400, "text/plain", "Faltam parametros.");
    }
  });

  server.begin();
  Serial.println("Servidor web iniciado.");

  // --- Cria as Tarefas do FreeRTOS ---
  // Core 0 é geralmente usado pelo WiFi, então usamos o Core 1 para as nossas tarefas.
  xTaskCreatePinnedToCore(
    taskLeitorRFID,      // Função da tarefa
    "LeitorRFID",        // Nome da tarefa
    2048,                // Tamanho da pilha (stack)
    NULL,                // Parâmetros da tarefa
    1,                   // Prioridade
    NULL,                // Handle da tarefa
    1);                  // Core onde vai correr

  xTaskCreatePinnedToCore(
    taskControloSistema, // Função da tarefa
    "ControloSistema",   // Nome da tarefa
    2048,                // Tamanho da pilha
    NULL,                // Parâmetros
    2,                   // Prioridade mais alta para controlo
    NULL,                // Handle
    1);                  // Core

  Serial.println("Tarefas do RTOS criadas. Sistema a funcionar.");
}

// O loop principal fica vazio, pois toda a lógica está nas tarefas do RTOS.
void loop() {
  vTaskDelete(NULL); // Opcional: deleta a tarefa do loop() para libertar recursos.
}