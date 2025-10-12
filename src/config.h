 #include <Arduino.h>
 class Config{
     public:
       const String name = "noszlop_inkub";
       const float temp_target = 30;
       const float heating_start_temp = 6;
       const boolean invert_heating = false; // Invert heating logic
 };