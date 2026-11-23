#pragma once
/**
 * @file ring_buffer.h
 * @brief A single-producer (ISR), single-consumer (loop) FIFO of frames.
 *
 * Pure C++ (no Arduino). The head is written by push() only, the tail by pop() only, so no
 * interrupt masking is needed on an 8-bit MCU. Both are free-running uint8_t counters: N is a power
 * of two up to 128, and the difference head - tail is the fill level.
 */

#include <stdint.h>

#include "frame.h"

template< uint8_t N >
struct RingBuffer
{
  static_assert(N != 0 && (N & (N - 1)) == 0, "N must be a power of two");
  static_assert(N <= 128, "N must fit the uint8_t counters");

  /** False, and an overrun latched, when full: the frame is dropped. */
  bool push(const FrameData &f)
  {
    if (static_cast< uint8_t >(head_ - tail_) == N)
    {
      ++overruns_;
      ++overrunTicks_;
      return false;
    }
    frames_[head_ & (N - 1)] = f;
    head_ = static_cast< uint8_t >(head_ + 1);
    return true;
  }

  /** False when empty. The frame popped after an overrun carries the overrun flag. */
  bool pop(FrameData &f)
  {
    if (head_ == tail_)
    {
      return false;
    }
    f = frames_[tail_ & (N - 1)];
    tail_ = static_cast< uint8_t >(tail_ + 1);
    const uint8_t ticks{ overrunTicks_ };
    f.overrun = (ticks != seenTicks_);
    seenTicks_ = ticks;
    return true;
  }

  /** Frames lost so far. */
  uint32_t overruns() const
  {
    return overruns_;
  }

  /** Empties the buffer and forgets the overruns; call with the producer stopped. */
  void clear()
  {
    tail_ = head_;
    overruns_ = 0;
    seenTicks_ = overrunTicks_;
  }

private:
  FrameData frames_[N];
  volatile uint8_t head_{ 0 };
  volatile uint8_t tail_{ 0 };
  volatile uint32_t overruns_{ 0 };
  // a wrapping 8-bit copy of the count, so that one byte tells the consumer whether a frame was lost
  volatile uint8_t overrunTicks_{ 0 };
  uint8_t seenTicks_{ 0 };
};
