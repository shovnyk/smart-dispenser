#include "LinearInterpolation.h"

static struct data_point table[] = {
  {1000,	0.00008},
  {900,	0.000085},
  {800,	0.00010},
  {700,	0.000115},
  {600,	0.00013},
  {500,	0.00015},
  {400,	0.0002},
  {300,	0.0003},
  {200,	0.00045},
  {100,	0.0008}
};

int num_table_entries = 10;

int interpolate_linear(struct data_point *result)
{
  float y1, y2, slope;
  int x1, x2;
  int vol = result->volume_mL;
  
  for (int i = 0; i < num_table_entries; ++i) {
    if (table[i].volume_mL == vol) {
      result->dispense_factor = table[i].dispense_factor;
      break;
    }
    if (table[i].volume_mL > vol) {
      continue;        
    }
    x1 = table[i].volume_mL;
    y1 = table[i].dispense_factor;
    x2 = table[i - 1].volume_mL;
    y2 = table[i - 1].dispense_factor;
    slope = (y2 - y1)/(x2 - x1);
    result->dispense_factor = slope * (vol - x1) + y1;
    break;
  }
  return 0;
}
