 #include <Arduino.h>
 class Config{
     public:
       const String name = "noszlop_uveghaz";
       const float temp_target = 8;
       const float heating_start = 5;
       const boolean invert_heating = false; // Invert heating logic
 };