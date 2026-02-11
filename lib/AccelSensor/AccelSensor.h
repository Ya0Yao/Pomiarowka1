#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>

class AccelSensor {
  private:
    Adafruit_ADXL343 accel;
    
    // Zmienne do przechowywania wartości po filtracji
    float _xFiltered;
    float _yFiltered;
    float _zFiltered;
    
    // Czy to pierwszy odczyt? (żeby nie startować od zera)
    bool _firstRun;

    // Współczynnik wygładzania (0.1 = bardzo gładko/wolno, 0.9 = szarpie/szybko)
    // 0.2 to dobry kompromis do auta.
    const float _alpha = 0.2; 

  public:
    AccelSensor(int32_t sensorID = 12345);
    bool begin();
    void update();
    
    // Teraz te funkcje zwrócą już "ładne" dane
    float getX(); 
    float getY(); 
    float getZ();
};