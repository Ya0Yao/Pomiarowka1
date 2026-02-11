#include "AccelSensor.h"

AccelSensor::AccelSensor(int32_t sensorID) : accel(sensorID) {
  _xFiltered = 0;
  _yFiltered = 0;
  _zFiltered = 0;
  _firstRun = true;
}

bool AccelSensor::begin() {
  if(!accel.begin()) return false;
  
  // Ustawiamy zakres +/- 4G (optymalne do auta osobowego/sportowego)
  // +/- 2G to za mało na dziury, +/- 8G czy 16G traci precyzję.
  accel.setRange(ADXL343_RANGE_4_G); 
  return true;
}

void AccelSensor::update() {
  sensors_event_t event;
  accel.getEvent(&event);
  
  // 1. Pobieramy surowe dane i zamieniamy na G
  float rawX = event.acceleration.x / 9.81;
  float rawY = event.acceleration.y / 9.81;
  float rawZ = event.acceleration.z / 9.81;

  // 2. Obsługa pierwszego uruchomienia
  // Jeśli to pierwszy raz, nie mamy "poprzedniej" wartości, 
  // więc przypisujemy surową, żeby filtr nie startował od zera.
  if (_firstRun) {
    _xFiltered = rawX;
    _yFiltered = rawY;
    _zFiltered = rawZ;
    _firstRun = false;
  } else {
    // 3. Filtr EMA (Średnia krocząca)
    // NowaWartość = (20% Nowego) + (80% Starego)
    _xFiltered = (_alpha * rawX) + ((1.0 - _alpha) * _xFiltered);
    _yFiltered = (_alpha * rawY) + ((1.0 - _alpha) * _yFiltered);
    _zFiltered = (_alpha * rawZ) + ((1.0 - _alpha) * _zFiltered);
  }
}

// Zwracamy już przefiltrowane dane
float AccelSensor::getX() { return _xFiltered; }
float AccelSensor::getY() { return _yFiltered; }
float AccelSensor::getZ() { return _zFiltered; }