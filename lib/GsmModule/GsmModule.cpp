#include "GsmModule.h"

GsmModule::GsmModule(int txPin, int rxPin, int pwrPin, long baud) {
  _txPin = txPin;
  _rxPin = rxPin;
  _pwrPin = pwrPin;
  _baud = baud;
  _isOnline = false;
}

bool GsmModule::begin() {
  // 1. Konfiguracja pinu PWR
  pinMode(_pwrPin, OUTPUT);
  digitalWrite(_pwrPin, LOW); // Stan spoczynkowy (zależnie od tranzystora na HAT)

  // 2. Uruchomienie UART2 (Serial2)
  // ESP32-S3 pozwala na mapowanie dowolnych pinów
  // RX: _rxPin (47), TX: _txPin (35)
  Serial2.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
  
  // Dajemy chwilę na stabilizację UART
  delay(100);

  // 3. Próba komunikacji - jeśli brak odpowiedzi, włączamy zasilanie
  Serial.println("GSM: Sprawdzanie polaczenia...");
  if (!checkStatus()) {
    Serial.println("GSM: Brak odpowiedzi. Próba włączenia zasilania (PWRKEY)...");
    powerUp();
    
    // Czekamy aż moduł wstanie (katalogowo nawet kilka sekund na 'Call Ready')
    // Ale na proste 'AT' powinien odpowiedzieć szybciej.
    for (int i = 0; i < 10; i++) {
      delay(500);
      if (checkStatus()) {
        Serial.println("GSM: Moduł uruchomiony!");
        break;
      }
    }
  }

  // 4. Konfiguracja początkowa (opcjonalna)
  if (_isOnline) {
    sendAT("ATE0"); // Wyłącz echo (żeby nie śmiecić w buforze)
    sendAT("AT+CMEE=2"); // Pełne komunikaty błędów
    return true;
  } else {
    Serial.println("GSM: Nie udalo sie uruchomic modulu.");
    return false;
  }
}

void GsmModule::powerUp() {
  // Sekwencja dla Waveshare HAT (zazwyczaj sterowane stanem WYSOKIM przez tranzystor)
  // Jeśli nie zadziała, spróbuj odwrócić stany (HIGH -> LOW -> HIGH)
  digitalWrite(_pwrPin, HIGH);
  delay(1000); // Przytrzymaj 1s (A7670E wymaga >500ms)
  digitalWrite(_pwrPin, LOW);
  delay(3000); // Czekaj na boot systemu GSM
}

bool GsmModule::checkStatus() {
  String response = sendAT("AT", 500);
  if (response.indexOf("OK") != -1) {
    _isOnline = true;
    return true;
  }
  _isOnline = false;
  return false;
}

String GsmModule::sendAT(String command, unsigned long timeout) {
  // Wyczyść bufor przed wysłaniem
  while (Serial2.available()) Serial2.read();

  Serial2.println(command);
  
  String response = "";
  unsigned long start = millis();
  
  while (millis() - start < timeout) {
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;
    }
  }
  
  // Opcjonalnie: wypisz na konsolę debug (zakomentuj w produkcji)
  // Serial.print("CMD: "); Serial.print(command); 
  // Serial.print(" -> RESP: "); Serial.println(response);

  return response;
}