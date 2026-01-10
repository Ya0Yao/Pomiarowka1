#include "CurrentSensor.h"

CurrentSensor::CurrentSensor(int pinSignal, int pinVref, float sensitivity, float dividerRatio, int turns) {
  _pinSignal = pinSignal;
  _pinVref = pinVref;
  _sensitivity = sensitivity;
  _dividerRatio = dividerRatio;
  _turns = turns;
}

void CurrentSensor::begin() {
  pinMode(_pinSignal, INPUT);
  pinMode(_pinVref, INPUT);
}

float CurrentSensor::readCurrent() {
  long sumSig = 0;
  long sumRef = 0;
  
  // 400 próbek bez delay'a zajmie procesorowi tylko kilka milisekund.
  // Zapewnia to świetne wygładzanie bez blokowania pętli 10Hz.
  int samples = 400; 

  for(int i=0; i<samples; i++) {
    sumSig += analogRead(_pinSignal);
    sumRef += analogRead(_pinVref);
    // BRAK delay(1) - celowy zabieg dla wydajności!
  }

  // Średnia z próbek
  float avgSig = sumSig / (float)samples;
  float avgRef = sumRef / (float)samples;

  // Przeliczenie na napięcie (ESP32 ADC 12-bit: 0-4095 -> 0-3.3V)
  float voltSigPin = (avgSig / 4095.0) * 3.3;
  float voltRefPin = (avgRef / 4095.0) * 3.3;

  // Odtworzenie napięć przed dzielnikami
  float realSig = voltSigPin * _dividerRatio;
  float realRef = voltRefPin * _dividerRatio;

  // Różnica napięć (Sygnał - Vref)
  float voltageDelta = realSig - realRef;

  // Obliczenie surowego prądu
  float rawAmps = (voltageDelta / _sensitivity) / (float)_turns;

  // --- KOREKTA OFFSETU ---
  // Twój czujnik zaniża o 0.35A, więc dodajemy tę wartość.
  float finalAmps = rawAmps + 0.35; 

  // --- MARTWA STREFA (DEADZONE) ---
  // Jeśli wynik jest bliski zeru (szum), pokaż 0.00
  if (abs(finalAmps) < 0.2) {
    return 0.0;
  }

  return finalAmps;
}