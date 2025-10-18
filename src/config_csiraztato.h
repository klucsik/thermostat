 #include <Arduino.h>
 class Config{
     public:
       const String name = "noszlop_csiraztato";
       const float temp_target = 29;
       const float heating_start_temp = 27;
       const boolean invert_heating = false; // Invert heating logic
 };