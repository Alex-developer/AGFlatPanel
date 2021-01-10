#ifndef AGFLATPANELDISPLAY
#define AGFLATPANELDISPLAY

#include "agflatpanel.h"
#include <math.h>

void initDisplay();
void updateDisplay(panelMode, int, interfaceProtocol);
void showSerial( char * text);

#endif