// Smart Control v0.1
// ACHTUNG: SCHALTER UND SERIAL GLEICHZEITIG GIBT CHAOS (WIP)
 
#include <Arduino.h>
#include <string.h>     // For strcmp()
#include <EEPROM.h>     // For saving motor positions

#define   MAX_DATA_LEN    11     // Max length of Serial transmission data (3 + USED_SWITCHES / 2)
#define   TERMINATOR_CHAR '\r'   // Termination char for Serial message
#define   FIRST_PIN       2      // The first GPIO pin you want to use
#define   MAX_SWITCHES    16     // Max amount of switches, do not change
#define   USED_SWITCHES   2      // Number of switches currently connected
#define   MAX_MOTOR_TIME  10000  // Time it takes the motor to completetely open/close in ms

bool relay_outputs[USED_SWITCHES];                // For setRelays() function
double EEPROM_timer_start[USED_SWITCHES / 2];     // For EEPROM calculation
double message_age;                               // For calculating end of fakeSwitch duration
int fakeSwitch_new_position[USED_SWITCHES / 2];   // For saving fakeSwitch position in EEPROM
double fakeSwitch_duration[USED_SWITCHES / 2];    // For faking switch press duration
double max_fakeSwitch_duration;                   // For resetting fakingSwitches
bool fakingSwitches = false;                      // For managing when to accept serial vs switches
bool processingSwitches = false;                  // For managing when to accept serial vs switches

// the buffer for the received chars
// 1 extra char for the terminating character "\0"
char g_buffer[MAX_DATA_LEN + 1];

// function prototypes
void processSerialData(void);
void processSwitches(void);
void setRelays(void);
bool addData(char);
void processData(void);
void initFakeSwitches(void);
void getEEPROM(void);
void processFakeSwitches(void);

void setup() {
  //  Serial.begin(9600);

  // Turn all relays off on program start
  for (int i = 0; i < USED_SWITCHES; i++) {
    pinMode(i + FIRST_PIN, INPUT_PULLUP);
    pinMode(i + FIRST_PIN + MAX_SWITCHES, OUTPUT);
    relay_outputs[i] = true;
  }
}

void loop() {
 
  // Here we read the sensors, calculate the output etc.
  delay(100);

  // Make sure that switches are not currently faked via serial
  // Still run though if a switch is currently held
  if (!fakingSwitches or processingSwitches) {
    // Here we write all switch states into the bool array relay_outputs
    processSwitches();
  }

  // Make sure that switches are not already being faked and..
  // ..that no switch is currently held
  // if (!fakingSwitches and !processingSwitches) {
  //   // Here we process data from serial line
  //   processSerialData();
  // }

  // Checks whether fakeSwitch timers ran out and turns of motors via relay_outputs array
  //if (fakingSwitches) {processFakeSwitches();}

  // Here we set all relays to the values in relay_outputs
  setRelays();

}
// Function to process physical switches and set relay_outputs array accordingly
void processSwitches(void) {
  for (int j = 0; j < USED_SWITCHES; j += 2) {
    if (digitalRead(j + FIRST_PIN) == LOW) { // Hoch-schalter gedrückt
      processingSwitches = true;
      if (EEPROM_timer_start[j] == 0) {EEPROM_timer_start[j] = millis();}

      relay_outputs[j] = false; // Motor an
      relay_outputs[j + 1] = true; // Richtung hoch
    } else if (digitalRead(j + FIRST_PIN + 1) == LOW) { // Runter-schalter gedrückt
        processingSwitches = true;
        if (EEPROM_timer_start[j + 1] == 0) {EEPROM_timer_start[j + 1] = millis();}

        relay_outputs[j] = false; // Motor an
        relay_outputs[j + 1] = false; // Richtung runter
    } else { // kein Schalter gedrückt
        processingSwitches = false;
        relay_outputs[j] = true; // Motor aus

        if (EEPROM_timer_start[j / 2] > 0) { // If switch has been pressed and released
          // Calculate the relative movement of the motor in percentage points
          int tmp_relative_movement = ((millis() - EEPROM_timer_start[j / 2]) / MAX_MOTOR_TIME) * 100;
          // Get previous motor position from EEPROM
          int tmp_EEPROM = EEPROM.read(j / 2);

          if (relay_outputs[j + 1]) { // Richtung hoch
            if (tmp_EEPROM - tmp_relative_movement >= 0) {
              EEPROM.write(j / 2, tmp_EEPROM - tmp_relative_movement);
            } else {
              EEPROM.write(j / 2, 0);
            }
            
          } else { // Richtung runter
              if (tmp_EEPROM + tmp_relative_movement <= 100) {
                EEPROM.write(j / 2, tmp_EEPROM + tmp_relative_movement);
              } else {
                EEPROM.write(j / 2, 100);
              }
          }
          EEPROM_timer_start[j / 2] = 0;
        }
    }
  }
}

