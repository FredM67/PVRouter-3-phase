#pragma once
/**
 * @file burst_pattern.h
 * @brief The load's ON/OFF pattern, one step per mains cycle: a list of {on, off} pairs, repeated.
 *
 * Pure C++ (no Arduino), tested on the PC.
 */

#include <stdint.h>

struct BurstPattern
{
  struct Step
  {
    uint16_t onCycles;
    uint16_t offCycles;
  };

  static constexpr uint8_t MAX_STEPS{ 8 };

  /** The default: 5, 10, 25 and 50 cycles ON, each followed by 150 OFF. */
  BurstPattern()
  {
    const Step defaults[]{ { 5, 150 }, { 10, 150 }, { 25, 150 }, { 50, 150 } };
    set(defaults, 4);
  }

  /** Replaces the pattern. False, and the pattern unchanged, for no pair, too many, or a pair of 0 and 0. */
  bool set(const Step *steps, uint8_t count)
  {
    if ((count == 0) || (count > MAX_STEPS))
    {
      return false;
    }
    uint32_t total{ 0 };
    for (uint8_t k = 0; k != count; ++k)
    {
      if ((steps[k].onCycles == 0) && (steps[k].offCycles == 0))
      {
        return false;
      }
      total += steps[k].onCycles + steps[k].offCycles;
    }
    for (uint8_t k = 0; k != count; ++k)
    {
      steps_[k] = steps[k];
    }
    count_ = count;
    period_ = total;
    return true;
  }

  /** Cycles in one repetition of the pattern. */
  uint32_t period() const
  {
    return period_;
  }

  uint8_t count() const
  {
    return count_;
  }

  const Step &step(uint8_t k) const
  {
    return steps_[k];
  }

  /** Whether the load is ON during mains cycle `cycle` (0 is the first cycle of the pattern). */
  bool onForCycle(uint32_t cycle) const
  {
    uint32_t position{ cycle % period_ };
    for (uint8_t k = 0; k != count_; ++k)
    {
      if (position < steps_[k].onCycles)
      {
        return true;
      }
      position -= steps_[k].onCycles;
      if (position < steps_[k].offCycles)
      {
        return false;
      }
      position -= steps_[k].offCycles;
    }
    return false;  // not reached: position < period_
  }

  /**
   * The same sequence, stepped one cycle at a time with 16-bit counters only: what the ISR uses
   * (a 32-bit modulo is too slow there). reset(), then next() once per cycle, gives the state of
   * cycle 0, 1, 2...
   */
  struct Cursor
  {
    void reset(const BurstPattern &p)
    {
      step_ = 0;
      onPhase_ = true;
      left_ = p.step(0).onCycles;
    }

    bool next(const BurstPattern &p)
    {
      while (left_ == 0)  // ends: each pair has at least one cycle
      {
        if (onPhase_)
        {
          onPhase_ = false;
          left_ = p.step(step_).offCycles;
        }
        else
        {
          onPhase_ = true;
          step_ = static_cast< uint8_t >((step_ + 1 == p.count()) ? 0 : step_ + 1);
          left_ = p.step(step_).onCycles;
        }
      }
      --left_;
      return onPhase_;
    }

  private:
    uint8_t step_{ 0 };
    bool onPhase_{ true };
    uint16_t left_{ 0 };
  };

private:
  Step steps_[MAX_STEPS]{};
  uint8_t count_{ 0 };
  uint32_t period_{ 1 };
};
