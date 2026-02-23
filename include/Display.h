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
      u8g2.drawStr(10, 30, "System Start");
      u8g2.sendBuffer();
    }

    void showStatus(String line1, String line2) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0, 25, line1.c_str());
      u8g2.drawStr(0, 45, line2.c_str());
      u8g2.sendBuffer();
    }

    // Dodane dwa parametry na końcu: bool gsmReady, unsigned long gsmPing
    void updateScreen(float temp, float cpuTemp, unsigned long rpm1, unsigned long rpm2, float amps, float v2, bool b1, bool b2, bool gpsFix, float ax, float ay, float az, bool gsmReady, unsigned long gsmPing) {
      u8g2.clearBuffer();

      // --- GÓRA (Status) ---
      u8g2.setFont(u8g2_font_5x8_tf);
      
      // Temp zewn.
      u8g2.setCursor(0, 8);
      u8g2.print("T:"); u8g2.print((int)temp);

      // Status GPS i GSM na środku
      u8g2.setCursor(30, 8);
      if (gpsFix) u8g2.print("G[+]"); else u8g2.print("G[ ]");
      
      u8g2.setCursor(55, 8);
      if (gsmReady) {
        u8g2.print("LTE:"); u8g2.print(gsmPing); // Pokaże np "LTE:125" (czas pingu/wysylania)
      } else {
        u8g2.print("LTE[ ]");
      }

      // Temp CPU
      u8g2.setCursor(95, 8);
      u8g2.print("CPU:"); u8g2.print((int)cpuTemp);
      
      u8g2.drawLine(0, 10, 128, 10);

      // --- ŚRODEK (G-FORCE) ---
      u8g2.setFont(u8g2_font_6x12_tf); 
      u8g2.setCursor(0, 25);
      u8g2.print("X: "); u8g2.print(ax, 2); 
      
      u8g2.setCursor(64, 25);
      u8g2.print("Y: "); u8g2.print(ay, 2);

      u8g2.drawLine(0, 30, 128, 30);

      // --- DÓŁ (Silnik) ---
      u8g2.setFont(u8g2_font_6x10_tf);

      u8g2.setCursor(0, 42);
      u8g2.print("R1:"); u8g2.print(rpm1);
      
      u8g2.setCursor(0, 56);
      u8g2.print("I :"); u8g2.print(amps, 1); u8g2.print("A"); 

      u8g2.setCursor(68, 42);
      u8g2.print("R2:"); u8g2.print(rpm2);
      
      u8g2.setCursor(68, 56);
      u8g2.print("V2:"); u8g2.print(v2, 1); u8g2.print("V");
      
      u8g2.drawLine(64, 32, 64, 64);
      u8g2.sendBuffer();
    }
};