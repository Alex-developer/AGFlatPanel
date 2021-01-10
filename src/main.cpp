/*
AG Flat Box

A PC Controlled Flats panel, with manual override.

The flats panel emulates an Alnitak Flat panel using two protocols, one will work with SGP and
the other with NINA. The protocal selection is via a gpio pin, using a switch.

The flat panel is a modified cheap light panel from amazon (uk) (https://www.amazon.co.uk/gp/product/B073F97NS3)

The panel has the electronics removed and the leds connected directly to the PWM controller.

A rotary encoder, with switch, is used to allow manual control

*/

/*
What: LEDLightBoxAlnitak - PC controlled lightbox implmented using the 
	Alnitak (Flip-Flat/Flat-Man) command set found here:
	http://www.optecinc.com/astronomy/pdf/Alnitak%20Astrosystems%20GenericCommandsR3.pdf
Who: 
	Created By: Jared Wellman - jared@mainsequencesoftware.com
When: 
	Last modified:  2013/May/05
Typical usage on the command prompt:
Send     : >S000\n      //request state
Recieve  : *S19000\n    //returned state
Send     : >B128\n      //set brightness 128
Recieve  : *B19128\n    //confirming brightness set to 128
Send     : >J000\n      //get brightness
Recieve  : *B19128\n    //brightness value of 128 (assuming as set from above)
Send     : >L000\n      //turn light on (uses set brightness value)
Recieve  : *L19000\n    //confirms light turned on
Send     : >D000\n      //turn light off (brightness value should not be changed)
Recieve  : *D19000\n    //confirms light turned off.
*/

/*
  What: LEDLightBoxAlnitak - PC controlled lightbox implmented using the
  Alnitak (Flip-Flat/Flat-Man) command set found here:
  https://www.optecinc.com/astronomy/catalog/alnitak/resources/Alnitak_GenericCommandsR4.pdf
  Who:
  Created By: Jared Wellman - jared@mainsequencesoftware.com
  Adapted to V4 protocol, motor handling added By: Igor von Nyssen - igor@vonnyssen.com
  When:
  Last modified:  2020/January/19
  Typical usage on the command prompt:
  Send     : >SOOO\r      //request state
  Recieve  : *S19OOO\n    //returned state
  Send     : >B128\r      //set brightness 128
  Recieve  : *B19128\n    //confirming brightness set to 128
  Send     : >JOOO\r      //get brightness
  Recieve  : *B19128\n    //brightness value of 128 (assuming as set from above)
  Send     : >LOOO\r      //turn light on (uses set brightness value)
  Recieve  : *L19OOO\n    //confirms light turned on
  Send     : >DOOO\r      //turn light off (brightness value should not be changed)
  Recieve  : *D19OOO\n    //confirms light turned off.
  Tested with this stepper motor: https://smile.amazon.com/gp/product/B01CP18J4A/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1
  and these boards
   Arduino Uno - https://smile.amazon.com/gp/product/B01EWOE0UU/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1
   Arduino Leonardo - https://smile.amazon.com/gp/product/B00R237VGO/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1
  and NINA imaging software: https://nighttime-imaging.eu
*/

#include <Arduino.h>
#include <Stepper.h>
#include <math.h>
#include "agflatpanel.h"
#include "pwm.h"
#include "display.h"

/*
* The pins for the rotary encoder
* Adjust as required
*/
#define BRIGHTNESS_ENCODER_PIN_A 2
#define BRIGHTNESS_ENCODER_PIN_B 3
#define BRIGHTNESS_ENCODER_BUTTON 6

// The increment in brightness for each turn of the rotary encoder
#define MANUAL_INCREMENT 1

// The PWM drive pin for the flat panel
#define FLAT_PANEL_PWM_PIN 10

// The pinn controlling the manual led
#define MANUAL_LED_PIN 7

// The pin for the switch to choose protocols
#define PROTOCOL_SWITCH 8

/*
* The pins for the stepper motor
* Adjust as required
*/
#define STEPPER_MOTOR_PIN_1 8
#define STEPPER_MOTOR_PIN_2 10
#define STEPPER_MOTOR_PIN_3 9
#define STEPPER_MOTOR_PIN_4 11
// steps per revolution for the motor
#define STEPS 2038 
// desired motor speed
#define RPMS 1 

