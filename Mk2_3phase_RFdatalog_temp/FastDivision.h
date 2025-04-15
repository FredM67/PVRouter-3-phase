
#ifndef FASTDIVISION_H
#define FASTDIVISION_H

#include <Arduino.h>

extern uint16_t divu10(uint16_t n) __attribute__((noinline));  //29 cycles

constexpr auto divu8(uint16_t n)
{
  return n >> 3;
}

constexpr auto divu4(uint16_t n)
{
  return n >> 2;
}

constexpr auto divu2(uint16_t n)
{
  return n >> 1;
}

constexpr auto divu1(uint16_t n)
{
  return n;
}

extern void divmod10(uint32_t in, uint32_t &div, uint8_t &mod) __attribute__((noinline));

#endif /* FASTDIVISION_H */
