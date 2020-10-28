// Smart Control v0.3
// WARNING: SWICTHES AND SERIAL INTEROPERABILITY IS WIP
 
#define DEBUG false

#include <Arduino.h>
#include <string.h>     // For strcmp()
#include <EEPROM.h>     // For saving motor positions
#include "config.h"     // config file for pins, motor durations..

// Maybe required if position calculation is too imprecise
//#define   MOTOR_DELAY        50    // delay until motor reacts to input
#define   MOTOR_OFF         HIGH
#define   MOTOR_ON          LOW
#define   MOTOR_UP          LOW
#define   MOTOR_DOWN        HIGH
#define   MAX_DATA_LEN      11     // Max length of Serial transmission data (3 + motor_amount)
#define   TERMINATOR_CHAR   '\r'   // Termination char for Serial message

const uint8_t input_pins[] = {INPUT_PINS};                  // Array of input pin numbers
const uint8_t output_pins[] = {OUTPUT_PINS};                // Array of output pin numbers
const uint8_t relay_amount = sizeof(input_pins);            // Number of relays in use
const uint8_t motor_amount = relay_amount / 2;              // makes code more readable
const int16_t motor_durations[2][motor_amount] = {{MOTOR_UP_DUR}, {MOTOR_DOWN_DUR}};
uint8_t relay_outputs[2][motor_amount];                     // For updateRelays() function
unsigned long EEPROM_timer_start[motor_amount];             // For EEPROM calculation
unsigned long message_age;                                  // For calculating end of relay switch duration after serial command
int position_delta[motor_amount];                           // For saving new motor position in EEPROM after serial command
unsigned int command_duration[motor_amount];                // For calculating relay switch duration according to serial
unsigned int max_command_duration;                          // For resetting executingCommand
bool executingCommand = false;                              // For managing when to accept serial vs switches
uint16_t activeSwitches = 0;                                // For managing when to accept serial vs switches

// the buffer for the received chars
// 1 extra char for the terminating character "\0"
char g_buffer[MAX_DATA_LEN + 1];

// function prototypes
void processSerialData(void);
void processSwitches(void);
void updateRelays(void);
void addData(char);
void processData(void);
void processSerialCommand(void);
void getEEPROM(void);
void processCommandTimers(void);

void setup() {
  #if DEBUG
    Serial.begin(9600);
  #endif
  Serial1.begin(57600);

  // Turn all relays off on program start
  for (int i = 0; i < relay_amount; i++) {
    pinMode(input_pins[i], INPUT_PULLUP);
    pinMode(output_pins[i], OUTPUT);
  }
  for (int i = 0; i < motor_amount; i++) {
    relay_outputs[0][i] = MOTOR_OFF;
    relay_outputs[1][i] = MOTOR_DOWN;
    updateRelays();
  }
}

