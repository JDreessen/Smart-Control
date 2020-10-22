// Smart Control v0.2
// WARNING: SWICTHES AND SERIAL INTEROPERABILITY IS WIP
 
#include <Arduino.h>
#include <string.h>     // For strcmp()
#include <EEPROM.h>     // For saving motor positions

// ----<CUSTOMIZABLE>----
// These have to have the same length
#define   INPUT_PINS        22, 23, 24, 25
#define   OUTPUT_PINS       26, 27, 28, 29
#define   MOTOR_UP_DUR    -10000, -10000
#define   MOTOR_DOWN_DUR  10000, 10000
// --<END CUSTOMIZABLE>--

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
const uint8_t motor_amount = relay_amount / 2;              // maes code more readable
const long motor_durations[2][relay_amount] = {{MOTOR_UP_DUR}, {MOTOR_DOWN_DUR}};

uint8_t relay_outputs[2][motor_amount];                     // For setRelays() function
unsigned long EEPROM_timer_start[motor_amount];             // For EEPROM calculation
unsigned long message_age;                                  // For calculating end of relay switch duration after serial command
int position_delta[motor_amount];                           // For saving new motor position in EEPROM after serial command
unsigned int command_duration[motor_amount];                // For calculating relay switch duration according to serial
unsigned int max_command_duration;                          // For resetting executingCommand
bool executingCommand = false;                              // For managing when to accept serial vs switches
uint8_t activeSwitches = 0;                                 // For managing when to accept serial vs switches

// the buffer for the received chars
// 1 extra char for the terminating character "\0"
char g_buffer[MAX_DATA_LEN + 1];

// function prototypes
void processSerialData(void);
void processSwitches(void);
void setRelays(void);
void addData(char);
void processData(void);
void processSerialCommand(void);
void getEEPROM(void);
void processCommandTimers(void);

void setup() {
  Serial.begin(9600);

  // Turn all relays off on program start
  for (int i = 0; i < relay_amount; i++) {
    pinMode(input_pins[i], INPUT_PULLUP);
    pinMode(output_pins[i], OUTPUT);
  }
  for (int i = 0; i < motor_amount; i++) {
    relay_outputs[0][i] = MOTOR_OFF;
    relay_outputs[1][i] = MOTOR_UP;
  }
}

void loop() {
 
  // Here we read the sensors, calculate the output etc.
  delay(100);

  // Make sure that switches are not currently faked via serial
  // Still run though if a switch is currently held
  if (!executingCommand || activeSwitches > 0) {
    // Here we write all switch states into the bool array relay_outputs
    processSwitches();
  }

  ///* debug comment
  // Make sure that no command is running and..
  // ..that no switch is currently held
  if (!executingCommand && activeSwitches == 0) {
    // Here we process data from serial line
    processSerialData();
  }

  // Checks whether fakeSwitch timers ran out and turns off motors via relay_outputs array
  if (executingCommand) {processCommandTimers();}
  // debug*/

  // Here we set all relays to the values in relay_outputs
  setRelays();

}
// Function to process physical switches and set relay_outputs array accordingly
void processSwitches(void) {
  for (int i = 0; i < motor_amount; i++) {
    if (digitalRead(input_pins[i * 2]) == LOW) { // up-switch pressed
      activeSwitches++;
      if (EEPROM_timer_start[i] == 0) {EEPROM_timer_start[i] = millis();}

      relay_outputs[0][i] = MOTOR_ON;
      relay_outputs[1][i] = MOTOR_UP;
    } else if (digitalRead(input_pins[i * 2 + 1]) == LOW) { // down-switch pressed
        activeSwitches++;
        if (EEPROM_timer_start[i] == 0) {EEPROM_timer_start[i] = millis();}

        relay_outputs[0][i] = MOTOR_ON;
        relay_outputs[1][i] = MOTOR_DOWN;
    } else { // no switch pressed
        if (activeSwitches > 0) {activeSwitches--;}
        relay_outputs[0][i] = MOTOR_OFF;

        if (EEPROM_timer_start[i] > 0) { // If switch has been pressed and released
          
          // Get previous motor position from EEPROM
          uint8_t tmp_EEPROM = EEPROM.read(i);

          // Calculate the relative movement of the motor in percentage points
          int tmp_relative_movement = ((millis() - EEPROM_timer_start[i]) / (float)motor_durations[relay_outputs[1][i]][i]) * 100;
          Serial.print("Index: ");
          Serial.print(i);
          Serial.print(" |  Delta: ");
          Serial.println(tmp_relative_movement);
          if (0 <= (tmp_EEPROM + tmp_relative_movement) && (tmp_EEPROM + tmp_relative_movement) <= 100) { // if new percentage value is between 0 and 100
            EEPROM.write(i, tmp_EEPROM + tmp_relative_movement); // save new value too EEPROM
          } else if ((tmp_EEPROM + tmp_relative_movement) < 0) {
            EEPROM.write(i, 0);
          } else if (100 < (tmp_EEPROM + tmp_relative_movement)) {
            EEPROM.write(i, 100);
          }
          EEPROM_timer_start[i] = 0;
        }
    }
  }
}

