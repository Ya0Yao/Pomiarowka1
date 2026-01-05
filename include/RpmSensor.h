#include <Arduino.h>

// Tablica liczników dla max 2 czujników
volatile unsigned long globalPulseCounts[2] = {0, 0};

// Osobne funkcje przerwań (muszą być "na zewnątrz" klasy)
void IRAM_ATTR isr_sensor0() { globalPulseCounts[0]++; }
void IRAM_ATTR isr_sensor1() { globalPulseCounts[1]++; }

class RpmSensor {
  private:
    int pin;
    int sensorId; // 0 dla pierwszego, 1 dla drugiego
    unsigned long lastTime;
    unsigned long interval;
    unsigned long rpm;

  public:
    // Konstruktor teraz wymaga podania ID (0 lub 1)
    RpmSensor(int sensorPin, int id, unsigned long readInterval) {
      pin = sensorPin;
      sensorId = id;
      interval = readInterval;
      rpm = 0;
      lastTime = 0;
      if (sensorId > 1) sensorId = 1; // Zabezpieczenie
    }

    void begin() {
      pinMode(pin, INPUT); // Pamiętaj o zewnętrznym dzielniku napięcia!
      
      // Przypisujemy odpowiednią funkcję przerwania zależnie od ID
      if (sensorId == 0) {
        attachInterrupt(digitalPinToInterrupt(pin), isr_sensor0, FALLING);
      } else {
        attachInterrupt(digitalPinToInterrupt(pin), isr_sensor1, FALLING);
      }
    }

    void update() {
      if (millis() - lastTime >= interval) {
        float minutes = (millis() - lastTime) / 60000.0;
        lastTime = millis();

        // Pobieramy impulsy z odpowiedniego licznika
        noInterrupts();
        unsigned long pulses = globalPulseCounts[sensorId];
        globalPulseCounts[sensorId] = 0; // Zerujemy licznik
        interrupts();

        if (minutes > 0) {
          rpm = (unsigned long)(pulses / minutes);
        } else {
          rpm = 0;
        }
      }
    }

    unsigned long getRPM() {
      return rpm;
    }
};