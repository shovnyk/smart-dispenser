#include "LoadCell.h"

static float calibration_factor = -220.50;

static int data_pin = 27;
static int clock_pin = 14;

HX711 scale;

void loadcell_init()
{
  scale.begin(data_pin, clock_pin);
  scale.set_scale(calibration_factor);
}

void load_cell_calibrate(float calconst)
{
  // Not implemented.
}