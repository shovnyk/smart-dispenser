#pragma

#include "HX711.h"

extern HX711 scale;

void loadcell_init();

inline void loadcell_tare() {
 scale.tare(); 
}

inline float loadcell_get_weight() {
  return scale.get_units();
}

void load_cell_calibrate(float calconst);
