#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// configuração do lcd
LiquidCrystal_I2C LCD(0x27, 20, 4);

#define NTP_SERVER     "a.ntp.br"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

#define PULSE_PER_BEAT    1
#define INTERRUPT_PIN     5
#define SAMPLING_INTERVAL 1000

//envia dados como bpm e hora
#define TOPICO_SUBSCRIBE    "/TEF/PulseGuard008/cmd"
#define TOPICO_PUBLISH      "/TEF/PulseGuard008/attrs"
#define TOPICO_PUBLISH_2    "/TEF/PulseGuard008/attrs/bpm"  
#define TOPICO_PUBLISH_3    "/TEF/PulseGuard008/attrs/h"  

#define ID_MQTT  "fiware_PG"

// configurar rede wifi 
const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";

const char* BROKER_MQTT = "46.17.108.113";
int BROKER_PORT = 1883;

WiFiClient espClient;
PubSubClient MQTT(espClient);
char EstadoSaida = '0';

volatile uint16_t pulse;
uint16_t pulseCount;
uint16_t count;

float heartRate;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR HeartRateInterrupt() {
  portENTER_CRITICAL_ISR(&mux);
  pulse++;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  initSerial();
  initWiFi();
  initMQTT();
  delay(5000);
  MQTT.publish(TOPICO_PUBLISH, "s|on");
  initLCD();
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), HeartRateInterrupt, RISING);
}

void initSerial() {
  Serial.begin(115200);
  while (!Serial) {
  }
}

// função para iniciar o LCD
void initLCD() {
  LCD.init();
  LCD.backlight();
  LCD.setCursor(0, 0);
  LCD.print("Connecting to ");
  LCD.setCursor(0, 1);
  LCD.print("Wi-Fi ");

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    spinner();
  }

  Serial.println("");
  Serial.println("WiFi conectado!!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.println("Online");
  LCD.setCursor(0, 1);
  LCD.println("...");

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}

void initWiFi() {
  delay(10);
  Serial.println("Conexao via WI-FI");
  Serial.print("Conectando-se na rede: ");
  Serial.println(SSID);
  Serial.println("...");

  reconectWiFi();
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("* Tentando se conectar no servidor... ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado com sucesso eba!");
      MQTT.subscribe(TOPICO_SUBSCRIBE);
    } else {
      Serial.println("Falha ao reconecta.");
      Serial.println("Haverá nova tentativa para conectar ao servidor");
      delay(5000);
    }
  }
}

void reconectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  Serial.println();
  Serial.print("Conectado com sucesso na rede ");
  Serial.print(SSID);
  Serial.println(" IP: ");
  Serial.println(WiFi.localIP());
}

void VerificaConexoesWiFIEMQTT(void) {
  if (!MQTT.connected())
    reconnectMQTT();

  reconectWiFi();
}

void spinner() {
  static int8_t counter = 0;
  const char* glyphs = "\xa1\xa5\xdb";
  LCD.setCursor(15, 1);
  LCD.print(glyphs[counter++]);
  if (counter == strlen(glyphs)) {
    counter = 0;
  }
}

struct tm timeinfo;

void printLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    LCD.setCursor(0, 1);
    LCD.println("Connection Err");
    return;
  }

  timeinfo.tm_hour -= 3;
  if (timeinfo.tm_hour < 0) {
    timeinfo.tm_hour += 24;
  }

  // declaração de horário no lcd, mostrando hora, minuto e segundo
  LCD.setCursor(8, 0);
  LCD.println(&timeinfo, "%H:%M:%S");
  printHeartRateOnLCD();
}

void printHeartRateOnLCD() {
  LCD.setCursor(0, 2);
  // Exibe a frequência cardíaca abaixo da data e hora
  LCD.print("Heart Rate: ");
  LCD.print(heartRate);
  LCD.print(" BPM    ");

  // exibe os pulsos  do coração
  LCD.setCursor(0, 3);
  LCD.print("Pulses: ");
  LCD.print(pulse);
}

void loop() {
  HeartRate();
  printLocalTime();
  delay(250);

  VerificaConexoesWiFIEMQTT();

  MQTT.loop();
  delay(100);
}

// função para verificar a freq cardíaca 
void HeartRate() {
  char heartRateBuffer[6];
  char timeBuffer[20];

  static unsigned long startTime;
  if (millis() - startTime < SAMPLING_INTERVAL) return;
  startTime = millis();

  portENTER_CRITICAL(&mux);
  count = pulse;
  pulseCount += count;
  pulse = 0;
  portEXIT_CRITICAL(&mux);

 // Ajuste na fórmula para mapear a faixa de 0 Hz a 220 Hz para a frequência cardíaca em BPM
  heartRate = map(count, 0, 220, 0, 220);
  dtostrf(heartRate, 4, 2, heartRateBuffer);
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

  MQTT.publish(TOPICO_PUBLISH_2, heartRateBuffer);
  MQTT.publish(TOPICO_PUBLISH_3, timeBuffer);

  Serial.println("Heart Rate: " + String(heartRate, 2) + " BPM");
}
