// Smart Control v0.9
// WARNING: SWICTHES AND SERIAL INTEROPERABILITY IS WIP
 
#define DEBUG false

#include <Arduino.h>
//#include <string.h>     // For strcmp()
#include <EEPROM.h>     // For saving motor positions
#include "config.h"     // config file for pins, motor durations..

// Maybe required if position calculation is too imprecise
#define   MOTOR_DELAY       1500   // delay until motor reacts to input
#define   MOTOR_OFF         HIGH
#define   MOTOR_ON          LOW
#define   MOTOR_UP          LOW
#define   MOTOR_DOWN        HIGH
#define   MAX_DATA_LEN      3+16   // Max length of Serial transmission data (3 + motor_amount)
#define   TERMINATOR_CHAR   '\r'   // Termination char for Serial message
#define   CHAR2INT(c)       ((int)c - 48)
#define   SIGN(x)           ((x>0)? (1):(-1))
#define   DIR2BOOL(x)       ((x+1)/2)

const uint8_t input_pins[] = {INPUT_PINS};                  // Array of input pin numbers
const uint8_t output_pins[] = {OUTPUT_PINS};                // Array of output pin numbers
const uint8_t relay_amount = sizeof(input_pins);            // Number of relays in use
const uint8_t motor_amount = relay_amount / 2;              // makes code more readable
const long motor_durations[2][motor_amount] = {{MOTOR_UP_DUR}, {MOTOR_DOWN_DUR}};
bool relay_outputs[2][motor_amount];                     // For updateRelays() function // Warum uint8_t??
unsigned long message_age;                                  // For calculating end of relay switch duration after serial command
unsigned int command_duration[motor_amount];                // For calculating relay switch duration according to serial

bool risingEdge[relay_amount];
bool lastSwitchState[relay_amount];
unsigned long switchTimer[8];
byte runningTimers = 0;
int8_t ser_rel_movement[8];
// the buffer for the received chars
// 1 extra char for the terminating character "\0"
char g_buffer[MAX_DATA_LEN + 1];

uint8_t send_buffer[8];

void processSerialData(void);
void processCmdTimer(uint8_t);
void switchPressed(int, int);
void switchReleased(int, int);
bool debouncedRead(uint8_t);
void processData(void);
bool PinStateChanged(uint8_t, bool*, bool*);
void serialCommand_set(void);
int8_t translateValue(char);
void updateRelays(void);
void addData(char);

void setup() {
  #if DEBUG
    Serial.begin(9600);
  #endif

  for (int i = 0; i < relay_amount; i++) {
    pinMode(input_pins[i], INPUT_PULLUP);
    pinMode(output_pins[i], OUTPUT);
    lastSwitchState[i] = debouncedRead(input_pins[i]);
    risingEdge[i] = lastSwitchState[i];
  }
  for (int i = 0; i < motor_amount; i++) {
    relay_outputs[0][i] = MOTOR_OFF;
    relay_outputs[1][i] = MOTOR_DOWN;
  }
  updateRelays();
  Serial1.begin(9600);
}

void loop() {
  processSerialData();

  for (int i = 0; i < relay_amount; i++) {
    if (bitRead(runningTimers, i)) {
      processCmdTimer(i);
    }
    else {
      if (PinStateChanged(input_pins[i], &lastSwitchState[i], &risingEdge[i])) {
        if (!risingEdge[i]) {
          switchPressed(i/2, i%2);
        }
        else if (switchTimer[i/2] > 0) {
          switchReleased(i/2, i%2);
        }
      }
      else if (switchTimer[i/2] > 0 && millis() - switchTimer[i/2] - MOTOR_DELAY > (unsigned)abs(motor_durations[i%2][i/2])) { // if max time exceeded
        switchReleased(i/2, i%2); //switch isn't released, but timer finished so do the same
      }
    }
  }

  updateRelays(); 
}

// Function to set motor relays according to the relay_outputs array
void updateRelays(void) {
  #if DEBUG
    //Serial.print("Relay amount: ");
    //Serial.println(relay_amount);
  #endif
  for (int i = 0; i < motor_amount; i++) {
    #if DEBUG
      // Serial.print("writing ");
      // Serial.print(relay_outputs[1][i]);
      // Serial.print(" to ");
      // Serial.println(output_pins[i + 1]);
    #endif
    digitalWrite(output_pins[i * 2 + 1], relay_outputs[1][i]); // set direction relay state
    #if DEBUG
      // Serial.print("writing ");
      // Serial.print(relay_outputs[0][i]);
      // Serial.print(" to ");
      // Serial.println(output_pins[i]);
    #endif
    digitalWrite(output_pins[i * 2], relay_outputs[0][i]);     // set power relay state
  }
}

