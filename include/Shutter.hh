#ifndef SHUTTER_HH
#define SUHTTER_HH

#include <Arduino.h>
#include "ShutterSwitch.hh"
#include <Preferences.h>
#include <PubSubClient.h>
#include "tools.h"

#define   MOTOR_DELAY       1500   // delay until motor reacts to input
#define   MOTOR_OFF         HIGH
#define   MOTOR_ON          LOW
#define   MOTOR_UP          LOW
#define   MOTOR_DOWN        HIGH

class Shutter {
  using pin=const uint8_t;
  private:
    //uint8_t _id;
    pin _pins[2];
    int16_t _position;
    static Preferences* _prefs;
    static PubSubClient* _mqtt_client;
    const char* _name;
    const String _pref_timers[2];
  public:
    ShutterSwitch sSwitch;
    unsigned long move_duration; // For movement by command
    unsigned long move_timestamp;
    long max_durations[2];
    uint8_t running;
    bool state[2];

    Shutter(pin (&switchPinsIN)[2], pin (&pinsIN)[2], const char* NVSkeyIN) : 
      _pins{pinsIN[0], pinsIN[1]},
      _position(0),
      state{MOTOR_OFF, MOTOR_DOWN},
      move_timestamp(0),
      _name(NVSkeyIN),
      sSwitch(switchPinsIN),
      move_duration(0),
      _pref_timers{String(_name) + "_up", String(_name) + "_down"},
      running(0)
      {}
    ~Shutter() {}
    // set GPIO mode and initial state
    void setup() {
      //sSwitch.setup();
      pinMode(_pins[0], OUTPUT);
      pinMode(_pins[1], OUTPUT);
      overwritePos(_prefs->getUChar(_name));
      max_durations[0] = _prefs->getLong(_pref_timers[0].c_str());
      max_durations[1] = _prefs->getLong(_pref_timers[1].c_str());
      write();
    }
    // change power/direction of motor
    void set(bool power, bool direction) {
      state[0] = power;
      state[1] = direction;
    }
    //TODO: begin movement and set timer duration
    void moveTo(int8_t newPos) {
      if (newPos - _position != 0 && 0 <= newPos && newPos <= 100) { // Don't do anything if motor is already on position or if value is invalid
        set(MOTOR_ON, DIR2BOOL(SIGN(newPos - _position)));
        running = 2;
        move_duration = (abs(newPos - _position) / 100.0 * abs(max_durations[state[1]])) + MOTOR_DELAY;
        move_timestamp = millis();
      }
    }
    //TODO: stop movement (before executing new command)
    void stopMovement() {
      if (running > 0) {
        unsigned long dt = millis() - move_timestamp;
        if (dt >= MOTOR_DELAY) {
          _position += round((float)(dt - MOTOR_DELAY) / (max_durations[state[1]]) * 100);
          if (_position > 100) _position = 100;
          else if (_position < 0) _position = 0;
          _prefs->putUChar(_name, (uint8_t)_position);
        }
        set(MOTOR_OFF, MOTOR_DOWN);
        running = 0;
      }
      #if DEBUG
        Serial.print(_name);
        Serial.print(" Position:");
        Serial.println(_position);
      #endif
    }
    // start moving in direction, save current time and set running=1
    void move(bool direction) {
      set(MOTOR_ON, direction);
      move_timestamp = millis();
      running = 1;
    }
    //check if max move duration is exceeded (with margin for self-callibration) and stop movement
    void checkExceedingMaxDuration() {
      if (running == 1 && millis() - move_timestamp >= abs(max_durations[state[1]]) + MOTOR_DELAY + 1000) {
        stopMovement();
        publishPos();
      }
    }
    void checkCommandTimerCompletion() {
      if (running == 2 && millis() - move_timestamp >= move_duration) {
        stopMovement();
        publishPos();  // Also publish pos when command is done?
      }
    }
    //NOTE: either set retained=false or always update status, otherwise status desyncs when handling command
    void publishPos() {
        memcpy(mqtt_statusTopicBuf+mqtt_prefixLen, getName(), 2);
        memcpy(mqtt_statusTopicBuf+mqtt_prefixLen+2, "/status", 8); // could be strcpy
        uint8_t payload[4]; itoa(getPos(), (char*)payload, 10);
        _mqtt_client->publish(mqtt_statusTopicBuf, payload, strlen((const char*)payload), false);
    }
    // stop moving, calc and write new position to EEPROM and set running=0
    void handleSwitch() {
      stopMovement();
      switch(sSwitch.getState()) {
          case 0:
            publishPos();
            break;
          case 1:
            move(MOTOR_UP);
            break;
          case -1:
            move(MOTOR_DOWN);
            break;
        }
    }
    //WIP
    void updateTimerUp(byte* msg, uint len) {
      max_durations[0] = 0;
      for (int i = 0; i < len; ++i)
        max_durations[0] = 10*max_durations[0] + CHAR2INT(msg[i]);
      max_durations[0] = -max_durations[0];
      _prefs->putLong(_pref_timers[0].c_str(), max_durations[0]);
    }
    void updateTimerDown(byte* msg, uint len) {
      max_durations[1] = 0;
      for (int i = 0; i < len; ++i)
        max_durations[1] = 10*max_durations[1] + CHAR2INT(msg[i]);
      _prefs->putLong(_pref_timers[1].c_str(), max_durations[1]);
    }
    void handleCommand(byte* msg, uint len) {
      stopMovement();
      int tmp_newPos = 0;
      for (int i = 0; i < len; ++i)
        tmp_newPos = 10*tmp_newPos + CHAR2INT(msg[i]);
      moveTo(tmp_newPos);
    }
    // write changes to GPIO
    void write() {
        digitalWrite(_pins[0], state[0]);
        digitalWrite(_pins[1], state[1]);
    }
    void overwritePos(const uint8_t pos) {
      this->_position = pos;
    }
    const int getPos() const {return _position;}
    const char* getName() const {return this->_name;}

    void loop() {
      if (sSwitch.hasChanged())
        handleSwitch();
      
      checkExceedingMaxDuration();
    
      checkCommandTimerCompletion();
    }
};

#endif