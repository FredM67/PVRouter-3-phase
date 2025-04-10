
#ifndef FASTDIVISION_H
#define FASTDIVISION_H

#include <Arduino.h>

extern uint16_t divu10(uint16_t n) __attribute__((noinline));  //29 cycles
#define divu8(n) ((uint16_t)((uint16_t)n >> 3))                //These are done as #defines as no improvement can be made.
#define divu4(n) ((uint16_t)((uint16_t)n >> 2))                //These are done as #defines as no improvement can be made.
#define divu2(n) ((uint16_t)((uint16_t)n >> 1))                //These are done as #defines as no improvement can be made.
#define divu1(n) ((uint16_t)((uint16_t)n))                     //These are done as #defines as no improvement can be made.

extern void divmod10(uint32_t in, uint32_t &div, uint8_t &mod) __attribute__((noinline));

#endif /* FASTDIVISION_H */
