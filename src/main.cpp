
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <SparkFun_u-blox_GNSS_v3.h>

// Multitasking (FreeRTOS)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "Config.h"
#include "TempSensor.h"
#include "SdLogger.h"
#include "RpmSensor.h"
#include "Display.h"
#include "Button.h"
#include "VoltageSensor.h"
#include "CurrentSensor.h"
#include "AccelSensor.h"

// ==========================================
#define SIM_PIN "0966"  // <--- TWÓJ PIN (Jeśli brak pinu, zakomentuj użycie w kodzie)
// ==========================================

#define LOG_INTERVAL_MS   100   // 10Hz logowania telemetrii
#define SLOW_INTERVAL_MS  2000  // 0.5Hz dla odczytów wolnych

// --- OBIEKTY ---
SFE_UBLOX_GNSS_SERIAL myGNSS;
TempSensor tempModule(PIN_ONE_WIRE, SLOW_INTERVAL_MS); 
SdLogger logger(SD_CS_PIN);
RpmSensor rpmEngine1(PIN_RPM_1, 0, LOG_INTERVAL_MS); 
RpmSensor rpmEngine2(PIN_RPM_2, 1, LOG_INTERVAL_MS); 
CurrentSensor ammeter(PIN_CURRENT_SIG, PIN_CURRENT_REF, CURRENT_SENS, CURRENT_DIVIDER, CURRENT_TURNS);
VoltageSensor batt1(PIN_VOLT_1, RESISTOR_R1, RESISTOR_R2);
VoltageSensor batt2(PIN_VOLT_2, RESISTOR_R1, RESISTOR_R2);
Display oled;
Button btn1(PIN_BTN_1);
Button btn2(PIN_BTN_2);
AccelSensor accel; 

// --- SEMAFORY (Ochrona zasobów sprzętowych) ---
SemaphoreHandle_t gpsMutex; 
SemaphoreHandle_t gsmMutex;
SemaphoreHandle_t sdMutex; // Ochrona systemu plików FAT

// --- DANE GPS ---
struct SharedGpsData {
  double lat = 0.0; double lon = 0.0;
  float alt = 0.0; float speed = 0.0;
  int sats = 0; bool fix = false;
  float pdop = 99.9;
  unsigned long pkts = 0;
  
  // Zmienne czasu rzeczywistego (UTC z satelitów)
  int year = 0; int month = 0; int day = 0;
  int hour = 0; int minute = 0; int second = 0;
} gpsData;

// --- DANE GSM ---
String gsmPayloadBuffer = ""; 
int gsmSampleCount = 0;       
bool isGsmReady = false;
unsigned long lastSendTimeMs = 0;

// --- ZMIENNE GLOBALNE ---
bool isSdReady = false;
float currentAmps = 0.0, v1 = 0.0, v2 = 0.0, cpuTemp = 0.0;
float extTemps[3] = {0.0, 0.0, 0.0};
float accX = 0.0, accY = 0.0, accZ = 0.0;

// Zmienne do obsługi kalibracji guzikiem:
unsigned long btn1PressStart = 0;
bool btn1Held = false;
bool calibrationDone = false;

// --- EXCEL FIX (Kropka -> Przecinek) ---
String toCsv(float val, int prec = 2) {
  String s = String(val, prec); s.replace(".", ","); return s;
}
String toCsv(double val, int prec) {
  String s = String(val, prec); s.replace(".", ","); return s;
}

// ==========================================
// FUNKCJA: LOGOWANIE SYSTEMOWE Z CZASEM Z GPS
// ==========================================
void writeSysLog(String msg) {
  unsigned long ts = millis();
  String timePrefix = "[" + String(ts) + "ms]";

  // Próba pobrania dokładnego czasu rzeczywistego z modułu u-blox
  if (xSemaphoreTake(gpsMutex, (TickType_t)5) == pdTRUE) {
    if (gpsData.year > 2020) { // Jeśli moduł zsynchronizował się i podaje poprawny rok
      char timeStr[30];
      sprintf(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d]", 
              gpsData.year, gpsData.month, gpsData.day, 
              gpsData.hour, gpsData.minute, gpsData.second);
      timePrefix = String(timeStr);
    }
    xSemaphoreGive(gpsMutex);
  }

  String logMsg = timePrefix + " " + msg;
  
  // Wysłanie podglądu do komputera
  Serial.println(logMsg);

  // Bezpieczny zapis do pliku systemowego (zabezpieczony przed wątkiem 10Hz)
  if (isSdReady) {
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      File file = SD.open("/system_log.txt", FILE_APPEND);
      if (file) {
        file.println(logMsg);
        file.close();
      }
      xSemaphoreGive(sdMutex);
    }
  }
}

