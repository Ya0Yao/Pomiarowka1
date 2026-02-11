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

// --- KONFIGURACJA ---
#define LOG_INTERVAL_MS   100   // 10Hz logowania
#define SLOW_INTERVAL_MS  2000  // 0.5Hz dla temperatur

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

// --- DANE GPS (Współdzielone) ---
SemaphoreHandle_t gpsMutex; 
struct SharedGpsData {
  double lat = 0.0; double lon = 0.0;
  float alt = 0.0; float speed = 0.0;
  int sats = 0; bool fix = false;
  unsigned long pkts = 0;
} gpsData;

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

// --- TASK GPS (Rdzeń 0) ---
void TaskGPS(void *pvParameters) {
  for (;;) {
    if (myGNSS.checkUblox()) {
      if (xSemaphoreTake(gpsMutex, (TickType_t)10) == pdTRUE) {
        gpsData.fix = (myGNSS.getFixType() > 0);
        gpsData.sats = myGNSS.getSIV();
        gpsData.lat = myGNSS.getLatitude() / 10000000.0;
        gpsData.lon = myGNSS.getLongitude() / 10000000.0;
        gpsData.alt = myGNSS.getAltitudeMSL() / 1000.0;
        gpsData.speed = myGNSS.getGroundSpeed() * 0.0036; 
        gpsData.pkts++;
        xSemaphoreGive(gpsMutex);
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  
  // 1. I2C NA PINACH 36 i 37 (Zgodnie z PCB)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
  gpsMutex = xSemaphoreCreateMutex();

  oled.begin();
  oled.showStatus("SYSTEM START", "Piny 36/37");
  delay(500);

  // 2. START AKCELEROMETRU
  if (accel.begin()) {
    Serial.println("ADXL343: OK");
    oled.showStatus("ADXL343 OK", "PCB Dziala!");
  } else {
    Serial.println("ADXL343: BLAD");
    oled.showStatus("ADXL BLAD", "Sprawdz 36/37");
    delay(1000);
  }
  delay(200);

  // 3. START GPS (Rdzeń 0)
  Serial1.begin(38400, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  if (myGNSS.begin(Serial1)) {
    myGNSS.setUART1Output(COM_TYPE_UBX); 
    myGNSS.setNavigationFrequency(10); 
    xTaskCreatePinnedToCore(TaskGPS, "GPS_Task", 4096, NULL, 1, NULL, 0);
    Serial.println("GPS: OK (Core 0)");
    oled.showStatus("GPS OK", "Core 0 Active");
  } else {
    oled.showStatus("GPS BLAD", "Brak modulu");
  }
  delay(500);

  // 4. START SD
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
    logger.begin(); 
    isSdReady = true;
    oled.showStatus("SD OK", logger.getFileName().substring(1));
    // NAGŁÓWEK CSV
    logger.logData("Czas_ms;Lat;Lon;Alt;Sats;Spd;Temp1;Temp2;Temp3;AccX;AccY;AccZ;CPU;RPM1;RPM2;Amps;Volt1;Volt2;Btn1;Btn2;PktGPS"); 
  } else {
    oled.showStatus("SD BLAD", "Logowanie OFF");
  }
  
  // Reszta
  tempModule.begin();
  rpmEngine1.begin();
  rpmEngine2.begin();
  btn1.begin();
  btn2.begin();
  ammeter.begin(); 
  batt1.begin();
  batt2.begin();
  batt2.setCalibration(CALIB_FACTOR_V2);
}

void loop() {
  unsigned long currentMillis = millis();

  // Aktualizacje szybkie
  rpmEngine1.update();
  rpmEngine2.update();
  tempModule.update();
  
  // Odczyt akcelerometru
  accel.update();
  accX = accel.getX(); 
  accY = accel.getY();
  accZ = accel.getZ();

  bool b1 = btn1.isPressed();
  bool b2 = btn2.isPressed();

  // --- LOGOWANIE DANYCH (10Hz) ---
  static unsigned long lastLog = 0;
  if (currentMillis - lastLog >= LOG_INTERVAL_MS) {
    lastLog = currentMillis;

    currentAmps = ammeter.readCurrent();
    
    // GPS z drugiego rdzenia
    double lLat=0, lLon=0; float lAlt=0, lSpd=0; int lSats=0; bool lFix=false; unsigned long lPkt=0;
    
    if (xSemaphoreTake(gpsMutex, (TickType_t)5) == pdTRUE) {
      lLat = gpsData.lat; lLon = gpsData.lon; lAlt = gpsData.alt;
      lSpd = gpsData.speed; lSats = gpsData.sats; lFix = gpsData.fix; lPkt = gpsData.pkts;
      xSemaphoreGive(gpsMutex);
    }

    if (isSdReady) {
      float t1 = tempModule.getTemp(0);
      float t2 = tempModule.getTemp(1);
      float t3 = tempModule.getTemp(2);

      String line = String(currentMillis) + ";" + 
                    toCsv(lLat, 7) + ";" + toCsv(lLon, 7) + ";" + 
                    toCsv(lAlt, 2) + ";" + String(lSats) + ";" + 
                    toCsv(lSpd, 1) + ";" +
                    toCsv(t1, 1) + ";" + toCsv(t2, 1) + ";" + toCsv(t3, 1) + ";" +
                    toCsv(accX, 2) + ";" + toCsv(accY, 2) + ";" + toCsv(accZ, 2) + ";" + 
                    toCsv(cpuTemp, 1) + ";" +        
                    String(rpmEngine1.getRPM()) + ";" + 
                    String(rpmEngine2.getRPM()) + ";" + 
                    toCsv(currentAmps, 2) + ";" +    
                    toCsv(v1, 2) + ";" + toCsv(v2, 2) + ";" + 
                    String(b1) + ";" + String(b2) + ";" + String(lPkt);
      logger.logData(line);
    }
  }

  // --- POMIARY WOLNE (0.5Hz) ---
  static unsigned long lastSlow = 0;
  if (currentMillis - lastSlow >= SLOW_INTERVAL_MS) {
    lastSlow = currentMillis;
    v1 = batt1.readVoltage();
    v2 = batt2.readVoltage();
    cpuTemp = temperatureRead(); 
    extTemps[0] = tempModule.getTemp(0);
  }

  // --- EKRAN (5Hz) ---
  static unsigned long lastScreen = 0;
  if (currentMillis - lastScreen >= 200) {
    lastScreen = currentMillis;
    bool fix = false;
    if (xSemaphoreTake(gpsMutex, (TickType_t)5) == pdTRUE) { fix = gpsData.fix; xSemaphoreGive(gpsMutex); }
    
    // TUTAJ przekazujemy accX, accY, accZ do funkcji wyświetlacza!
    oled.updateScreen(extTemps[0], cpuTemp, rpmEngine1.getRPM(), rpmEngine2.getRPM(), currentAmps, v2, b1, b2, fix, accX, accY, accZ);
  }
}