// Function to set motor relays according to the relay_outputs array
void setRelays(void) {
  // Serial.print("Relay amount: ");
  // Serial.println(relay_amount);
  for (int i = 0; i < motor_amount; i++) {
    /* debug
    Serial.print("writing ");
    Serial.print(relay_outputs[1][i]);
    Serial.print(" to ");
    Serial.println(output_pins[i + 1]);
    debug */
    digitalWrite(output_pins[i * 2 + 1], relay_outputs[1][i]); // set direction relay state
    /*
    Serial.print("writing ");
    Serial.print(relay_outputs[0][i]);
    Serial.print(" to ");
    Serial.println(output_pins[i]);
    */
    digitalWrite(output_pins[i * 2], relay_outputs[0][i]);     // set power relay state
  }
}

// Function to process incoming Serial data
void processSerialData(void) {
  int data;
  while ( Serial.available() > 0 ) {
      data = Serial.read();addData((char)data);  
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

    return;
}

// process the data - command
// strcmp compares two strings and returns 0 if they are the same.
void processData(void) {
    if (g_buffer[0] == 's' && g_buffer[1] == 'e' && g_buffer[2] == 't') { // If buffer begins with "set"
       processSerialCommand();
    }
    // Send current motor positions via Serial
    // ACHTUNG: SCHALTER DARF WAHRSCHEINLICH NICHT GEDRÜCKT SEIN
    // (vielleicht gehts jetzt doch?)
    else if (strcmp(g_buffer, "get") == 0 ) {                               // If buffer equals "get"
       for (int m = 0; m < motor_amount - 1; m++) {
        Serial.print(EEPROM.read(m));
        Serial.print(",");
       }
       Serial.println(EEPROM.read(motor_amount - 1));
    }
    else if (strcmp(g_buffer, "test") == 0 ) {                              // If buffer equals "test" (unused)
      Serial.println("OK");
    }
    else if (strcmp(g_buffer, "reset") == 0 ) {                              // If buffer equals "reset"
      for (int i=0; i < motor_amount; i++) {
        EEPROM.write(i, 0);
        Serial.println("DONE");
      }
    }
    else {
      //DEBUG
      //Serial.print("Unknown command ");
      //Serial.println(g_buffer);
    }
}

// Function to move motors according to the received Serial data
void processSerialCommand() {
  max_command_duration = 0;
  message_age = millis();
  executingCommand = true;
  Serial.println(g_buffer); //debug
  for (int i = 0; i < motor_amount; i++) {
    // Even though that should never happen, make sure motor is not currently running!
    if (relay_outputs[0][i] == MOTOR_OFF) {
      int tmp_EEPROM = EEPROM.read(i);
      Serial.print("Index: "); //debug
      Serial.println(i); //debug
      Serial.print("EEPROM: "); //debug
      Serial.println(tmp_EEPROM); //debug
      Serial.print("Buffer val: "); //debug
      Serial.println(((int)g_buffer[3 + i] - 48) * 10);
      if (g_buffer[3 + i] == 'c') {
        if (tmp_EEPROM != 100) {
          relay_outputs[0][i] = MOTOR_ON;
          relay_outputs[1][i] = MOTOR_DOWN;
          position_delta[i] = 100 - tmp_EEPROM;
          Serial.print("Delta: "); //debug
          Serial.println(position_delta[i]);
          command_duration[i] = position_delta[i] / 100.0 * motor_durations[1][i];
          Serial.println(command_duration[i]);
        }
      } else if (tmp_EEPROM < ((int)g_buffer[3 + i] - 48) * 10) {
          Serial.println("runterfahren"); //debug
          relay_outputs[0][i] = MOTOR_ON;
          relay_outputs[1][i] = MOTOR_DOWN;
          position_delta[i] = (((int)g_buffer[3 + i] - 48) * 10) - tmp_EEPROM;
          Serial.print("Delta: "); //debug
          Serial.println(position_delta[i]);
          command_duration[i] = position_delta[i] / 100.0 * motor_durations[1][i];
          Serial.println(command_duration[i]);
      } else if (tmp_EEPROM > ((int)g_buffer[3 + i] - 48) * 10) {
          Serial.println("hochfahren"); //debug
          relay_outputs[0][i] = MOTOR_ON;
          relay_outputs[1][i] = MOTOR_UP;
          position_delta[i] = (tmp_EEPROM - (((int)g_buffer[3 + i] - 48) * 10)) * -1;
          Serial.print("Delta: "); //debug
          Serial.println(position_delta[i]);
          command_duration[i] = position_delta[i] / 100.0 * motor_durations[0][i];
          Serial.print("Delta time: "); //debug
          Serial.println(command_duration[i]);
      } else {
          relay_outputs[0][i] = MOTOR_OFF;
      }
      if (max_command_duration < command_duration[i]) {max_command_duration = command_duration[i];}
    }
  }
}

// Function to process the command timers
void processCommandTimers() {
  //Serial.println("Executing command.."); //debug
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
        Serial.println("Timer finished."); //debug
        relay_outputs[0][i] = MOTOR_OFF;
        EEPROM.write(i, EEPROM.read(i) + position_delta[i]);
        command_duration[i] = 0;
      }
    }
  }
}