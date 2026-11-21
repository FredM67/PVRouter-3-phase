/**
 * @file load_priorities.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The load priority state machine: which load to switch at each mains cycle
 * @version 1.0
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details Each element of the priority array is one logical load, the highest priority
 *          first: its low 7 bits are the physical load number (an index of physicalLoadPin[]),
 *          its high bit says whether the load is ON.
 *
 *          Once per mains cycle, the ISR compares the energy bucket with two thresholds:
 *          above the upper one, the first load OFF in priority order comes ON; below the
 *          lower one, the last load ON goes OFF. After a switch, no other load may switch for
 *          POST_TRANSITION_MAX_COUNT cycles, so that the switch has time to show in the bucket:
 *          only the load just switched may switch back. The threshold follows the bucket
 *          meanwhile.
 *
 *          A threshold goes back to its default only when the bucket is in the other half:
 *          while the bucket stays in the upper half, the upper threshold keeps the level the
 *          bucket reached during the last post-transition period, and a load comes ON only if
 *          the bucket rises above it. The same for the lower threshold in the lower half.
 *
 *          Then the logical states give the physical ones: a load is ON if its override is
 *          active, or if diversion is enabled and its logical state is ON. Calibration mode
 *          keeps every load OFF.
 *
 * @note This header has no Arduino dependency, so it can be exercised by the native test
 *       suite. Everything runs in the ADC ISR: the functions are forced inline, except
 *       Switching::onNewCycle() (see there).
 *
 * @ingroup TimeCritical
 */

#ifndef LOAD_PRIORITIES_H
#define LOAD_PRIORITIES_H

#include <stdint.h>

#include "load_map.h"  // LoadStates

inline constexpr uint8_t loadStateMask{ 0x7FU };                      /**< bit mask for masking load state */
inline constexpr uint8_t loadStateOnBit{ (uint8_t)(~loadStateMask) }; /**< bit mask for load state ON */

namespace LoadPriorities
{
/**
 * @brief The next logical load to switch ON: the first one OFF, in priority order.
 *
 * @return its position in the priority array, or N if every load is ON
 */
template< uint8_t N >
__attribute__((always_inline)) inline uint8_t nextToAdd(const uint8_t (&priorities)[N])
{
  for (uint8_t index = 0; index < N; ++index)
  {
    if (0x00 == (priorities[index] & loadStateOnBit))
    {
      return index;
    }
  }

  return N;
}

/**
 * @brief The next logical load to switch OFF: the last one ON, in priority order.
 *
 * @return its position in the priority array, or N if every load is OFF
 */
template< uint8_t N >
__attribute__((always_inline)) inline uint8_t nextToRemove(const uint8_t (&priorities)[N])
{
  uint8_t index{ N };
  do
  {
    if (priorities[--index] & loadStateOnBit)
    {
      return index;
    }
  } while (index);

  return N;
}

/**
 * @brief Rotates the priorities by one: the last load becomes the first, the others move
 *        down one place. Each load keeps its state.
 *
 * @note Nothing to do with a single load. The loop below would start at 0 there, read
 *       priorities[-1], and wrap around: 255 writes past the array, in the ISR.
 */
template< uint8_t N >
__attribute__((always_inline)) inline void rotate(uint8_t (&priorities)[N])
{
  if constexpr (N > 1)
  {
    uint8_t i{ static_cast< uint8_t >(N - 1) };
    const auto temp{ priorities[i] };
    do
    {
      priorities[i] = priorities[i - 1];
      --i;
    } while (i);
    priorities[0] = temp;
  }
}

/**
 * @brief Maps the logical load states to the physical ones.
 *
 * @tparam CALIBRATION every load stays OFF (calibration mode)
 * @param isOverridden callable, physical load number -> true if its override is active
 *
 * @pre The load numbers of @p priorities are 0..N-1, each once: they index @p physical
 *      unchecked. validation.h checks loadPrioritiesAtStartup at compile time
 *      (check_load_priorities()), and rotate() only reorders them.
 */
template< bool CALIBRATION, uint8_t N, typename IsOverridden >
__attribute__((always_inline)) inline void toPhysical(const uint8_t (&priorities)[N], LoadStates (&physical)[N],
                                                      const bool diversionEnabled, IsOverridden isOverridden)
{
  uint8_t idx{ N };
  do
  {
    --idx;
    const auto iLoad{ priorities[idx] & loadStateMask };

    const bool bOn{ isOverridden(iLoad) || (diversionEnabled && (priorities[idx] & loadStateOnBit)) };
    physical[iLoad] = (!CALIBRATION && bOn) ? LoadStates::LOAD_ON : LoadStates::LOAD_OFF;
  } while (idx);
}

/** @brief The fixed levels of the energy bucket, in bucket units. */
struct Bucket
{
  int32_t capacity;     /**< full bucket */
  int32_t midPoint;     /**< half of the capacity */
  int32_t lowerDefault; /**< lower threshold, set again at each cycle in the upper half */
  int32_t upperDefault; /**< upper threshold, set again at each cycle in the lower half */
};

/**
 * @brief The switching decision, once per mains cycle, and its state.
 *
 * @tparam N number of loads
 */
template< uint8_t N >
struct Switching
{
  static constexpr uint8_t POST_TRANSITION_MAX_COUNT{ 3 }; /**< allows each transition to take effect */

