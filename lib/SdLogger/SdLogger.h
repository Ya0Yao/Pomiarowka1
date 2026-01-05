#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class SdLogger {
  private:
    int _csPin;
    String _currentFileName; // Przechowuje nazwę pliku (np. /log_5.csv)

  public:
    SdLogger(int csPin);
    
    // Funkcja szukająca wolnego numeru pliku
    void begin(); 
    
    // Zapis danych
    void logData(String data);
    
    // Pobranie nazwy pliku (do wyświetlenia na ekranie)
    String getFileName(); 
};