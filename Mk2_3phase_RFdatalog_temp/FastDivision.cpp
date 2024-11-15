#include "FastDivision.h"

unsigned int divu10(unsigned int n)
{
  unsigned long working;
  asm volatile(
    "ldi  %A1, %3   \n\t"
    "ldi  %C1, 0x00 \n\t"
    "ldi  %D1, 0x00 \n\t"

    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 5.
    "ror  %A0       \n\t"

    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"

    "mul  %A0, %A1  \n\t"  //A * X
    "mov  %B1,  r1  \n\t"

    "add  %B1,  r0  \n\t"  //A* X'
    "adc  %C1,  r1  \n\t"

    "mul  %B0, %A1  \n\t"
    "add  %B1,  r0  \n\t"  //A* X'
    "adc  %C1,  r1  \n\t"
    "adc  %D1, %D1  \n\t"  //D1 is known 0, we need to grab the carry

    "add  %C1,  r0  \n\t"  //A* X'
    "adc  %D1,  r1  \n\t"

    "movw %A0, %C1  \n\t"  // >> 16

    "eor   r1,  r1  \n\t"

    : "=r"(n), "=r"(working)
    : "0"(n), "M"(0x33)
    : "r1", "r0");
  return n;
}

unsigned int divu7(unsigned int n)
{
  unsigned long working;
  asm volatile(
    "ldi  %A1, %3   \n\t"
    "ldi  %B1, 0xFF \n\t"
    "ldi  %D1, 0x00 \n\t"
    "cpi  %A0, 0xFE \n\t"
    "cpc  %B0, %B1  \n\t"
    "brlo 1f        \n\t"
    "ldi  %C1, 0x01 \n\t"  //final answer++
    "rjmp 2f        \n\t"
    "1:             \n\t"
    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"
    "ldi  %C1, 0x00 \n\t"
    "2:             \n\t"

    "mul  %A0, %A1  \n\t"  //A * X
    "mov  %B1,  r1  \n\t"

    "mul  %B0, %A1  \n\t"
    "add  %B1,  r0  \n\t"  //B * X
    "adc  %C1,  r1  \n\t"

    "ldi  %A1, %4   \n\t"
    "mul  %A0, %A1  \n\t"  //A * X'
    "add  %B1,  r0  \n\t"
    "adc  %C1,  r1  \n\t"
    "adc  %D1, %D1  \n\t"  //D1 is known 0, we need to grab the carry

    "mul  %B0, %A1  \n\t"
    "add  %C1,  r0  \n\t"  //B* X'
    "adc  %D1,  r1  \n\t"

    "movw %A0, %C1  \n\t"  // >> 16

    "eor   r1,  r1  \n\t"

    "lsr  %B0       \n\t"  // we have divided by 1.75, now divide by 4.
    "ror  %A0       \n\t"
    "lsr  %B0       \n\t"
    "ror  %A0       \n\t"

    : "=r"(n), "=r"(working)
    : "0"(n), "M"(0x49), "M"(0x92)
    : "r1", "r0");
  return n;
}

unsigned int divu9(unsigned int n)
{
  unsigned long working;
  asm volatile(
    "ldi  %A1, %3   \n\t"
    "ldi  %B1, 0xFF \n\t"
    "ldi  %D1, 0x00 \n\t"
    "cpi  %A0, 0xFF \n\t"
    "cpc  %B0, %B1  \n\t"
    "brlo 1f        \n\t"
    "ldi  %C1, 0x01 \n\t"  //final answer++
    "rjmp 2f        \n\t"
    "1:             \n\t"
    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"
    "ldi  %C1, 0x00 \n\t"
    "2:             \n\t"

    "mul  %A0, %A1  \n\t"  //A * X
    "mov  %B1,  r1  \n\t"

    "mul  %B0, %A1  \n\t"
    "add  %B1,  r0  \n\t"  //B * X
    "adc  %C1,  r1  \n\t"

    "ldi  %A1, %4   \n\t"
    "mul  %A0, %A1  \n\t"  //A * X'
    "add  %B1,  r0  \n\t"
    "adc  %C1,  r1  \n\t"
    "adc  %D1, %D1  \n\t"  //D1 is known 0, we need to grab the carry

    "mul  %B0, %A1  \n\t"
    "add  %C1,  r0  \n\t"  //B* X'
    "adc  %D1,  r1  \n\t"

    "movw %A0, %C1  \n\t"  // >> 16

    "eor   r1,  r1  \n\t"

    "lsr  %B0       \n\t"  // we have divided by 2.25, now divide by 4.
    "ror  %A0       \n\t"
    "lsr  %B0       \n\t"
    "ror  %A0       \n\t"

    : "=r"(n), "=r"(working)
    : "0"(n), "M"(0xC7), "M"(0x71)
    : "r1", "r0");
  return n;
}

