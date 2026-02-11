#pragma once
#include <Arduino.h>
#include <SparkFun_u-blox_GNSS_v3.h> // Biblioteka v3 dla NEO-F10N

class GpsSensor {
  private:
    SFE_UBLOX_GNSS_SERIAL myGNSS;
    int _rxPin;
    int _txPin;
    long _baud;
    
    // Zmienne do przechowywania ostatnich odczytów
    double _latitude;
    double _longitude;
    float _altitude;
    byte _siv; // Satellites In View
    bool _fix;

  public:
    GpsSensor(int rxPin, int txPin, long baud);
    
    bool begin();
    void update(); // Wywołuj w pętli loop()
    
    // Gettery
    double getLatitude();
    double getLongitude();
    float getAltitude();
    byte getSIV();
    bool hasFix();
};