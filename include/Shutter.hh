#ifndef SHUTTER_HH
#define SUHTTER_HH

#include <Arduino.h>
#include "ShutterSwitch.hh"
#include "tools.h"

#define   MOTOR_DELAY       1500   // delay until motor reacts to input
#define   MOTOR_OFF         HIGH
#define   MOTOR_ON          LOW
#define   MOTOR_UP          LOW
#define   MOTOR_DOWN        HIGH

using pin=const uint8_t;

class Shutter {
  private:
    pin _pins[2];
    uint8_t _position;
    bool _state[2];
    unsigned long _move_timestamp;
    const uint8_t _EEPROMpos;
  public:
    ShutterSwitch sSwitch;
    unsigned long move_duration;
    const long max_durations[2];
    bool running;

    Shutter(pin (&switchPinsIN)[2], pin (&pinsIN)[2], const long (&motor_durationsIN)[2], const uint8_t &EEPROMposIN) : 
      _pins{pinsIN[0], pinsIN[1]},
      _position(0),
      _state{MOTOR_OFF, MOTOR_DOWN},
      _move_timestamp(0),
      _EEPROMpos(EEPROMposIN),
      sSwitch(switchPinsIN),
      move_duration(0),
      max_durations{motor_durationsIN[0], motor_durationsIN[1]},
      running(true)
      {}
    // set GIOP mode and initial state
    void setup() {
      for (pin p : _pins) {
        pinMode(p, OUTPUT);
        write();
      }
      _position = EEPROM.read(_EEPROMpos);
    }
    // change power/direction of motor
    void set(bool power, bool direction) {
      _state[0] = power;
      _state[1] = direction;
    }
    //TODO: begin movement and set timer duration
    void move(int8_t newPos) {
      if (newPos - _position != 0) { // Don't do anything if motor is already on position
        running = true;
        set(MOTOR_ON, DIR2BOOL(SIGN(newPos - _position)));
        move_duration = (abs(newPos - _position) / 100.0 * abs(max_durations[_state[1]])) + MOTOR_DELAY;
        _move_timestamp = millis();
      }
    }
    //TODO: stop movement (before executing new command)
    void stop() {
      set(MOTOR_OFF, MOTOR_DOWN);
      //TODO: add unfinished movement delta to _position
      _position += (millis() - _move_timestamp) / (max_durations[_state[1]]); //TODO: Maybe check if value is even legal
      EEPROM.write(_EEPROMpos, _position);
      running = false;
    }
    //TODO: check for timer completion and stop movement
    void update() {
      if (millis() - _move_timestamp >= move_duration) { //MOTOR_DELAY is accounted for
        stop();
      }
    }
    // write changes to GPIO
    void write() {
        digitalWrite(_pins[0], _state[0]);
        digitalWrite(_pins[1], _state[1]);
    }
};

#endif