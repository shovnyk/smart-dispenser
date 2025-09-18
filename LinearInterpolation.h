#pragma once

struct data_point {
  int volume_mL;
  float dispense_factor;
};

int interpolate_linear(struct data_point *);
