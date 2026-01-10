#pragma once

// --- PINY ---

// Temperatura (zostaje na 4, zgodnie z Twoją uwagą)
#define PIN_ONE_WIRE      4   

#define PIN_RPM_1         14  // Obroty 1
#define PIN_RPM_2         17  // Obroty 2

#define PIN_BTN_1         1   // Przycisk 1 (Prawa strona)
#define PIN_BTN_2         2   // Przycisk 2 (Prawa strona)

// Karta SD (Lewa strona, standard SPI)
#define SD_CS_PIN         10
#define SD_MOSI_PIN       11
#define SD_SCK_PIN        12
#define SD_MISO_PIN       13

// --- WYŚWIETLACZ OLED (NOWE MIEJSCE - PRAWA STRONA) ---
// Przeniesione na piny 38-42, aby zwolnić ADC1 na lewej stronie
#define OLED_MOSI         42
#define OLED_CLK          41
#define OLED_CS           40
#define OLED_DC           39
#define OLED_RST          38

// --- POMIAR PRĄDU (LEM HTFS 200-P) ---
// Teraz używamy zwolnionych pinów 5 i 6 (Czyste ADC1)
#define PIN_CURRENT_SIG   5   // Sygnał z czujnika
#define PIN_CURRENT_REF   6   // Vref z czujnika

// Konfiguracja: Rezystory 2k i 3.3k -> Mnożnik ~1.606
#define CURRENT_DIVIDER   1.6061 
#define CURRENT_SENS      0.00625  // 1.25V / 200A
#define CURRENT_TURNS     2        // Podwójna pętla

// --- POMIAR NAPIĘCIA ---
// Używamy zwolnionych pinów 7 i 8 (Czyste ADC1)
#define PIN_VOLT_1        7   // Akumulator 1
#define PIN_VOLT_2        8   // Akumulator 2


// Rezystory dzielników napięcia
#define RESISTOR_R1       47000.0 
#define RESISTOR_R2       10000.0

// Kalibracja napięć
#define CALIB_FACTOR_V1   1.13
#define CALIB_FACTOR_V2   1.13