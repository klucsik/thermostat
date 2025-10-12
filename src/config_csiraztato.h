 #include <Arduino.h>
 class Config{
     public:
       const String name = "noszlop_csiraztato";
       const float temp_target = 29;
       const float heating_start = 27;
       const boolean heating_start_temp = false; // Invert heating logic
 };