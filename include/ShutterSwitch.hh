#ifndef SHUTTERSWITCH_HH
#define SHUTTERSWITCH_HH

#include <Arduino.h>
#include "tools.h"


class ShutterSwitch {
  using pin=const uint8_t;
  
  private:
    pin _pins[2];
    int8_t _state;
    bool _lastSwitchState[2];
    bool _risingEdge[2];

  public:
   unsigned long timer;

   ShutterSwitch(pin (&pinsIN)[2]) :
      _pins{pinsIN[0], pinsIN[1]},
      _state{0},
      timer(0)
      {}
    ~ShutterSwitch() {}
    void setup() {
      pinMode(_pins[0], INPUT_PULLUP);
      pinMode(_pins[1], INPUT_PULLUP);
      _lastSwitchState[0] = debouncedRead(_pins[0]);
      _lastSwitchState[1] = debouncedRead(_pins[1]);
      _risingEdge[0] = _lastSwitchState[0];
      _risingEdge[1] = _lastSwitchState[1];
    }

    int read(uint8_t i_pin) {return digitalRead(this->_pins[i_pin]);}

    int debouncedRead(const uint8_t& pin) const{
      uint8_t counter = 0;
      for (int i = 0; i < 10; i++) {
        if (digitalRead(pin) == LOW) {counter++;}
        delayMicroseconds(200);
      }
      if (counter > 5) {return LOW;}
      return HIGH;
    }

    bool hasChanged() {
      for (uint8_t i=0; i<2; ++i) {
        int buttonState = debouncedRead(_pins[i]);

        //Here starts the code for detecting an edge
        if (buttonState != _lastSwitchState[i]) {
            if (buttonState == LOW) {
              _risingEdge[i] = 0;
              _state = BOOL2DIR(i);
            }
            else {
              _risingEdge[i] = 1;
              _state = 0;
            }
            _lastSwitchState[i] = buttonState;
            return true;
        }
      }
      return false;
    }

    const int8_t& getState() const {return _state;}
};

#endif