int encoderPos = 0;
int brightness = 0;
int lastBrightness = 0;
panelMode mode = ASCOM;
panelMode lastMode = ASCOM;
int lastButtonState = -1;
int valRotary;
int lastValRotary;
interfaceProtocol protocol = LEGACY;

Stepper stepper(STEPS, STEPPER_MOTOR_PIN_1, STEPPER_MOTOR_PIN_2, STEPPER_MOTOR_PIN_3, STEPPER_MOTOR_PIN_4); 

devices deviceId = FLAT_MAN;
motorStatuses motorStatus = STOPPED;
lightStatuses lightStatus = OFF;
shutterStatuses coverStatus = NEITHER_OPEN_NOR_CLOSED;
motorDirections motorDirection = NONE;
float targetAngle = 0.0;
float currentAngle = 0.0;

void setup()
{
  pinMode(PROTOCOL_SWITCH, INPUT_PULLUP);
  protocol = (interfaceProtocol)digitalRead(PROTOCOL_SWITCH);

  #ifdef USE_DISPLAY
    initDisplay();
    updateDisplay(mode, brightness, protocol);
  #endif

  setPWMFrequency();

  Serial.begin(9600);
  pinMode(BRIGHTNESS_ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(BRIGHTNESS_ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(BRIGHTNESS_ENCODER_BUTTON, INPUT_PULLUP);
  attachInterrupt(0, doEncoder, CHANGE);
  pinMode(FLAT_PANEL_PWM_PIN, OUTPUT);
  pinMode(MANUAL_LED_PIN, OUTPUT);
  setBrightness(brightness);
  setState(mode);


}

void loop()
{
  processEncoder();
  processSerial();

  protocol = (interfaceProtocol)digitalRead(PROTOCOL_SWITCH);
}

/**
 * Set the LED to indicate the panels mode, either ASCOM or MANUAL
 */
void setState(panelMode mode) {
  if (mode != lastMode) {
    if (mode == ASCOM) {
      digitalWrite(MANUAL_LED_PIN, 0);
    } else {
      digitalWrite(MANUAL_LED_PIN, 1);
    }
    #ifdef USE_DISPLAY
      updateDisplay(mode, brightness, protocol);
    #endif
    lastMode = mode;
  }
}

/**
 * Check and process data from the rotary encoder
 * 
 * Also handles the button to switch from manual to ascom mode
 */
void processEncoder()
{
  int buttonState = digitalRead(BRIGHTNESS_ENCODER_BUTTON);
  if (lastButtonState == -1)
    lastButtonState = buttonState;

  if (buttonState == 1)
  {
    if (buttonState != lastButtonState)
    {
      if (mode == ASCOM) {
        mode = MANUAL;
      } else {
        mode = ASCOM;
      }
      setState(mode);
      if (mode == ASCOM)
        setBrightness(0);
      else
        setBrightness(brightness);
    }
  }
  lastButtonState = buttonState;

  if (valRotary > lastValRotary)
  {
    brightness -= MANUAL_INCREMENT;
    if (brightness < 0)
      brightness = 0;
    setBrightness(brightness);
  }

  if (valRotary < lastValRotary)
  {
    brightness += MANUAL_INCREMENT;
    if (brightness > 255)
      brightness = 255;
    setBrightness(brightness);
  }
  lastValRotary = valRotary;
}

/**
 * Sets the brightness of the display via the PWM pin
 */
void setBrightness(int level) {
    if (level != lastBrightness) {
      analogWrite(FLAT_PANEL_PWM_PIN, level);
      #ifdef USE_DISPLAY
        updateDisplay(mode, level, protocol);
      #endif
      lastBrightness = level;
    }
}


/**
 * Process data from the serial port (Ascom). 
 * 
 * This will either use the v4 protocal or legacy protocal depending on the 
 * status of the GPIO protocol pin.
 * 
 */
void processSerial()
{
  if (digitalRead(PROTOCOL_SWITCH) == 1) {
    version1();
  } else {
    version4();
    handleMotor();
  }
}

/**
 * The version 4 protocol (NINA and SGP4)
 */
void version4() {
  if ( Serial.available() >= 6 ) { // all incoming communications are fixed length at 6 bytes including the \n

    mode = ASCOM;
    setState(mode);
    
    char* cmd;
    char* data;
    char temp[10];

    char str[20];
    memset(str, 0, 20);
    Serial.readBytesUntil('\r', str, 20);

    cmd = str + 1;
    data = str + 2;

    // useful for debugging to make sure your commands came through and are parsed correctly.
    if ( false ) {
      sprintf( temp, "cmd = >%s%s\n", cmd, data);
      Serial.write(temp);
    }

    switch ( *cmd )
    {
      /*
        Ping device
        Request: >POOO\r
        Return : *PiiOOO\n
        id = deviceId
      */
      case 'P':
        sprintf(temp, "*P%dOOO\n", deviceId);
        Serial.write(temp);
        break;

      /*
        Open shutter
        Request: >OOOO\r
        Return : *OiiOOO\n
        id = deviceId

        This command is only supported on the Flip-Flat, set the deviceId accordingly
      */
      case 'O':
        sprintf(temp, "*O%dOOO\n", deviceId);
        setShutter(OPEN);
        Serial.write(temp);
        break;


      /*
        Close shutter
        Request: >COOO\r
        Return : *CiiOOO\n
        id = deviceId

        This command is only supported on the Flip-Flat, set the deviceId accordingly
      */
      case 'C':
        sprintf(temp, "*C%dOOO\n", deviceId);
        setShutter(CLOSED);
        Serial.write(temp);
        break;

      /*
        Turn light on
        Request: >LOOO\r
        Return : *LiiOOO\n
        id = deviceId
      */
      case 'L':
        sprintf(temp, "*L%dOOO\n", deviceId);
        Serial.write(temp);
        lightStatus = ON;
        setBrightness(brightness);
        break;

      /*
        Turn light off
        Request: >DOOO\r
        Return : *DiiOOO\n
        id = deviceId
      */
      case 'D':
        sprintf(temp, "*D%dOOO\n", deviceId);
        Serial.write(temp);
        lightStatus = OFF;
        setBrightness(0);
        break;

      /*
        Set brightness
        Request: >Bxxx\r
         xxx = brightness value from 000-255
        Return : *Biiyyy\n
        id = deviceId
         yyy = value that brightness was set from 000-255
      */
      case 'B':
        brightness = atoi(data);
        if ( lightStatus == ON )
          setBrightness(brightness);
        sprintf( temp, "*B%d%03d\n", deviceId, brightness );
        Serial.write(temp);
        break;

      /*
        Get brightness
        Request: >JOOO\r
        Return : *Jiiyyy\n
        id = deviceId
         yyy = current brightness value from 000-255
      */
      case 'J':
        sprintf( temp, "*J%d%03d\n", deviceId, brightness);
        Serial.write(temp);
        break;

      /*
        Get device status:
        Request: >SOOO\r
        Return : *SidMLC\n
        id = deviceId
         M  = motor status( 0 stopped, 1 running)
         L  = light status( 0 off, 1 on)
         C  = Cover Status( 0 moving, 1 closed, 2 open, 3 timed out)
      */
      case 'S':
        sprintf( temp, "*S%d%d%d%d\n", deviceId, motorStatus, lightStatus, coverStatus);
        Serial.write(temp);
        break;

      /*
        Get firmware version
        Request: >VOOO\r
        Return : *Vii001\n
        id = deviceId
      */
      case 'V': // get firmware version
        sprintf(temp, "*V%d003\n", deviceId);
        Serial.write(temp);
        break;
    }

    while ( Serial.available() > 0 ) {
      Serial.read();
    }
  }
}

/**
 * The version 1 protocol (SGP3)
 */
void version1() {
  if (Serial.available() >= 6) // all incoming communications are fixed length at 6 bytes including the \n
  {

    mode = ASCOM;
    setState(mode);

    char *cmd;
    char *data;
    char temp[10];

    char str[20];
    memset(str, 0, 20);

    // I don't personally like using the \n as a command character for reading.
    // but that's how the command set is.
    Serial.readBytesUntil('\n', str, 20);

    cmd = str + 1;
    data = str + 2;

    // useful for debugging to make sure your commands came through and are parsed correctly.
    if (false)
    {
      sprintf(temp, "cmd = >%s%s;", cmd, data);
      Serial.println(temp);
    }

    switch (*cmd)
    {
      /*
    Ping device
      Request: >P000\n
      Return : *Pii000\n
        id = deviceId
    */
    case 'P':
      sprintf(temp, "*P%d000", deviceId);
      Serial.println(temp);
      break;

    /*
    Open shutter
      Request: >O000\n
      Return : *Oii000\n
        id = deviceId

      This command is only supported on the Flip-Flat!
    */
    case 'O':
      sprintf(temp, "*O%d000", deviceId);
      setShutter(OPEN);
      Serial.println(temp);
      break;

    /*
    Close shutter
      Request: >C000\n
      Return : *Cii000\n
        id = deviceId

      This command is only supported on the Flip-Flat!
    */
    case 'C':
      sprintf(temp, "*C%d000", deviceId);
      setShutter(CLOSED);
      Serial.println(temp);
      break;

      /*
    Turn light on
      Request: >L000\n
      Return : *Lii000\n
        id = deviceId
    */
    case 'L':
      sprintf(temp, "*L%d000", deviceId);
      Serial.println(temp);
      lightStatus = ON;
      setBrightness(brightness);
      break;

      /*
    Turn light off
      Request: >D000\n
      Return : *Dii000\n
        id = deviceId
    */
    case 'D':
      sprintf(temp, "*D%d000", deviceId);
      Serial.println(temp);
      lightStatus = OFF;
      setBrightness(0);
      break;

      /*
    Set brightness
      Request: >Bxxx\n
        xxx = brightness value from 000-255
      Return : *Biiyyy\n
        id = deviceId
        yyy = value that brightness was set from 000-255
    */
    case 'B':
      brightness = atoi(data);
      if (lightStatus == ON)
        setBrightness(brightness);
      sprintf(temp, "*B%d%03d", deviceId, brightness);
      Serial.println(temp);
      break;

      /*
    Get brightness
      Request: >J000\n
      Return : *Jiiyyy\n
        id = deviceId
        yyy = current brightness value from 000-255
    */
    case 'J':
      sprintf(temp, "*J%d%03d", deviceId, brightness);
      Serial.println(temp);
      break;

      /*
    Get device status:
      Request: >S000\n
      Return : *SidMLC\n
        id = deviceId
        M  = motor status( 0 stopped, 1 running)
        L  = light status( 0 off, 1 on)
        C  = Cover Status( 0 moving, 1 closed, 2 open)
    */
    case 'S':
      sprintf(temp, "*S%d%d%d%d", deviceId, motorStatus, lightStatus, coverStatus);
      Serial.println(temp);
      break;

      /*
    Get firmware version
      Request: >V000\n
      Return : *Vii001\n
        id = deviceId
    */
    case 'V': // get firmware version
      sprintf(temp, "*V%d001", deviceId);
      Serial.println(temp);
      break;
    }

    while (Serial.available() > 0)
      Serial.read();
  }
}

void setShutter(shutterStatuses val)
{
  if ( val == OPEN && coverStatus != OPEN )
  {
    motorDirection = OPENING;
    targetAngle = 90.0;
  }
  else if ( val == CLOSED && coverStatus != CLOSED )
  {
    motorDirection = CLOSING;
    targetAngle = 0.0;
  }
}

void handleMotor()
{
  if (currentAngle < targetAngle && motorDirection == OPENING) {
    motorStatus = RUNNING;
    coverStatus = NEITHER_OPEN_NOR_CLOSED;
    stepper.step(1);
    currentAngle = currentAngle + 360.0 / STEPS;
    if (currentAngle >= targetAngle) {
      motorStatus = STOPPED;
      motorDirection = NONE;
      coverStatus = OPEN;
    }
  } else if (currentAngle > targetAngle && motorDirection == CLOSING) {
    motorStatus = RUNNING;
    coverStatus = NEITHER_OPEN_NOR_CLOSED;
    stepper.step(-1);
    currentAngle = currentAngle - 360.0 / STEPS;
    if (currentAngle <= targetAngle) {
      motorStatus = STOPPED;
      motorDirection = NONE;
      coverStatus = CLOSED;
    }
  }
}

/**
 * Handles the rotary encoder interrupt
 */
void doEncoder()
{
  if (mode == MANUAL)
  {
    if (digitalRead(BRIGHTNESS_ENCODER_PIN_A) == digitalRead(BRIGHTNESS_ENCODER_PIN_B))
      encoderPos++;
    else
      encoderPos--;
    valRotary = encoderPos / 2.5;
  }
}