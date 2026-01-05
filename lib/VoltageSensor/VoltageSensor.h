#pragma once
#include <Arduino.h>

class VoltageSensor {
  private:
    int _pin;
    float _r1;
    float _r2;
    float _calibrationFactor; // Zmienna kalibracyjna

  public:
    VoltageSensor(int pin, float r1, float r2);
    
    void begin();
    
    // --- TO JEST TO, CZEGO BRAKUJE ---
    void setCalibration(float factor); 
    // ---------------------------------
    
    float readVoltage();
};