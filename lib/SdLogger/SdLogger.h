#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class SdLogger {
  private:
    int _csPin;
    String _currentFileName;
    File _logFile;            // Trzymamy otwarty plik
    unsigned long _lastFlush; // Czas ostatniego zapisu fizycznego

  public:
    SdLogger(int csPin);
    
    void begin(); 
    void logData(String data);
    String getFileName(); 
    void close(); // Funkcja do bezpiecznego zamknięcia (opcjonalna)
};