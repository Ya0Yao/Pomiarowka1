#include <Arduino.h>

class Button {
  private:
    int pin;
    bool lastState;
    unsigned long lastDebounceTime;
    unsigned long debounceDelay = 50; // 50ms opóźnienia na drgania

  public:
    Button(int p) : pin(p), lastState(true), lastDebounceTime(0) {}

    void begin() {
      // INPUT_PULLUP: Pin jest normalnie w stanie WYSOKIM (1).
      // Wciśnięcie przycisku zwiera do masy (GND), dając stan NISKI (0).
      pinMode(pin, INPUT_PULLUP);
    }

    // Zwraca TRUE tylko w momencie wciśnięcia (nie trzymania)
    bool isPressed() {
      int reading = digitalRead(pin);

      if (reading != lastState) {
        lastDebounceTime = millis();
      }

      bool pressed = false;
      if ((millis() - lastDebounceTime) > debounceDelay) {
        // Jeśli stan jest NISKI (0), to znaczy że przycisk jest wciśnięty
        if (reading == LOW) { 
           pressed = true;
        }
      }
      
      // Zwracamy true, jeśli jest wciśnięty (LOW)
      return (reading == LOW);
    }
};