#include <Arduino.h>

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
  public:
  const long max_durations[2];
    bool running;

    Shutter(pin (&pinsIN)[2], const long (&motor_durationsIN)[2]) : 
      _pins{pinsIN[0], pinsIN[1]},
      _position(0),
      _state{MOTOR_OFF, MOTOR_DOWN},
      max_durations{motor_durationsIN[0], motor_durationsIN[1]},
      running(true)
      {}
    // set GIOP mode and initial state
    void setup() {
        for (pin p : _pins) {
            pinMode(p, OUTPUT);
            write();
        }
        
    }
    // change power/direction of motor
    void set(bool power, bool direction) {
        _state[0] = power;
        _state[1] = direction;
    }
    //TODO: begin movement and set timer duration
    void move(uint8_t pos) {

    }
    //TODO: stop movement (before executing new command)
    void stop() {

    }
    //TODO: check for timer completion and stop movement
    void update() {

    }
    // write changes to GPIO
    void write() {
        digitalWrite(_pins[0], _state[0]);
        digitalWrite(_pins[1], _state[1]);
    }
};