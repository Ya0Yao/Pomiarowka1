#include "GpsSensor.h"

GpsSensor::GpsSensor(int rxPin, int txPin, long baud) {
  _rxPin = rxPin;
  _txPin = txPin;
  _baud = baud;
  _latitude = 0.0;
  _longitude = 0.0;
  _altitude = 0.0;
  _siv = 0;
  _fix = false;
}

bool GpsSensor::begin() {
  // Uruchamiamy sprzętowy UART1 na zdefiniowanych pinach
  // Serial1.begin(baud, config, rx, tx)
  Serial1.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
  
  // Czekamy chwilę na stabilizację UART
  delay(100);

  // Inicjalizacja biblioteki u-blox na Serial1
  // Dla v3 używamy metody begin z przekazaniem Streamu
  if (myGNSS.begin(Serial1)) {
    
    // Konfiguracja (opcjonalna, biblioteka v3 sama wykrywa ustawienia)
    myGNSS.setUART1Output(COM_TYPE_UBX); // Wymuszamy UBX dla wydajności
    myGNSS.setNavigationFrequency(5);    // 5Hz (5 pomiarów na sekundę)
    
    Serial.println(F("GPS: Modul NEO-F10N wykryty!"));
    return true;
  }
  
  Serial.println(F("GPS: Nie wykryto modulu!"));
  return false;
}

void GpsSensor::update() {
  // Biblioteka v3 "mieli" dane w tle przy każdym wywołaniu funkcji get...
  // ale warto sprawdzać to cyklicznie.
  
  // Sprawdzamy czy mamy fixa
  _fix = (myGNSS.getFixType() > 0);
  _siv = myGNSS.getSIV();

  if (_fix) {
    // Pobieramy dane (biblioteka zwraca long * 10^7, my konwertujemy na double)
    _latitude = myGNSS.getLatitude() / 10000000.0;
    _longitude = myGNSS.getLongitude() / 10000000.0;
    _altitude = myGNSS.getAltitudeMSL() / 1000.0; // mm -> m
  }
}

double GpsSensor::getLatitude() { return _latitude; }
double GpsSensor::getLongitude() { return _longitude; }
float GpsSensor::getAltitude() { return _altitude; }
byte GpsSensor::getSIV() { return _siv; }
bool GpsSensor::hasFix() { return _fix; }