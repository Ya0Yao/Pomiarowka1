#pragma once
#include <Arduino.h>

class GsmModule {
  private:
    int _txPin;
    int _rxPin;
    int _pwrPin;
    long _baud;
    bool _isOnline;

  public:
    // Konstruktor
    GsmModule(int txPin, int rxPin, int pwrPin, long baud = 115200);

    // Inicjalizacja (uruchamia Serial2 i włącza moduł)
    bool begin();

    // Procedura włączania (impuls na pinie PWR)
    void powerUp();

    // Wysyłanie komend AT
    String sendAT(String command, unsigned long timeout = 1000);
    
    // Sprawdzenie czy moduł żyje
    bool checkStatus();

    // Gettery
    bool isOnline() { return _isOnline; }
};