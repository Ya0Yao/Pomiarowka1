#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>

class AccelSensor {
  private:
    Adafruit_ADXL343 accel;
    float _x, _y, _z;
  public:
    AccelSensor(int32_t sensorID = 12345);
    bool begin();
    void update();
    float getX(); float getY(); float getZ();
};