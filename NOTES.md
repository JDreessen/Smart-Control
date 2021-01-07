# BUGS
- wrong length of set* command may not be handled correctly (8.11. possibly fixed)
- setting all 8 motors buggy

# TODO
- for MQTT, don't block new commands, instead finish motor movement prematurely (but save new position)
- add README.md
- (decrease RAM usage)
- (maybe) cleanup processCommand funcion: similar to esp wifi server, if expression matches, ecexute atttached function. ex. commandHandler.attach("set", setCommand())

# DONE
- replace setRelays with digitalWrite
- if switch is pressed but motor has finished, listen for serial again
- motors not controlled via serial shoud react to swtiches
- understand markdown format ^^
- turn pins and durations into structs / unions --- implemented Shutter class