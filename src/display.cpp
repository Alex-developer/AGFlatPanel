#include "agflatpanel.h"

#ifdef USE_DISPLAY

#include "lcdgfx.h"
#include "lcdgfx_gui.h"
#include "display.h"

/**
 *   Nano/Atmega328 PINS: connect LCD to D5 (D/C), D4 (CS), D3 (RES), D11(DIN), D13(CLK)
 *   Attiny SPI PINS:     connect LCD to D4 (D/C), GND (CS), D3 (RES), D1(DIN), D2(CLK)
 *   ESP8266: connect LCD to D1(D/C), D2(CS), RX(RES), D7(DIN), D5(CLK)
 */

DisplaySSD1327_128x128_SPI display(4,{-1, 9, 5, 0,-1,-1});   // Use this line for Atmega328p (3=RST, 4=CE, 5=D/C)
//DisplaySSD1327_128x128_I2C display(-1);                    // This line is suitable for most platforms by default
//DisplaySSD1327_128x128_SPI display(-1,{-1, 0, 1, 0, -1, -1); // Use this line for nano pi (RST not used, 0=CE, gpio1=D/C)
//DisplaySSD1327_128x128_SPI display(24,{-1, 0, 23, 0,-1,-1}); // Use this line for Raspberry  (gpio24=RST, 0=CE, gpio23=D/C)
//DisplaySSD1327_128x128_SPI display(22,{-1, 5, 21, 0,-1,-1}); // Use this line for ESP32 (VSPI)  (gpio22=RST, gpio5=CE for VSPI, gpio21=D/C)
//DisplaySSD1327_128x128_SPI display(4,{-1, -1, 5, 0,-1,-1});  // Use this line for ESP8266 Arduino style rst=4, CS=-1, DC=5
                                                             // And ESP8266 RTOS IDF. GPIO4 is D2, GPIO5 is D1 on NodeMCU boards


void initDisplay() {
    display.begin();
    display.setFixedFont(ssd1306xled_font6x8);

    display.clear( );
    display.setColor(GRAY_COLOR4(255));    
}

void showSerial( char * text) {
    display.setFixedFont(ssd1306xled_font6x8);
    display.clear();

    display.setColor(GRAY_COLOR4(255));
    display.printFixed(0,  8, text, STYLE_NORMAL);
}

void updateDisplay(panelMode mode, int brightness, interfaceProtocol protocol) {
    const char * strMode;
    String strBrightness;
    String strProtocol;

    if (mode == ASCOM)
        strMode =  "Mode: ASCOM";
    else
        strMode =  "Mode: MANUAL";

    int percent = round((brightness / 255.0) * 100); 
    strBrightness = "Brightness: " + String(percent) + "%";

    if (protocol == LEGACY)
        strProtocol = "Protocol: Legacy";
    else
        strProtocol = "Protocol: V4";

    display.setFixedFont(ssd1306xled_font6x8);
    display.clear();

    display.setColor(GRAY_COLOR4(255));
    display.printFixed(0,  8, "AG Flats Panel", STYLE_NORMAL);
    display.printFixed(0,  18, strMode, STYLE_NORMAL);    
    display.printFixed(0,  28, "Emulation: Alnitak", STYLE_NORMAL);    
    display.printFixed(0,  38, "Model: Flat Man", STYLE_NORMAL);       
    display.printFixed(0,  48, strProtocol.c_str(), STYLE_NORMAL);       
    display.printFixed(0,  58, strBrightness.c_str(), STYLE_NORMAL);           
}

#endif