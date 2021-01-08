// Smart Control v0.99
// WARNING: SWICTHES AND SERIAL INTEROPERABILITY IS WIP
 
#define DEBUG false

#include <Arduino.h>
//#include <string.h>       // For strcmp()
#include <EEPROM.h>         // For saving motor positions
#include "CommandBuffer.hh"
//#include "config.h"       // config file for pins, motor durations..
#include "Shutter.hh"       //TODO: replace config.h with Shutter.hh when ready
#include "ShutterSwitch.hh" //TODO: Everything to do with switches should be handled by this ShutterSwitch class
#include "tools.h"

#define   MAX_DATA_LEN      3+16   // Max length of Serial transmission data (3 + motor_amount)
#define   TERMINATOR_CHAR   '\r'   // Termination char for Serial message

Shutter shutters[8] = 
  {
  // InputPin  OutputPin     Duration
    {0, {25, 23}, {-28100, 26100}}, // Küche
    {1, {28, 26}, {-28100, 26100}}, // Esszimmer
    {2, {36, 34}, {-29500, 26100}}, // Wohnzimmer groß
    {3, {32, 30}, {-19200, 17600}}, // Wohnzimmer klein
    {4, {29, 27}, {-19200, 17800}}, // Lea links
    {5, {24, 22}, {-19200, 17800}}, // Lea rechts
    {6, {37, 35}, {-19200, 17600}}, // Bad
    {7, {33, 31}, {-28100, 26100}}  // HaWi
  };
ShutterSwitch switches[8] = 
  {
    {{51, 53}},
    {{43, 45}},
    {{42, 40}},
    {{44, 46}},
    {{48, 38}},
    {{50, 52}},
    {{47, 49}},
    {{39, 41}}
  };
const uint8_t motor_amount = sizeof(shutters) / sizeof(shutters[0]);  // Number of relays in use
const uint8_t relay_amount = motor_amount * 2;              // makes code more readable
//unsigned long message_age;                                  // For calculating end of relay switch duration after serial command

//int8_t ser_rel_movement[8];

uint8_t send_buffer[8];

CommandBuffer<HardwareSerial, MAX_DATA_LEN, TERMINATOR_CHAR> buffer(Serial1);

//void processCmdTimer(uint8_t);
void switchAction(int, int);
void processCommand(void);
void serialCommand_set(void);
void updateRelays(void);

void setup() {
  #if DEBUG
    Serial.begin(9600);
  #endif
  Serial1.begin(9600);

  for (Shutter s : shutters) {
    s.setup();
  }
}

void loop() {
  /* replace with MQTT Messaging system
  if (runningTimers == 0) { 
    if (buffer.available()) {
      processCommand();
    } else {
      buffer.update();
    }
    // alternative: process multiple commands back-to-back
    // if (!buffer.available()){ buffer.update; }
    // while (buffer.available()) {
    //   processCommand();
    //   buffer.update();
    // }
  }
  */
  for (int i = 0; i < motor_amount; i++) {
    if (shutters[i].running) {
      //processCmdTimer(i);
      shutters[i].update();
    }
    else { //TODO: Always check whether PinStateChanged. if motor is moving: stop and write position
      if (switches[i].hasChanged()) {
        switchAction(i, switches[i].getState());
      }
      else if (switches[i].getState() != 0 && switches[i].timer > 0 && millis() - switches[i].timer > MOTOR_DELAY + (unsigned)abs(shutters[i].max_durations[i%2])) { // if max time exceeded
        //i immer gerade, i%2 immer 0
        switchAction(i, switches[i].getState()); //switch isn't released, but timer finished so do the same
      }
    }
  }

  updateRelays(); 
}

// Function to set motor relays according to the relay_outputs array
void updateRelays(void) {
  for (Shutter s : shutters) {
    s.write();
  }
}

// process the data - command
// strcmp compares two strings and returns 0 if they are the same.
void processCommand(void) {
  // If buffer begins with "set" and is of correct length
  if (buffer.content[0] == 's' && buffer.content[1] == 'e' && buffer.content[2] == 't') {
    //if (strlen(buffer.content) == 3 + motor_amount) {
    send_buffer[0] = 127;
    for (int i = 1; i < 8; i++) {
      send_buffer[i] = 0;
    }
    Serial1.write(send_buffer, sizeof(send_buffer));
    
    serialCommand_set();
  }
  else if (strcmp(buffer.content, "get") == 0 ) {                               // If buffer equals "get"
    for (int i = 0; i < motor_amount; i++) {
      send_buffer[i] = EEPROM.read(i);
    }
    for (int i = motor_amount; i < 8; i++) {
      send_buffer[i] = 255;
    }
    Serial1.write(send_buffer, sizeof(send_buffer));
    
  }
  else if (strcmp(buffer.content, "reset") == 0 ) {                              // If buffer equals "reset"
    for (int i=0; i < motor_amount; i++) {
      EEPROM.write(i, 0);
    }
    send_buffer[0] = 129;
      for (int i = 1; i < 8; i++) {
        send_buffer[i] = 0;
      }
    Serial1.write(send_buffer, sizeof(send_buffer));

  }
  else {
    send_buffer[0] = 130;
      for (int i = 1; i < 8; i++) {
        send_buffer[i] = 0;
      }
    Serial1.write(send_buffer, sizeof(send_buffer));
  }
}

void switchAction(int switchID, int switchState) {
  if (switchState == 0) {
    shutters[switchID].set(MOTOR_ON, switchState);
    switches[switchID].timer = millis();
  } else {
    shutters[switchID].set(MOTOR_OFF, MOTOR_DOWN);

    if (millis() - switches[switchID].timer > MOTOR_DELAY) {
      int rel_movement = switchState * ((float)(millis() - switches[switchID].timer) / (MOTOR_DELAY + abs(shutters[switchID].max_durations[DIR2BOOL(switchState)])) * 100);
      // Make sure rel_movement is has legal value
      // Is this redundant?
      if (rel_movement > 100) {rel_movement = 100;}
      else if (rel_movement < -100) {rel_movement = -100;}
      #if DEBUG
        Serial.print("relative_movement: ");
        Serial.println(rel_movement);
      #endif
      int newPos = (signed)EEPROM.read(switchID) + rel_movement;
      if (newPos > 100) {newPos = 100;}
      else if (newPos < 0) {newPos = 0;}
      EEPROM.write(switchID, newPos);
    }
  }
  switches[switchID].timer = 0; //reset switch timer once it's no longer in use
}

void serialCommand_set(void) {
  for (uint8_t i=0; i < (strlen(buffer.content)-3); i +=2) {
    uint8_t motorID = CHAR2INT(buffer.content[i+3]);
    if (0 <= motorID && motorID < motor_amount) { // buffer.content[i+3]=Motor-Index
      shutters[motorID].move(buffer.read(i+4));
    }
  }
}
//TODO: replace with shutter.update() (almost done)
// void processCmdTimer(uint8_t motorID) {
//   if (millis() - shutters >= shutters[motorID].move_duration) { //MOTOR_DELAY is accounted for
//     shutters[motorID].stop();
//   }
// }