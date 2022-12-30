/**
 * @file utils.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief Some utility functions
 * @version 0.1
 * @date 2021-10-04
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef __UTILS_RF_H__
#define __UTILS_RF_H__

#ifdef RF_PRESENT
#include <JeeLib.h>

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
#endif  // RF_PRESENT

#endif  // __UTILS_RF_H__
