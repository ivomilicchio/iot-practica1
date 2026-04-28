#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <WiFiManager.h>

const int SERVER_PORT = 80;
const int BAUD_RATE = 115200;
const int LED_PIN = 2; 

bool led_on = false;
AsyncWebServer server(SERVER_PORT);
WiFiManager wifiManager;

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(LED_PIN, OUTPUT);

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
    json += "\"temp\":" + String(random(10, 30)) + ",";
    json += "\"hum\":"  + String(random(10, 30)) + ",";
    json += "\"led\":"  + String(led_on ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  // WIFI MANAGER
  Serial.println("Iniciando WiFiManager");
  //wifiManager.resetSettings(); 
  
  if (!wifiManager.autoConnect("Portal_Config_ESP32")) {
    Serial.println("Fallo en la conexión, reiniciando");
    delay(3000);
    ESP.restart();
  }

  // INICIAR SERVIDOR
  server.begin();
  Serial.println("Servidor iniciado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  
}