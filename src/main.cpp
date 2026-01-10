#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "Config.h"
#include "TempSensor.h"
#include "SdLogger.h"
#include "RpmSensor.h"
#include "Display.h"
#include "Button.h"
#include "VoltageSensor.h"
#include "CurrentSensor.h"

// --- KONFIGURACJA CZASÓW ---
#define LOG_INTERVAL_MS   100   // 10Hz (Logowanie, RPM, PRĄD)
#define SLOW_INTERVAL_MS  2000  // 0.5Hz (Temperatury, Napięcia)

// --- OBIEKTY ---
TempSensor tempModule(PIN_ONE_WIRE, SLOW_INTERVAL_MS); 
SdLogger logger(SD_CS_PIN);

RpmSensor rpmEngine1(PIN_RPM_1, 0, LOG_INTERVAL_MS); 
RpmSensor rpmEngine2(PIN_RPM_2, 1, LOG_INTERVAL_MS); 

// Amperomierz na nowych pinach 5 i 6
CurrentSensor ammeter(PIN_CURRENT_SIG, PIN_CURRENT_REF, CURRENT_SENS, CURRENT_DIVIDER, CURRENT_TURNS);

// Woltomierze na nowych pinach 7 i 8
VoltageSensor batt1(PIN_VOLT_1, RESISTOR_R1, RESISTOR_R2);
VoltageSensor batt2(PIN_VOLT_2, RESISTOR_R1, RESISTOR_R2);

Display oled;
Button btn1(PIN_BTN_1);
Button btn2(PIN_BTN_2);

// --- ZMIENNE GLOBALNE ---
bool isSdReady = false;
float currentAmps = 0.0;
float v1 = 0.0;
float v2 = 0.0;
float cpuTemp = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000); 

  oled.begin();
  oled.showStatus("Start...", "10Hz Mode");
  delay(1000);

  Serial.println("\n--- START 10Hz ---");

  // Inicjalizacja SD
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  delay(200); 

  bool sdSuccess = false;
  for (int i = 0; i < 10; i++) {
    if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
      sdSuccess = true;
      break; 
    }
    oled.showStatus("Szukam SD...", "Proba " + String(i+1));
    SPI.end(); delay(50);
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    delay(500); 
  }

  if (!sdSuccess) { 
    oled.showStatus("AWARIA SD!", "Brak karty?");
    isSdReady = false;
    delay(2000);
  } else {
    logger.begin(); 
    isSdReady = true;
    String fileName = logger.getFileName();
    fileName.replace("/", ""); 
    oled.showStatus("REC: " + fileName, "Start...");
    delay(1000);
    // Nagłówek logu
    logger.logData("Czas_ms,Temp,CPU,RPM1,RPM2,Amps,Volt1,Volt2,Btn1,Btn2"); 
  }

  // Start czujników
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
  // Szybkie aktualizacje
  rpmEngine1.update();
  rpmEngine2.update();
  tempModule.update();
  
  bool b1 = btn1.isPressed();
  bool b2 = btn2.isPressed();
  unsigned long currentMillis = millis();

  // --- SZYBKA PĘTLA 10Hz (Logowanie i Prąd) ---
  static unsigned long lastLog = 0;
  if (currentMillis - lastLog >= LOG_INTERVAL_MS) {
    lastLog = currentMillis;

    // 1. Pomiar prądu (SZYBKI) - tutaj, aby był świeży co 100ms
    currentAmps = ammeter.readCurrent();

    // 2. Logowanie
    if (isSdReady) {
      String line = String(currentMillis) + "," + 
                    String(tempModule.getTemp(0)) + "," + 
                    String(cpuTemp) + "," +        
                    String(rpmEngine1.getRPM()) + "," + 
                    String(rpmEngine2.getRPM()) + "," + 
                    String(currentAmps) + "," +    
                    String(v1) + "," + 
                    String(v2) + "," + 
                    String(b1) + "," + String(b2);
      logger.logData(line);
    }
  }

  // --- WOLNA PĘTLA 0.5Hz (Napięcia i Temp) ---
  static unsigned long lastSlow = 0;
  if (currentMillis - lastSlow >= SLOW_INTERVAL_MS) {
    lastSlow = currentMillis;
    
    v1 = batt1.readVoltage();
    v2 = batt2.readVoltage();
    cpuTemp = temperatureRead(); 
  }

  // Ekran (5Hz)
  static unsigned long lastScreen = 0;
  if (currentMillis - lastScreen >= 200) {
    lastScreen = currentMillis;
    oled.updateScreen(tempModule.getTemp(0), cpuTemp, rpmEngine1.getRPM(), rpmEngine2.getRPM(), currentAmps, v1, b1, b2, currentMillis);
  }
}