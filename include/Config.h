#pragma once

// ==========================================
// LEWA LISTWA (ANALOG -> BUFOR -> DIGITAL)
// ==========================================

// --- 1. STREFA ANALOGOWA (Góra) ---
#define PIN_CURRENT_SIG   4
#define PIN_CURRENT_REF   5
#define PIN_VOLT_1        6
#define PIN_VOLT_2        7

// Parametry Kalibracyjne
#define CURRENT_DIVIDER   1.6061 
#define CURRENT_SENS      0.00625  
#define CURRENT_TURNS     2        
#define RESISTOR_R1       47000.0 
#define RESISTOR_R2       10000.0
#define CALIB_FACTOR_V1   1.13  
#define CALIB_FACTOR_V2   1.13  

// --- 2. STREFA BUFOROWA (Środek) ---
#define PIN_BTN_1         15  
#define PIN_BTN_2         16  

// --- 3. STREFA CYFROWA (Dół) ---
#define GPS_TX_PIN        17  
#define GPS_RX_PIN        18  
#define GPS_BAUD          38400 

#define PIN_ONE_WIRE      8   

// RPM - Piny bezpieczne jako input (JTAG/LOG)
#define PIN_RPM_1         3   
#define PIN_RPM_2         46  

// --- 4. STREFA SPI i EXPANSION (Sam Dół) ---
#define EXT_INT_PIN       9   
#define SD_CS_PIN         10  
#define SD_MOSI_PIN       11  
#define SD_SCK_PIN        12  
#define SD_MISO_PIN       13  
#define EXT_SPI_CS_PIN    14  

// ==========================================
// PRAWA LISTWA (MOLEX -> EKRAN -> GSM)
// ==========================================

#define PIN_MOLEX_3PIN_1     
#define PIN_MOLEX_3PIN_2     

// Ekran OLED (Software SPI)
#define OLED_RST          42
#define OLED_DC           41
#define OLED_CS           40
#define OLED_CLK          39
#define OLED_MOSI         38

// I2C (400kHz)
#define I2C_SDA_PIN       36  
#define I2C_SCL_PIN       37  
#define ADXL343_ADDR      0x53

// GSM (Prawy Dolny Róg)
#define GSM_TX_PIN        35  
#define GSM_RX_PIN        47  
#define GSM_PWR_PIN       21  
#define GSM_BAUD          115200

// Dioda Statusowa (wbudowana RGB)
#define PIN_BOARD_LED     48