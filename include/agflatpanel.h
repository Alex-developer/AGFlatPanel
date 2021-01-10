#ifndef AGFLATPANEL
#define AGFLATPANEL

#define USE_DISPLAY 1

enum shutterStatuses {
  NEITHER_OPEN_NOR_CLOSED = 0, // ie not open or closed...could be moving
  CLOSED,
  OPEN,
  TIMED_OUT
};

enum devices
{
  FLAT_MAN_L = 10,
  FLAT_MAN_XL = 15,
  FLAT_MAN = 19,
  FLIP_FLAT = 99
};

enum motorStatuses
{
  STOPPED = 0,
  RUNNING
};

enum lightStatuses
{
  OFF = 0,
  ON
};

enum motorDirections {
  OPENING = 0,
  CLOSING,
  NONE
};

enum panelMode {
  ASCOM = 0,
  MANUAL
};

enum interfaceProtocol {
  V4 = 0,
  LEGACY
};

void doEncoder();
void processEncoder();
void processSerial();
void setShutter(shutterStatuses);
void setBrightness(int);
void version1();
void version4();
void handleMotor();
void setState(panelMode);

#endif