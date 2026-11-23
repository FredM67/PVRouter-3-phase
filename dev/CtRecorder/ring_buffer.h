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
      overrunPending_ = true;
      return false;
    }
    FrameData &slot{ frames_[head_ & (N - 1)] };
    slot = f;
    if (overrunPending_)  // the first frame stored after a drop: frames were lost just before it
    {
      slot.overrun = true;
      overrunPending_ = false;
    }
    head_ = static_cast< uint8_t >(head_ + 1);
    return true;
  }

  /** False when empty. The frame stored right after a drop carries the overrun flag (set by push()). */
  bool pop(FrameData &f)
  {
    if (head_ == tail_)
    {
      return false;
    }
    f = frames_[tail_ & (N - 1)];
    tail_ = static_cast< uint8_t >(tail_ + 1);
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
    overrunPending_ = false;
  }

private:
  FrameData frames_[N];
  volatile uint8_t head_{ 0 };
  volatile uint8_t tail_{ 0 };
  volatile uint32_t overruns_{ 0 };
  bool overrunPending_{ false };  // producer only: a frame was dropped, the next one stored carries the flag
};
