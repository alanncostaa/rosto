#include "esp_camera.h"
#include <WiFi.h>

// ====== CONFIG WI-FI ======
const char* ssid = "brisa-2700561";
const char* password = "n5wllwyo";

// ====== SERVIDOR (PC) ======
const char* serverHost = "192.168.0.7";   // IP do PC
const int   serverPort = 5000;             // Porta do Flask

// ====== PINAGEM ESP32-CAM AI-Thinker ======
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// LED do flash (opcional)
#define LED_PIN 4

const char* buzzerIP = "192.168.108.71";  // IP do outro ESP


void sendBuzzerAlert() {
  WiFiClient client;
  if (client.connect(buzzerIP, 80)) {
    client.println("GET /buzz HTTP/1.1");
    client.println("Host: " + String(buzzerIP));
    client.println("Connection: close");
    client.println();
  }
}

void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Tamanho da imagem (você pode testar SVGA ou VGA para ficar mais leve)
  config.frame_size = FRAMESIZE_SVGA; // 640x480
  config.jpeg_quality = 4;           // 0-63 (quanto menor, melhor qualidade)
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("Erro ao iniciar camera: 0x%x", err);
    while (true) {
      delay(1000);
    }
  }
  sensor_t *s = esp_camera_sensor_get();

  // Nitidez e contraste melhores
  s->set_brightness(s, 1);   // -2 a 2
  s->set_contrast(s, 2);     // -2 a 2
  s->set_saturation(s, 1);   // -2 a 2

  // Reduz ruído digital
  s->set_denoise(s, 1);

  // Reduz borrado
  s->set_sharpness(s, 2);    // -2 a 2

  // Aumenta detalhe nas sombras
  s->set_gainceiling(s, GAINCEILING_32X);

  // Corrige imagem estourada
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 1);

  // Foca melhor no rosto
  s->set_lenc(s, 1);  // Lens correction
  s->set_whitebal(s, 1);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Conectando a %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

String sendFrameToServer(uint8_t* data, size_t len) {
  WiFiClient client;
  if (!client.connect(serverHost, serverPort)) {
    Serial.println("Falha ao conectar ao servidor");
    return "";
  }

  // Requisição HTTP POST simples com corpo = imagem JPEG
  String header = "";
  header += "POST /frame HTTP/1.1\r\n";
  header += "Host: " + String(serverHost) + ":" + String(serverPort) + "\r\n";
  header += "Content-Type: image/jpeg\r\n";
  header += "Content-Length: " + String(len) + "\r\n";
  header += "Connection: close\r\n\r\n";

  client.print(header);
  client.write(data, len);

  // Ler resposta
  String response = "";
  long timeout = millis() + 5000;
  while (client.connected() && millis() < timeout) {
    while (client.available()) {
      char c = client.read();
      response += c;
    }
  }
  client.stop();

  // Separar cabeçalho do corpo
  int bodyIndex = response.indexOf("\r\n\r\n");
  if (bodyIndex >= 0) {
    return response.substring(bodyIndex + 4);
  }
  return response;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Iniciando camera...");
  startCamera();

  Serial.println("Conectando WiFi...");
  connectWiFi();
}

void loop() {
  // Captura frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Falha ao capturar frame");
    delay(500);
    return;
  }

  // (Opcional) acender LED rapidamente para melhorar iluminação
  // digitalWrite(LED_PIN, HIGH);
  // delay(50);
  // digitalWrite(LED_PIN, LOW);

  Serial.println("Enviando frame para servidor...");
  String body = sendFrameToServer(fb->buf, fb->len);

  esp_camera_fb_return(fb);

  Serial.println("Resposta do servidor:");
  Serial.println(body);

  // Se o corpo for JSON tipo {"status":"fadiga"}
  if (body.indexOf("\"status\":\"fadiga\"") >= 0) {
    Serial.println(">>> FADIGA DETECTADA! ENVIANDO BUZZER <<<");
    sendBuzzerAlert();
}

  delay(500); // intervalo entre capturas (ajuste conforme desempenho)
}
