#include "AccelSensor.h"

AccelSensor::AccelSensor(int32_t sensorID) : accel(sensorID) {
  _x = 0; _y = 0; _z = 0;
}

bool AccelSensor::begin() {
  if(!accel.begin()) return false;
  accel.setRange(ADXL343_RANGE_4_G); // Zakres +/- 4G
  return true;
}

void AccelSensor::update() {
  sensors_event_t event;
  accel.getEvent(&event);
  
  // Przeliczamy m/s^2 na G (dzielimy przez 9.81)
  _x = event.acceleration.x / 9.81;
  _y = event.acceleration.y / 9.81;
  _z = event.acceleration.z / 9.81;
}

float AccelSensor::getX() { return _x; }
float AccelSensor::getY() { return _y; }
float AccelSensor::getZ() { return _z; }