unsigned int divu11(unsigned int n)
{
  unsigned long working;
  asm volatile(
    "ldi  %A1, %3   \n\t"
    "ldi  %B1, 0xFF \n\t"
    "ldi  %D1, 0x00 \n\t"
    "cpi  %A0, 0xFF \n\t"
    "cpc  %B0, %B1  \n\t"
    "brlo 1f        \n\t"
    "ldi  %C1, 0x01 \n\t"  //final answer++
    "rjmp 2f        \n\t"
    "1:             \n\t"
    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"
    "ldi  %C1, 0x00 \n\t"
    "2:             \n\t"

    "mul  %A0, %A1  \n\t"  //A * X
    "mov  %B1,  r1  \n\t"

    "mul  %B0, %A1  \n\t"
    "add  %B1,  r0  \n\t"  //B * X
    "adc  %C1,  r1  \n\t"

    "ldi  %A1, %4   \n\t"
    "mul  %A0, %A1  \n\t"  //A * X'
    "add  %B1,  r0  \n\t"
    "adc  %C1,  r1  \n\t"
    "adc  %D1, %D1  \n\t"  //D1 is known 0, we need to grab the carry

    "mul  %B0, %A1  \n\t"
    "add  %C1,  r0  \n\t"  //B* X'
    "adc  %D1,  r1  \n\t"

    "movw %A0, %C1  \n\t"  // >> 16

    "eor   r1,  r1  \n\t"

    "lsr  %B0       \n\t"  // we have divided by 2.75, now divide by 4.
    "ror  %A0       \n\t"
    "lsr  %B0       \n\t"
    "ror  %A0       \n\t"

    : "=r"(n), "=r"(working)
    : "0"(n), "M"(0x17), "M"(0x5D)
    : "r1", "r0");
  return n;
}

unsigned int divu14(unsigned int n)
{
  unsigned long working;
  asm volatile(
    "ldi  %A1, %3   \n\t"
    "ldi  %B1, 0xFF \n\t"
    "ldi  %D1, 0x00 \n\t"

    "lsr  %B0       \n\t"
    "ror  %A0       \n\t"  // we have divided by 2, now divide by 7.

    "cpi  %A0, 0xFE \n\t"
    "cpc  %B0, %B1  \n\t"
    "brlo 1f        \n\t"
    "ldi  %C1, 0x01 \n\t"  //final answer++
    "rjmp 2f        \n\t"
    "1:             \n\t"
    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"
    "ldi  %C1, 0x00 \n\t"
    "2:             \n\t"

    "mul  %A0, %A1  \n\t"  //A * X
    "mov  %B1,  r1  \n\t"

    "mul  %B0, %A1  \n\t"
    "add  %B1,  r0  \n\t"  //B * X
    "adc  %C1,  r1  \n\t"

    "ldi  %A1, %4   \n\t"
    "mul  %A0, %A1  \n\t"  //A * X'
    "add  %B1,  r0  \n\t"
    "adc  %C1,  r1  \n\t"
    "adc  %D1, %D1  \n\t"  //D1 is known 0, we need to grab the carry

    "mul  %B0, %A1  \n\t"
    "add  %C1,  r0  \n\t"  //B* X'
    "adc  %D1,  r1  \n\t"

    "movw %A0, %C1  \n\t"  // >> 16

    "eor   r1,  r1  \n\t"

    "lsr  %B0       \n\t"  // we have divided by 7, now divide by 2.
    "ror  %A0       \n\t"

    : "=r"(n), "=r"(working)
    : "0"(n), "M"(0x24), "M"(0x49)
    : "r1", "r0");
  return n;
}

unsigned int divu24(unsigned int n)
{
  //unsigned long working;
  asm volatile(
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 12.
    "ror  %A0       \n\t"
    ".global divu12helper \n\t"
    "divu12helper:  \n\t"
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 6.
    "ror  %A0       \n\t"
    ".global divu6helper \n\t"
    "divu6helper:   \n\t"
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 3.
    "ror  %A0       \n\t"

    "movw r18, r24  \n\t"
    "rjmp divu3helper \n\t"
    :
    : "r"(n)
    : "r1", "r0", "r18", "r19");
  return n;
}
unsigned int divu3(unsigned int n)
{
  //unsigned long working;
  asm volatile(
    "ldi  r25, 0xFF \n\t"
    "cpi  %A0, 0xFF \n\t"
    "cpc  %B0, r25  \n\t"
    "brlo divu3helper \n\t"
    "ldi  r26, 0x01 \n\t"  //final answer++
    "rjmp 1f        \n\t"
    "divu3helper:   \n\t"
    "subi %A0, 0xFF \n\t"  //n++
    "sbci %B0, 0xFF \n\t"
    "ldi  r26, 0x00 \n\t"
    "1:             \n\t"
    "ldi  r24, %1   \n\t"
    "rjmp divuhelper \n\t"

    :
    : "r"(n), "M"(0x55)
    : "r1", "r0", "r24", "r25", "r26", "r27");
  return n;
}

