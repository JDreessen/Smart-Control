# BUGS
- wrong length of set* command may not be handled correctly (8.11. possibly fixed)

# TODO
- if switch is pressed but motor has finished, listen for serial again
- add ability to choose which motors to control via serial (via motor IDs)
- motors not controlled via serial shoud react to swtiches (via array of motor IDs controlled by serial) (even better: array with length of motor amount and values 0=inactive, 1=switchControlled, 2=serialControlled)

- add README.md
- understand markdown format ^^
- (decrease RAM usage)
- (maybe) turn pins and durations into structs (for readability) / structs with unions (for efficiency)

# DONE
- replace setRelays with digitalWrite