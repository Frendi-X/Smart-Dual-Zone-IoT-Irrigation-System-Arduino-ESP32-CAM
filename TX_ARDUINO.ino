/* ------------------------------------------ LIBRARY ----------------------------------- */
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

/* ----------------------------------------- PIN SETUP ----------------------------------- */
#define SOIL_A A0
#define SOIL_B A1
#define WATER_A A2
#define WATER_B A3

#define RELAY_MAIN 2
#define RELAY_DRAIN_A 3
#define RELAY_DRAIN_B 4

#define SERVO_PIN 5

#define RX_PIN 11
#define TX_PIN 10

#define KERING LOW
#define BASAH HIGH

/* ------------------------------------------- OBJEK ------------------------------------- */
Servo servoIrigasi;
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial mySerial(RX_PIN, TX_PIN);

/* ------------------------------------- KONFIGURASI BATAS ------------------------------- */
const int soilDryThreshold = 430;     // -----> Tanah kering di atas nilai ini
const int soilWetThreshold = 300;     // -----> Tanah basah di bawah nilai ini

/* -------------------------------------- VARIABEL STATUS --------------------------------- */
bool irrigatingA = false;
bool irrigatingB = false;
String lcdMessage = "";
String statusA, statusB;
String serialInput = "";

/* -------------------------------------- VARIABEL SERVO ---------------------------------- */
int currentAngle = 0;
int targetAngle = 0;
unsigned long lastMove = 0;
const int stepDelay = 1;              // -----> Jeda per langkah (ms)
const int stepSize = 3;               // -----> Ukuran langkah (derajat per update)
bool servoMoving = false;

const int servoToA = 0;
const int servoToB = 180;
const int servoMiddle = 90;

/* ---------------------------------- VARIABEL PARSING DATA ------------------------------- */
String data;
char CharData;
String StringData, dataSubs;
int index1, index2;
int Data;

/* ----------------------------------- VARIABEL DATA SENSOR -------------------------------- */
int PersentaseA, PersentaseB;
int soilA, soilB, waterA, waterB;

/* ------------------------------- SETUP ----------------------------- */
void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  pinMode(RELAY_MAIN, OUTPUT);
  pinMode(RELAY_DRAIN_A, OUTPUT);
  pinMode(RELAY_DRAIN_B, OUTPUT);

  pinMode(SOIL_A, INPUT);
  pinMode(SOIL_B, INPUT);
  pinMode(WATER_A, INPUT_PULLUP);
  pinMode(WATER_B, INPUT_PULLUP);

  digitalWrite(RELAY_MAIN, HIGH);
  digitalWrite(RELAY_DRAIN_A, HIGH);
  digitalWrite(RELAY_DRAIN_B, HIGH);

  servoIrigasi.attach(SERVO_PIN);
  servoIrigasi.write(servoMiddle);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Sistem Irigasi ");
  lcd.setCursor(0, 1);
  lcd.print("    Otomatis    ");
  delay(2000);
  lcd.clear();

  Serial.println("=== SISTEM IRIGASI OTOMATIS A/B SIAP ===");
}

/* -------------------------------------------- LOOP -------------------------------------- */
void loop() {

  /* ------------------------------- CEK PESAN DARI ESP32 CAM ----------------------------- */
  checkSerialCommand();

  /* ------------------------------------- BACA SENSOR ------------------------------------ */
  soilA = analogRead(SOIL_A);
  soilB = analogRead(SOIL_B);
  waterA = digitalRead(WATER_A);
  waterB = digitalRead(WATER_B);

  /* ----------------------------------- LOGIKA IRIGASI ----------------------------------- */
  if (soilA > soilDryThreshold && waterA == KERING) {
    irrigateToA(waterA);  // -------> Tanah A kering, tidak hujan → nyalakan pompa utama untuk A
  }
  else if (soilB > soilDryThreshold && waterB == KERING) {
    irrigateToB(waterB);  // -------> Tanah B kering, tidak hujan → nyalakan pompa utama untuk B
  }
  else if (waterA == BASAH || waterB == BASAH) {
    stopIrrigation();     // -------> Jika ada hujan di A atau B → hentikan penyiraman
  }
  else {
    stopIrrigation();     // -------> Tidak perlu penyiraman
  }

  /* ----------------------------- LOGIKA PEMBUANGAN - ZONA A ------------------------------ */
  if (soilA <= soilDryThreshold && waterA == BASAH) {
    drainA(waterA);       // -------> Tanah A basah + hujan → nyalakan drainase A
  }
  else if (waterA == KERING) {
    stopDrainA();         // -------> Air surut → matikan drainase A
  }

  /* ----------------------------- LOGIKA PEMBUANGAN - ZONA B ------------------------------ */
  if (soilB <= soilDryThreshold && waterB == BASAH) {
    drainB(waterB);       // -------> Tanah B basah + hujan → nyalakan drainase B
  }
  else if (waterB == KERING) {
    stopDrainB();         // -------> Air surut → matikan drainase B
  }

  /* ------------------------------------- UPDATE LCD --------------------------------------- */
  updateLCD(soilA, soilB, waterA, waterB);

  /* ---------------------------------- UPDATE STEP SERVO ----------------------------------- */
  updateServoSmooth();
}



