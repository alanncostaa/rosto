#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Rede IFALArapiraca";
const char* password = "redeifal";

#define BUZZER_PIN 13   // ajuste o pino do buzzer

WebServer server(80);

void handleBuzz() {
  Serial.println(">>> BUZZ ON <<<");

  // toca o buzzer
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);

  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nConectado!");
  Serial.print("IP do ESP BUZZER: ");
  Serial.println(WiFi.localIP());

  server.on("/buzz", handleBuzz);
  server.begin();
}

void loop() {
  server.handleClient();
}
