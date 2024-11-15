
#ifndef FASTDIVISION_H
#define FASTDIVISION_H

#include <Arduino.h>

extern unsigned int divu60(unsigned int n) __attribute__((noinline));                      //31 cycles
extern unsigned int divu30(unsigned int n) asm("divu30helper") __attribute__((noinline));  //29 cycles
extern unsigned int divu24(unsigned int n) __attribute__((noinline));                      //33 cycles
extern unsigned int divu20(unsigned int n) __attribute__((noinline));                      //31 cycles
extern unsigned int divu15(unsigned int n) __attribute__((noinline));                      //32 cycles
extern unsigned int divu14(unsigned int n) __attribute__((noinline));                      //41 cycles
//extern unsigned int divu13(unsigned int n) __attribute__((noinline));  //Can't make this one work, sorry :(
extern unsigned int divu12(unsigned int n) asm("divu12helper") __attribute__((noinline));  //31 cycles
extern unsigned int divu11(unsigned int n) __attribute__((noinline));                      //41 cycles
extern unsigned int divu10(unsigned int n) asm("divu10helper") __attribute__((noinline));  //29 cycles
extern unsigned int divu9(unsigned int n) __attribute__((noinline));                       //41 cycles
#define divu8(n) (unsigned int)((unsigned int)n >> 3)                                      //These are done as #defines as no improvement can be made.
extern unsigned int divu7(unsigned int n) __attribute__((noinline));                       //41 cycles
extern unsigned int divu6(unsigned int n) asm("divu6helper") __attribute__((noinline));    //29 cycles
extern unsigned int divu5(unsigned int n) __attribute__((noinline));                       //32 cycles
#define divu4(n) (unsigned int)((unsigned int)n >> 2)                                      //These are done as #defines as no improvement can be made.
extern unsigned int divu3(unsigned int n) __attribute__((noinline));                       //32 cycles
#define divu2(n) (unsigned int)((unsigned int)n >> 1)                                      //These are done as #defines as no improvement can be made.
#define divu1(n) (unsigned int)((unsigned int)n)                                           //These are done as #defines as no improvement can be made.

extern void divmod10(uint32_t in, uint32_t &div, uint8_t &mod) __attribute__((noinline));

#endif /* FASTDIVISION_H */
