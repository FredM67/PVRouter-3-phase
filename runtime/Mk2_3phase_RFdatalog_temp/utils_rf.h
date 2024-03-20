/**
 * @file utils_rf.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions for the RF chip
 * @version 0.1
 * @date 2023-02-09
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _UTILS_RF_H
#define _UTILS_RF_H

#ifdef RF_PRESENT
#include <JeeLib.h>

inline constexpr bool RF_CHIP_PRESENT{ true };

/**
 * @brief Send the logging data through RF.
 * @details For better performance, the RFM12B needs to remain in its
 *          active state rather than being periodically put to sleep.
 *
 */
inline void send_rf_data()
{
  // check whether it's ready to send, and an exit route if it gets stuck
  uint32_t i = 0;
  while (!rf12_canSend() && i < 10)
  {
    rf12_recvDone();
    ++i;
  }
  rf12_sendNow(0, &tx_data, sizeof tx_data);
}
#else
inline constexpr bool RF_CHIP_PRESENT{ false };
#endif  // RF_PRESENT

#endif  // _UTILS_RF_H
