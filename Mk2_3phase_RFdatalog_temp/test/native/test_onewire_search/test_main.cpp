/**
 * @file test_main.cpp
 * @brief The ROM search of onewire_search.h, on a simulated 1-Wire bus
 *
 * @details The simulated bus is wired-AND, as the real one: a device sends a 0 by pulling the line
 *          low, so the master reads 1 only if every device in the search sends 1. After each bit
 *          and its complement, the master writes the branch it takes: the devices with the other
 *          bit leave the search until the next reset.
 */

#include <unity.h>

#include <stdint.h>
#include <string.h>

#include "onewire_search.h"

namespace
{
constexpr uint8_t MAX_DEVICES{ 8 };

/** A 1-Wire bus with a few devices, as the search sees it. */
class SimulatedBus
{
public:
  void add(const uint8_t (&rom)[8])
  {
    if (count != MAX_DEVICES)
    {
      memcpy(roms[count++], rom, 8);
    }
  }

  /** Plugs the devices back in (after unplugAll()). */
  void replugAll(uint8_t devices)
  {
    count = devices;
  }

  bool reset()
  {
    ++resets;
    for (uint8_t i = 0; i != count; ++i)
    {
      inSearch[i] = true;
    }
    bit = 0;
    readComplement = false;
    return count != 0;  // presence pulse
  }

  void write(uint8_t command)
  {
    lastCommand = command;
  }

  uint8_t read_bit()
  {
    uint8_t line{ 1 };
    for (uint8_t i = 0; i != count; ++i)
    {
      if (inSearch[i] && (bitOf(i) == readComplement))  // sends 0: bit 0, or complement of 1
      {
        line = 0;
      }
    }
    readComplement = !readComplement;
    return line;
  }

  void write_bit(uint8_t value)
  {
    for (uint8_t i = 0; i != count; ++i)
    {
      if (bitOf(i) != (value != 0))
      {
        inSearch[i] = false;
      }
    }
    ++bit;
  }

  /** Unplugs every device: no presence pulse at the next reset. */
  void unplugAll()
  {
    plugged = count;
    count = 0;
  }

  uint8_t plugged{ 0 };

  uint8_t lastCommand{ 0 };
  uint16_t resets{ 0 };

private:
  bool bitOf(uint8_t device) const
  {
    return (roms[device][bit / 8] >> (bit % 8)) & 1U;
  }

  uint8_t roms[MAX_DEVICES][8]{};
  bool inSearch[MAX_DEVICES]{};
  uint8_t count{ 0 };
  uint8_t bit{ 0 };
  bool readComplement{ false };
};

/** The devices found, at most MAX_DEVICES + 1 searches (one more: a search that never ends). */
uint8_t searchAll(SimulatedBus& bus, uint8_t (&found)[MAX_DEVICES + 1][8])
{
  RomSearch< SimulatedBus > search{ bus };
  uint8_t rom[8]{};
  uint8_t n{ 0 };
  while ((n != MAX_DEVICES + 1) && search.next(rom))
  {
    memcpy(found[n++], rom, 8);
  }
  return n;
}

bool contains(const uint8_t (&found)[MAX_DEVICES + 1][8], uint8_t n, const uint8_t* rom)
{
  for (uint8_t i = 0; i != n; ++i)
  {
    if (0 == memcmp(found[i], rom, 8))
    {
      return true;
    }
  }
  return false;
}

// DS18B20 codes: family 0x28, serial number, CRC (the search does not check it)
constexpr uint8_t SENSOR_A[8]{ 0x28, 0xBE, 0x41, 0x6B, 0x09, 0x00, 0x00, 0xA4 };
constexpr uint8_t SENSOR_B[8]{ 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0xB7 };
constexpr uint8_t SENSOR_C[8]{ 0x28, 0x1B, 0xD7, 0x6A, 0x09, 0x00, 0x00, 0x37 };  // B but bit 63, the last one sent
constexpr uint8_t SENSOR_D[8]{ 0x28, 0xFF, 0x00, 0x12, 0x34, 0x56, 0x78, 0x9A };
}  // namespace

void setUp()
{
}

void tearDown()
{
}

void test_no_device()
{
  SimulatedBus bus;
  uint8_t found[MAX_DEVICES + 1][8]{};

  TEST_ASSERT_EQUAL_UINT8(0, searchAll(bus, found));
}

void test_one_device()
{
  SimulatedBus bus;
  bus.add(SENSOR_A);
  uint8_t found[MAX_DEVICES + 1][8]{};

  TEST_ASSERT_EQUAL_UINT8(1, searchAll(bus, found));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(SENSOR_A, found[0], 8);
  TEST_ASSERT_EQUAL_HEX8(0xF0, bus.lastCommand);  // SEARCH ROM
}

