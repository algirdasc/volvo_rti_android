#include <LowPower.h>
#include <SoftwareSerial.h>
#include "lin_frame.h"

#define SERIAL_BAUD 115200

#define WAKE_UP_PIN 2

#define RPI_RELAY_PIN 5

#define LIN_RX_PIN 6
#define LIN_TX_PIN 9
#define LIN_CS_PIN 8
#define LIN_FAULT_PIN 7

#define SYN_FIELD 0x55
#define SWM_ID 0x20

#define RTI_TX_PIN 4
#define RTI_RX_PIN 3
#define RTI_INTERVAL 100

#define DEBUG 0

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

SoftwareSerial RTI(RTI_RX_PIN, RTI_TX_PIN);
SoftwareSerial LIN(LIN_RX_PIN, LIN_TX_PIN);

// Volvo V50 2007 SWM key codes

// BTN_NEXT       20 0 10 0 0 EF
// BTN_PREV       20 0 2 0 0 FD
// BTN_VOL_UP     20 0 0 1 0 FE
// BTN_VOL_DOWN   20 0 80 0 0 7F
// BTN_BACK       20 0 1 0 0 F7
// BTN_ENTER      20 0 8 0 0 FE
// BTN_UP         20 1 0 0 0 FE
// BTN_DOWN       20 2 0 0 0 FD
// BTN_LEFT       20 4 0 0 0 FB
// BTN_RIGHT      20 8 0 0 0 F7

// IGN_KEY_ON     50 E 0 F1
LinFrame linFrame;

// Volvo RTI Display;
bool RTI_ON = false;
int RTI_BRIGHTNESS = 10;
enum RTI_DISPLAY_MODE_NAME {RTI_RGB, RTI_PAL, RTI_NTSC, RTI_OFF};
const char RTI_DISPLAY_MODES[] = {0x40, 0x45, 0x4C, 0x46};
const char RTI_BRIGHTNESS_LEVELS[] = {0x20, 0x61, 0x62, 0x23, 0x64, 0x25, 0x26, 0x67, 0x68, 0x29, 0x2A, 0x2C, 0x6B, 0x6D, 0x6E, 0x2F};

void setup()
{      
  Serial.begin(SERIAL_BAUD);
  
  LIN.begin(9600);
  RTI.begin(2400);

  // LINBUS pins
  pinMode(LIN_CS_PIN, OUTPUT);
  digitalWrite(LIN_CS_PIN, HIGH);
  pinMode(LIN_FAULT_PIN, OUTPUT);
  digitalWrite(LIN_FAULT_PIN, HIGH);

  linFrame = LinFrame();

  // RPi relay
  pinMode(RPI_RELAY_PIN, OUTPUT);
  digitalWrite(RPI_RELAY_PIN, LOW);

  // Sleep
  // enterSleep();
}

void loop()
{
  // Process serial input
  serialLoop();

  // Process LINBUX input
  linbusLoop();

  // Send data to RTI Volvo Display
  rtiLoop();
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

  // skip zero values 20 0 0 0 0 FF
  if (linFrame.get_byte(5) == 0xff) {
    return;    
  }

  if (!linFrame.isValid()) {
    return;
  }

  for (int i = 0; i < linFrame.num_bytes(); i++) {
    Serial.print(linFrame.get_byte(i), HEX);
    Serial.print(" ");
  }
  Serial.println();  
}

void serialLoop()
{
  if (Serial.available()) {
    int COMMAND_SIZE = 30;
    char input[COMMAND_SIZE + 1];
    byte size = Serial.readBytes(input, COMMAND_SIZE);
    input[size - 1] = 0;

    parseSerialCommand(input);
  }
}

void parseSerialCommand(char* input)
{ 
  char *command = strtok(input, "=");
  char *argument = strtok(NULL, "=");
  
  if (!argument) {
    if (strcmp(input, "DISPLAY_UP") == 0) {
      RTI_ON = true;
    } else if (strcmp(input, "DISPLAY_DOWN") == 0) {
      RTI_ON = false;
    } else if (strcmp(input, "PI_OFF") == 0) {
      digitalWrite(RPI_RELAY_PIN, LOW);
    } else if (strcmp(input, "PI_ON") == 0) {
      digitalWrite(RPI_RELAY_PIN, HIGH);
    } else if (strcmp(input, "POWER_SAVE") == 0) {
      enterSleep();
    }
  } else {
    if (strcmp(command, "DISPLAY_BRIGHTNESS") == 0) {
      int brightness = atoi(argument);
      if (brightness > -1 && brightness < 16) {
        RTI_BRIGHTNESS = brightness;
      }    
    }
  }
}

float lastRtiWrite = millis();
int rtiStep = 0;
void rtiLoop()
{
  if ((millis() - lastRtiWrite) > RTI_INTERVAL) {
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
    lastRtiWrite = millis();
  }
}

void rtiWrite(char byte)
{
  RTI.write(byte);
  delay(RTI_INTERVAL);
}

void checkIgnition()
{
  if (digitalRead(WAKE_UP_PIN) == HIGH) {
    // Serial.println("ignition on");
  } else {
    // Serial.println("ignition off");
  }
}

void enterSleep()
{  
   // PI shutdown
//   Serial.println("EVENT_SHUTDOWN");
//   delay(3000);
//   digitalWrite(RPI_RELAY_PIN, LOW);

   // Prepare for sleep
   Serial.flush();

   digitalWrite(LED_BUILTIN, LOW);   
   attachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN), wakeUp, CHANGE);
   LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
   detachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN));    
   digitalWrite(LED_BUILTIN, HIGH);
}

void wakeUp()
{  
  
}