/* ------------------------------------- SET TARGET SERVO ----------------------------------- */
void setServoTarget(int angle) {
  targetAngle = constrain(angle, 0, 180);
  servoMoving = true;
}

/* ----------------------------- UPDATE POSISI SERVO - NON BLOCKING -------------------------- */
void updateServoSmooth() {
  if (!servoMoving) return;

  unsigned long now = millis();
  if (now - lastMove >= stepDelay) {
    lastMove = now;

    if (abs(targetAngle - currentAngle) <= stepSize) {
      currentAngle = targetAngle;
      servoIrigasi.write(currentAngle);
      servoMoving = false;
    } else if (currentAngle < targetAngle) {
      currentAngle += stepSize;
      servoIrigasi.write(currentAngle);
    } else if (currentAngle > targetAngle) {
      currentAngle -= stepSize;
      servoIrigasi.write(currentAngle);
    }
  }
}

/* ----------------------------------- FUNGSI IRIGASI - SAWAH A ------------------------------- */
void irrigateToA(int waterLevelA) {
  setServoTarget(servoToA);
  digitalWrite(RELAY_MAIN, LOW);
  irrigatingA = true;
  irrigatingB = false;
  lcdMessage = "Irigasi Sawah A";

  if (waterLevelA == BASAH) stopIrrigation();
}

/* ----------------------------------- FUNGSI IRIGASI - SAWAH B ------------------------------- */
void irrigateToB(int waterLevelB) {
  setServoTarget(servoToB);
  digitalWrite(RELAY_MAIN, LOW);
  irrigatingB = true;
  irrigatingA = false;
  lcdMessage = "Irigasi Sawah B";

  if (waterLevelB == BASAH) stopIrrigation();
}

/* -------------------------------------- FUNGSI STOP IRIGASI ---------------------------------- */
void stopIrrigation() {
  if (irrigatingA || irrigatingB) {
    //Serial.println("Irigasi berhenti, air cukup.");
  }
  irrigatingA = false;
  irrigatingB = false;
  digitalWrite(RELAY_MAIN, HIGH);
  setServoTarget(servoMiddle);
  lcdMessage = "Pompa Utama OFF";
}

/* ---------------------------------- FUNGSI PEMBUANGAN - SAWAH A ------------------------------ */
void drainA(int waterLevelA) {
  digitalWrite(RELAY_DRAIN_A, LOW);
  lcdMessage = "Pembuangan A ON";
  if (waterLevelA == KERING) stopDrainA();
}

/* ---------------------------------- FUNGSI PEMBUANGAN - SAWAH B ------------------------------ */
void drainB(int waterLevelB) {
  digitalWrite(RELAY_DRAIN_B, LOW);
  lcdMessage = "Pembuangan B ON";
  if (waterLevelB == KERING) stopDrainB();
}

/* -------------------------------- FUNGSI STOP PEMBUANGAN - SAWAH A ---------------------------- */
void stopDrainA() {
  digitalWrite(RELAY_DRAIN_A, HIGH);
}

/* -------------------------------- FUNGSI STOP PEMBUANGAN - SAWAH B ---------------------------- */
void stopDrainB() {
  digitalWrite(RELAY_DRAIN_B, HIGH);
}

