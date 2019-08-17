//Bit fiddling routines of general use.

#ifndef __UTIL_BITFIDDLE_H
#define __UTIL_BITFIDDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>



//this takes a nybble and returns a byte with the bits doubled
extern const uint8_t g_abyDoubleBitsNybble[16];

//this takes a byte, and returns a byte with the bits reversed
extern const uint8_t g_nBitReverseByte[256];

//this will return a 1 if the number of bits of the index are odd (thereby
//making the net result with the returned bit even)
const uint8_t g_abyEvenParityTableByte[256];



#ifdef __cplusplus
}
#endif

#endif
