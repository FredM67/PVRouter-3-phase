
#ifndef FASTDIVISION_H
#define FASTDIVISION_H

#include <Arduino.h>

unsigned int divu60(unsigned int n) __attribute__((noinline));  //31 cycles
unsigned int divu15(unsigned int n) __attribute__((noinline));  //32 cycles
unsigned int divu10(unsigned int n) __attribute__((noinline));  //29 cycles
#define divu8(n) (unsigned int)((unsigned int)n >> 3)           //These are done as #defines as no improvement can be made.
unsigned int divu5(unsigned int n) __attribute__((noinline));   //32 cycles
#define divu4(n) (unsigned int)((unsigned int)n >> 2)           //These are done as #defines as no improvement can be made.
#define divu2(n) (unsigned int)((unsigned int)n >> 1)           //These are done as #defines as no improvement can be made.
#define divu1(n) (unsigned int)((unsigned int)n)                //These are done as #defines as no improvement can be made.

void divmod10(uint32_t in, uint32_t &div, uint8_t &mod) __attribute__((noinline));

unsigned int divu5(unsigned int n)
{
  asm volatile(
    "ldi  r25, 0xFF \n\t"
    "cpi  %A0, 0xFF \n\t"
    "cpc  %B0, r25  \n\t"
    "brlo divu5helper \n\t"
    "ldi  r26, 0x01 \n\t"  //final answer++
    "rjmp 1f        \n\t"
    "divu5helper:   \n\t"
    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"
    "ldi  r26, 0x00 \n\t"
    "1:             \n\t"
    "ldi  r24, %1   \n\t"
    "rjmp divuhelper \n\t"

    :
    : "r"(n), "M"(0x33)
    : "r1", "r0", "r24", "r25", "r26", "r27");
  return n;
}

unsigned int divu10(unsigned int n)
{
  asm volatile(
    "divu10helper:  \n\t"
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 5.
    "ror  %A0       \n\t"

    "movw r18, r24  \n\t"
    "rjmp divu5helper \n\t"
    :
    : "r"(n)
    : "r1", "r0", "r18", "r19");
  return n;
}

unsigned int divu15(unsigned int n)
{
  asm volatile(
    "ldi  r25, 0xFF \n\t"
    "cpi  %A0, 0xFF \n\t"
    "cpc  %B0, r25  \n\t"
    "brlo divu15helper \n\t"
    "ldi  r26, 0x01 \n\t"  //final answer++
    "rjmp 1f  \n\t"
    "divu15helper:  \n\t"
    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"
    "ldi  r26, 0x00 \n\t"
    "1:    \n\t"
    "ldi  r24, %2   \n\t"
    "divuhelper:    \n\t"  //division code for all of: 3,5,6,10,12,15,20,24,30,60. Now that's efficiency!
    "ldi  r27, 0x00 \n\t"
    "mul  %A0, r24  \n\t"  //A * X
    "mov  r25,  r1  \n\t"

    "add  r25,  r0  \n\t"  //A* X'
    "adc  r26,  r1  \n\t"

    "mul  %B0, r24  \n\t"
    "add  r25,  r0  \n\t"  //A* X'
    "adc  r26,  r1  \n\t"
    "adc  r27, r27  \n\t"  //D1 is known 0, we need to grab the carry

    "add  r26,  r0  \n\t"  //A* X'
    "adc  r27,  r1  \n\t"

    "movw %A0, r26  \n\t"  // >> 16

    "eor   r1,  r1  \n\t"

    : "=r"(n)
    : "0"(n), "M"(0x11)
    : "r1", "r0", "r24", "r25", "r26", "r27");
  return n;
}

unsigned int divu60(unsigned int n)
{
  asm volatile(
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 30.
    "ror  %A0       \n\t"
    ".global divu30helper     \n\t"
    "divu30helper:            \n\t"
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 15.
    "ror  %A0       \n\t"

    "movw r18, r24  \n\t"
    "rjmp divu15helper \n\t"
    :
    : "r"(n)
    : "r1", "r0", "r18", "r19", "r26", "r27");
  return n;
}

#endif /* FASTDIVISION_H */
