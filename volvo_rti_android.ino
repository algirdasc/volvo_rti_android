#include <SoftwareSerial.h>
#include <EEPROM.h>
#include "src/SendOnlySoftwareSerial.h"
#include "src/lin_frame.h"

#define SERIAL_BAUD   115200

#define VERY_LONG_PRESS_BUTTON_TIMEOUT  10000 // 10 seconds
#define LONG_PRESS_BUTTON_TIMEOUT       3000 // 3 seconds
#define SWM_TIMEOUT                     300000 // 5 minutes

#define LIN_RX_PIN    12
#define LIN_TX_PIN    9
#define LIN_CS_PIN    11
#define LIN_FAULT_PIN 10

#define SYN_FIELD     0x55
#define SWM_ID        0x20

#define RTI_TX_PIN    8
#define RTI_INTERVAL  100

#define RPI_POWER_PIN 5
#define RPI_PWM_HIGH  179
#define RPI_PWM_LOW   0

#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

SendOnlySoftwareSerial RTI(RTI_TX_PIN);
SoftwareSerial LIN(LIN_RX_PIN, LIN_TX_PIN);

// BTN_NEXT       20 0 10 0 0 EF
// BTN_PREV       20 0 2 0 0 FD
// BTN_VOL_UP     20 0 0 1 0 FE
// BTN_VOL_DOWN   20 0 80 0 0 7F
// BTN_BACK       20 0 1 0 0 FE
// BTN_ENTER      20 0 8 0 0 F7
// BTN_UP         20 1 0 0 0 FE
// BTN_DOWN       20 2 0 0 0 FD
// BTN_LEFT       20 4 0 0 0 FB
// BTN_RIGHT      20 8 0 0 0 F7

LinFrame linFrame;

#define JOYSTICK_UP     0x1
#define JOYSTICK_DOWN   0x2
#define JOYSTICK_LEFT   0x4
#define JOYSTICK_RIGHT  0x8
#define BUTTON_BACK     0x1
#define BUTTON_ENTER    0x8
#define BUTTON_BOTH     0x9
#define BUTTON_NEXT     0x10
#define BUTTON_PREV     0x2

byte CURRENT_BUTTON, CURRENT_JOYSTICK;
bool RTI_ON = false;
int RTI_BRIGHTNESS = 10;
enum RTI_DISPLAY_MODE_NAME { RTI_RGB, RTI_PAL, RTI_NTSC, RTI_OFF };
const char RTI_DISPLAY_MODES[] = { 0x40, 0x45, 0x4C, 0x46 };
const char RTI_BRIGHTNESS_LEVELS[] = { 0x20, 0x61, 0x62, 0x23, 0x64, 0x25, 0x26, 0x67, 0x68, 0x29, 0x2A, 0x2C, 0x6B, 0x6D, 0x6E, 0x2F };
bool linFramesHandled = false;

unsigned long currentMillis, lastSWMFrameAt, lastRtiWriteAt, lastSerialAt, buttonPressedAt, joystickPressedAt;

void setup()
{      
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.begin(SERIAL_BAUD);

  RTI_ON = EEPROM.read(0);

  RTI.begin(2400);  
  LIN.begin(9600);

  pinMode(LIN_CS_PIN, OUTPUT);
  digitalWrite(LIN_CS_PIN, HIGH);
  
  pinMode(LIN_FAULT_PIN, OUTPUT);
  digitalWrite(LIN_FAULT_PIN, HIGH);

  linFrame = LinFrame();
    
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("VOLDUINO INIT");

  pinMode(RPI_POWER_PIN, OUTPUT);
  analogWrite(RPI_POWER_PIN, RPI_PWM_LOW);
}

bool IsSWMTimedOut, LastIsSWMTimedOut = false;

void loop()
{ 
  currentMillis = millis();

  // Process serial input
  serialLoop();

  // Process LINBUS input
  linbusLoop();

  // Send data to RTI Volvo Display
  rtiLoop();
    
  IsSWMTimedOut = since(lastSWMFrameAt) > SWM_TIMEOUT;  

  if (LastIsSWMTimedOut != IsSWMTimedOut) {
    if (IsSWMTimedOut == true) {
      debugln("RPI: Power OFF");
      powerOffPi();
    } else {
      debugln("RPI: Power ON");
      powerOnPi();
    }
  }

  LastIsSWMTimedOut = IsSWMTimedOut;
}

void linbusLoop()
{
  if (LIN.available()) {    
    digitalWrite(LED_BUILTIN, HIGH);
    
    byte b = LIN.read();
    byte n = linFrame.num_bytes();

    if (b == SYN_FIELD && n > 2 && linFrame.get_byte(n - 1) == 0) {
      linFrame.pop_byte();
      parseLinFrame();
      linFrame.reset();            
    } else if (n == LinFrame::kMaxBytes) {
      linFrame.reset();                  
    } else {
      linFrame.append_byte(b);      
    }
    
    digitalWrite(LED_BUILTIN, LOW);
  }  
}

void parseLinFrame()
{
  if (linFrame.get_byte(0) != SWM_ID) {
    // debugln("LIN frame is not SWM_ID");
    return;
  }

  lastSWMFrameAt = currentMillis;

  // skip zero values 20 0 0 0 0 FF
  if (linFrame.get_byte(5) == 0xff) {
    // debugln("LIN frame zero value");
    if (CURRENT_JOYSTICK != 0x00) {
      handleJoystickRelease();
    }

    if (CURRENT_BUTTON != 0x00) {
      handleButtonRelease(false);
    }
    
    CURRENT_BUTTON = 0x00;
    CURRENT_JOYSTICK = 0x00;
    linFramesHandled = false;
    return;    
  }

  if (!linFrame.isValid()) {
    // debugln("LIN frame is invalid");
    return;
  }    

  // Prevent flood
  handleJoystickPress();
  handleButtonPress();

  for (int i = 0; i < linFrame.num_bytes(); i++) {
    if (DEBUG) {  
      Serial.print(linFrame.get_byte(i), HEX);
      Serial.print(" ");
    }
  }  
  
  debugln("");
}

