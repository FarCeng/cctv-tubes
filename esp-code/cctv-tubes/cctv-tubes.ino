#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// == PENGATURAN PENGGUNA ==
const char* ssid = "OPPOF";
const char* password = "12345678";
const char* serverUrl = "https://farceng.pythonanywhere.com/";

// Pinout Kamera (AI Thinker) - Tidak ada perubahan
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22
// Pin Perangkat Keras Tambahan
#define FLASH_LED_PIN    4
#define SERVO_TILT_PIN 12
#define SERVO_PAN_PIN  13
#define SERVO_FREQUENCY 50
#define PWM_RESOLUTION 16
#define SERVO_MIN_PULSE_WIDTH 3277
#define SERVO_MAX_PULSE_WIDTH 6554
#define LEDC_CHANNEL_LED  1
#define LEDC_CHANNEL_PAN  2
#define LEDC_CHANNEL_TILT 3

// Variabel untuk menyimpan state terakhir
int lastPan = 90;
int lastTilt = 90;
int lastLedIntensity = 0;

// Variabel status
bool isStreaming = false;

// Variabel untuk timing
unsigned long previousMillis = 0;
const long interval = 500;

// Deklarasi fungsi
void initializeCamera();
void connectToWifi();
void getAndExecuteCommands();
void streamVideo();
void captureAndSendImage();
void setServoAngle(int channel, int angle); // Fungsi baru untuk kontrol servo

void setup() {
  Serial.begin(115200);
  // Inisialisasi kamera
  initializeCamera();
  // Setup PWM untuk LED menggunakan channel 1
  ledcSetup(LEDC_CHANNEL_LED, 5000, 8);
  ledcAttachPin(FLASH_LED_PIN, LEDC_CHANNEL_LED);
  ledcWrite(LEDC_CHANNEL_LED, 0);
  // Setup PWM untuk SERVO PAN menggunakan channel 2
  ledcSetup(LEDC_CHANNEL_PAN, SERVO_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(SERVO_PAN_PIN, LEDC_CHANNEL_PAN);
  // Setup PWM untuk SERVO TILT menggunakan channel 3
  ledcSetup(LEDC_CHANNEL_TILT, SERVO_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(SERVO_TILT_PIN, LEDC_CHANNEL_TILT);
  // Atur posisi awal servo menggunakan fungsi baru
  setServoAngle(LEDC_CHANNEL_PAN, lastPan);
  setServoAngle(LEDC_CHANNEL_TILT, lastTilt);
  // Hubungkan ke WiFi
  connectToWifi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus. Mencoba menghubungkan kembali...");
    connectToWifi();
    delay(5000);
    return;
  }
  
  if (isStreaming) {
    streamVideo();
    getAndExecuteCommands();
  } else {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      getAndExecuteCommands();
    }
  }
}

// Fungsi untuk mengubah nilai servo
void setServoAngle(int channel, int angle) {
  // Memetakan sudut 0-180 ke rentang pulse width yang sesuai
  int pulseWidth = map(angle, 0, 180, SERVO_MIN_PULSE_WIDTH, SERVO_MAX_PULSE_WIDTH);
  // Menulis nilai duty cycle ke channel PWM yang ditentukan
  ledcWrite(channel, pulseWidth);
}

