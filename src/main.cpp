#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h> // Dodano obsługę I2C

#include "Config.h"
#include "TempSensor.h"
#include "SdLogger.h"
#include "RpmSensor.h"
#include "Display.h"
#include "Button.h"
#include "VoltageSensor.h"
#include "CurrentSensor.h"

// --- KONFIGURACJA CZASÓW ---
#define LOG_INTERVAL_MS   100   // 10Hz
#define SLOW_INTERVAL_MS  2000  // 0.5Hz

// --- OBIEKTY ---
// Pin OneWire brany z nowego Configu (Pin 8)
TempSensor tempModule(PIN_ONE_WIRE, SLOW_INTERVAL_MS); 
SdLogger logger(SD_CS_PIN);

// Obroty - nowe piny 3 i 46
RpmSensor rpmEngine1(PIN_RPM_1, 0, LOG_INTERVAL_MS); 
RpmSensor rpmEngine2(PIN_RPM_2, 1, LOG_INTERVAL_MS); 

// Amperomierz - nowe piny 4 i 5
CurrentSensor ammeter(PIN_CURRENT_SIG, PIN_CURRENT_REF, CURRENT_SENS, CURRENT_DIVIDER, CURRENT_TURNS);

// Woltomierze - nowe piny 6 i 7
VoltageSensor batt1(PIN_VOLT_1, RESISTOR_R1, RESISTOR_R2);
VoltageSensor batt2(PIN_VOLT_2, RESISTOR_R1, RESISTOR_R2);

Display oled;
// Przyciski - nowe piny 15 i 16
Button btn1(PIN_BTN_1);
Button btn2(PIN_BTN_2);

// --- ZMIENNE GLOBALNE ---
bool isSdReady = false;
float currentAmps = 0.0;
float v1 = 0.0;
float v2 = 0.0;
float cpuTemp = 0.0;
// Tablica na temperatury zewnetrzne (zakladamy max 3)
float extTemps[3] = {0.0, 0.0, 0.0}; 

void setup() {
  Serial.begin(115200);
  
  // --- START I2C (Dla akcelerometru ADXL343 itp) ---
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
  // --- START GPS i GSM (Serial) ---
  // Inicjalizacja portów szeregowych dla GPS i GSM (jesli beda uzywane)
  // Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  // Serial2.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);

  delay(1000); 

  oled.begin();
  oled.showStatus("System Start", "Config 2.0");
  delay(1000);

  Serial.println("\n--- START SYSTEMU (Nowy Config) ---");

  // Inicjalizacja SD
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  delay(200); 

  bool sdSuccess = false;
  for (int i = 0; i < 5; i++) { // Zmniejszylem liczbe prob dla szybkosci
    if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
      sdSuccess = true;
      break; 
    }
    oled.showStatus("SD Error", "Proba " + String(i+1));
    delay(200); 
  }

  if (!sdSuccess) { 
    oled.showStatus("BRAK KARTY SD", "Logowanie OFF");
    isSdReady = false;
    delay(2000);
  } else {
    logger.begin(); 
    isSdReady = true;
    String fileName = logger.getFileName();
    fileName.replace("/", ""); 
    oled.showStatus("SD OK:", fileName);
    delay(1000);
    // Nagłówek logu - DODANO KOLUMNY DLA 3 TERMOMETRÓW
    logger.logData("Czas_ms,Temp1,Temp2,Temp3,CPU,RPM1,RPM2,Amps,Volt1,Volt2,Btn1,Btn2"); 
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
  
  // Konfiguracja diody statusowej (jesli uzywasz)
  // pinMode(PIN_BOARD_LED, OUTPUT);
}

void loop() {
  // Szybkie aktualizacje
  rpmEngine1.update();
  rpmEngine2.update();
  tempModule.update();
  
  bool b1 = btn1.isPressed();
  bool b2 = btn2.isPressed();
  unsigned long currentMillis = millis();

  // --- SZYBKA PĘTLA (Logowanie i Prąd) ---
  static unsigned long lastLog = 0;
  if (currentMillis - lastLog >= LOG_INTERVAL_MS) {
    lastLog = currentMillis;

    currentAmps = ammeter.readCurrent();

    if (isSdReady) {
      // Pobieranie 3 temperatur (lub -127 jesli brak czujnika)
      float t1 = tempModule.getTemp(0);
      float t2 = tempModule.getTemp(1);
      float t3 = tempModule.getTemp(2);

      String line = String(currentMillis) + "," + 
                    String(t1) + "," + 
                    String(t2) + "," + 
                    String(t3) + "," + 
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

  // --- WOLNA PĘTLA (Napięcia i Temp) ---
  static unsigned long lastSlow = 0;
  if (currentMillis - lastSlow >= SLOW_INTERVAL_MS) {
    lastSlow = currentMillis;
    
    v1 = batt1.readVoltage();
    v2 = batt2.readVoltage();
    cpuTemp = temperatureRead(); 
    
    // Aktualizacja tablicy dla ekranu
    extTemps[0] = tempModule.getTemp(0);
    extTemps[1] = tempModule.getTemp(1);
    extTemps[2] = tempModule.getTemp(2);
  }

  // Ekran
  static unsigned long lastScreen = 0;
  if (currentMillis - lastScreen >= 200) {
    lastScreen = currentMillis;
    // Uwaga: Funkcja updateScreen w Display.h przyjmuje tylko 1 temperaturę.
    // Wyświetlamy pierwszą (główną), reszta jest w logach.
    oled.updateScreen(extTemps[0], cpuTemp, rpmEngine1.getRPM(), rpmEngine2.getRPM(), currentAmps, v2, b1, b2, currentMillis);
  }
}