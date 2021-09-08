#include <LowPower.h>
#include <SoftwareSerial.h>
#include "src/SendOnlySoftwareSerial.h"
#include "src/lin_frame.h"

#define SERIAL_BAUD   115200

#define RPI_RELAY_PIN 5
#define RPI_TIMEOUT   600000 // 10 minutes

#define LIN_RX_PIN    6
#define LIN_TX_PIN    9
#define LIN_CS_PIN    8
#define LIN_FAULT_PIN 7

#define SYN_FIELD     0x55
#define SWM_ID        0x20

#define RTI_TX_PIN    4
#define RTI_INTERVAL  100

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
#define BUTTON_NEXT     0x10
#define BUTTON_PREV     0x2

bool RTI_ON = false;
int RTI_BRIGHTNESS = 10;
enum RTI_DISPLAY_MODE_NAME { RTI_RGB, RTI_PAL, RTI_NTSC, RTI_OFF };
const char RTI_DISPLAY_MODES[] = { 0x40, 0x45, 0x4C, 0x46 };
const char RTI_BRIGHTNESS_LEVELS[] = { 0x20, 0x61, 0x62, 0x23, 0x64, 0x25, 0x26, 0x67, 0x68, 0x29, 0x2A, 0x2C, 0x6B, 0x6D, 0x6E, 0x2F };

unsigned long currentMillis, lastSWMFrameAt, lastRtiWriteAt, lastSerialAt, piShutdownAt;

void setup()
{      
  Serial.begin(SERIAL_BAUD);

  RTI.begin(2400);  
  LIN.begin(9600);

  // LINBUS pins
  pinMode(LIN_CS_PIN, OUTPUT);
  digitalWrite(LIN_CS_PIN, HIGH);
  pinMode(LIN_FAULT_PIN, OUTPUT);
  digitalWrite(LIN_FAULT_PIN, HIGH);

  linFrame = LinFrame();

  // RPi relay
  pinMode(RPI_RELAY_PIN, OUTPUT);
  digitalWrite(RPI_RELAY_PIN, LOW);

  // LED off
  digitalWrite(LED_BUILTIN, LOW);

  debugln("Setup complete");
 
}

void loop()
{
  currentMillis = millis();

  // Process serial input
  serialLoop();

  // Process LINBUS input
  linbusLoop();

  // Send data to RTI Volvo Display
  rtiLoop();
  
  // Check if we need to sleep
  if (since(lastSWMFrameAt) > RPI_TIMEOUT) {        
    powerOffPi();    
  } else {
    powerOnPi(); 
  }
}

void linbusLoop()
{
  if (LIN.available()) {
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
  }
}

void parseLinFrame()
{
  if (linFrame.get_byte(0) != SWM_ID) {
    return;
  }

  lastSWMFrameAt = currentMillis;

  // skip zero values 20 0 0 0 0 FF
  if (linFrame.get_byte(5) == 0xff) {
    return;    
  }

  if (!linFrame.isValid()) {
    return;
  }    

  handleJoystick();
  handleButton();

  // for (int i = 0; i < linFrame.num_bytes(); i++) {
  //   Serial.print(linFrame.get_byte(i), HEX);
  //   Serial.print(" ");
  // }
  // Serial.println();  
}

void handleJoystick() 
{
  byte joystick = linFrame.get_byte(1);

  if (!joystick) {
    return;
  }

  switch (joystick) {
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
}

void handleButton()
{
  byte button = linFrame.get_byte(2);

  if (!button) {
    return;
  }

  switch (button) {
    case BUTTON_ENTER:
      Serial.println("EVENT_KEY_ENTER");
      break;
    case BUTTON_BACK:
      Serial.println("EVENT_KEY_BACK");
      break;
  }
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
      RTI_ON = true;
      Serial.println("CMD_DISPLAY_UP");
    } else if (strcmp(input, "DISPLAY_DOWN") == 0) {
      RTI_ON = false;
      Serial.println("CMD_DISPLAY_DOWN");
    } else if (strcmp(input, "RPI_OFF") == 0) {
      digitalWrite(RPI_RELAY_PIN, LOW);
      Serial.println("CMD_RPI_OFF");
    } else if (strcmp(input, "RPI_ON") == 0) {
      digitalWrite(RPI_RELAY_PIN, HIGH);
      Serial.println("CMD_RPI_ON");
    }
  } else {
    if (strcmp(command, "DISPLAY_BRIGHTNESS") == 0) {
      int brightness = atoi(argument);
      if (brightness > -1 && brightness < 16) {        
        RTI_BRIGHTNESS = brightness;
        Serial.println("CMD_DISPLAY_BRIGHTNESS");
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
  delay(RTI_INTERVAL);
}

int rpiStep = -1;
void powerOffPi()
{  
  if (digitalRead(RPI_RELAY_PIN) == LOW) {
    return;
  }
  
  if (rpiStep == -1) {
    Serial.println("EVENT_SHUTDOWN");
    piShutdownAt = currentMillis;
    rpiStep++;
  }

  if (rpiStep == 0) {
    long sincePiShutdown = since(piShutdownAt); 
    if (sincePiShutdown > 10000) {
      RTI_ON = false;
    }
    
    if (sincePiShutdown > 20000) {
      rpiStep++;
    }
  }

  if (rpiStep == 1) {
    if (digitalRead(RPI_RELAY_PIN) == HIGH) {
      digitalWrite(RPI_RELAY_PIN, LOW);      
      Serial.println("EVENT_PI_OFF");
      delay(1000); // just to make sure pi is completely off
      rpiStep = -1;
    }
  }
}

void powerOnPi()
{ 
  if (rpiStep > -1) {
    // Continue shutdown sequence
    powerOffPi(); 
    return;
  }
  
  if (digitalRead(RPI_RELAY_PIN) == LOW) {
    digitalWrite(RPI_RELAY_PIN, HIGH);
    Serial.println("EVENT_PI_ON");
    rpiStep = -1;
  }
}

long since(long timestamp) {
  return currentMillis - timestamp;
}
