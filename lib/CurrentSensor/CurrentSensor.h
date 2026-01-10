#pragma once
#include <Arduino.h>

class CurrentSensor {
  private:
    int _pinSignal;
    int _pinVref;
    float _sensitivity;
    float _dividerRatio;
    int _turns;

  public:
    // Konstruktor
    CurrentSensor(int pinSignal, int pinVref, float sensitivity, float dividerRatio, int turns);
    
    void begin();
    
    // Funkcja odczytu (zoptymalizowana pod szybkość)
    float readCurrent();
};