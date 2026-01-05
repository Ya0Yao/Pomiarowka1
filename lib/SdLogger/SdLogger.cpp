#include "SdLogger.h"

SdLogger::SdLogger(int csPin) {
  _csPin = csPin;
  _currentFileName = "/log_0.csv"; // Domyślna nazwa
}

void SdLogger::begin() {
  // Pętla sprawdzająca pliki od log_0.csv do log_999.csv
  for (int i = 0; i < 1000; i++) {
    String candidateName = "/log_" + String(i) + ".csv";
    
    // Jeśli plik o takiej nazwie NIE istnieje...
    if (!SD.exists(candidateName)) {
      _currentFileName = candidateName; // ...to bierzemy tę nazwę!
      return; 
    }
  }
}

void SdLogger::logData(String data) {
  // Otwieramy plik o ustalonej nazwie w trybie dopisywania (APPEND)
  File file = SD.open(_currentFileName, FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
  }
}

String SdLogger::getFileName() {
  return _currentFileName;
}