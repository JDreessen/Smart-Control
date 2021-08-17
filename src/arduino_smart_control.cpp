// Smart Control v1.0-SNAPSHOT1

#define DEBUG false

#include <Arduino.h>
#include "Shutter.hh"
#include "ShutterSwitch.hh"
#include "tools.h"

#define   MAX_DATA_LEN      9      // Max length of Serial transmission data (1x statusBit + 2x motorName + 5x message + 1x terminator)
#define   TERMINATOR_CHAR   '\r'   // Termination char for Serial message

// the buffer for the received chars
// 1 extra char for the terminating character "\0"
char g_buffer[MAX_DATA_LEN + 1];


///*
Shutter shutters[8] = 
  {
  // good Duration values: (-28100, 26100), (-29500, 26100), (-19200, 17800)
  // SwitchPin OutputPin NVSkey
    {{51, 53}, {25, 23}, "M0"}, // Küche
    {{43, 45}, {28, 26}, "M1"}, // Esszimmer
    {{42, 40}, {36, 34}, "M2"}, // Wohnzimmer groß
    {{44, 46}, {32, 30}, "M3"}, // Wohnzimmer klein
    {{48, 38}, {29, 27}, "M4"}, // Lea links
    {{50, 52}, {24, 22}, "M5"}, // Lea rechts
    {{47, 49}, {37, 35}, "M6"}, // Bad
    {{39, 41}, {33, 31}, "M7"}  // HaWi
  };
//*/
/*
Shutter shutters[] = {
  {{6, 7}, {3, 2}, {-10000, 10000}, "M0"},
  {{8, 9}, {5, 4}, {-15000, 15000}, "M1"}
};
*/
//int nShutters = sizeof(shutters) / sizeof(shutters[0]);

void updateRelays(void);
void serialLoop(void);
void addData(char);
void processData(void);

void setup() {
  #if DEBUG
    Serial.begin(9600);
  #endif
  Serial1.begin(9600);


  for (Shutter& s : shutters) {
    s.sSwitch.setup();
    s.setup();
  }
}

// improved loop
void loop() {
  for (Shutter& shutter : shutters)
    shutter.loop();
  serialLoop();

  updateRelays();
}


// Function to set motor relays according to the relay_outputs array
void updateRelays() {
  for (Shutter& s : shutters) {
    s.write();
  }
}
// Function to process incoming Serial data
void serialLoop() {
  int data;
  while ( Serial1.available() > 0 ) {
    data = Serial1.read(); addData((char)data);  
    if (data == TERMINATOR_CHAR) {
      processData();
    }
  }
}
// Put received character into the buffer.
// When a complete command is received return true, otherwise return false.
// The command is terminated by Enter character ("\r")
void addData(char nextChar) {  
  // This is position in the buffer where we put next char.
  // Static var will remember its value across function calls.
  static uint8_t currentIndex = 0;

  // Ignore some characters - new line, space and tabs
  if ((nextChar == '\n') || (nextChar == ' ') || (nextChar == '\t')) {
    return;
  }
  
  // If we receive Enter character...
  if (nextChar == TERMINATOR_CHAR) {
    // ...terminate the string by NULL character "\0" and return true
    g_buffer[currentIndex] = '\0';
    currentIndex = 0;
    return;
  }

  // For normal character just store it in the buffer and increment Index
  g_buffer[currentIndex] = nextChar;
  currentIndex++;

  // Check for too many chars
  if (currentIndex >= MAX_DATA_LEN) {
    // If data exceeds max length reset our position and return true
    // so that the data received so far can be processed - the caller should
    // see if it is valid command or not
    g_buffer[MAX_DATA_LEN] = '\0';
    currentIndex = 0;
    return;
  }
}

// process the data - command
void processData(void) {
  #if DEBUG
    Serial.println(g_buffer);
  #endif
  //WIP: add ability to configure delays via mqtt
  //NOTE: putting switch statement into for loop may be cleaner
  switch (g_buffer[0]) {
    case 's':
      for (Shutter& shutter : shutters) {
        if (!memcmp(g_buffer+1, shutter.getName(), 2)) {
          shutter.stopMovement();
          shutter.moveTo(atoi(g_buffer+3)); //TODO: maybe sanitize input via min(abs(atoi(bla)), 65535)
          break;
        }
      }
      break;
    case 'u':
      for (Shutter& shutter : shutters) {
        if (!memcmp(g_buffer+1, shutter.getName(), 2)) {
          shutter.updateMaxUpDuration(atoi(g_buffer+3));
          break;
        }
      }
      break;
    case 'd':
      for (Shutter& shutter : shutters) {
        if (!memcmp(g_buffer+1, shutter.getName(), 2)) {
          shutter.updateMaxDownDuration(atoi(g_buffer+3));
          break;
        }
      }
      break;
    case 'r':
      for (Shutter& shutter : shutters) {
        if (!memcmp(g_buffer+1, shutter.getName(), 2)) {
          shutter.updateReactionDelay(atoi(g_buffer+3));
          break;
        }
      }
      break;
  }

  // if (g_buffer[2] == ':' && g_buffer[6] == '\0') {
  //   for (Shutter& shutter : shutters) {
  //     if (!memcmp(g_buffer, shutter.getName(), 2)) {
  //       shutter.stopMovement();
  //       shutter.moveTo(atoi(g_buffer+3));
  //     }
  //   }
  // }
  //END WIP
}