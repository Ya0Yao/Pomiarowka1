#include "VoltageSensor.h"

VoltageSensor::VoltageSensor(int pin, float r1, float r2) {
  _pin = pin;
  _r1 = r1;
  _r2 = r2;
  _calibrationFactor = 1.0; 
}

void VoltageSensor::begin() {
  pinMode(_pin, INPUT);
}

void VoltageSensor::setCalibration(float factor) {
  _calibrationFactor = factor;
}

float VoltageSensor::readVoltage() {
  long sum = 0;
  int samples = 20;
  
  for(int i=0; i<samples; i++) {
    sum += analogRead(_pin);
    delay(2);
  }
  
  float averageRaw = sum / (float)samples;
  float pinVoltage = (averageRaw / 4095.0) * 3.3;
  float batteryVoltage = pinVoltage * ((_r1 + _r2) / _r2);
  
  float finalVoltage = batteryVoltage * _calibrationFactor;

  // --- ZMIANA: MARTWA STREFA 1.0V ---
  // Podnosimy próg z 0.5 na 1.0V, aby wyeliminować przypadkowe "duchy"
  // na niepodłączonych kablach.
  if (finalVoltage < 1.0) {
    return 0.0;
  }

  return finalVoltage;
}