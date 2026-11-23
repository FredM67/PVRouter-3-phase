/**
 * @file golden_main.cpp
 * @brief Host reference of the recorder's current path, for the Python port to be checked against.
 *
 * stdin: a first line "<alpha> <gain>", then one "<v> <i>" per line (10-bit raw values, as the frames carry them).
 * stdout: a "COEF" line (the fixed-point coefficients of CtFilter::Coefficients, in the order of the struct), then
 * one "I out" per line, as the sketch computes it: sample_path.h, then the firmware's CT filter when the gain is not 0.
 *
 * With "--warm N": the first N pairs are processed without output, then a "STATE" line gives the state of the
 * filters before pair N (what the sketch snapshots at the start of a recording: dc offset, integrator, confirmed,
 * last confirmed, persistence count, lpfX64, lpfLowBits) and the output goes on with the remaining pairs.
 *
 * Build: g++ -std=c++17 -I dev/CtRecorder -I Mk2_3phase_RFdatalog_temp dev/CtRecorder/test/host/golden_main.cpp
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ct_filter.h"
#include "sample_path.h"

int main(int argc, char **argv)
{
  unsigned long warm{ 0 };
  if (argc == 3 && strcmp(argv[1], "--warm") == 0)
  {
    warm = strtoul(argv[2], nullptr, 10);
  }

  float alpha{};
  float gain{};
  if (scanf("%f %f", &alpha, &gain) != 2)
  {
    return 1;
  }
  const CtFilter::Coefficients coefficients{ CtFilter::coefficientsOf(gain, alpha) };
  const bool filterOn{ gain != 0.0F };
  printf("COEF %d %d %d %u %d %d %d\n",
         coefficients.alpha,
         coefficients.alphaX2,
         coefficients.alphaX4,
         static_cast< unsigned >(coefficients.alphaShift),
         coefficients.gain,
         coefficients.gainHalf,
         coefficients.gainQuarter);

  SamplePath path;
  CtFilter::LowPassBoost lowPass;
  unsigned v{};
  unsigned i{};
  unsigned long n{ 0 };
  while (scanf("%u %u", &v, &i) == 2)
  {
    if (n == warm && warm != 0)
    {
      printf("STATE %u %lu %d %d %u %d %u\n",
             static_cast< unsigned >(path.dcOffset()),
             static_cast< unsigned long >(path.integrator()),
             path.confirmedPositive() ? 1 : 0,
             path.lastConfirmedPositive() ? 1 : 0,
             static_cast< unsigned >(path.persistenceCount()),
             lowPass.lpfX64,
             static_cast< unsigned >(lowPass.lpfLowBits));
    }
    path.onVoltage(static_cast< uint16_t >(v << 6));
    int16_t iOut{ path.currentMinusDc(static_cast< uint16_t >(i << 6)) };
    if (filterOn)
    {
      iOut = lowPass.apply(iOut, coefficients);
    }
    if (n >= warm)
    {
      printf("%d\n", iOut);
    }
    ++n;
  }
  return 0;
}
