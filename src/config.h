#define TEST false
// These have to have the same length
#if TEST 
    #define   INPUT_PINS        22, 23, 24, 25
    #define   OUTPUT_PINS       26, 27, 28, 29
    #define   MOTOR_UP_DUR    -20000, -10000
    #define   MOTOR_DOWN_DUR  20000, 10000
#else
    // 1 - Küche, 2 - Esszimmer, 3 - Wohnzimmer Gross, 4 - Wohnzimmer Klein, 5 - Lea links, 6 - Lea rechts,  7 - Bad, 8 - HaWi
    #define   INPUT_PINS      51, 53, 43, 45, 42, 40, 44, 46, 48, 38, 50, 52, 47, 49, 39, 41
    #define   OUTPUT_PINS     25, 23, 28, 26, 36, 34, 32, 30, 29, 27, 24, 22, 37, 35, 33, 31
    #define   MOTOR_UP_DUR    -28100, -28100, -29500, -19200, -19200, -19200, -19200, -28100
    #define   MOTOR_DOWN_DUR   26100,  26100,  26100,  17600,  17600,  17600,  17600,  26100
#endif