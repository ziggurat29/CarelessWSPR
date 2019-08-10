//Alternatives to stdlib functions for improving flash/stack footprint
#ifndef __UTIL_ALTLIB_H
#define __UTIL_ALTLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef COUNTOF
#define COUNTOF(arr) (sizeof(arr)/sizeof(arr[0]))
#endif



//our stdlib has no strrev() so we make our own
char* strrev ( char* str );

//This is a special purpose itoa() that doesn't handle radices, but does do
//leading padding to 'padding' positions.  If padding is insufficient to
//hold the number, the number will be truncated /in the leading digits/, so
//keep that in mind.
//If padding is 0, then the more usual atoi (radix 10) behaviour is performed.
//Also, this returns the /end/ of the string (at the nul), which is more
//useful for progressive concatenation.
char* my_itoa_sortof ( char* psz, long val, int padding );

//We don't really need fancy printf() float support, so we use our own impl
//of a 'ftoa()'.  This avoid the linker flags '-u _printf_float', and saves
//4672 bytes of flash.
char* my_ftoa ( char* psz, float f );


//a specialized atol() for use with my_strtof()
//Note: does not consider long strings that cause overflow.  Know your data.
unsigned long int my_atoul ( const char* str, const char** endptr );
long int my_atol ( const char* str, const char** endptr );


//we don't need a full strtof implementation, and avoiding the one in the
//stdlib saves about 5.5 k that we can put to other use.
//Note:  don't put ridiculous string in, e.g. long mantissas/fractions, as
//these will overflow the int's used in parsing those components.
float my_strtof ( const char* str, const char** endptr );



#ifdef __cplusplus
}
#endif

#endif
