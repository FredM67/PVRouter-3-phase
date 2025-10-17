/**
 * @file mult_asm.h
 * @author Based on florentbr's suggestions and avrfreertos optimizations
 * @brief Assembly-optimized multiplication functions for AVR microcontrollers
 * @version 0.1
 * @date 2025-10-09
 *
 * @copyright Copyright (c) 2025
 *
 * This file contains highly optimized assembly multiplication functions for
 * time-critical operations on AVR microcontrollers. These functions eliminate
 * the overhead of GCC's library calls for 16/32-bit multiplications.
 *
 * Based on:
 * - florentbr's optimization suggestions for PVRouter
 * - avrfreertos multiplication optimizations by feilipu
 * - OpenMusicLabs AVR assembly techniques
 */

#ifndef MULT_ASM_H
#define MULT_ASM_H

#include <stdint.h>

/**
 * @brief Optimized 16×16→32 signed multiplication with assembly
 * 
 * This function performs a signed 16-bit × 16-bit multiplication returning
 * a 32-bit result using hand-optimized AVR assembly. It's significantly
 * faster than GCC's library multiplication functions.
 * 
 * @param result Reference to int32_t variable to store the result
 * @param a First 16-bit signed value
 * @param b Second 16-bit signed value  
 * 
 * @note On AVR: ~15-20 cycles vs ~50+ cycles for library calls
 * @note Fallback available for non-AVR platforms
 * @note Based on avrfreertos and OpenMusicLabs techniques
 * @note Function provides type checking and debugging support
 * 
 * @ingroup TimeCritical
 */
static inline __attribute__((always_inline)) void mult16x16_to32(int32_t& result, int16_t a, int16_t b)
{
#ifdef __AVR__
  asm volatile(
    "clr r26                \n\t"  // Clear temporary register
    "mul %A1, %A2           \n\t"  // a_lo * b_lo
    "movw %A0, r0           \n\t"  // Store low 16 bits
    "muls %B1, %B2          \n\t"  // a_hi * b_hi (signed)
    "movw %C0, r0           \n\t"  // Store high 16 bits
    "mulsu %B1, %A2         \n\t"  // a_hi * b_lo (signed*unsigned)
    "sbc %D0, r26           \n\t"  // Handle sign extension
    "add %B0, r0            \n\t"  // Add partial result
    "adc %C0, r1            \n\t"
    "adc %D0, r26           \n\t"
    "mulsu %B2, %A1         \n\t"  // b_hi * a_lo (signed*unsigned)
    "sbc %D0, r26           \n\t"  // Handle sign extension
    "add %B0, r0            \n\t"  // Add partial result
    "adc %C0, r1            \n\t"
    "adc %D0, r26           \n\t"
    "clr r1                 \n\t"  // Restore r1 to zero
    : "=&r"(result)
    : "a"(a), "a"(b)
    : "r26");
#else
  result = static_cast< int32_t >(a) * b;
#endif
}

/**
 * @brief Optimized 16×8→16 signed multiplication with Q8 fractional
 * 
 * This function performs a signed 16-bit × unsigned 8-bit Q8 fractional
 * multiplication with rounding, returning a 16-bit result. The 8-bit value
 * is treated as a fraction (Q8 format: 8 fractional bits).
 * 
 * @param result Reference to int16_t variable to store the result
 * @param value 16-bit signed value
 * @param fraction 8-bit unsigned fraction (Q8 format: 0.0 to 0.996)
 * 
 * @note On AVR: ~9 cycles with built-in rounding
 * @note Q8 format: fraction = 256 * actual_fraction
 * @note Example: fraction=128 represents 0.5, fraction=64 represents 0.25
 * @note Based on avrfreertos and OpenMusicLabs techniques
 * @note Function provides type checking and debugging support
 * 
 * @ingroup TimeCritical
 */
static inline __attribute__((always_inline)) void mult16x8_q8(int16_t& result, int16_t value, uint8_t fraction)
{
#ifdef __AVR__
  asm volatile(
    "mulsu %B[val], %[frac]  \n\t"  // value_hi * fraction (signed*unsigned)
    "movw %A[ret], r0        \n\t"  // Store result
    "mul %A[val], %[frac]    \n\t"  // value_lo * fraction
    "lsl r0                  \n\t"  // Shift for rounding
    "adc %A[ret], r1         \n\t"  // Add with carry (rounding)
    "clr r1                  \n\t"  // Restore r1 to zero
    "adc %B[ret], r1         \n\t"  // Propagate carry
    : [ret] "=&r"(result)
    : [val] "a"(value), [frac] "a"(fraction));
#else
  result = static_cast< int16_t >((static_cast< int32_t >(value) * fraction + 0x80) >> 8);
#endif
}

/**
 * @brief Convert floating-point fraction to Q8 format
 * 
 * Helper function to convert a floating-point fraction (0.0 to 1.0)
 * to the Q8 format used by mult16x8_q8().
 * 
 * @param frac Floating-point fraction (0.0 to 1.0)
 * @return Q8 format value (0 to 255)
 * 
 * @note This is typically used at compile time with constexpr
 * @note Example: float_to_q8(0.5) returns 128
 */
constexpr uint8_t float_to_q8(float frac)
{
  return static_cast< uint8_t >(frac * 256.0f + 0.5f);
}

/**
 * @brief Convert Q8 format back to floating-point
 * 
 * Helper function to convert Q8 format back to floating-point
 * for debugging and testing purposes.
 * 
 * @param q8_val Q8 format value (0 to 255)
 * @return Floating-point fraction (0.0 to ~1.0)
 * 
 * @note Primarily for testing and debugging
 */
constexpr float q8_to_float(uint8_t q8_val)
{
  return static_cast< float >(q8_val) / 256.0f;
}

#endif /* MULT_ASM_H */