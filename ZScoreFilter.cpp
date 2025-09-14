#include "ZScoreFilter.h"
#include <cmath>

static int buf[N];
static int idx = 0, filled = 0;
static int last_good = 0.0585F;

static float mean(int *arr, int n)
{
  float s = 0;
  for (int i = 0; i < n; i++) s += arr[i];
  return s / n;
}

static float stddev(int *arr, int n, float m)
{
  float s = 0;
  for (int i = 0; i < n; i++) {
    float d = arr[i] - m;
    s += d * d;
  }
  return std::sqrt(s / n);
}

int z_score_filter(int new_val)
{
  buf[idx] = new_val;
  idx = (idx + 1) % N;
  if (filled < N)
    filled++;

  float m = mean(buf, filled);
  float sd = stddev(buf, filled, m);

  // Avoid div/0.
  if (sd == 0)
    sd = 1;

  // Calculate z-score.
  float z = std::fabs(new_val - m) / sd;

  if (z > Z_THRESHOLD) { // Reject outlier.
    return last_good;
  }
  else {
    last_good = new_val;
    return new_val;
  }
}