// Function to set motor relays according to the relay_outputs array
void setRelays(void) {
  for (int k = 0; k < USED_SWITCHES; k++) {
    digitalWrite(k + MAX_SWITCHES + FIRST_PIN, relay_outputs[k]);
  }
}

// Function to process incoming Serial data
void processSerialData(void) {
  int data;
  bool dataReady;
  while ( Serial.available() > 0 ) {
      data = Serial.read();
      dataReady = addData((char)data);  
      if ( dataReady )
        delay(50);
        processData();
  }
}

// Put received character into the buffer.
// When a complete command is received return true, otherwise return false.
// The command is terminated by Enter character ("\r")
bool addData(char nextChar) {  
  // This is position in the buffer where we put next char.
  // Static var will remember its value across function calls.
  static uint8_t currentIndex = 0;

    // Ignore some characters - new line, space and tabs
    if ((nextChar == '\n') || (nextChar == ' ') || (nextChar == '\t'))
        return false;

    // If we receive Enter character...
    if (nextChar == TERMINATOR_CHAR) {
        // ...terminate the string by NULL character "\0" and return true
        g_buffer[currentIndex] = '\0';
        currentIndex = 0;
        return true;
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
      return true;
    }

    return false;
}

// process the data - command
// strcmp compares two strings and returns 0 if they are the same.
void processData(void) {
    if (g_buffer[0] == 's' and g_buffer[1] == 'e' and g_buffer[2] == 't') { // If buffer begins with "set"
       initFakeSwitches();
    }
    // Send current motor positions via Serial
    // ACHTUNG: SCHALTER DARF WAHRSCHEINLICH NICHT GEDRÜCKT SEIN
    // (vielleicht gehts jetzt doch?)
    else if (strcmp(g_buffer, "get") == 0 ) {                               // If buffer equals "get"
       for (int m = 0; m < USED_SWITCHES / 2 - 1; m++) {
        Serial.print(EEPROM.read(m));
        Serial.print(",");
       }
       Serial.println(EEPROM.read(USED_SWITCHES / 2 - 1));
    }
    else if (strcmp(g_buffer, "test") == 0 ) {                              // If buffer equals "test" (unused)
       Serial.println("OK");}
    else {
      //DEBUG
      //Serial.print("Unknown command ");
      //Serial.println(g_buffer);
    }
}

// Function to move motors according to the received Serial data
void initFakeSwitches() {
  fakingSwitches = true;
  max_fakeSwitch_duration = 0;
  message_age = millis();
  for (int l = 0; l < USED_SWITCHES / 2; l++) {
    // Even though that should never happen, make sure motor is not currently running!
    if (!relay_outputs[l * 2]) {
      int tmp_EEPROM = EEPROM.read(l);
      if (g_buffer[l] == 'c') {
        if (tmp_EEPROM != 100) {
          relay_outputs[l * 2] = true;
          relay_outputs[l * 2 + 1] = false;
          fakeSwitch_new_position[l] = 100 - tmp_EEPROM;
          fakeSwitch_duration[l] = fakeSwitch_new_position[l] / 100 * MAX_MOTOR_TIME;
          if (fakeSwitch_duration[l] > max_fakeSwitch_duration) {max_fakeSwitch_duration = fakeSwitch_duration[l];}
        }
      } else if (tmp_EEPROM < (int)g_buffer[l] * 10) {
          relay_outputs[l * 2] = true;
          relay_outputs[l * 2 + 1] = false;
          fakeSwitch_new_position[l] = ((int)g_buffer[l] * 10) - tmp_EEPROM;
          fakeSwitch_duration[l] = fakeSwitch_new_position[l] / 100 * MAX_MOTOR_TIME;
          if (fakeSwitch_duration[l] > max_fakeSwitch_duration) {max_fakeSwitch_duration = fakeSwitch_duration[l];}
      } else if (tmp_EEPROM > (int)g_buffer[l] * 10) {
          relay_outputs[l * 2] = true;
          relay_outputs[l * 2 + 1] = true;
          fakeSwitch_new_position[l] = tmp_EEPROM - ((int)g_buffer[l] * 10);
          fakeSwitch_duration[l] = fakeSwitch_new_position[l] / 100 * MAX_MOTOR_TIME;
          if (fakeSwitch_duration[l] > max_fakeSwitch_duration) {max_fakeSwitch_duration = fakeSwitch_duration[l];}
      } else {
          relay_outputs[l * 2] = false;
      }
    }
  }
}

// Function to process the fakeSwitch timers
void processFakeSwitches() {
  // Reset fakingSwitches if all timers ran out
  if (millis() - message_age >= max_fakeSwitch_duration) {
    fakingSwitches = false;
  }
  // turn off motor if its respective timer ran out
  for (int m = 0; m < USED_SWITCHES / 2; m++) {
    if (millis() - message_age >= fakeSwitch_duration[m]) {
      relay_outputs[m * 2] = false;
      EEPROM.write(m, fakeSwitch_new_position[m]);
    }
  }
}