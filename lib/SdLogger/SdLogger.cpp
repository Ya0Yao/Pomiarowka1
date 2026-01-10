#include "SdLogger.h"

SdLogger::SdLogger(int csPin) {
  _csPin = csPin;
  _currentFileName = "/log_0.csv";
  _lastFlush = 0;
}

void SdLogger::begin() {
  // Szukamy wolnej nazwy
  for (int i = 0; i < 1000; i++) {
    String candidateName = "/log_" + String(i) + ".csv";
    if (!SD.exists(candidateName)) {
      _currentFileName = candidateName;
      break; 
    }
  }

  // Otwieramy plik RAZ i trzymamy otwarty
  // Używamy FILE_WRITE (tworzy lub dopisuje)
  _logFile = SD.open(_currentFileName, FILE_WRITE);
  
  if (_logFile) {
     Serial.println("Plik otwarty: " + _currentFileName);
  } else {
     Serial.println("Blad otwarcia pliku!");
  }
}

void SdLogger::logData(String data) {
  if (_logFile) {
    _logFile.println(data);

    // Fizyczny zapis na kartę (FLUSH) robimy co 1 sekundę (1000 ms)
    // Dzięki temu nie blokujemy procesora co 0.1s
    if (millis() - _lastFlush >= 1000) {
      _logFile.flush();
      _lastFlush = millis();
    }
  } else {
    // Próba ratunkowa - ponowne otwarcie jeśli plik "zniknął"
    _logFile = SD.open(_currentFileName, FILE_WRITE);
  }
}

String SdLogger::getFileName() {
  return _currentFileName;
}

void SdLogger::close() {
  if (_logFile) {
    _logFile.close();
  }
}