  int32_t lowerThreshold{ 0 };      /**< dynamic lower threshold */
  int32_t upperThreshold{ 0 };      /**< dynamic upper threshold */
  bool recentTransition{ false };   /**< a load state has been recently toggled */
  uint8_t postTransitionCount{ 0 }; /**< counts the number of cycles since the last transition */
  uint8_t activeLoad{ N };          /**< the logical load switched last */

  /**
   * @brief The decision of one mains cycle: may switch one load ON or OFF.
   *
   * @details Not inlined on purpose, as updatePhysicalLoadStates() in processing.h: inlined, it
   *          grew the ISR by 76 bytes, with 4 more registers to save and a stack frame on every
   *          call. Called once per mains cycle, it costs a call and a return there. Grid
   *          simulator: average 0.1 cycle less, one-off max 2 to 8 cycles more; with remote loads,
   *          32 cycles less on average and 77 less at most.
   *
   * @param energy the level of the energy bucket
   */
  __attribute__((noinline)) void onNewCycle(uint8_t (&priorities)[N], const int32_t& energy, const Bucket& bucket)
  {
    // Restrictions apply for the period immediately after a load has been switched.
    // if (recentTransition)
    //   recentTransition = (++postTransitionCount < POST_TRANSITION_MAX_COUNT);
    // for optimization, the next line is equivalent to the two lines above
    recentTransition &= (++postTransitionCount < POST_TRANSITION_MAX_COUNT);

    if (energy > bucket.midPoint)
    {
      // the energy state is in the upper half of the working range
      lowerThreshold = bucket.lowerDefault;  // reset the "opposite" threshold
      if (energy > upperThreshold)
      {
        // Because the energy level is high, some action may be required
        onHighEnergy(priorities, energy, bucket.capacity);
      }
    }
    else
    {
      // the energy state is in the lower half of the working range
      upperThreshold = bucket.upperDefault;  // reset the "opposite" threshold
      if (energy < lowerThreshold)
      {
        // Because the energy level is low, some action may be required
        onLowEnergy(priorities, energy);
      }
    }
  }

  /**
   * @brief The energy level is high: may switch the next load ON.
   *
   * @details During a post-transition period, the upper threshold follows any increase of the
   *          energy level, and only the load switched last may switch.
   */
  __attribute__((always_inline)) inline void onHighEnergy(uint8_t (&priorities)[N], const int32_t& energy, const int32_t capacity)
  {
    const auto tempLoad{ nextToAdd(priorities) };

    if (tempLoad == N)
    {
      return;
    }

    bool bOK_toAddLoad{ true };
    // a load which is now OFF has been identified for potentially being switched ON
    if (recentTransition)
    {
      // During the post-transition period, any increase in the energy level is noted.
      upperThreshold = energy;

      // the energy thresholds must remain within range
      if (upperThreshold > capacity)
      {
        upperThreshold = capacity;
      }

      // Only the active load may be switched during this period. All other loads must
      // wait until the recent transition has had sufficient opportunity to take effect.
      bOK_toAddLoad = (tempLoad == activeLoad);
    }

    if (bOK_toAddLoad)
    {
      priorities[tempLoad] |= loadStateOnBit;
      activeLoad = tempLoad;
      postTransitionCount = 0;
      recentTransition = true;
    }
  }

  /**
   * @brief The energy level is low: may switch the last load ON to OFF.
   *
   * @details During a post-transition period, the lower threshold follows any decrease of the
   *          energy level, and only the load switched last may switch.
   */
  __attribute__((always_inline)) inline void onLowEnergy(uint8_t (&priorities)[N], const int32_t& energy)
  {
    const auto tempLoad{ nextToRemove(priorities) };

    if (tempLoad == N)
    {
      return;
    }

    bool bOK_toRemoveLoad{ true };
    // a load which is now ON has been identified for potentially being switched OFF
    if (recentTransition)
    {
      // During the post-transition period, any decrease in the energy level is noted.
      lowerThreshold = energy;

      // the energy thresholds must remain within range
      if (lowerThreshold < 0)
      {
        lowerThreshold = 0;
      }

      // Only the active load may be switched during this period. All other loads must
      // wait until the recent transition has had sufficient opportunity to take effect.
      bOK_toRemoveLoad = (tempLoad == activeLoad);
    }

    if (bOK_toRemoveLoad)
    {
      priorities[tempLoad] &= loadStateMask;
      activeLoad = tempLoad;
      postTransitionCount = 0;
      recentTransition = true;
    }
  }
};
}  // namespace LoadPriorities

#endif /* LOAD_PRIORITIES_H */
