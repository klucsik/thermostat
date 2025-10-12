 #include <Arduino.h>
 class Config{
     public:
       const String name = "noszlop_teszt";
       const float temp_target = 8;
       const float heating_start_temp = 5;
       const boolean invert_heating = false; // Invert heating logic
 };