#include <WiFiManager.h>
#include <WiFi.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("TEST");
  WiFiManager wm;
  wm.resetSettings();
  Serial.print("Wifi guardado? -->");
  Serial.println(WiFi.status());


}

void loop() {
  // put your main code here, to run repeatedly:

}
