#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// --- BIBLIOTEKI ---
#include "Config.h"
#include "TempSensor.h"
#include "SdLogger.h"
#include "RpmSensor.h"
#include "Display.h"
#include "Button.h"
#include "VoltageSensor.h"

// --- KONFIGURACJA CZASÓW ---
#define LOG_INTERVAL_MS   100   // 10Hz (0.1s) - Logowanie danych i RPM
#define SLOW_INTERVAL_MS  2000  // 0.5Hz (2.0s) - Odczyt temperatur i napięć

// --- OBIEKTY ---
TempSensor tempModule(PIN_ONE_WIRE, SLOW_INTERVAL_MS); 
SdLogger logger(SD_CS_PIN);

// Obroty
RpmSensor rpmEngine1(PIN_RPM_1, 0, LOG_INTERVAL_MS); 
RpmSensor rpmEngine2(PIN_RPM_2, 1, LOG_INTERVAL_MS); 

// Napięcie
VoltageSensor batt1(PIN_VOLT_1, RESISTOR_R1, RESISTOR_R2);
VoltageSensor batt2(PIN_VOLT_2, RESISTOR_R1, RESISTOR_R2);

Display oled;
Button btn1(PIN_BTN_1);
Button btn2(PIN_BTN_2);

// --- ZMIENNE GLOBALNE ---
bool isSdReady = false;
float v1 = 0.0;       // Zmienna na napięcie V1
float v2 = 0.0;       // Zmienna na napięcie V2
float cpuTemp = 0.0;  // Zmienna na temperaturę procesora

void setup() {
  Serial.begin(115200);
  delay(1000); 

  // 1. EKRAN STARTOWY
  oled.begin();
  oled.showStatus("System Start...", "Booting...");
  delay(1000);

  Serial.println("\n--- START REJESTRATORA ---");

  // 2. STABILNA INICJALIZACJA KARTY SD (Retry System)
  // Próbujemy uruchomić SPI
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  delay(200); // Ważne opóźnienie na start zasilania karty

  bool sdSuccess = false;
  
  // Pętla prób (10 prób, łącznie ok. 5 sekund walki o połączenie)
  for (int i = 0; i < 10; i++) {
    // Próba połączenia
    if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
      sdSuccess = true;
      break; // Udało się! Wychodzimy z pętli
    }
    
    // Jeśli błąd:
    Serial.println("SD Blad... proba " + String(i+1) + "/10");
    oled.showStatus("Szukam SD...", "Proba " + String(i+1));
    
    // Restartujemy magistralę SPI (to pomaga, gdy karta się zawiesi)
    SPI.end(); 
    delay(50);
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    // Czekamy chwilę przed kolejną próbą
    delay(500); 
  }

  if (!sdSuccess) { 
    // Jeśli po 10 próbach nadal nic
    Serial.println("BLAD KRYTYCZNY: Brak karty SD.");
    oled.showStatus("AWARIA SD!", "Brak karty?");
    isSdReady = false;
    delay(2000);
  } else {
    // Jeśli sukces
    Serial.println("SUKCES: Karta SD OK.");
    
    logger.begin(); // Znajdź wolny plik (log_X.csv)
    isSdReady = true;

    String fileName = logger.getFileName();
    Serial.println("Nagrywanie do: " + fileName);
    
    fileName.replace("/", ""); // Kosmetyka dla ekranu
    oled.showStatus("REC: " + fileName, "Start za 2s...");
    delay(2000);
    
    // Nagłówek pliku CSV (z dodanym CPU_Temp)
    logger.logData("Czas_ms,Ext_Temp,CPU_Temp,RPM1,RPM2,Volt1,Volt2,Btn1,Btn2"); 
  }

  // 3. START POZOSTAŁYCH CZUJNIKÓW
  tempModule.begin();
  rpmEngine1.begin();
  rpmEngine2.begin();
  btn1.begin();
  btn2.begin();

  // 4. START I KALIBRACJA NAPIĘCIA
  // Pobieramy wartości CALIB_FACTOR z pliku Config.h
  batt1.begin();
  batt1.setCalibration(CALIB_FACTOR_V1); 
  
  batt2.begin();
  batt2.setCalibration(CALIB_FACTOR_V2);

  Serial.println("System gotowy. Zapis co 0.1s.");
}

void loop() {
  // --- AKTUALIZACJA CIĄGŁA ---
  rpmEngine1.update();
  rpmEngine2.update();
  tempModule.update(); // TempSensor sam pilnuje czasu 2s
  
  bool b1 = btn1.isPressed();
  bool b2 = btn2.isPressed();
  unsigned long currentMillis = millis();

  // --- POMIAR WOLNY (Co 2 sekundy) ---
  // Mierzymy napięcia i temperatury rzadziej, żeby nie obciążać CPU
  static unsigned long lastSlow = 0;
  if (currentMillis - lastSlow >= SLOW_INTERVAL_MS) {
    lastSlow = currentMillis;
    
    v1 = batt1.readVoltage();
    v2 = batt2.readVoltage();
    
    // Odczyt temperatury wewnętrznej procesora
    cpuTemp = temperatureRead(); 
  }

  // --- EKRAN OLED (Co 200ms) ---
  static unsigned long lastScreen = 0;
  if (currentMillis - lastScreen >= 200) {
    lastScreen = currentMillis;
    
    // Wyświetlamy komplet danych
    oled.updateScreen(
      tempModule.getTemp(0), // Temp zewn. (silnik)
      cpuTemp,               // Temp wewn. (procesor)
      rpmEngine1.getRPM(), 
      rpmEngine2.getRPM(), 
      v1, 
      v2, 
      b1, 
      b2, 
      currentMillis
    );
  }

  // --- ZAPIS NA KARTĘ SD (Co 100ms) ---
  static unsigned long lastLog = 0;
  if (currentMillis - lastLog >= LOG_INTERVAL_MS) {
    lastLog = currentMillis;

    if (isSdReady) {
      // Budujemy linię danych
      String line = String(currentMillis) + "," + 
                    String(tempModule.getTemp(0)) + "," + 
                    String(cpuTemp) + "," +        // Dodano do logu
                    String(rpmEngine1.getRPM()) + "," + 
                    String(rpmEngine2.getRPM()) + "," + 
                    String(v1) + "," + 
                    String(v2) + "," + 
                    String(b1) + "," + String(b2);
      
      logger.logData(line);
      
      // Opcjonalnie: podgląd w terminalu (zakomentuj dla wydajności)
      // Serial.println(line); 
    }
  }
}