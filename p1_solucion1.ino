#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>

const int SERVER_PORT = 80;
const int BAUD_RATE = 115200;
const int LED_PIN = 2; 

//WiFi Credentials
const char* ssid = "Fibertel WiFi974 2.4GHz";
const char* pswd = "0142051897";

bool led_on = false;

AsyncWebServer server(SERVER_PORT);

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(LED_PIN, OUTPUT);

  if (!LittleFS.begin()) {
    Serial.println("Error montando LittleFS");
  return;
  }

  // WIFI CONFIG
  WiFi.begin(ssid, pswd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado.");
  Serial.println(WiFi.localIP());

  // Archivos estáticos
  server.serveStatic("/assets/", LittleFS, "/assets/");

  // HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  // RUTA DE ACTUALIZACIÓN
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("led_state")) {
    led_on = request->getParam("led_state")->value() == "1";
    digitalWrite(LED_PIN, led_on ? HIGH : LOW);
  }
  request->send(200, "application/json", "{\"led\":" + String(led_on ? "true" : "false") + "}");
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"temp\":" + String(random(10, 30)) + ",";
    json += "\"hum\":"  + String(random(10, 30)) + ",";
    json += "\"led\":"  + String(led_on ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
});

  server.begin();
}

void loop() {

}