// ==========================================
// FUNKCJE POMOCNICZE GSM 
// ==========================================
String sendAT(String cmd, unsigned long timeout) {
  while (Serial2.available()) Serial2.read(); 
  Serial2.println(cmd);
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
  return response;
}

unsigned long sendTelemetryPacket(String data) {
  unsigned long startOp = millis();
  while (Serial2.available()) Serial2.read();
  Serial2.print("AT+CIPSEND=0,"); Serial2.println(data.length());
  
  unsigned long waitStart = millis();
  bool readyToSend = false;
  while (millis() - waitStart < 500) {
    if (Serial2.available() && Serial2.read() == '>') { readyToSend = true; break; }
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
  
  if (readyToSend) {
    Serial2.print(data);
    waitStart = millis();
    while (millis() - waitStart < 1000) {
      if (Serial2.available()) {
        char c = Serial2.read();
        if (c == 'K' || c == 'R') break; 
      }
      vTaskDelay(pdMS_TO_TICKS(1)); 
    }
  }
  return millis() - startOp;
}

// ==========================================
// TASKS (Działające w tle na Rdzeniu 0)
// ==========================================

void TaskGPS(void *pvParameters) {
  for (;;) {
    if (myGNSS.getPVT()) {
      if (xSemaphoreTake(gpsMutex, (TickType_t)10) == pdTRUE) {
        gpsData.fix = (myGNSS.getFixType() > 0);
        gpsData.sats = myGNSS.getSIV();
        gpsData.lat = myGNSS.getLatitude() / 10000000.0;
        gpsData.lon = myGNSS.getLongitude() / 10000000.0;
        gpsData.alt = myGNSS.getAltitudeMSL() / 1000.0;
        gpsData.speed = myGNSS.getGroundSpeed() * 0.0036; 
        gpsData.pdop = myGNSS.getPDOP() / 100.0;
        gpsData.pkts++;

        // Wyciąganie czasu UTC dla logów
        gpsData.year = myGNSS.getYear();
        gpsData.month = myGNSS.getMonth();
        gpsData.day = myGNSS.getDay();
        gpsData.hour = myGNSS.getHour();
        gpsData.minute = myGNSS.getMinute();
        gpsData.second = myGNSS.getSecond();

        xSemaphoreGive(gpsMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}

void TaskGSM(void *pvParameters) {
  Serial2.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  vTaskDelay(pdMS_TO_TICKS(500)); 

  writeSysLog("GSM: Sprawdzanie stanu modemu SIMCom...");
  String testRes = sendAT("AT", 1000);
  if (testRes.indexOf("OK") == -1) {
    writeSysLog("GSM: Brak odpowiedzi. Uruchamianie impulsem z pinu Power...");
    pinMode(GSM_PWR_PIN, OUTPUT);
    digitalWrite(GSM_PWR_PIN, HIGH); 
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    digitalWrite(GSM_PWR_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(5000)); 
  } else {
    writeSysLog("GSM: Modem aktywny.");
  }
  
  sendAT("ATE0", 1000); 
  sendAT(String("AT+CPIN=\"") + SIM_PIN + "\"", 2000); 
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  sendAT("AT+CNMP=38", 1000); // Wymuszenie LTE
  sendAT("AT+COPS=0", 2000);

  writeSysLog("GSM: Rozpoczeto wyszukiwanie stacji bazowej LTE...");

  for (;;) {
    if (!isGsmReady) {
      String regRes = sendAT("AT+CREG?", 500);
      String ceregRes = sendAT("AT+CEREG?", 500);
      
      if (regRes.indexOf(",1") != -1 || regRes.indexOf(",5") != -1 || 
          ceregRes.indexOf(",1") != -1 || ceregRes.indexOf(",5") != -1) {
        
        writeSysLog("GSM: Zalogowano w sieci. Konfiguracja kanalu IP...");
        sendAT("AT+CGDCONT=1,\"IP\",\"internet\"", 1000);
        sendAT("AT+CGACT=1,1", 2000);
        sendAT("AT+NETOPEN", 2000);
        sendAT("AT+CIPOPEN=0,\"UDP\",\"8.8.8.8\",53", 2000);
        
        isGsmReady = true;
        writeSysLog("GSM: Transmisja gotowa.");
      } else {
        if (ceregRes.indexOf(",3") != -1) {
            writeSysLog("BLAD GSM: Odrzucenie rejestracji przez operatora (Kod 3)!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); 
      }
    } else {
      String localPayload = "";
      if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (gsmSampleCount >= 10) { 
          localPayload = gsmPayloadBuffer;
          gsmPayloadBuffer = ""; 
          gsmSampleCount = 0;
        }
        xSemaphoreGive(gsmMutex);
      }

      if (localPayload.length() > 0) {
        lastSendTimeMs = sendTelemetryPacket(localPayload);
        if(lastSendTimeMs > 2500) {
            writeSysLog("OSTRZEZENIE: Wysoki czas wysylki pakietu (Ping): " + String(lastSendTimeMs) + " ms");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

// ==========================================
// SETUP (Główna konfiguracja, Rdzeń 1)
// ==========================================

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // Magistrala I2C dostępna dla reszty gniazd Molex
  
  gpsMutex = xSemaphoreCreateMutex();
  gsmMutex = xSemaphoreCreateMutex(); 
  sdMutex = xSemaphoreCreateMutex(); // Zabezpieczenie karty SD

  oled.begin();
  oled.showStatus("SYSTEM START", "Uruchamiam SD...");
  delay(500);

  // 1. INICJALIZACJA SD 
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
    isSdReady = true;
    logger.begin(); 
    logger.logData("Czas_ms;Lat;Lon;Alt;Sats;Spd;Temp1;Temp2;Temp3;AccX;AccY;AccZ;CPU;RPM1;RPM2;Amps;Volt1;Volt2;Btn1;Btn2;PktGPS"); 
    
    writeSysLog("======================================");
    writeSysLog("SYSTEM: BOOT (WZNOWIENIE ZASILANIA)");
    writeSysLog("======================================");
    writeSysLog("SD: Zapis CSV zainicjalizowany (Plik: " + logger.getFileName() + ")");
  } else {
    oled.showStatus("SD BLAD", "Brak modulu!");
    Serial.println("KRYTYCZNE: Błąd wczytywania karty SD!");
  }
  delay(500);

  // 2. INICJALIZACJA ADXL
  if (accel.begin()) {
    oled.showStatus("ADXL343 OK", "Akcelerometr dziala");
    writeSysLog("I2C: Moduł ADXL343 połączony.");
  } else {
    oled.showStatus("ADXL BLAD", "Sprawdz kable");
    writeSysLog("BLAD I2C: Nie wykryto układu ADXL343!");
  }
  delay(500);

  // 3. INICJALIZACJA GPS (UART1)
  Serial1.setRxBufferSize(1024);
  Serial1.begin(38400, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  if (myGNSS.begin(Serial1)) {
    myGNSS.setNavigationFrequency(10); 
    myGNSS.setUART1Output(COM_TYPE_UBX); 
    myGNSS.setAutoPVT(true);
    xTaskCreatePinnedToCore(TaskGPS, "GPS_Task", 4096, NULL, 1, NULL, 0);
    oled.showStatus("GPS", "10Hz Init OK");
    writeSysLog("GPS: Komunikacja UART u-blox zestawiona. AutoPVT 10Hz aktywne.");
  } else {
    oled.showStatus("GPS", "Brak modulu!");
    writeSysLog("BLAD UART: Moduł GPS milczy.");
  }
  delay(500);
  
  // 4. START GSM
  oled.showStatus("GSM TASK", "Trwa wlaczanie...");
  xTaskCreatePinnedToCore(TaskGSM, "GSM_Task", 8192, NULL, 1, NULL, 0);
  delay(500);

  // 5. RESZTA CZUJNIKÓW
  tempModule.begin(); rpmEngine1.begin(); rpmEngine2.begin();
  btn1.begin(); btn2.begin(); ammeter.begin(); batt1.begin(); batt2.begin();
  batt2.setCalibration(CALIB_FACTOR_V2);
  writeSysLog("SYSTEM: Czujniki temperatury, napiecia i prądu zainicjalizowane.");
}

// ==========================================
// LOOP (Główna pętla programu, Rdzeń 1)
// ==========================================

void loop() {
  unsigned long currentMillis = millis();

  // Odczyt sprzętowy (szybki)
  rpmEngine1.update(); rpmEngine2.update(); tempModule.update();
  accel.update(); accX = accel.getX(); accY = accel.getY(); accZ = accel.getZ();
  bool b1 = btn1.isPressed(); bool b2 = btn2.isPressed();

 
  static unsigned long lastLog = 0;
  if (currentMillis - lastLog >= LOG_INTERVAL_MS) {
    lastLog = currentMillis;

    currentAmps = ammeter.readCurrent();
    
    // Kopiowanie danych od u-bloxa
    double lLat=0, lLon=0; float lAlt=0, lSpd=0; int lSats=0; bool lFix=false; unsigned long lPkt=0;
    if (xSemaphoreTake(gpsMutex, (TickType_t)5) == pdTRUE) {
      lLat = gpsData.lat; lLon = gpsData.lon; lAlt = gpsData.alt; lSpd = gpsData.speed; 
      lSats = gpsData.sats; lFix = gpsData.fix; lPkt = gpsData.pkts;
      xSemaphoreGive(gpsMutex);
    }

    float t1 = tempModule.getTemp(0); float t2 = tempModule.getTemp(1); float t3 = tempModule.getTemp(2);
    String line = String(currentMillis) + ";" + 
                  toCsv(lLat, 7) + ";" + toCsv(lLon, 7) + ";" + toCsv(lAlt, 2) + ";" + String(lSats) + ";" + 
                  toCsv(lSpd, 1) + ";" + toCsv(t1, 1) + ";" + toCsv(t2, 1) + ";" + toCsv(t3, 1) + ";" +
                  toCsv(accX, 2) + ";" + toCsv(accY, 2) + ";" + toCsv(accZ, 2) + ";" + toCsv(cpuTemp, 1) + ";" +        
                  String(rpmEngine1.getRPM()) + ";" + String(rpmEngine2.getRPM()) + ";" + 
                  toCsv(currentAmps, 2) + ";" + toCsv(v1, 2) + ";" + toCsv(v2, 2) + ";" + 
                  String(b1) + ";" + String(b2) + ";" + String(lPkt);

    // BEZPIECZNY ZAPIS TELEMETRII (Chroni przed kolizją z system_log.txt)
    if (isSdReady) {
      if (xSemaphoreTake(sdMutex, (TickType_t)10) == pdTRUE) {
        logger.logData(line);
        xSemaphoreGive(sdMutex);
      }
    }

    // Przekazanie do kolejki UDP
    if (isGsmReady) {
      if (xSemaphoreTake(gsmMutex, (TickType_t)5) == pdTRUE) {
        gsmPayloadBuffer += line + "\n"; 
        gsmSampleCount++;
        xSemaphoreGive(gsmMutex);
      }
    }
  }

  // Odczyt sprzętowy (wolny)
  static unsigned long lastSlow = 0;
  if (currentMillis - lastSlow >= SLOW_INTERVAL_MS) {
    lastSlow = currentMillis;
    v1 = batt1.readVoltage(); v2 = batt2.readVoltage();
    cpuTemp = temperatureRead(); extTemps[0] = tempModule.getTemp(0);
  }

  // Aktualizacja klatek ekranu OLED
  static unsigned long lastScreen = 0;
  if (currentMillis - lastScreen >= 200) {
    lastScreen = currentMillis;
    
    bool currentFix = false;
    int currentSats = 0;
    float currentPdop = 99.9;
    
    if (xSemaphoreTake(gpsMutex, (TickType_t)5) == pdTRUE) { 
      currentFix = gpsData.fix; 
      currentSats = gpsData.sats;
      currentPdop = gpsData.pdop;
      xSemaphoreGive(gpsMutex); 
    }
    
    oled.updateScreen(extTemps[0], cpuTemp, rpmEngine1.getRPM(), rpmEngine2.getRPM(), currentAmps, v2, b1, b2, currentFix, currentSats, currentPdop, accX, accY, accZ, isGsmReady, lastSendTimeMs);
  }
}
// // #include <Arduino.h>
// // #include <U8g2lib.h>
// // #include <SPI.h>

// // // Piny GPS (z Twojego Config.h)
// // #define GPS_RX_PIN 18
// // #define GPS_TX_PIN 17
// // #define GPS_BAUD 38400

// // // Piny OLED (z Twojego Config.h)
// // #define OLED_RST  42
// // #define OLED_DC   41
// // #define OLED_CS   39
// // #define OLED_CLK  40
// // #define OLED_MOSI 38

// // // Inicjalizacja ekranu (dokładnie taka sama jak w Twoim projekcie)
// // U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, OLED_CLK, OLED_MOSI, OLED_CS, OLED_DC, OLED_RST);

// // String nmeaBuffer = ""; // Bufor na zbieranie znaków w jedną linię

// // void setup() {
// //   // Port do komunikacji z komputerem
// //   Serial.begin(115200);
  
// //   // Port sprzętowy do komunikacji z GPS
// //   Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

// //   // Uruchomienie ekranu OLED
// //   u8g2.begin();
// //   u8g2.clearBuffer();
// //   u8g2.setFont(u8g2_font_5x8_tf); // Mała czcionka, idealna do logów
// //   u8g2.drawStr(0, 15, "--- TEST GPS ---");
// //   u8g2.drawStr(0, 35, "Nasluchiwanie na:");
// //   u8g2.drawStr(0, 45, "RX: 17, TX: 18");
// //   u8g2.drawStr(0, 55, "Baud: 38400");
// //   u8g2.sendBuffer();
  
// //   delay(2000); // Zostawiamy napis powitalny na 2 sekundy
// // }

// // void loop() {
// //   // Czytaj wszystko, co przychodzi od GPS
// //   while (Serial1.available()) {
// //     char c = Serial1.read();
    
// //     // Równolegle wysyłamy też do Serial Monitora (do komputera)
// //     Serial.print(c); 
    
// //     // Jeśli znak to koniec linii (\n)
// //     if (c == '\n') {
// //       // Wyświetlamy na OLED tylko ramki z danymi (zaczynające się od $GN)
// //       if (nmeaBuffer.startsWith("$GN")) {
// //          u8g2.clearBuffer();
// //          u8g2.setCursor(0, 10);
// //          u8g2.print("Ostatnia ramka GPS:");
         
// //          // Ramki NMEA są bardzo długie (często ponad 70 znaków). 
// //          // Dzielimy je na 4 wiersze po 25 znaków, żeby nie wyszły poza ekran.
// //          u8g2.setCursor(0, 25); u8g2.print(nmeaBuffer.substring(0, 25));
// //          u8g2.setCursor(0, 35); u8g2.print(nmeaBuffer.substring(25, 50));
// //          u8g2.setCursor(0, 45); u8g2.print(nmeaBuffer.substring(50, 75));
// //          u8g2.setCursor(0, 55); u8g2.print(nmeaBuffer.substring(75, 100));
         
// //          u8g2.sendBuffer();
// //       }
      
// //       // Czyścimy bufor, żeby przygotować się na nową linijkę
// //       nmeaBuffer = ""; 
      
// //     } else if (c != '\r') { // Ignorujemy znak powrotu karetki
// //       // Dodajemy kolejne literki do bufora
// //       nmeaBuffer += c;
// //     }
// //   }
// // }

// #include <Arduino.h>
// #include <U8g2lib.h>
// #include <SPI.h>

// // ==========================================
// // PINY I USTAWIENIA (zgodne z Twoim Config.h)
// // ==========================================
// // --- GSM ---
// #define GSM_TX_PIN  32  
// #define GSM_RX_PIN  28  
// #define GSM_PWR_PIN 21  
// #define GSM_BAUD    115200

// // --- OLED ---
// #define OLED_RST  42
// #define OLED_DC   41
// #define OLED_CS   39
// #define OLED_CLK  40
// #define OLED_MOSI 38

// // Twój kod PIN do karty SIM
// #define SIM_PIN "0966"

// // Inicjalizacja ekranu
// U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, OLED_CLK, OLED_MOSI, OLED_CS, OLED_DC, OLED_RST);

// // Tablica przechowująca 6 ostatnich linijek na ekranie
// String lines[6] = {"", "", "", "", "", ""}; 

// // ==========================================
// // FUNKCJE POMOCNICZE
// // ==========================================

// // Dodaje nową linijkę na dół ekranu i przesuwa resztę do góry
// void addLine(String newLine) {
//   for (int i = 0; i < 5; i++) {
//     lines[i] = lines[i+1];
//   }
//   lines[5] = newLine;
  
//   u8g2.clearBuffer();
//   u8g2.setFont(u8g2_font_5x8_tf);
//   for (int i = 0; i < 6; i++) {
//     u8g2.setCursor(0, 10 + (i * 10));
//     u8g2.print(lines[i]);
//   }
//   u8g2.sendBuffer();
// }

// // Wysyła komendę AT i wyświetla odpowiedź na ekranie
// void sendCommand(String cmd, int waitTimeMs) {
//   addLine("-> " + cmd);  // Pokaż co wysyłamy
//   Serial2.println(cmd);  // Wyślij do modułu
  
//   unsigned long start = millis();
//   String responseLine = "";
  
//   while (millis() - start < waitTimeMs) {
//     while (Serial2.available()) {
//       char c = Serial2.read();
      
//       if (c == '\n') {
//         // Ignorujemy puste linie, powtórzenia komendy i same znaki powrotu
//         if (responseLine.length() > 0 && responseLine != cmd && responseLine != "\r") {
//           responseLine.replace("\r", ""); 
          
//           // Ucinamy tekst, żeby nie wyszedł poza szerokość ekranu
//           if(responseLine.length() > 25) {
//              responseLine = responseLine.substring(0, 25);
//           }
//           addLine(responseLine);
//         }
//         responseLine = "";
//       } else if (c != '\r') {
//         responseLine += c;
//       }
//     }
//   }
// }

// // ==========================================
// // GŁÓWNY PROGRAM
// // ==========================================

// void setup() {
//   // 1. Ekran Powitalny
//   u8g2.begin();
//   u8g2.clearBuffer();
//   u8g2.setFont(u8g2_font_5x8_tf);
//   u8g2.drawStr(0, 10, "--- AUTO TEST LTE ---");
//   u8g2.drawStr(0, 30, "Wzbudzam modul...");
//   u8g2.sendBuffer();

//   // 2. Sprzętowe Włączenie Modułu (Impuls)
//   pinMode(GSM_PWR_PIN, OUTPUT);
//   digitalWrite(GSM_PWR_PIN, HIGH);
//   delay(1000); 
//   digitalWrite(GSM_PWR_PIN, LOW);
  
//   u8g2.drawStr(0, 50, "Bootowanie (7 sek)...");
//   u8g2.sendBuffer();
//   delay(7000); // Czas na załadowanie systemu w module

//   // 3. Start Portu GSM
//   Serial2.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  
//   addLine("Gotowe!");
//   delay(1000);
  
//   // 4. Konfiguracja i Autoryzacja
//   // Wyłączamy echo
//   Serial2.println("ATE0");
//   delay(500);
//   while(Serial2.available()) Serial2.read(); // Czyszczenie śmieci z bufora

//   // Wpisujemy kod PIN
//   addLine("Odblokowuje SIM...");
//   sendCommand(String("AT+CPIN=\"") + SIM_PIN + "\"", 2000); 
  
//   addLine("Czekam na siec...");
//   delay(4000); // Dajemy czas na zalogowanie do BTS-a po podaniu PIN-u
// }

// void loop() {
//   // Co 5 sekund odpytujemy układ
  
//   // Test komunikacji
//   sendCommand("AT", 1000);
  
//   // Sprawdzenie zasięgu anteny (0-31 = ok, 99 = brak anteny/zasięgu)
//   sendCommand("AT+CSQ", 1000);
  
//   // Sprawdzenie logowania do sieci (0,1 = sieć domowa, 0,5 = roaming)
//   sendCommand("AT+CREG?", 1000);
  
//   addLine("-------------------------");
//   delay(4000); // Odczekaj przed kolejnym cyklem
// }