#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ======================================================
// CONFIGURACIÓN WIFI
// ======================================================

const char* WIFI_SSID = "ARTEFACTOS";
const char* WIFI_PASSWORD = "87654321";

// ======================================================
// CONFIGURACIÓN MQTT
// ======================================================

const char* MQTT_SERVER = "192.168.137.200";
const int MQTT_PORT = 1883;

// Topics utilizados por el dashboard React.
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";

// ======================================================
// CONFIGURACIÓN DE SENSORES
// ======================================================

// Pines definidos según la guía (DHT11 -> GPIO 4, LDR -> GPIO 32)
#define DHTPIN 4
#define DHTTYPE DHT11
#define LDRPIN 32

DHT dht(DHTPIN, DHTTYPE);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Enviar una lectura cada 2 segundos.
const unsigned long INTERVALO_ENVIO = 2000;
unsigned long ultimoEnvio = 0;

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a Mosquitto...");

    // Crear un Client ID diferente para este ESP32.
    String clientId = "ESP32-";
    clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" conectado");
    } else {
      Serial.print(" error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 segundos...");
      delay(2000);
    }
  }
}

// ======================================================
// LEER Y PUBLICAR SENSORES
// ======================================================

void publicarSensores() {
  // 1. Obtener la temperatura.
  float temperatura = dht.readTemperature();

  // 2. Obtener la humedad.
  float humedad = dht.readHumidity();

  // Verificar lecturas del DHT11
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Error al leer datos del sensor DHT11");
    return;
  }

  // 3. Obtener la lectura del sensor de iluminación (LDR)
  int valorADC = analogRead(LDRPIN);
  // Mapeo del ADC de 12 bits (0-4095) a porcentaje (0-100%)
  int luz = map(valorADC, 0, 4095, 0, 100);

  // 4 y 5. Preparar valores y publicar en sus topics MQTT
  mqttClient.publish(TOPIC_TEMPERATURA, String(temperatura, 2).c_str());
  mqttClient.publish(TOPIC_HUMEDAD, String(humedad, 2).c_str());
  mqttClient.publish(TOPIC_LUZ, String(luz).c_str());

  // 6. Mostrar las mediciones en el Monitor Serial
  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" °C");

  Serial.print("Humedad: ");
  Serial.print(humedad, 2);
  Serial.println(" %");

  Serial.print("LDR ADC: ");
  Serial.print(valorADC);
  Serial.print(" | Luz: ");
  Serial.print(luz);
  Serial.println(" %");

  Serial.println("------------------------------");
}

void setup() {
  Serial.begin(115200);

  // Inicializar los sensores y configurar los pines necesarios
  dht.begin();
  pinMode(LDRPIN, INPUT);

  conectarWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  // Si se pierde WiFi, volver a conectarse.
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Si se pierde MQTT, volver a conectarse.
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  // Publicar sensores cada 2 segundos.
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = millis();
    publicarSensores();
  }
}