/* ---------------------------------------- FUNGSI UPDATE LCD ----------------------------------- */
void updateLCD(int soilA, int soilB, int waterA, int waterB) {
  lcd.setCursor(0, 0);
  lcd.print(lcdMessage + "    ");

  lcd.setCursor(0, 1);
  lcd.print("SA:");
  PersentaseA = map(soilA, 280, 460, 100, 0);
  if (PersentaseA < 0) {
    lcd.print("0");
    lcd.print("% ");
  }
  else if (PersentaseA > 100) {
    lcd.print("100");
    lcd.print("% ");
  }
  else {
    lcd.print(PersentaseA);     // -----> Konversi ke Persentase
    lcd.print("% ");
  }


  lcd.print("SB:");
  PersentaseB = map(soilB, 280, 460, 100, 0);
  if (PersentaseB < 0) {
    lcd.print("0");
    lcd.print("% ");
  }
  else if (PersentaseB > 100) {
    lcd.print("100");
    lcd.print("% ");
  }
  else {
    lcd.print(PersentaseB);     // -----> Konversi ke Persentase
    lcd.print("% ");
  }

  Serial.print("Soil A  : ");
  Serial.print(soilA);
  Serial.print("\t" + String(map(soilA, 430, 300, 0, 100)));
  Serial.print("\t | \t Soil B  : ");
  Serial.print(soilB);
  Serial.print("\t" + String(map(soilB, 430, 300, 0, 100)) + "\n");

  Serial.print("Water A : ");
  Serial.print(waterA);
  Serial.print("\t" + String(waterA));
  Serial.print("\t | \t Water B : ");
  Serial.print(waterB);
  Serial.print("\t" + String(waterB) + "\n");
}

/* ------------------------------- KOMUNIKASI SERIAL (ESP32 <-> ARDUINO) ------------------------ */
void checkSerialCommand() {
  /* ------------------------------ LOOP PEMBACAAN DATA DARI ESP32 CAM -------------------------- */
  while (mySerial.available() > 0)
  {
    delay(10);
    CharData = mySerial.read();
    StringData += CharData;

    /* ------------------------------ PARSING DATA MASUK DARI ESP32 CAM ------------------------- */
    if (StringData.length() > 0 && CharData == '?')
    {
      index1 = StringData.indexOf('#');
      index2 = StringData.indexOf('?', index1 + 1);
      dataSubs = StringData.substring(index1 + 1, index2);
      StringData = "";

      // Mengubah Data String ke Float
      char buf[dataSubs.length()];
      dataSubs.toCharArray(buf, dataSubs.length() + 1);
      float Data1 = atof(buf);

      /* -------------------------------- DATA MASUK DARI ESP32 CAM ------------------------------ */
      Data = Data1;
      Serial.print("Data Masuk : ");
      Serial.println(Data);

      /* ---------------------------------- SEND DATA KE ESP32 CAM ------------------------------- */
      /* --------------------------- KATEGORI PENGELOMPOKAN STATUS TANAH ------------------------- */
      if (Data == 1) {
        if (soilA > soilDryThreshold && waterA == KERING) {
          mySerial.print("#1!");    // 1 -----> Tanah A kering, tidak hujan, POMPA MAIN ON
        }
        else if (soilB > soilDryThreshold && waterB == KERING) {
          mySerial.print("#2!");    // 2 -----> Tanah B kering, tidak hujan, POMPA MAIN ON
        }
        else if (waterA == BASAH || waterB == BASAH) {
          mySerial.print("#3!");    // 3 -----> Tanah Normal, POMPA MAIN OFF
        }
        else {
          mySerial.print("#4!");    // 3 -----> Tanah Normal, POMPA MAIN OFF, POMPA DRAIN A & DRAIN B OFF
        }

        /* ------------------------ KATEGORI PENGELOMPOKAN STATUS DRAINASE ------------------------ */
        // Zona B
        if (soilA <= soilDryThreshold && waterA == BASAH) {
          mySerial.print("#11!");   // 11 -----> Drainase A ON, POMPA DRAIN A ON
        }
        else if (waterA == KERING) {
          mySerial.print("#22!");   // 22 -----> Drainase A OFF, POMPA DRAIN A OFF
        }
        // Zona B
        if (soilB <= soilDryThreshold && waterB == BASAH) {
          mySerial.print("#33!");   // 33 -----> Drainase B ON, POMPA DRAIN B ON
        }
        else if (waterB == KERING) {
          mySerial.print("#44!");   // 44 -----> Drainase B OFF, POMPA DRAIN B OFF
        }

        /* ---------------------- SEND DATA PERSENTASE KELEMBABAN KE ESP32 CAM ---------------------- */
        if (PersentaseA < 0) {
          mySerial.print("#0@");
        }
        else if (PersentaseA > 100) {
          mySerial.print("#100@");
        }
        else {
          mySerial.print("#" + String(PersentaseA) + "@");
        }

        /* ---------------------- SEND DATA PERSENTASE KELEMBABAN KE ESP32 CAM ---------------------- */
        if (PersentaseB < 0) {
          mySerial.print("#0&");
        }
        else if (PersentaseB > 100) {
          mySerial.print("#100&");
        }
        else {
          mySerial.print("#" + String(PersentaseB) + "&");
        }
        /* -------------------------------------- RESET DATA ----------------------------------------- */
        Data = "";
      }
    }
  }
}
