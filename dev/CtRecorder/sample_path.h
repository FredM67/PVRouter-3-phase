#pragma once
/**
 * @file sample_path.h
 * @brief The router firmware's voltage path for one phase: DC offset, polarity, cycle start.
 *
 * Pure C++ (no Arduino). Mirrors Mk2_3phase_RFdatalog_temp/processing.cpp, for one phase and in the
 * order of processVoltageRawSample(): polarity, confirmation, half-cycle events, then the integrator.
 * Samples are the left-aligned ADC value (ADCW with ADLAR: the 10-bit result << 6).
 */

#include <stdint.h>

class SamplePath
{
public:
  /** Allows polarity changes to be confirmed (processing.h). */
  static constexpr uint8_t PERSISTENCE_FOR_POLARITY_CHANGE{ 1 };

  /**
   * One voltage sample. Returns it minus the DC offset, as the firmware's i_sampleVminusDC.
   * Updates polarity, integrator and offset.
   */
  int16_t onVoltage(uint16_t raw)
  {
    // processPolarity(): +32 is the rounding, half an ADC count when left-aligned
    sampleVminusDC_ = static_cast< int16_t >((raw | 32U) - dcOffset_);
    mostRecent_ = (sampleVminusDC_ > 0) ? POSITIVE : NEGATIVE;

    confirmPolarity();

    // processRawSamples(): half-cycle events of the confirmed polarity
    cycleStarted_ = false;
    if (confirmed_ == POSITIVE)
    {
      cycleStarted_ = (lastConfirmed_ != POSITIVE);  // start of a positive half cycle
    }
    else if (lastConfirmed_ != NEGATIVE)
    {
      // processMinusHalfCycle(): bits 15..30 of the integrator ('<< 1' only drops bit 31)
      dcOffset_ = static_cast< uint16_t >((filterDC_ << 1) >> 16);
    }

    // processVoltage(): integrate the sample, remember the polarity
    filterDC_ += static_cast< uint32_t >(static_cast< int32_t >(sampleVminusDC_));
    lastConfirmed_ = confirmed_;

    return sampleVminusDC_;
  }

  /** True right after a confirmed change to positive polarity (the firmware's cycle start). */
  bool cycleStarted() const
  {
    return cycleStarted_;
  }

  /** processCurrentRawSample(): the current minus the voltage's offset, before any CT filter. */
  int16_t currentMinusDc(uint16_t raw) const
  {
    return static_cast< int16_t >((raw | 32U) - dcOffset_);
  }

  uint16_t dcOffset() const
  {
    return dcOffset_;
  }

private:
  enum Polarity : uint8_t
  {
    NEGATIVE,
    POSITIVE
  };

  // confirmPolarity()
  void confirmPolarity()
  {
    if (mostRecent_ == lastConfirmed_)
    {
      count_ = 0;
      return;
    }
    if (++count_ > PERSISTENCE_FOR_POLARITY_CHANGE)
    {
      count_ = 0;
      confirmed_ = mostRecent_;
    }
  }

  // initializeProcessing(): nominal mid-point of the left-aligned ADC (511 << 6), integrator in Q17.15
  uint16_t dcOffset_{ 511U << 6 };
  uint32_t filterDC_{ static_cast< uint32_t >(511U << 6) << 15 };
  int16_t sampleVminusDC_{ 0 };
  Polarity mostRecent_{ NEGATIVE };
  Polarity confirmed_{ NEGATIVE };
  Polarity lastConfirmed_{ NEGATIVE };
  uint8_t count_{ 0 };
  bool cycleStarted_{ false };
};
