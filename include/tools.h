#include "Arduino.h"

#define   CHAR2INT(c)       ((int)c - 48)
#define   SIGN(x)           ((x>0) ? (1) : (-1))
#define   DIR2BOOL(x)       ((x + 1) / 2)
#define   BOOL2DIR(x)       ((x * 2) - 1)

#define TRANSLATE_VALUE(c)  ((c == 'c') ? (100) : (CHAR2INT(c) * 10))
