# BUGS
- wrong length of set* command may not be handled correctly (8.11. possibly fixed)
-> setting all 8 motors buggy

# TODO

- add README.md
- understand markdown format ^^
- (decrease RAM usage)
- (maybe) turn pins and durations into structs (for readability) / structs with unions (for efficiency)

# DONE
- replace setRelays with digitalWrite
- if switch is pressed but motor has finished, listen for serial again
- if switch is pressed but motor has finished, listen for serial again
- motors not controlled via serial shoud react to swtiches