void handleJoystickPress() 
{
  byte joystick = linFrame.get_byte(1);

  if (!joystick) {
    return;
  }

  if (CURRENT_JOYSTICK == 0 && CURRENT_JOYSTICK != joystick) {
    debugln("Started joystick press");
    CURRENT_JOYSTICK = joystick;
    joystickPressedAt = currentMillis;
  }
}

void handleJoystickRelease() 
{
  if (linFramesHandled) {
    return;
  }
  
  debugln("Started joystick release");

  switch (CURRENT_JOYSTICK) {
    case JOYSTICK_UP:
      Serial.println("EVENT_KEY_UP");
      break;
    case JOYSTICK_DOWN:
      Serial.println("EVENT_KEY_DOWN");
      break;
    case JOYSTICK_LEFT:
      Serial.println("EVENT_KEY_LEFT");
      break;
    case JOYSTICK_RIGHT:
      Serial.println("EVENT_KEY_RIGHT");
      break;
  }

  linFramesHandled = true;

  debugln("Joystick handled");
}

void handleButtonPress()
{
  byte button = linFrame.get_byte(2);

  if (!button) {
    return;
  }

  if (CURRENT_BUTTON == 0x00 && CURRENT_BUTTON != button) {
    debugln("Started button press");
    CURRENT_BUTTON = button;
    buttonPressedAt = currentMillis;
  }

  bool longPressButton = since(buttonPressedAt) >= LONG_PRESS_BUTTON_TIMEOUT && since(buttonPressedAt) < VERY_LONG_PRESS_BUTTON_TIMEOUT;

  if (longPressButton) {
    handleButtonRelease(longPressButton);
  }
}

void handleButtonRelease(bool longPressButton)
{
  if (linFramesHandled) {
    return;
  }
  
  debugln("Started button release");
     
  switch (CURRENT_BUTTON) {
    case BUTTON_BOTH:
      powerOnPi();
      Serial.println("EVENT_RPI_REBOOT");
    case BUTTON_ENTER:
      if (longPressButton) {
        set_rti(!RTI_ON);
      } else {
        if (!RTI_ON) {
          set_rti(true);
        } else {
          Serial.println("EVENT_KEY_ENTER");
        }
      }
      break;
    case BUTTON_BACK:
      if (longPressButton) {
        Serial.println("EVENT_KEY_BACK_LONG");
      } else {
        Serial.println("EVENT_KEY_BACK");
      }
      break;
  }

  linFramesHandled = true;

  debugln("Button handled");
}

void serialLoop()
{
  if (Serial.available()) {
    int COMMAND_SIZE = 30;
    char input[COMMAND_SIZE + 1];
    byte size = Serial.readBytes(input, COMMAND_SIZE);
    input[size - 1] = 0;

    parseSerialCommand(input);

    lastSerialAt = currentMillis;    
  }
}

void parseSerialCommand(char* input)
{ 
  char *command = strtok(input, "=");
  char *argument = strtok(NULL, "=");
  
  if (!argument) {    
    if (strcmp(input, "DISPLAY_UP") == 0) {
      set_rti(true);
      // Serial.println("CMD_DISPLAY_UP");
    } else if (strcmp(input, "DISPLAY_DOWN") == 0) {
      set_rti(false);
      // Serial.println("CMD_DISPLAY_DOWN");
    } else if (strcmp(input, "RPI_ON") == 0) {
      clickPiRelay();
      // Serial.println("CMD_RPI_ON");
    }
  } else {
    // Not used with AT065TN14 display
    if (strcmp(command, "DISPLAY_BRIGHTNESS") == 0) {
      int brightness = atoi(argument);
      if (brightness > -1 && brightness < 16) {        
        RTI_BRIGHTNESS = brightness;
        // Serial.println("CMD_DISPLAY_BRIGHTNESS");
      }    
    }
  }
}

int rtiStep = 0;
void rtiLoop()
{
  if (since(lastRtiWriteAt) < RTI_INTERVAL) {
    return;
  }
    
  if (rtiStep == 0) {
    rtiWrite(RTI_ON ? RTI_DISPLAY_MODES[RTI_NTSC] : RTI_DISPLAY_MODES[RTI_OFF]);      
  }

  if (rtiStep == 1) {
    rtiWrite(RTI_BRIGHTNESS_LEVELS[RTI_BRIGHTNESS]);      
  }

  if (rtiStep == 2) {      
    rtiWrite(0x83);      
    rtiStep = -1;      
  }

  rtiStep++;
  lastRtiWriteAt = currentMillis;  
}

void rtiWrite(char byte)
{
  RTI.write(byte);
}

void powerOffPi()
{  
  Serial.println("EVENT_RPI_SHUTDOWN");    
}

void powerOnPi()
{ 
  Serial.println("EVENT_RPI_ON");
  clickPiRelay();  
}

void clickPiRelay()
{
  analogWrite(RPI_POWER_PIN, RPI_PWM_HIGH);
  delay(1000);
  analogWrite(RPI_POWER_PIN, RPI_PWM_LOW);
}

long since(long timestamp) {
  return currentMillis - timestamp;
}

void set_rti(bool value)
{ 
  if (RTI_ON != value) {
    EEPROM.write(0, value ? 1 : 0);
    RTI_ON = value;
  }
}
