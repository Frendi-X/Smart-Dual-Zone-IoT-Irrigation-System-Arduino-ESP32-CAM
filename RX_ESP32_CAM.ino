/* ------------------------------------------ LIBRARY -------------------------------- */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "time.h"

/* -------------------------------------- KONFIGURASI WIFI ---------------------------- */
const char* ssid = "AndroidAP";
const char* password = "12345678";

/* ---------------------------------------- TELEGRAM BOT ------------------------------ */
String BOTtoken = "8351109574:AAFv5hOaN76bn1pEVpzbS6Tr2cRcYFG803A";    // -----> "8226373116:AAGKx1yQ3FTztb2Y1u8ind1VPY1R3xOkYhc";  PUNYA FRENDI
String CHAT_ID = "1047893131";                                         // -----> "7325127591"; PUNYA FRENDI

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

/* ----------------------------------- KONFIGURASI NTP SERVER -------------------------- */
const char* ntpServer = "pool.ntp.org";  // -----> Server NTP Global
const long gmtOffset_sec = 7 * 3600;     // -----> GMT+7 untuk WIB
const int daylightOffset_sec = 0;        // -----> Offset DST (biasanya 0 di Indonesia)

bool sendPhoto = false;
unsigned long lastTimeBotRan;
int botRequestDelay = 1000;

/* --------------------------------- VARIABEL SERIAL ARDUINO --------------------------- */
String data;
char CharData;
String StringData, dataSubs;
int index1, index2;

/* ---------------------------------- VARIABEL HASIL PARSING --------------------------- */
String soilA, soilB, waterA, waterB;
String pompaUtama, drainA, drainB, modeIrigasi;
int Data_statusTanah,
    kelembabanTanahA,
    kelembabanTanahB;

String status_tanahA, status_tanahB,
       status_mainPompa,
       status_drainPompaA,
       status_drainPompaB,
       aktivitas;

/* -------------------------------------- KONFIGURASI PIN ------------------------------- */
#define FLASH_LED_PIN 4
bool flashState = LOW;

/* ------------------------------------ KONFIGURASI KAMERA ------------------------------ */
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

/* ================================ FUNGSI INISIALISASI KAMERA ========================== */
void configInitCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 14;
    config.fb_count = 1;
  }

  // Inisialisasi kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    delay(2000);
    ESP.restart();
  }

  // Ubah resolusi jika perlu
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
}

/* ==================================== FUNGSI KIRIM FOTO ================================ */
String sendPhotoTelegram() {
  const char* server = "api.telegram.org";
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "Camera capture failed";
  }

  if (!clientTCP.connect(server, 443)) {
    Serial.println("Connection to Telegram failed!");
    return "Connection failed";
  }

  String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\";\r\n\r\n" + CHAT_ID +
                "\r\n--boundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--boundary--\r\n";

  clientTCP.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");
  clientTCP.println("Host: api.telegram.org");
  clientTCP.println("Content-Length: " + String(fb->len + head.length() + tail.length()));
  clientTCP.println("Content-Type: multipart/form-data; boundary=boundary");
  clientTCP.println();
  clientTCP.print(head);
  clientTCP.write(fb->buf, fb->len);
  clientTCP.print(tail);

  esp_camera_fb_return(fb);

  // Tunggu respons
  long timeout = millis() + 10000;
  String response;
  while (millis() < timeout && clientTCP.connected()) {
    while (clientTCP.available()) {
      char c = clientTCP.read();
      response += c;
    }
  }

  clientTCP.stop();
  Serial.println("Photo sent!");
  return response;
}

/* ==================================== FUNGSI RECONNECT WIFI ============================= */
void reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🔄 Reconnecting WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Reconnected to WiFi!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\n❌ WiFi Reconnect Failed!");
    }
  }
}