// Function to process incoming Serial data
void processSerialData(void) {
  int data;
  while ( Serial1.available() > 0 ) {
    data = Serial1.read();addData((char)data);  
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
// strcmp compares two strings and returns 0 if they are the same.
void processData(void) {
  // If buffer begins with "set" and is of correct length
  if (g_buffer[0] == 's' && g_buffer[1] == 'e' && g_buffer[2] == 't') {
    //if (strlen(g_buffer) == 3 + motor_amount) {
    serialCommand_set();
    send_buffer[0] = 127;
    for (int i = 1; i < 8; i++) {
      send_buffer[i] = 0;
    }
    //}
    // else {
    //   send_buffer[0] = 128;
    //   for (int i = 1; i < 8; i++) {
    //     send_buffer[i] = 0;
    //   }
    // }
    Serial1.write(send_buffer, sizeof(send_buffer));
    // Send current motor positions via Serial
  }
  else if (strcmp(g_buffer, "get") == 0 ) {                               // If buffer equals "get"
    for (int i = 0; i < motor_amount; i++) {
      send_buffer[i] = EEPROM.read(i);
    }
    for (int i = motor_amount; i < 8; i++) {
      send_buffer[i] = 255;
    }
    Serial1.write(send_buffer, sizeof(send_buffer));
    
  }
  else if (strcmp(g_buffer, "reset") == 0 ) {                              // If buffer equals "reset"
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

bool debouncedRead(uint8_t pin) {
  uint8_t counter = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(pin) == LOW) {counter++;}
    delayMicroseconds(200);
  }
  if (counter > 5) {return LOW;}
  return HIGH;
}

bool PinStateChanged(uint8_t pin, bool *lastSwitchState, bool *risingEdge) {
 //Get pin state
 bool buttonState = debouncedRead(pin);
 
 //Here starts the code for detecting an edge
 if (buttonState != *lastSwitchState) {
    if (buttonState == LOW) {
        *risingEdge = 0;
    }
    else {
      *risingEdge = 1;
    }
    *lastSwitchState = buttonState;
    return true;
 }
 
 return false;
}

void switchPressed(int motorID, int direction) {
  relay_outputs[0][motorID] = MOTOR_ON;
  relay_outputs[1][motorID] = direction; //index % 2 returns 0 or 1, depending on switch direction
  switchTimer[motorID] = millis();
}

void switchReleased(int motorID, int direction) {
  relay_outputs[0][motorID] = MOTOR_OFF;
  relay_outputs[1][motorID] = MOTOR_DOWN;

  if (millis() - switchTimer[motorID] > MOTOR_DELAY) {
    int rel_movement = (float)(millis() - switchTimer[motorID] - MOTOR_DELAY) / motor_durations[direction][motorID] * 100;
    // Make sure rel_movement is has legal value
    if (rel_movement > 100) {rel_movement = 100;}
    else if (rel_movement < -100) {rel_movement = -100;}
    #if DEBUG
      Serial.print("relative_movement: ");
      Serial.println(rel_movement);
    #endif
    int newPos = (signed)EEPROM.read(motorID) + rel_movement;
    if (newPos > 100) {newPos = 100;}
    else if (newPos < 0) {newPos = 0;}
    EEPROM.write(motorID, newPos);
  }
  switchTimer[motorID] = 0; //reset switch timer once it's no longer in use
}

void serialCommand_set(void) {
  message_age = millis();
  for (uint8_t i=0; i < (strlen(g_buffer)-3) / 2; i +=2) {
    uint8_t motorID = CHAR2INT(g_buffer[i+3]);
    if (0 <= motorID && motorID < motor_amount) { // g_buffer[i+3]=Motor-Index
      bitSet(runningTimers, motorID);
      ser_rel_movement[motorID] = (EEPROM.read(i/2) - translateValue(g_buffer[i+4])); // g_buffer[i+4]=soll-Position // int8_t geht auch // !!Vorzeichen invertiert!!
      if (ser_rel_movement > 0) { // Don't do anythin if motor is already on position
        relay_outputs[0][motorID] = MOTOR_ON;
        relay_outputs[1][motorID] = DIR2BOOL(-SIGN(ser_rel_movement[motorID]));
        command_duration[motorID] = (abs(ser_rel_movement[motorID]) / 100.0 * motor_durations[1][motorID]) + MOTOR_DELAY;
      }
    }
  }
}

void processCmdTimer(uint8_t motorID) {
  if (millis() - message_age >= command_duration[motorID]) {
    relay_outputs[0][motorID] = MOTOR_OFF;
    relay_outputs[1][motorID] = MOTOR_DOWN;
    EEPROM.write(motorID, EEPROM.read(motorID) - ser_rel_movement[motorID]);
    bitClear(runningTimers, motorID);
  }
}

int8_t translateValue(char c) {
  if (c == 'c') {return 100;}
  return CHAR2INT(c) * 10;
}