unsigned int divu20(unsigned int n)
{
  //unsigned long working;
  asm volatile(
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 10.
    "ror  %A0       \n\t"
    ".global divu10helper     \n\t"
    "divu10helper:            \n\t"
    "lsr  %B0       \n\t"  // we have divided by 2, now divide by 5.
    "ror  %A0       \n\t"

    "movw r18, r24  \n\t"
    "rjmp divu5helper \n\t"
    :
    : "r"(n)
    : "r1", "r0", "r18", "r19");
  return n;
}
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

void divmod10(uint32_t in, uint32_t &div, uint8_t &mod)
{
  //assumes that div/mod pointers arrive in r18:r19 and r20:r21 pairs (doesn't matter which way around)
  //and that in arrives in r22:r25 quad
  asm volatile(
    "movw r30, %2   \n\t"  //uint32_t* divPtr = &div;
    "movw r26, %1   \n\t"  //uint32_t* modPtr = &mod;

    "mov   r0, %A0  \n\t"  //byte temp = in
    "movw r18, %A0  \n\t"  //uint32_t q = in;
    "movw r20, %C0  \n\t"
    "ori  r18, 0x01 \n\t"  //q |= 1;

    "lsr  r25       \n\t"  //x = in >> 2 //note: x reuses registers of 'in', as 'in' was backed up in r0
    "ror  r24       \n\t"
    "ror  r23       \n\t"
    "ror  r22       \n\t"
    "lsr  r25       \n\t"
    "ror  r24       \n\t"
    "ror  r23       \n\t"
    "ror  r22       \n\t"

    "sub  r18, r22  \n\t"  //q = q - x;
    "sbc  r19, r23  \n\t"
    "sbc  r20, r24  \n\t"
    "sbc  r21, r25  \n\t"

    "movw r22, r18  \n\t"  //x = q;
    "movw r24, r20  \n\t"
    "lsr  r25       \n\t"  //x = x >> 4;
    "ror  r24       \n\t"
    "ror  r23       \n\t"
    "ror  r22       \n\t"
    "lsr  r25       \n\t"
    "ror  r24       \n\t"
    "ror  r23       \n\t"
    "ror  r22       \n\t"
    "lsr  r25       \n\t"
    "ror  r24       \n\t"
    "ror  r23       \n\t"
    "ror  r22       \n\t"
    "lsr  r25       \n\t"
    "ror  r24       \n\t"
    "ror  r23       \n\t"
    "ror  r22       \n\t"

    "add  r22, r18  \n\t"  //x = x + q
    "adc  r23, r19  \n\t"
    "adc  r24, r20  \n\t"
    "adc  r25, r21  \n\t"

    "movw r18, r22  \n\t"  //q = x
    "movw r20, r24  \n\t"
    "add  r18, r23  \n\t"  //q = q + (x >> 8)
    "adc  r19, r24  \n\t"
    "adc  r20, r25  \n\t"
    "adc  r21, r1   \n\t"

    "movw r18, r20  \n\t"  //q = q >> 16
    "eor  r20, r20  \n\t"
    "eor  r21, r21  \n\t"
    "add  r18, r23  \n\t"  //q = q + (x>>8)
    "adc  r19, r24  \n\t"
    "adc  r20, r25  \n\t"
    "adc  r21, r1   \n\t"
    "add  r18, r22  \n\t"  //q = q + x
    "adc  r19, r23  \n\t"
    "adc  r20, r24  \n\t"
    "adc  r21, r25  \n\t"

    "mov  r18, r19  \n\t"  //q = q >> 8
    "mov  r19, r20  \n\t"
    "mov  r20, r21  \n\t"
    "eor  r21, r21  \n\t"
    "add  r18, r22  \n\t"  //q = q + x
    "adc  r19, r23  \n\t"
    "adc  r20, r24  \n\t"
    "adc  r21, r25  \n\t"

    "andi r18, 0xF8 \n\t"  //q = q & ~0x7

    "sub   r0, r18  \n\t"  //in = in - q

    "lsr  r21       \n\t"  //q = q >> 2
    "ror  r20       \n\t"
    "ror  r19       \n\t"
    "ror  r18       \n\t"
    "lsr  r21       \n\t"
    "ror  r20       \n\t"
    "ror  r19       \n\t"
    "ror  r18       \n\t"

    "sub  r0, r18   \n\t"  //in = in - q
    "st    X, r0    \n\t"  //mod = in;

    "lsr  r21       \n\t"  //q = q >> 1
    "ror  r20       \n\t"
    "ror  r19       \n\t"
    "ror  r18       \n\t"

    "st	    Z, r18  \n\t"  //div = q
    "std  Z+1, r19  \n\t"
    "std  Z+2, r20  \n\t"
    "std  Z+3, r21  \n\t"

    :
    : "r"(in), "r"(&mod), "r"(&div)
    : "r0", "r26", "r27", "r31", "r31");
}
