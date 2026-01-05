#pragma once

// --- PINY ---
#define PIN_ONE_WIRE      4   // Temperatura

#define PIN_RPM_1         14  // Obroty 1
#define PIN_RPM_2         17  // Obroty 2

#define PIN_BTN_1         1   // Przycisk 1
#define PIN_BTN_2         2   // Przycisk 2

// Karta SD (Standardowe piny ESP32-S3)
#define SD_CS_PIN         10
#define SD_MOSI_PIN       11
#define SD_SCK_PIN        12
#define SD_MISO_PIN       13

// Wyświetlacz OLED
#define OLED_MOSI         5
#define OLED_CLK          6
#define OLED_CS           7
#define OLED_DC           15
#define OLED_RST          16

// --- POMIAR NAPIĘCIA ---
#define PIN_VOLT_1        3   // Akumulator 1
#define PIN_VOLT_2        18  // Akumulator 2

// ... w pliku Config.h ...

// Jeśli kalibrowałeś V1:
#define CALIB_FACTOR_V1   1.126  // <-- Tu wpisz SWÓJ wynik dzielenia

// Jeśli kalibrowałeś V2:
#define CALIB_FACTOR_V2   1.151    // (Zostaw 1.0 jeśli jeszcze nie mierzyłeś drugiego)

// Wartości twoich rezystorów (w Ohmach)
// Jeśli użyłeś 47k i 10k, wpisz: 47000.0 i 10000.0
#define RESISTOR_R1       47000.0 
#define RESISTOR_R2       10000.0