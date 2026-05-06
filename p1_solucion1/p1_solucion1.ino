#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <DHT.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <ArduinoJson.h>

const char* INFLUXDB_URL = "https://us-east-1-1.aws.cloud2.influxdata.com/";
const char* INFLUXDB_TOKEN = "limXJwqE4gAztqRgZcJDwAbAj0C260Hpo2UUnI5rkC5ris9MBda4kZmLWBmX1WIrJ57IzmQ2nCv4nnIlbs6uLg==";
const char* INFLUXDB_ORG = "ElPapuIoT";
const char* INFLUXDB_BUCKET = "p1_monitor";
const char* TZ_INFO = "UTC-3";

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensorPoint("clima");

const int SERVER_PORT = 80;
const int BAUD_RATE = 115200;

const int LED_PIN = 2; 
bool led_on = false;

const int DHT_PIN = 22;
const uint8_t DHT_TYPE = DHT11;
DHT dht(DHT_PIN, DHT_TYPE);


WiFiManager wifiManager;
AsyncWebServer server(SERVER_PORT);

void sendJson(AsyncWebServerRequest *request, JsonDocument &doc) {
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  client.setInsecure(); 

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  sensorPoint.addTag("device", "ESP32_Portatil");
  
  Serial.println("Intentando conectarse a InfluxDB Cloud...");
  if (client.validateConnection()) {
    Serial.println("Conectado a InfluxDB Cloud!");
    Serial.println(client.getServerUrl());

  } else {
    Serial.print("Error InfluxDB: ");
    Serial.println(client.getLastErrorMessage());
  }

  Serial.println("Inicializando LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS no pudo iniciarse.");
  }

  // --- ENDPOINTS ---
  server.serveStatic("/assets/", LittleFS, "/assets/");

  server.on("/", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "Error: index.html no encontrado");
    }
  });

  server.on("/update", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("led_state")) {
        led_on = request->getParam("led_state")->value() == "1";
        digitalWrite(LED_PIN, led_on ? HIGH : LOW);
    }

    JsonDocument doc;
    doc["led"] = led_on;
    sendJson(request, doc);
  });

  server.on("/data", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["temp"] = dht.readTemperature();
    doc["hum"]  = dht.readHumidity();
    doc["led"]  = led_on;
    sendJson(request, doc);
  });

  server.on("/history", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    String query = "from(bucket: \"" + String(INFLUXDB_BUCKET) + "\") "
                   "|> range(start: -1h) "
                   "|> filter(fn: (r) => r._measurement == \"clima\") "
                   "|> pivot(rowKey:[\"_time\"], columnKey: [\"_field\"], valueColumn: \"_value\") "
                   "|> limit(n:10)";

    FluxQueryResult result = client.query(query);

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    while (result.next()) {
      JsonObject entry = array.add<JsonObject>();
      entry["timestamp"] = result.getValueByName("_time").getDateTime().format("%Y-%m-%d %H:%M:%S");
      entry["temp"]      = result.getValueByName("temperatura").getDouble();
      entry["hum"]       = result.getValueByName("humedad").getDouble();
    }
    result.close();

    sendJson(request, doc);
  });

  // Redirecciona directamente al dashboard de grafana
  server.on("/api/metrics", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    String localGrafanaUrl = "http://localhost:3000/d-solo/adklczv/p1-iot?orgId=1&refresh=5s&from=1778049656915&to=1778071256915&timezone=browser&panelId=panel-1";
    request->redirect(localGrafanaUrl);
  });

  // --- WIFI ---
  wifiManager.setSaveConfigCallback([](){
    Serial.println("Reiniciando dispositivo...");
    delay(2000); 
    ESP.restart(); 
  });

  wifiManager.setConnectTimeout(20);
  if (!wifiManager.autoConnect("Portal_Config_ESP32", "password")) {
    Serial.println("Fallo conexión");
  }

  Serial.println("Inicializando servidor...");
  server.begin();
  Serial.println("Servidor iniciado");


unsigned long previousMillis = 0;
const long retryInterval = 20000;
bool isConfigPortalActive = false;

void loop() {
  if (millis() - previousMillis >= retryInterval) {
    previousMillis = millis();
    if (WiFi.status() != WL_CONNECTED && !isConfigPortalActive) {
      isConfigPortalActive = true;
      server.end();
      wifiManager.setConfigPortalTimeout(120);
      if (!wifiManager.startConfigPortal("Rescate_ESP32", "password")) {
        isConfigPortalActive = false;
      } else {
        ESP.restart();
      }
    }
  }

  static unsigned long lastDbWrite = 0;
  if (millis() - lastDbWrite > 30000) { 
    lastDbWrite = millis();
    
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if(!isnan(t) && !isnan(h)) {
      sensorPoint.clearFields();
      sensorPoint.addField("temperatura", t); 
      sensorPoint.addField("humedad", h);

      Serial.print("Writing: ");
      Serial.println(client.pointToLineProtocol(sensorPoint));

      if (!client.writePoint(sensorPoint)) {
        Serial.print("Error escritura InfluxDB: ");
        Serial.println(client.getLastErrorMessage());
      }
    }
  }
}