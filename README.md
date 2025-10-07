# 🌾 Smart Dual-Zone IoT Irrigation System (Arduino + ESP32-CAM)

Proyek ini adalah **sistem irigasi otomatis berbasis mikrokontroler** yang mampu mengatur penyiraman, pembuangan air, dan pelaporan kondisi lahan secara **real-time** melalui **Telegram Bot**.  
Sistem terdiri dari dua perangkat utama:
1. **Arduino UNO/Nano** → mengelola sensor tanah, sensor air, relay pompa, dan servo pengarah aliran air.  
2. **ESP32-CAM** → menangani koneksi WiFi, kamera untuk pengawasan visual, komunikasi Telegram Bot, serta sinkronisasi waktu NTP.

---

## 🧩 Fitur Utama
### 🪴 **Modul Arduino (Controller Irigasi)**
- 🚿 **Irigasi Otomatis Dua Zona (Sawah A & B)**  
  Menyalakan pompa utama dan mengarahkan air menggunakan **servo valve**.
- 💧 **Deteksi Kelembaban Tanah & Air Hujan**  
  Menggunakan sensor analog untuk kelembaban dan sensor digital untuk genangan air.
- ⚙️ **Pompa Pembuangan (Drain A & B)**  
  Aktif otomatis saat tanah terlalu basah atau area tergenang.
- 🔄 **Servo Non-Blocking Motion**  
  Servo bergerak halus dengan logika step per step.
- 🧾 **LCD I2C Display (16x2)**  
  Menampilkan status sistem, zona aktif, dan persentase kelembaban tanah.
- 🔁 **Komunikasi Serial ke ESP32-CAM**  
  Mengirim data sensor dan status ke modul ESP32 untuk dikirim ke Telegram.

### 📸 **Modul ESP32-CAM (Monitoring & Telegram)**
- 🌐 **Koneksi Internet via WiFi**
- ⏰ **Sinkronisasi Waktu Otomatis (NTP Server)**
- 🤖 **Integrasi dengan Telegram Bot**
  - `/start` → Menampilkan menu perintah.  
  - `/photo` → Mengambil dan mengirim foto area sawah.  
  - `/flash` → Menghidupkan/mematikan lampu flash ESP32-CAM.  
  - `/cekkondisi` → Meminta laporan kondisi sawah + foto terkini.
- 📷 **Pengambilan Foto Otomatis**
- 💬 **Laporan Telegram Real-Time**
  Menampilkan kondisi tanah, air, pompa utama, drainase, dan waktu terkini.

---

## ⚙️ **Skema Sistem**
📱 Telegram Bot
⬍
🌐 WiFi + NTP
⬍
ESP32-CAM ← Serial TX/RX → Arduino UNO/Nano
↳ Sensor Tanah A (A0)
↳ Sensor Tanah B (A1)
↳ Sensor Air A (A2)
↳ Sensor Air B (A3)
↳ Pompa Utama (Relay D2)
↳ Drain A (Relay D3)
↳ Drain B (Relay D4)
↳ Servo Valve (D5)
↳ LCD I2C (SDA/SCL)

---

## 🔩 **Komponen yang Digunakan**

| No | Komponen | Jumlah | Fungsi |
|----|-----------|---------|--------|
| 1 | Arduino UNO / Nano | 1 | Kontrol logika irigasi dan aktuator |
| 2 | ESP32-CAM | 1 | Pengiriman foto dan laporan Telegram |
| 3 | Sensor kelembaban tanah (Soil Moisture) | 2 | Mendeteksi kelembaban tanah A & B |
| 4 | Sensor hujan / ketinggian air (Water Level) | 2 | Deteksi hujan / genangan |
| 5 | Relay 4 Channel | 1 | Mengendalikan pompa utama & drainase |
| 6 | Servo Motor SG90 | 1 | Mengarahkan aliran air ke zona A/B |
| 7 | LCD I2C 16x2 | 1 | Tampilan status sistem |
| 8 | Pompa air DC / AC | 3 | Irigasi utama & drainase |
| 9 | Breadboard, kabel jumper, power supply | - | Pendukung sistem |

---

## 🧠 **Alur Kerja Sistem**

1. Arduino membaca sensor tanah dan air.  
2. Berdasarkan nilai sensor:
   - Jika **tanah A kering** → pompa utama ON, servo ke arah A.  
   - Jika **tanah B kering** → pompa utama ON, servo ke arah B.  
   - Jika **terdeteksi air hujan / basah** → pompa utama OFF, drainase aktif.
3. Arduino mengirim status dan persentase kelembaban ke ESP32-CAM.
4. ESP32-CAM menampilkan data di Serial Monitor dan mengirim laporan ke Telegram.
5. Pengguna dapat memantau kondisi sawah dan foto terkini langsung dari Telegram Bot.

---

## 🕹️ **Perintah Telegram yang Tersedia**

| Perintah | Fungsi |
|-----------|---------|
| `/start` | Menampilkan daftar perintah |
| `/photo` | Mengambil dan mengirim foto area sawah |
| `/flash` | Menyalakan / mematikan LED Flash ESP32-CAM |
| `/cekkondisi` | Mengirim data kelembaban, status pompa, dan foto terkini |

---

## 🧰 **Library yang Digunakan**

### Arduino UNO
```cpp
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
```

---

## 📸 **Contoh Pesan Telegram**
```yaml
📊 Laporan Kondisi Sawah

🌱 Sensor Tanah:
• Soil A: `65%`
• Soil B: `72%`

💧 Status Air:
• Water A: `Normal`
• Water B: `Normal`

⚙️ Status Perangkat:
• Pompa Utama: *ON*
• Drain A: *OFF*
• Drain B: *OFF*

🚜 Mode Operasi: _Pengisian Sawah A_
📅 Waktu: `07/10/2025 19:42:36`
```

---

## 🗂️ **Struktur File**
📁 SmartDualZoneIrrigation
 - ├── Arduino_Controller/
 - │   └── TX_ARDUINO.ino
 - ├── ESP32_CAM_Controller/
 - │   └── RX_ESP32_CAM.ino
 - ├── picture/
 - │   └── example_telegram_report.jpg
 - └── README.md

---

## 🛠️ **Cara Penggunaan**
1. Upload program Arduino_Controller.ino ke Arduino UNO/Nano.
2. Upload program ESP32_CAM_Controller.ino ke ESP32-CAM.
3. Hubungkan kabel serial:
   - ESP32-CAM TX(15) → Arduino RX(11)
   - ESP32-CAM RX(14) → Arduino TX(10)
4. Pastikan koneksi WiFi aktif dan token Telegram benar.
5. Jalankan sistem, kirim perintah /start dari Telegram untuk memulai.

---

 ## 👨‍💻 **Contacs us :** 
* [Frendi RoboTech](https://www.instagram.com/frendi.co/)
* [Whatsapp : +6287888227410](https://wa.me/+6287888227410)
* [Email    : frendirobotech@gmail.com](https://mail.google.com/mail/u/0/?view=cm&tf=1&fs=1&to=frendirobotech@gmail.com) atau [Email    : frendix45@gmail.com](https://mail.google.com/mail/u/0/?view=cm&tf=1&fs=1&to=frendix45@gmail.com)

---

## 👨‍💻 **Author**
Dikembangkan oleh: Imam Sa'id Nurfrendi [Reog Robotic & Robotech Electronics]  
Lisensi: Open Source (MIT)
