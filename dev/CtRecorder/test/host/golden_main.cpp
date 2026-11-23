/**
 * @file golden_main.cpp
 * @brief Host reference of the recorder's current path, for the Python port to be checked against.
 *
 * stdin: a first line "<alpha> <gain>", then one "<v> <i>" per line (10-bit raw values, as the frames carry them).
 * stdout: one "I out" per line, as the sketch computes it: sample_path.h, then the firmware's CT filter when
 * the gain is not 0.
 *
 * Build: g++ -std=c++17 -I dev/CtRecorder -I Mk2_3phase_RFdatalog_temp dev/CtRecorder/test/host/golden_main.cpp
 */

#include <stdint.h>
#include <stdio.h>

#include "ct_filter.h"
#include "sample_path.h"

int main()
{
  float alpha{};
  float gain{};
  if (scanf("%f %f", &alpha, &gain) != 2)
  {
    return 1;
  }
  const CtFilter::Coefficients coefficients{ CtFilter::coefficientsOf(gain, alpha) };
  const bool filterOn{ gain != 0.0F };

  SamplePath path;
  CtFilter::LowPassBoost lowPass;
  unsigned v{};
  unsigned i{};
  while (scanf("%u %u", &v, &i) == 2)
  {
    path.onVoltage(static_cast< uint16_t >(v << 6));
    int16_t iOut{ path.currentMinusDc(static_cast< uint16_t >(i << 6)) };
    if (filterOn)
    {
      iOut = lowPass.apply(iOut, coefficients);
    }
    printf("%d\n", iOut);
  }
  return 0;
}
