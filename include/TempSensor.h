#include <OneWire.h>
#include <DallasTemperature.h>

class TempSensor {
  private:
    OneWire oneWire;
    DallasTemperature sensors;
    unsigned long lastReadTime;
    unsigned long interval;
    float currentTemps[5]; // Bufor na max 5 czujników
    int deviceCount;

  public:
    TempSensor(int pin, unsigned long readInterval) 
      : oneWire(pin), sensors(&oneWire), interval(readInterval), lastReadTime(0), deviceCount(0) {}

    void begin() {
      sensors.begin();
      sensors.setWaitForConversion(false); // Tryb asynchroniczny (nie blokuje)
      deviceCount = sensors.getDeviceCount();
      Serial.print("Wykryto czujników temperatury: ");
      Serial.println(deviceCount);
      
      // Pierwsze żądanie
      sensors.requestTemperatures();
    }

   void update() {
      if (millis() - lastReadTime >= interval) {
        lastReadTime = millis();

        // 1. Odczytujemy wartości z POPRZEDNIEGO żądania
        for (int i = 0; i < deviceCount; i++) {
          float t = sensors.getTempCByIndex(i);
          
          // STARA WERSJA (zabezpieczona):
          // if (t != DEVICE_DISCONNECTED_C) {
          //   currentTemps[i] = t;
          // }

          // NOWA WERSJA (bezpośrednia):
          // Zapisujemy wartość zawsze - nawet jak jest to błąd (-127)
          currentTemps[i] = t; 
        }

        // 2. Prosimy o nowe pomiary
        sensors.requestTemperatures();
      }
    }

    float getTemp(int index) {
      if (index >= deviceCount) return -127.0;
      return currentTemps[index];
    }
    
    int getCount() { return deviceCount; }
};