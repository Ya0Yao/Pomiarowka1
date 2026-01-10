#include <Arduino.h>

volatile unsigned long globalPulseCounts[2] = {0, 0};
volatile unsigned long lastIsrTime0 = 0;
volatile unsigned long lastIsrTime1 = 0;
const unsigned long DEBOUNCE_US = 1000; 

void IRAM_ATTR isr_sensor0() { 
  unsigned long now = micros();
  if (now - lastIsrTime0 > DEBOUNCE_US) { globalPulseCounts[0]++; lastIsrTime0 = now; }
}

void IRAM_ATTR isr_sensor1() { 
  unsigned long now = micros();
  if (now - lastIsrTime1 > DEBOUNCE_US) { globalPulseCounts[1]++; lastIsrTime1 = now; }
}

// --- KONFIGURACJA WYGŁADZANIA ---
#define AVG_SAMPLES 10 // Uśredniamy z ostatnich 10 pomiarów (czyli z 1 sekundy)

class RpmSensor {
  private:
    int pin, sensorId, pulsesPerRev;
    unsigned long lastTime, interval;
    
    // Zmienne do średniej kroczącej
    unsigned long readings[AVG_SAMPLES]; // Bufor na wyniki
    int readIndex;
    unsigned long total;
    unsigned long averageRpm;

  public:
    RpmSensor(int sensorPin, int id, unsigned long readInterval) {
      pin = sensorPin; sensorId = id; interval = readInterval;
      lastTime = 0;
      if (sensorId > 1) sensorId = 1;
      pulsesPerRev = 2; // 2 magnesy

      // Inicjalizacja bufora zerami
      readIndex = 0;
      total = 0;
      averageRpm = 0;
      for (int i = 0; i < AVG_SAMPLES; i++) {
        readings[i] = 0;
      }
    }

    void begin() {
      pinMode(pin, INPUT); 
      if (sensorId == 0) attachInterrupt(digitalPinToInterrupt(pin), isr_sensor0, FALLING);
      else attachInterrupt(digitalPinToInterrupt(pin), isr_sensor1, FALLING);
    }

    void update() {
      if (millis() - lastTime >= interval) {
        float minutes = (millis() - lastTime) / 60000.0;
        lastTime = millis();
        
        noInterrupts();
        unsigned long pulses = globalPulseCounts[sensorId];
        globalPulseCounts[sensorId] = 0;
        interrupts();

        // 1. Oblicz surowy, "skaczący" wynik
        unsigned long rawRpm = 0;
        if (minutes > 0) {
           rawRpm = (unsigned long)((float)pulses / (float)pulsesPerRev / minutes);
        }

        // 2. ŚREDNIA KROCZĄCA (Wygładzanie)
        total = total - readings[readIndex];       // Odejmij najstarszy wynik
        readings[readIndex] = rawRpm;              // Wpisz nowy surowy wynik
        total = total + readings[readIndex];       // Dodaj nowy do sumy
        readIndex = readIndex + 1;                 // Przesuń indeks
        if (readIndex >= AVG_SAMPLES) readIndex = 0; // Zapętl indeks

        averageRpm = total / AVG_SAMPLES;          // Oblicz średnią
      }
    }

    // Zwracamy teraz wygładzoną wartość
    unsigned long getRPM() { 
      return averageRpm; 
    }
};