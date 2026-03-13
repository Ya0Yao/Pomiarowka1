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