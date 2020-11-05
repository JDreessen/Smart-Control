#define TEST false
// These have to have the same length
#if TEST 
    #define   INPUT_PINS        22, 23, 24, 25
    #define   OUTPUT_PINS       26, 27, 28, 29
    #define   MOTOR_UP_DUR    -20000, -10000
    #define   MOTOR_DOWN_DUR  20000, 10000
#else
    // 1 - Bad, 2 - Küche
    #define   INPUT_PINS      47, 49, 51, 53
    #define   OUTPUT_PINS     37, 35, 25, 23
    #define   MOTOR_UP_DUR    -19200, -19200
    #define   MOTOR_DOWN_DUR  17600, 17600
#endif