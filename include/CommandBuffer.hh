#ifndef HH_COMMANDBUFFER
#define HH_COMMANDBUFFER

#include <Arduino.h>

template <typename interfaceType, uint8_t commandSize, char terminator_char>
class CommandBuffer {
    private:
        char _data;
        bool _available = 0;
        // This is position in the buffer where we put next char.
        uint8_t _currentIndex = 0;
        interfaceType& _serialInterface;

    public:
        char content[commandSize+1];
        CommandBuffer(interfaceType serialInterface) : _serialInterface(serialInterface) {}
        void update() {
            _available = false;
            while ( _serialInterface.available() > 0 && !_available) {
                _data = _serialInterface.read(); addData(_data);
            }
        }
        bool available() { return _available; }
        
        // Put received character into the buffer.
        // When a complete command is received return true, otherwise return false.
        // The command is terminated by Enter character ("\r")
        void addData(char nextChar) {  
            // Ignore some characters - new line, space and tabs
            if ((nextChar == '\n') || (nextChar == ' ') || (nextChar == '\t')) {
                return;
            }
            
            // If we receive Enter character...
            if (nextChar == terminator_char) {
                // ...terminate the string by NULL character "\0" and return true
                content[_currentIndex] = '\0';
                _currentIndex = 0;
                _available = true;
                return;
            }

            // For normal character just store it in the buffer and increment Index
            content[_currentIndex] = nextChar;
            _currentIndex++;

            // Check for too many chars
            if (_currentIndex >= commandSize) {
                // If data exceeds max length reset our position and return true
                // so that the data received so far can be processed - the caller should
                // see if it is valid command or not
                content[commandSize] = '\0';
                _currentIndex = 0;
                return;
            }
        }
        uint8_t read(uint8_t i) {
            if (this->content[i] == 'c') {return 100;}
            else {return this->content[i] * 10;}
        }
};

#endif // HH_COMMANDBUFFER