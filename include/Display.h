#pragma once
#include <U8g2lib.h>
#include <SPI.h>
#include "Config.h"

class Display {
  private:
    U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2;

  public:
    Display() : u8g2(U8G2_R0, OLED_CLK, OLED_MOSI, OLED_CS, OLED_DC, OLED_RST) {}

    void begin() {
      u8g2.begin();
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(10, 30, "System OK");
      u8g2.sendBuffer();
    }

    void showStatus(String line1, String line2) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0, 25, line1.c_str());
      u8g2.drawStr(0, 45, line2.c_str());
      u8g2.sendBuffer();
    }

    // --- ZAKTUALIZOWANA FUNKCJA (Dodano argument cpuTemp) ---
    void updateScreen(float temp, float cpuTemp, unsigned long rpm1, unsigned long rpm2, float v1, float v2, bool b1, bool b2, unsigned long timeMs) {
      u8g2.clearBuffer();

      // 1. PASEK GÓRNY (Czas | CPU | Przyciski)
      u8g2.setFont(u8g2_font_5x8_tf); // Bardzo mała, czytelna czcionka
      
      // Czas
      u8g2.setCursor(0, 8);
      u8g2.print(timeMs / 1000); u8g2.print("s");

      // CPU Temp (na środku)
      u8g2.setCursor(35, 8);
      u8g2.print("CPU:"); u8g2.print((int)cpuTemp); u8g2.print("C");

      // Przyciski (po prawej)
      u8g2.setCursor(90, 8);
      if (b1) u8g2.print("[1]"); else u8g2.print(" . ");
      if (b2) u8g2.print("[2]"); else u8g2.print(" . ");

      // Linia oddzielająca górę
      u8g2.drawLine(0, 10, 128, 10);

      // 2. GŁÓWNA TEMPERATURA (Silnik/Zewn)
      u8g2.setFont(u8g2_font_ncenB08_tr); 
      u8g2.setCursor(0, 26);
      u8g2.print("Ext Temp: "); 
      if(temp <= -50) u8g2.print("--"); else u8g2.print(temp, 1);

      // 3. SEKCJA DANYCH (Obroty i Volty)
      u8g2.setFont(u8g2_font_6x10_tf);

      // Lewa strona (Silnik 1)
      u8g2.setCursor(0, 42);
      u8g2.print("R1:"); u8g2.print(rpm1);
      u8g2.setCursor(0, 56);
      u8g2.print("V1:"); u8g2.print(v1, 1); u8g2.print("V");

      // Prawa strona (Silnik 2)
      u8g2.setCursor(68, 42);
      u8g2.print("R2:"); u8g2.print(rpm2);
      u8g2.setCursor(68, 56);
      u8g2.print("V2:"); u8g2.print(v2, 1); u8g2.print("V");
      
      // Pionowa kreska na dole
      u8g2.drawLine(64, 32, 64, 64);

      u8g2.sendBuffer();
    }
};