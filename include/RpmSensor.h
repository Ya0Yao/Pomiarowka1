#include <Arduino.h>

// Tablica liczników dla max 2 czujników
volatile unsigned long globalPulseCounts[2] = {0, 0};

// --- DODANA SEKCJA FILTRACJI (DEBOUNCE) ---
// Zmienne przechowujące czas ostatniego impulsu dla każdego czujnika
volatile unsigned long lastIsrTime0 = 0;
volatile unsigned long lastIsrTime1 = 0;

// Czas martwy w mikrosekundach (1000us = 1ms).
// Jeśli impulsy przychodzą częściej, są ignorowane jako szum.
const unsigned long DEBOUNCE_US = 1000; 
// ------------------------------------------

// Osobne funkcje przerwań z dodanym warunkiem czasowym
void IRAM_ATTR isr_sensor0() { 
  unsigned long now = micros();
  // Akceptujemy impuls tylko jeśli minęło więcej czasu niż DEBOUNCE_US
  if (now - lastIsrTime0 > DEBOUNCE_US) {
    globalPulseCounts[0]++; 
    lastIsrTime0 = now;
  }
}

void IRAM_ATTR isr_sensor1() { 
  unsigned long now = micros();
  if (now - lastIsrTime1 > DEBOUNCE_US) {
    globalPulseCounts[1]++; 
    lastIsrTime1 = now;
  }
}

class RpmSensor {
  private:
    int pin;
    int sensorId; // 0 dla pierwszego, 1 dla drugiego
    unsigned long lastTime;
    unsigned long interval;
    unsigned long rpm;

  public:
    // Konstruktor wymaga podania ID (0 lub 1)
    RpmSensor(int sensorPin, int id, unsigned long readInterval) {
      pin = sensorPin;
      sensorId = id;
      interval = readInterval;
      rpm = 0;
      lastTime = 0;
      if (sensorId > 1) sensorId = 1; // Zabezpieczenie
    }

    void begin() {
      // Pamiętaj: przy zasilaniu 5V/12V wymagany zewnętrzny dzielnik napięcia (np. 4.7k + 10k)
      // oraz ewentualnie kondensator 100nF między pinem a masą dla lepszej filtracji.
      pinMode(pin, INPUT); 
      
      // Przypisujemy odpowiednią funkcję przerwania zależnie od ID
      // Używamy FALLING, bo czujnik NJK-5002C zwiera sygnał do masy przy wykryciu magnesu
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

        // Pobieramy impulsy z odpowiedniego licznika w sekcji krytycznej
        noInterrupts();
        unsigned long pulses = globalPulseCounts[sensorId];
        globalPulseCounts[sensorId] = 0; // Zerujemy licznik po odczycie
        interrupts();

        if (minutes > 0) {
          // Obliczamy RPM
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