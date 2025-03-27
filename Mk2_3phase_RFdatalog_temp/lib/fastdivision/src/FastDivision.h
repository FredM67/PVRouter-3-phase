
#ifndef FASTDIVISION_H
#define FASTDIVISION_H

#include <Arduino.h>

extern unsigned int divu60(unsigned int n) __attribute__((noinline));                      //31 cycles
extern unsigned int divu15(unsigned int n) __attribute__((noinline));                      //32 cycles
extern unsigned int divu10(unsigned int n) __attribute__((noinline));  //29 cycles
#define divu8(n) (unsigned int)((unsigned int)n >> 3)                                      //These are done as #defines as no improvement can be made.
extern unsigned int divu5(unsigned int n) __attribute__((noinline));                       //32 cycles
#define divu4(n) (unsigned int)((unsigned int)n >> 2)                                      //These are done as #defines as no improvement can be made.
#define divu2(n) (unsigned int)((unsigned int)n >> 1)                                      //These are done as #defines as no improvement can be made.
#define divu1(n) (unsigned int)((unsigned int)n)                                           //These are done as #defines as no improvement can be made.

extern void divmod10(uint32_t in, uint32_t &div, uint8_t &mod) __attribute__((noinline));

#endif /* FASTDIVISION_H */