void test_two_devices()
{
  SimulatedBus bus;
  bus.add(SENSOR_A);
  bus.add(SENSOR_B);
  uint8_t found[MAX_DEVICES + 1][8]{};

  TEST_ASSERT_EQUAL_UINT8(2, searchAll(bus, found));
  TEST_ASSERT_TRUE(contains(found, 2, SENSOR_A));
  TEST_ASSERT_TRUE(contains(found, 2, SENSOR_B));
}

void test_codes_differing_in_their_last_bit()
{
  SimulatedBus bus;
  bus.add(SENSOR_B);
  bus.add(SENSOR_C);
  uint8_t found[MAX_DEVICES + 1][8]{};

  TEST_ASSERT_EQUAL_UINT8(2, searchAll(bus, found));
  TEST_ASSERT_TRUE(contains(found, 2, SENSOR_B));
  TEST_ASSERT_TRUE(contains(found, 2, SENSOR_C));
}

void test_four_devices_each_found_once()
{
  SimulatedBus bus;
  bus.add(SENSOR_A);
  bus.add(SENSOR_B);
  bus.add(SENSOR_C);
  bus.add(SENSOR_D);
  uint8_t found[MAX_DEVICES + 1][8]{};

  TEST_ASSERT_EQUAL_UINT8(4, searchAll(bus, found));
  TEST_ASSERT_TRUE(contains(found, 4, SENSOR_A));
  TEST_ASSERT_TRUE(contains(found, 4, SENSOR_B));
  TEST_ASSERT_TRUE(contains(found, 4, SENSOR_C));
  TEST_ASSERT_TRUE(contains(found, 4, SENSOR_D));
}

void test_eight_devices()
{
  SimulatedBus bus;
  uint8_t roms[MAX_DEVICES][8]{};
  for (uint8_t i = 0; i != MAX_DEVICES; ++i)
  {
    // codes that share long prefixes: conflicts deep in the 64 bits
    const uint8_t rom[8]{ 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, static_cast< uint8_t >(i * 37), static_cast< uint8_t >(i) };
    memcpy(roms[i], rom, 8);
    bus.add(rom);
  }
  uint8_t found[MAX_DEVICES + 1][8]{};

  TEST_ASSERT_EQUAL_UINT8(MAX_DEVICES, searchAll(bus, found));
  for (uint8_t i = 0; i != MAX_DEVICES; ++i)
  {
    TEST_ASSERT_TRUE(contains(found, MAX_DEVICES, roms[i]));
  }
}

void test_no_search_after_the_last_device()
{
  SimulatedBus bus;
  bus.add(SENSOR_A);
  RomSearch< SimulatedBus > search{ bus };
  uint8_t rom[8]{};

  TEST_ASSERT_TRUE(search.next(rom));
  const uint16_t resets{ bus.resets };
  TEST_ASSERT_FALSE(search.next(rom));
  TEST_ASSERT_EQUAL_UINT16(resets, bus.resets);  // no bus traffic once the last one is found
}

void test_devices_unplugged_between_two_searches()
{
  SimulatedBus bus;
  bus.add(SENSOR_A);
  bus.add(SENSOR_D);
  RomSearch< SimulatedBus > search{ bus };
  uint8_t rom[8]{};

  TEST_ASSERT_TRUE(search.next(rom));
  bus.unplugAll();
  TEST_ASSERT_FALSE(search.next(rom));  // no presence pulse
}

void test_search_starts_over_after_a_reset_without_presence()
{
  SimulatedBus bus;
  bus.add(SENSOR_A);
  bus.add(SENSOR_D);
  RomSearch< SimulatedBus > search{ bus };
  uint8_t first[8]{};
  uint8_t rom[8]{};

  TEST_ASSERT_TRUE(search.next(first));
  bus.unplugAll();
  TEST_ASSERT_FALSE(search.next(rom));
  bus.replugAll(bus.plugged);
  TEST_ASSERT_TRUE(search.next(rom));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(first, rom, 8);  // the first device again, not the second
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_no_device);
  RUN_TEST(test_one_device);
  RUN_TEST(test_two_devices);
  RUN_TEST(test_codes_differing_in_their_last_bit);
  RUN_TEST(test_four_devices_each_found_once);
  RUN_TEST(test_eight_devices);
  RUN_TEST(test_no_search_after_the_last_device);
  RUN_TEST(test_devices_unplugged_between_two_searches);
  RUN_TEST(test_search_starts_over_after_a_reset_without_presence);
  return UNITY_END();
}