/* =============================== FUNGSI HANDLE PESAN TELEGRAM =========================== */
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user!", "");
      continue;
    }

    if (text == "/start") {
      String welcome = "Selamat datang di Sistem Irigasi Otomatis 🌾\n\n";
      welcome += "Perintah yang tersedia:\n";
      welcome += "/photo - Ambil foto area\n";
      welcome += "/flash - Nyalakan/matikan flash\n";
      welcome += "/cekkondisi - Cek kondisi sawah + kirim foto";
      bot.sendMessage(CHAT_ID, welcome, "");
    }

    else if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      bot.sendMessage(CHAT_ID, flashState ? "💡 Flash ON" : "💡 Flash OFF", "");
    }

    else if (text == "/photo") {
      sendPhoto = true;
    }

    else if (text == "/cekkondisi") {
      Serial2.println("#1?"); // kirim ke Arduino
      bot.sendMessage(CHAT_ID, "🔍 Memeriksa kondisi sawah...", "");
      sendPhoto = true;
    }
  }
}

/* ==================================== FUNGSI KIRIM TELEGRAM ============================== */
void kirimTelegram() {
  String pesan = "📊 *Laporan Kondisi Sawah*\n\n";
  pesan += "🌱 *Sensor Tanah:*\n";
  pesan += "• Soil A: `" + String(kelembabanTanahA) + "`\n";
  pesan += "• Soil B: `" + String(kelembabanTanahB) + "`\n\n";

  pesan += "💧 *Status Air:*\n";
  pesan += "• Water A: `" + status_tanahA + "`\n";
  pesan += "• Water B: `" + status_tanahB + "`\n\n";

  pesan += "⚙️ *Status Perangkat:*\n";
  pesan += "• Pompa Utama: *" + status_mainPompa + "*\n";
  pesan += "• Drain A: *" + status_drainPompaA + "*\n";
  pesan += "• Drain B: *" + status_drainPompaB + "*\n\n";

  pesan += "🚜 *Mode Operasi:* _" + aktivitas + "_\n";
  pesan += "\n📅 Waktu: `" + waktuSekarang() + "`";

  bot.sendMessage(CHAT_ID, pesan, "Markdown");
  Serial.println("✅ Pesan terkirim ke Telegram.");
}

/* ================================== FUNGSI AMBIL DATA WAKTU NTP =========================== */
String waktuSekarang() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Tidak tersedia";
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

/* ============================================ SETUP() ===================================== */
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 15, 14);      // -----> Serial Komunikasi Arduino

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  configInitCamera();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  Serial.print("🔌 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\n✅ Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  /* ---------------------------------- SINKRON WAKTU NTP SRVER ----------------------------- */
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Menunggu sinkronisasi waktu NTP...");
  delay(1000);

  Serial.println("Waktu sekarang: " + waktuSekarang());
}

