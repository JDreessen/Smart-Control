# BUGS
- 

# TODO
- shutterSwitch is pretty cluttered -> simplify class interface

- add README.md

# DONE
- for MQTT, don't block new commands, instead finish motor movement prematurely (but save new position)
- replace setRelays with digitalWrite
- if switch is pressed but motor has finished, listen for serial again
- motors not controlled via serial shoud react to swtiches
- understand markdown format ^^
- turn pins and durations into structs / unions --- implemented Shutter class

# NOTES
- Doesn't actually matter if motor is moving, just end movement before moving again
- new and improved loop:
    - if switch has changed
        - stop motor and write to EEPROM
        - if switchState !=