#include "VoltageSensor.h"

VoltageSensor::VoltageSensor(int pin, float r1, float r2) {
  _pin = pin;
  _r1 = r1;
  _r2 = r2;
  _calibrationFactor = 1.0; // Domyślnie 1.0
}

void VoltageSensor::begin() {
  pinMode(_pin, INPUT);
}

// --- TU JEST BRAKUJĄCA FUNKCJA ---
void VoltageSensor::setCalibration(float factor) {
  _calibrationFactor = factor;
}
// ---------------------------------

float VoltageSensor::readVoltage() {
  long sum = 0;
  int samples = 20;
  
  for(int i=0; i<samples; i++) {
    sum += analogRead(_pin);
    delay(2);
  }
  
  float averageRaw = sum / (float)samples;
  
  // Przeliczenie ADC (3.3V = 4095)
  float pinVoltage = (averageRaw / 4095.0) * 3.3;
  
  // Dzielnik napięcia
  float batteryVoltage = pinVoltage * ((_r1 + _r2) / _r2);
  
  // Zastosowanie kalibracji
  return batteryVoltage * _calibrationFactor;
}