void loop() {
  // Make sure that no command is being executed
  // If a switch is pressed run regardless
  // if (!executingCommand || activeSwitches > 0) {
  //   // Here we write all switch states into the bool array relay_outputs
  //   processSwitches();
  // }

  if (executingCommand) { // Check whether a command is being processed
    // Checks whether command timers ran out and turns off motors via relay_outputs array
    processCommandTimers();
  } else {
    processSwitches();
    if (activeSwitches == 0) { // Make sure no switches are pressed
      // Here we process data from serial line
      processSerialData();
    }
  }
}
// Function to process physical switches and set relay_outputs array accordingly
void processSwitches(void) {
  for (int i = 0; i < motor_amount; i++) {
    if (digitalRead(input_pins[i * 2]) == LOW) { // up-switch pressed
      bitSet(activeSwitches, i);
      relay_outputs[0][i] = MOTOR_ON;
      relay_outputs[1][i] = MOTOR_UP;
      if (EEPROM_timer_start[i] == 0) {
        EEPROM_timer_start[i] = millis();
      } else if (millis() - EEPROM_timer_start[i] > 1.0 * abs(motor_durations[0][i])) {
        relay_outputs[0][i] = MOTOR_OFF;
        relay_outputs[1][i] = MOTOR_DOWN;
      }
      updateRelays();
    } else if (digitalRead(input_pins[i * 2 + 1]) == LOW) { // down-switch pressed
      bitSet(activeSwitches, i + 8);
      relay_outputs[0][i] = MOTOR_ON;
      relay_outputs[1][i] = MOTOR_DOWN;
      if (EEPROM_timer_start[i] == 0) {
        EEPROM_timer_start[i] = millis();
      } else if (millis() - EEPROM_timer_start[i] > 1.0 * motor_durations[1][i]) {
        relay_outputs[0][i] = MOTOR_OFF;
        relay_outputs[1][i] = MOTOR_DOWN; // turn all relays off
      }
      updateRelays();
    } else { // no switch pressed
      bitClear(activeSwitches, i);
      bitClear(activeSwitches, i + 8);
      if (EEPROM_timer_start[i] > 0) { // If switch has been pressed and released
        // Get previous motor position from EEPROM
        uint8_t tmp_EEPROM = EEPROM.read(i);

        // ugly way of making sure tmp_relative_movement doesn't overflow
        int8_t tmp_relative_movement;
        if (millis() - EEPROM_timer_start[i] <= 1.0 * abs(motor_durations[relay_outputs[1][i]][i])) {
          // Calculate the relative movement of the motor in percentage points
          tmp_relative_movement = ((millis() - EEPROM_timer_start[i]) / (float)motor_durations[relay_outputs[1][i]][i]) * 100;
        } else {
          tmp_relative_movement = (abs(motor_durations[relay_outputs[1][i]][i]) / motor_durations[relay_outputs[1][i]][i]) * 100;
        }
        #if DEBUG
          Serial.print(F("Index: "));
          Serial.print(i);
          Serial.print(F(" |  Delta: "));
          Serial.println(tmp_relative_movement);
        #endif
        if (0 <= (tmp_EEPROM + tmp_relative_movement) && (tmp_EEPROM + tmp_relative_movement) <= 100) { // if new percentage value is between 0 and 100
          EEPROM.write(i, tmp_EEPROM + tmp_relative_movement); // save new value too EEPROM
        } else if ((tmp_EEPROM + tmp_relative_movement) < 0) {
          EEPROM.write(i, 0);
        } else if (100 < (tmp_EEPROM + tmp_relative_movement)) {
          EEPROM.write(i, 100);
        }
        EEPROM_timer_start[i] = 0;
      }
      relay_outputs[0][i] = MOTOR_OFF;
      relay_outputs[1][i] = MOTOR_DOWN;
      updateRelays();
    }
  }
}

