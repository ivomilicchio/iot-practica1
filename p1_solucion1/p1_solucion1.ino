#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <DHT.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// --- CONFIGURACIÓN INFLUXDB ---
// Nota: En la nube, usualmente no se usa el puerto :8086 en la URL, se usa el estándar HTTPS
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com/" 
#define INFLUXDB_TOKEN "limXJwqE4gAztqRgZcJDwAbAj0C260Hpo2UUnI5rkC5ris9MBda4kZmLWBmX1WIrJ57IzmQ2nCv4nnIlbs6uLg=="
#define INFLUXDB_ORG "ElPapuIoT"
#define INFLUXDB_BUCKET "p1_monitor"
#define TZ_INFO "UTC-3" 

// Declaración del cliente (Solo la declaración aquí)
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensorPoint("clima");

const int SERVER_PORT = 80;
const int BAUD_RATE = 115200;
const int LED_PIN = 2; 
const int DHT_PIN = 22;
const uint8_t DHT_TYPE = DHT11;

DHT dht(DHT_PIN, DHT_TYPE);
bool led_on = false;
WiFiManager wifiManager;
AsyncWebServer server(SERVER_PORT);

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(LED_PIN, OUTPUT);
  //dht.begin();


  // --- CORRECCIÓN: Configuración del cliente dentro de una función ---
  client.setInsecure(); 
  // --- SINCRONIZACIÓN HORA (Crítico para InfluxDB Cloud) ---
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

  server.on("/data", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"temp\":" + String(dht.readTemperature()) + 
                  ",\"hum\":" + String(dht.readHumidity()) + 
                  ",\"led\":" + String(led_on ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });

  server.on("/history", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    String query = "from(bucket: \"" + String(INFLUXDB_BUCKET) + "\") "
                   "|> range(start: -1h) "
                   "|> filter(fn: (r) => r._measurement == \"clima\") "
                   "|> pivot(rowKey:[\"_time\"], columnKey: [\"_field\"], valueColumn: \"_value\") "
                   "|> limit(n:10)";
    
    FluxQueryResult result = client.query(query);
    String jsonOutput = "[";
    while (result.next()) {
        if (jsonOutput != "[") jsonOutput += ",";
        jsonOutput += "{\"timestamp\":\"" + result.getValueByName("_time").getDateTime().format("%Y-%m-%d %H:%M:%S") + "\",";
        jsonOutput += "\"temp\":" + String(result.getValueByName("temperatura").getDouble()) + ",";
        jsonOutput += "\"hum\":" + String(result.getValueByName("humedad").getDouble()) + "}";
    }
    jsonOutput += "]";
    result.close();
    request->send(200, "application/json", jsonOutput);
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
}

unsigned long milisAnteriores = 0;
const long intervaloReintento = 20000;
bool modoPortalActivo = false;

void loop() {
  if (millis() - milisAnteriores >= intervaloReintento) {
    milisAnteriores = millis();
    if (WiFi.status() != WL_CONNECTED && !modoPortalActivo) {
      modoPortalActivo = true;
      server.end();
      wifiManager.setConfigPortalTimeout(120);
      if (!wifiManager.startConfigPortal("Rescate_ESP32", "password")) {
        modoPortalActivo = false;
      } else {
        ESP.restart();
      }
    }
  }

  static unsigned long lastDbWrite = 0;
  if (millis() - lastDbWrite > 30000) { 
    lastDbWrite = millis();
    
    /*
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    */
    float t = random(10.0, 30.0);
    float h = random(40.0, 60.0);
    
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