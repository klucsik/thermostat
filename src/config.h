 #include <Arduino.h>
 class Config{
     public:
       const String name = "noszlop_telikert_hutes";
       const float temp_target = 10;
       const float heating_start_temp = 7;
       const boolean invert_heating = false; // Invert heating logic
 };