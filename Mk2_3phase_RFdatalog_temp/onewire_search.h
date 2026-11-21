/**
 * @file onewire_search.h
 * @author Frédéric Metrich (frederic.metrich@live.fr)
 * @brief The ROM search of a 1-Wire bus, on the bit primitives of the bus
 * @version 0.1
 * @date 2026-11-21
 *
 * @copyright Copyright (c) 2026-2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @details The firmware builds the OneWire library without its own search (-DONEWIRE_SEARCH=0 in
 *          platformio.ini): its object would keep 11 bytes of search state for good, and the
 *          firmware addresses its sensors by the ROM codes of config.h. Calibration mode lists
 *          the sensors on the bus with this one instead, whose state lives on the stack.
 *
 *          The algorithm of Maxim's application note 187 ("1-Wire Search Algorithm"): each
 *          search walks the 64 bits of the ROM codes; at each bit, every device still in the
 *          search sends its bit, then its complement. 0 and 1 means a conflict: some devices
 *          have a 0 there, others a 1. The search takes the 0 branch at a new conflict, and the
 *          1 branch at the last one taken as 0 before, so each search finds the next device.
 *
 *          No Arduino dependency: the native tests run it on a simulated bus.
 *
 * @ingroup TemperatureSensing
 */

#ifndef ONEWIRE_SEARCH_H
#define ONEWIRE_SEARCH_H

#include <stdint.h>

/**
 * @brief The successive searches of a 1-Wire bus, one device each.
 *
 * @tparam Bus Has `reset()` (true if a device answers), `write(uint8_t)`, `read_bit()` and
 *             `write_bit(uint8_t)`, as OneWire.
 *
 * @code
 * RomSearch search{ oneWire };
 * uint8_t rom[8];
 * while (search.next(rom)) { ... }
 * @endcode
 */
template< typename Bus >
class RomSearch
{
public:
  explicit RomSearch(Bus& bus)
    : bus{ bus }
  {
  }

  /**
   * @brief Finds the next device on the bus.
   *
   * @param rom Its ROM code: family code first, CRC last (to check by the caller). The same
   *            array at each call: a search follows the previous code up to its last conflict.
   * @return false when every device has been found, if no device answers, or if the bus does
   *         not answer as expected (the search then starts over at the next call).
   */
  bool next(uint8_t (&rom)[8])
  {
    if (lastDevice)
    {
      return false;
    }
    if (!bus.reset())
    {
      lastDiscrepancy = 0;  // no device answers: the next search starts over
      return false;
    }
    bus.write(SEARCH_ROM);

    uint8_t lastZero{ 0 };  // last conflict where the 0 branch is taken, 1..64 (0: none)
    for (uint8_t bit = 1; bit <= 64; ++bit)
    {
      const uint8_t idBit{ bus.read_bit() };
      const uint8_t complementBit{ bus.read_bit() };
      uint8_t& romByte{ rom[(bit - 1) / 8] };
      const uint8_t mask{ static_cast< uint8_t >(1U << ((bit - 1) % 8)) };

      if (idBit && complementBit)
      {
        // no device in the search any more: unplugged, or noise
        lastDiscrepancy = 0;
        return false;
      }

      bool direction{ idBit != 0 };  // no conflict: every device left has this bit
      if (idBit == complementBit)
      {
        // a conflict: 1 at the last one taken as 0, 0 at a new one, as before up to there
        direction = (bit < lastDiscrepancy) ? ((romByte & mask) != 0) : (bit == lastDiscrepancy);
        if (!direction)
        {
          lastZero = bit;
        }
      }

      romByte = static_cast< uint8_t >(direction ? (romByte | mask) : (romByte & ~mask));
      bus.write_bit(direction ? 1 : 0);  // the devices with the other bit leave the search
    }

    lastDiscrepancy = lastZero;
    lastDevice = (0 == lastZero);
    return true;
  }

private:
  static constexpr uint8_t SEARCH_ROM{ 0xF0 }; /**< 1-Wire command: search the ROM codes */

  Bus& bus;
  uint8_t lastDiscrepancy{ 0 }; /**< the conflict to take as 1 at the next search */
  bool lastDevice{ false };     /**< the last device has been found */
};

#endif /* ONEWIRE_SEARCH_H */
