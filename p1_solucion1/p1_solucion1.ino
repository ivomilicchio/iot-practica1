#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <DHT.h>

const int SERVER_PORT = 80;
const int BAUD_RATE = 115200;
const int LED_PIN = 2; 

const int     DHT_PIN  = 22;
const uint8_t DHT_TYPE = DHT11;

DHT dht(DHT_PIN, DHT_TYPE);

bool led_on = false;
WiFiManager wifiManager;
AsyncWebServer server(SERVER_PORT);


void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(LED_PIN, OUTPUT);

  dht.begin();

  // Montar LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS no pudo iniciarse.");
  }

  // CONFIGURACION DE ENDPOINTS
  server.serveStatic("/assets/", LittleFS, "/assets/");

  server.on("/", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "Archivo index.html no encontrado en LittleFS");
    }
  });

  server.on("/update", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("led_state")) {
      led_on = request->getParam("led_state")->value() == "1";
      digitalWrite(LED_PIN, led_on ? HIGH : LOW);
    }
    request->send(200, "application/json", "{\"led\":" + String(led_on ? "true" : "false") + "}");
  });

  server.on("/data", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"temp\":" + String(dht.readTemperature()) + ",";
    json += "\"hum\":"  + String(dht.readHumidity()) + ",";
    json += "\"led\":"  + String(led_on ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  // WIFI MANAGER

  // Manejo de nuevas credenciales
  wifiManager.setSaveConfigCallback([](){
    // Esta función se activa al cambiar las credenciales
    // Reinicia el ESP para que se inicialice correctamente el servidor web
      Serial.println("Configuración guardada. Reiniciando en 2 segundos...");
      delay(2000); 
      ESP.restart(); 
  });

  // Disparar AP cuando no hay WiFi por determinado tiempo
  // Permite cambio de red

  wifiManager.setConnectTimeout(20);


  Serial.println("Iniciando WiFiManager");
  //wifiManager.resetSettings(); 
  bool res = wifiManager.autoConnect("Portal_Config_ESP32", "password");
  if (!res) {
    Serial.println("Fallo en la conexión");
  } else {
    Serial.print("WIFI GUARDADO? ");
    Serial.println(wifiManager.getWiFiIsSaved());
    Serial.println(wifiManager.getWiFiSSID());
    Serial.println(wifiManager.getWiFiPass());
  }


  // INICIAR SERVIDOR
  server.begin();

  Serial.println("Servidor iniciado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

unsigned long milisAnteriores = 0;
const long intervaloReintento = 20000; // 20 segundos de espera
bool modoPortalActivo = false;

void loop() {
  // Verificamos el estado del WiFi cada 20 segundos
  unsigned long milisActuales = millis();

  if (milisActuales - milisAnteriores >= intervaloReintento) {
    milisAnteriores = milisActuales;

    if (WiFi.status() != WL_CONNECTED && !modoPortalActivo) {
      Serial.println("¡Conexión perdida! Iniciando portal de rescate...");
      
      modoPortalActivo = true; // Evitamos que el loop intente abrir el portal varias veces

      server.end();
      delay(100);
      
      
      // Configuramos un tiempo de espera (timeout)
      // Si en 120 segundos nadie se conecta al portal, el ESP sigue con su loop
      wifiManager.setConfigPortalTimeout(120);

      // Abrimos el portal. Esta línea detiene el loop temporalmente hasta que:
      // 1. Se configure una red exitosamente.
      // 2. Se agote el tiempo (timeout).
      if (!wifiManager.startConfigPortal("Rescate_ESP32", "password")) {
        Serial.println("Portal cerrado por timeout. Reintentando en 20s...");
        modoPortalActivo = false;
      } else {
        // Si llegamos aquí, el usuario configuró el WiFi con éxito
        Serial.println("Reconectado!");
        ESP.restart(); // Reiniciamos para limpiar el stack de red y arrancar el servidor
      }
    }
  }
}