/* ============================================ LOOP() ====================================== */
void loop() {
  /* ----------------------------------- WIFI SELALU TERHUBUNG ------------------------------ */
  reconnect();

  /* ---------------------------------- BACA DATA DARI ARDUINO ------------------------------ */
  while (Serial2.available() > 0)
  {
    delay(10);
    CharData = Serial2.read();
    StringData += CharData;

    /* ------------------ PARSING DATA MASUK DARI ARDUINO -> DATA STATUS TANAH -------------- */
    if (StringData.length() > 0 && CharData == '!')
    {
      index1 = StringData.indexOf('#');
      index2 = StringData.indexOf('!', index1 + 1);
      dataSubs = StringData.substring(index1 + 1, index2);
      StringData = "";

      // Mengubah Data String ke Float
      char buf[dataSubs.length()];
      dataSubs.toCharArray(buf, dataSubs.length() + 1);
      float Data11 = atof(buf);

      /* ------------------------ DATA MASUK DARI ARDUINO (STATUS TANAH) --------------------- */
      Data_statusTanah = Data11;
      Serial.print("Status Tanah : ");
      Serial.println(Data_statusTanah);

      /* ------------------------------------ 1 -> STATUS ------------------------------------ */
      if (Data_statusTanah == 1) {
        status_tanahA = "KERING";
        status_mainPompa = "ON";
        aktivitas = "Pengisian Sawah A";
      }
      /* ------------------------------------ 2 -> STATUS ------------------------------------ */
      else if (Data_statusTanah == 2) {
        status_tanahB = "KERING";
        status_mainPompa = "ON";
        aktivitas = "Pengisian Sawah B";
      }
      /* ------------------------------------ 3 -> STATUS ------------------------------------ */
      else if (Data_statusTanah == 3) {
        status_tanahA = "NORMAL";
        status_tanahB = "NORMAL";
        status_mainPompa = "OFF";
        aktivitas = "Standby - Tanah Normal";
      }
      /* ------------------------------------ 4 -> STATUS ------------------------------------ */
      else if (Data_statusTanah == 4) {
        status_tanahA = "NORMAL";
        status_tanahB = "NORMAL";
        status_mainPompa = "OFF";
        status_drainPompaA = "OFF";
        status_drainPompaB = "OFF";
        aktivitas = "Standby - Tanah Normal";
      }

      /* ------------------------------------ 11 -> STATUS ------------------------------------ */
      if (Data_statusTanah == 11) {
        status_tanahA = "NORMAL";
        status_drainPompaA = "ON";
        aktivitas = "Pembuangan A - Tanah Normal";
      }
      /* ------------------------------------ 22 -> STATUS ------------------------------------ */
      else if (Data_statusTanah == 22) {
        status_tanahA = "NORMAL";
        status_drainPompaA = "OFF";
        aktivitas = "Standby - Tanah Normal";
      }
      /* ------------------------------------ 33 -> STATUS ------------------------------------ */
      else if (Data_statusTanah == 33) {
        status_tanahB = "NORMAL";
        status_drainPompaB = "ON";
        aktivitas = "Pembuangan B - Tanah Normal";
      }
      /* ------------------------------------ 44 -> STATUS ------------------------------------ */
      else if (Data_statusTanah == 44) {
        status_tanahB = "NORMAL";
        status_drainPompaB = "OFF";
        aktivitas = "Standby - Tanah Normal";
      }
    }

    /* ------------- PARSING DATA MASUK DARI ARDUINO -> NILAI KELEMBABAN (%) SAWAH A ---------- */
    if (StringData.length() > 0 && CharData == '@')
    {
      index1 = StringData.indexOf('#');
      index2 = StringData.indexOf('@', index1 + 1);
      dataSubs = StringData.substring(index1 + 1, index2);
      StringData = "";

      //Mengubah Data String ke Float
      char buf[dataSubs.length()];
      dataSubs.toCharArray(buf, dataSubs.length() + 1);
      float Data = atof(buf);

      /* ------------------- DATA MASUK DARI ARDUINO ( PERSENTASE KELEMBABAN A) ---------------- */
      kelembabanTanahA = Data;
      Serial.print("Kelembaban Tanah A : ");
      Serial.println(kelembabanTanahA);

    }

    /* ------------- PARSING DATA MASUK DARI ARDUINO -> NILAI KELEMBABAN (%) SAWAH B ---------- */
    if (StringData.length() > 0 && CharData == '&')
    {
      index1 = StringData.indexOf('#');
      index2 = StringData.indexOf('&', index1 + 1);
      dataSubs = StringData.substring(index1 + 1, index2);
      StringData = "";

      //Mengubah Data String ke Float
      char buf[dataSubs.length()];
      dataSubs.toCharArray(buf, dataSubs.length() + 1);
      float Data = atof(buf);

      /* ------------------- DATA MASUK DARI ARDUINO ( PERSENTASE KELEMBABAN B) ---------------- */
      kelembabanTanahB = Data;
      Serial.print("Kelembaban Tanah B : ");
      Serial.println(kelembabanTanahB);

      /* ----------------------------------- KIRIM PESAN TELEGRAM ------------------------------ */
      kirimTelegram();
    }
  }

  /* ------------------------------ KIRIM FOTO KE TELEGRAM JIKA DIMINTA ------------------------ */
  if (sendPhoto) {
    bot.sendMessage(CHAT_ID, "📸 Mengambil foto...");
    sendPhotoTelegram();
    sendPhoto = false;
  }

  /* --------------------------------- CEK PESAN MASUK DARI TELEGRAM --------------------------- */
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages) handleNewMessages(numNewMessages);
    lastTimeBotRan = millis();
  }
}