void getAndExecuteCommands() {
  HTTPClient http;
  String url = String(serverUrl) + "/get-commands";
  http.begin(url);
  
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<512> doc; 
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("deserializeJson() GAGAL! Kode error: ");
      Serial.println(error.c_str());
      http.end();
      return;
    }

    Serial.println("Parsing JSON berhasil, memproses perintah...");

    isStreaming = doc["stream"];
    int pan = doc["servo_pan"];
    int tilt = doc["servo_tilt"];
    int ledIntensity = doc["led_intensity"];
    
    // Atur gerakan Servo
    if (pan != lastPan) {
      setServoAngle(LEDC_CHANNEL_PAN, pan); // Panggil fungsi servo untuk Pan
      lastPan = pan;
      Serial.print("Pan set to: ");
      Serial.println(pan);
    }

    if (tilt != lastTilt) {
      setServoAngle(LEDC_CHANNEL_TILT, tilt); // Panggil fungsi servo untuk Tilt
      lastTilt = tilt;
      Serial.print("Tilt set to: ");
      Serial.println(tilt);
    }

    // Atur nilai intensitas LED 
    if (ledIntensity != lastLedIntensity) {
        ledcWrite(LEDC_CHANNEL_LED, ledIntensity);
        lastLedIntensity = ledIntensity;
        Serial.print("LED Intensity set to: ");
        Serial.println(ledIntensity);
    }

    if (doc["capture"].as<bool>()) {
      Serial.println("Perintah capture diterima, mengambil gambar...");
      captureAndSendImage();
    }
    
  } else {
    if(httpResponseCode > 0) {
        Serial.printf("[HTTP] GET... gagal, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
  }

  http.end();
}

void streamVideo() {
 if (!isStreaming) return;
 camera_fb_t * fb = esp_camera_fb_get();
 if (!fb) {
  Serial.println("Gagal mengambil frame kamera");
  return;
 }
 HTTPClient http;
 String url = String(serverUrl) + "/upload-frame";
 http.begin(url);
 http.addHeader("Content-Type", "image/jpeg");
 int httpResponseCode = http.POST(fb->buf, fb->len);
 if (httpResponseCode != 200 && httpResponseCode > 0) {
  Serial.printf("[STREAM] Gagal mengirim frame, kode: %d\n", httpResponseCode);
 }
 http.end();
 esp_camera_fb_return(fb);
}

void captureAndSendImage() {
 camera_fb_t * fb = esp_camera_fb_get();
 if (!fb) {
  Serial.println("Gagal mengambil frame dari kamera");
  return;
 }
 HTTPClient http;
 String url = String(serverUrl) + "/upload-image";
 http.begin(url);
 http.addHeader("Content-Type", "image/jpeg");
 int httpResponseCode = http.POST(fb->buf, fb->len);
 if (httpResponseCode == 200) {
  Serial.println("Gambar berhasil di-upload.");
 } else {
  Serial.printf("[CAPTURE] Error mengirim gambar: %d - %s\n", httpResponseCode, http.errorToString(httpResponseCode).c_str());
 }
 http.end();
 esp_camera_fb_return(fb);
}

void initializeCamera() {
 camera_config_t config;
 config.ledc_channel = LEDC_CHANNEL_0;
 config.ledc_timer = LEDC_TIMER_0;
 config.pin_d0 = Y2_GPIO_NUM;
 config.pin_d1 = Y3_GPIO_NUM;
 config.pin_d2 = Y4_GPIO_NUM;
 config.pin_d3 = Y5_GPIO_NUM;
 config.pin_d4 = Y6_GPIO_NUM;
 config.pin_d5 = Y7_GPIO_NUM;
 config.pin_d6 = Y8_GPIO_NUM;
 config.pin_d7 = Y9_GPIO_NUM;
 config.pin_xclk = XCLK_GPIO_NUM;
 config.pin_pclk = PCLK_GPIO_NUM;
 config.pin_vsync = VSYNC_GPIO_NUM;
 config.pin_href = HREF_GPIO_NUM;
 config.pin_sccb_sda = SIOD_GPIO_NUM;
 config.pin_sccb_scl = SIOC_GPIO_NUM;
 config.pin_pwdn = PWDN_GPIO_NUM;
 config.pin_reset = RESET_GPIO_NUM;
 config.xclk_freq_hz = 20000000;
 config.pixel_format = PIXFORMAT_JPEG;
 config.frame_size = FRAMESIZE_VGA;
 config.jpeg_quality = 12;
 config.fb_count = 2;
 esp_err_t err = esp_camera_init(&config);
 if (err != ESP_OK) {
  Serial.printf("Inisialisasi kamera gagal dengan error 0x%x", err);
  ESP.restart();
 }
}

void connectToWifi() {
 WiFi.begin(ssid, password);
 Serial.print("Menghubungkan ke WiFi");
 while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
 }
 Serial.println("\nWiFi terhubung!");
 Serial.print("Alamat IP ESP32: ");
 Serial.println(WiFi.localIP());
}