// Function to set motor relays according to the relay_outputs array
void updateRelays(void) {
  #if DEBUG
    Serial.print("Relay amount: ");
    Serial.println(relay_amount);
  #endif
  for (int i = 0; i < motor_amount; i++) {
    #if DEBUG
      Serial.print("writing ");
      Serial.print(relay_outputs[1][i]);
      Serial.print(" to ");
      Serial.println(output_pins[i + 1]);
    #endif
    digitalWrite(output_pins[i * 2 + 1], relay_outputs[1][i]); // set direction relay state
    #if DEBUG
      Serial.print("writing ");
      Serial.print(relay_outputs[0][i]);
      Serial.print(" to ");
      Serial.println(output_pins[i]);
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
  if ((nextChar == '\n') || (nextChar == ' ') || (nextChar == '\t'))
      return;

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
    if (g_buffer[0] == 's' && g_buffer[1] == 'e' && g_buffer[2] == 't' && g_buffer[motor_amount + 2] != '\0' && g_buffer[motor_amount + 3] == '\0') {
      processSerialCommand();
      Serial1.write(1);
      for (int i = 0; i < 7; i++) {
        Serial1.write(0);
      }
    }
    // Send current motor positions via Serial
    // ACHTUNG: SCHALTER DARF WAHRSCHEINLICH NICHT GEDRÜCKT SEIN
    // (vielleicht gehts jetzt doch?)
    else if (strcmp(g_buffer, "get") == 0 ) {                               // If buffer equals "get"
      for (int i = 0; i < motor_amount; i++) {
        Serial1.write(EEPROM.read(i));
      }
      for (int i = 0; i < 8 - motor_amount; i++) {
        Serial1.write(0);
      }
    }
    else if (strcmp(g_buffer, "reset") == 0 ) {                              // If buffer equals "reset"
      for (int i=0; i < motor_amount; i++) {
        EEPROM.write(i, 0);
      }
      Serial1.write(2);
      for (int i = 0; i < 7; i++) {
        Serial1.write(0);
      }
    }
    else {
      Serial1.write(3);
      for (int i = 0; i < 7; i++) {
        Serial1.write(0);
      }
    }
}

// Function to move motors according to the received Serial data
void processSerialCommand() {
  max_command_duration = 0;
  message_age = millis();
  executingCommand = true;
  #if DEBUG
    Serial.println(g_buffer);
  #endif
  for (int i = 0; i < motor_amount; i++) {
    // Even though that should never happen, make sure motor is not currently running!
    if (relay_outputs[0][i] == MOTOR_OFF) {
      int tmp_EEPROM = EEPROM.read(i);
      #if DEBUG
        Serial.print(F("Index: "));
        Serial.println(i);
        Serial.print(F("EEPROM: "));
        Serial.println(tmp_EEPROM
        Serial.print(F("Buffer val: "));
        Serial.println(((int)g_buffer[3 + i] - 48) * 10);
      #endif
      if (g_buffer[3 + i] == 'c' && tmp_EEPROM != 100) {
        relay_outputs[0][i] = MOTOR_ON;
        relay_outputs[1][i] = MOTOR_DOWN;
        updateRelays();
        position_delta[i] = 100 - tmp_EEPROM;
        #if DEBUG
          Serial.print(F("Delta: "));
          Serial.println(position_delta[i]);
        #endif
        command_duration[i] = position_delta[i] / 100.0 * motor_durations[1][i];
        #if DEBUG
          Serial.println(command_duration[i]);
        #endif
      } else if (tmp_EEPROM < ((int)g_buffer[3 + i] - 48) * 10) {
        #if DEBUG
          Serial.println("runterfahren");
        #endif
        relay_outputs[0][i] = MOTOR_ON;
        relay_outputs[1][i] = MOTOR_DOWN;
        updateRelays();
        position_delta[i] = (((int)g_buffer[3 + i] - 48) * 10) - tmp_EEPROM;
        #if DEBUG
          Serial.print(F("Delta: "));
          Serial.println(position_delta[i]);
        #endif
        command_duration[i] = position_delta[i] / 100.0 * motor_durations[1][i];
        #if DEBUG
          Serial.print(F("Delta time: "));
          Serial.println(command_duration[i]);
        #endif
      } else if (tmp_EEPROM > ((int)g_buffer[3 + i] - 48) * 10) {
        #if DEBUG
          Serial.println("hochfahren");
        #endif
        relay_outputs[0][i] = MOTOR_ON;
        relay_outputs[1][i] = MOTOR_UP;
        updateRelays();
        position_delta[i] = (tmp_EEPROM - (((int)g_buffer[3 + i] - 48) * 10)) * -1;
        #if DEBUG
          Serial.print(F("Delta: "));
          Serial.println(position_delta[i]);
        #endif
        command_duration[i] = position_delta[i] / 100.0 * motor_durations[0][i];
        #if DEBUG
          Serial.print(F("Delta time: "));
          Serial.println(command_duration[i]);
        #endif
      } else {
        relay_outputs[0][i] = MOTOR_OFF;
        relay_outputs[1][i] = MOTOR_DOWN;
        updateRelays();
      }
      if (max_command_duration < command_duration[i]) {
        max_command_duration = command_duration[i];
      }
    }
  }
}

// Function to process the command timers
void processCommandTimers() {
  // Reset executingCommand if all timers ran out
  if (millis() - message_age >= max_command_duration) {
    executingCommand = false;
    // message_age = 0;
    // max_command_duration = 0;
  }
  // turn off motor if its respective timer ran out
  for (int i = 0; i < motor_amount; i++) {
    //stupid fix
    if (command_duration[i] > 0) {
      if (millis() - message_age >= command_duration[i]) {
        #if DEBUG
          Serial.println(F("Timer finished."));
        #endif
        relay_outputs[0][i] = MOTOR_OFF;
        relay_outputs[1][i] = MOTOR_DOWN;
        updateRelays();
        EEPROM.write(i, EEPROM.read(i) + position_delta[i]);
        command_duration[i] = 0;
      }
    }
  }
}