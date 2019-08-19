//Alternatives to stdlib functions for improving flash/stack footprint
#include "util_altlib.h"
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>


//our stdlib has no strrev() so we make our own
char* strrev ( char* str )
{
	if ( ! str || ! *str )
		return str;
	char* p1 = str;
	char* p2 = str;
	while ( *p2 )
		++p2;
	--p2;
	while ( p2 > p1 )
	{
		char ch = *p2;
		*p2 = *p1;
		*p1 = ch;
		++p1;
		--p2;
	}
	return str;
}


//This is a special purpose itoa() that doesn't handle radices, but does do
//leading padding to 'padding' positions.  If padding is insufficient to
//hold the number, the number will be truncated /in the leading digits/, so
//keep that in mind.
//If padding is 0, then the more usual atoi (radix 10) behaviour is performed.
//Also, this returns the /end/ of the string (at the nul), which is more
//useful for progressive concatenation.
char* my_itoa_sortof ( char* psz, long val, int padding )
{
	char* pchNow = psz;
	//we build the string backwards, then strrev at the end
	if ( padding < 1 )	//no padding; conventional conversion
	{
		if ( 0 != val )
		{
			int bNegative = 0;
			if ( val < 0 )
			{
				bNegative = 1;
				val = -val;
			}
			while ( 0 != val )
			{
				div_t dv = div ( val, 10 );
				*pchNow++ = '0' + dv.rem;
				val = dv.quot;
			}
			if ( bNegative )
			{
				*pchNow++ = '-';
			}
		}
		else
		{
			//if it was just 0, emit that one char
			*pchNow++ = '0';
		}
	}
	else
	{
		//in the padded case we track how many positions are left
		//YYY can this be factored with the non-padded case?  it's so similar
		if ( 0 != val )
		{
			int bNegative = 0;
			if ( val < 0 )
			{
				bNegative = 1;
				val = -val;
			}
			while ( 0 != val && 0 != padding )
			{
				div_t dv = div ( val, 10 );
				*pchNow++ = '0' + dv.rem;
				--padding;
				val = dv.quot;
			}
			while ( 0 != padding )	//pad any leftovers
			{
				*pchNow++ = '0';
				--padding;
			}
			if ( bNegative )
			{
				*pchNow++ = '-';
			}
		}
		else
		{
			//if it was just 0, emit that one char
			*pchNow++ = '0';
			--padding;
			while ( 0 != padding )	//pad any leftovers
			{
				*pchNow++ = '0';
				--padding;
			}
		}
	}

	*pchNow = '\0';
	strrev ( psz );
	return pchNow;
}


//We don't really need fancy printf() float support, so we use our own impl
//of a 'ftoa()'.  This avoid the linker flags '-u _printf_float', and saves
//4672 bytes of flash.
char* my_ftoa ( char* psz, float f )
{
	char* cp = psz;
	unsigned long l, rem;

	if(f < 0)
	{
		*cp++ = '-';
		f = -f;
	}
	l = (unsigned long)f;
	f -= (float)l;
	rem = (unsigned long)(f * 1e6);
	cp = my_itoa_sortof ( cp, l, 0 );
	*cp++ = '.';
	cp = my_itoa_sortof ( cp, rem, 6 );
	return psz;
}



//a specialized atol() for use with my_strtof()
//Note: does not consider long strings that cause overflow.  Know your data.
unsigned long int my_atoul ( const char* str, const char** endptr )
{
	unsigned long int accum = 0;
	int digits;
	const char* pszNow = str;

	while ( ' ' == *pszNow ) ++pszNow;	//skip spaces
	digits = 0;
	while ( 1 )
	{
		if ( isdigit ( *pszNow ) )	//if digit, accumulate in mantissa
		{
			accum *= 10;
			accum += (*pszNow - '0');
		}
		else	//if other, finished
		{
			break;
		}
		++pszNow;
		++digits;
	}

	//finish up with the results
	if ( 0 == digits )	//failure
	{
		if ( NULL != endptr )	//at the beginning
			*endptr = str;
		return 0UL;
	}
	if ( NULL != endptr )	//at the end
		*endptr = pszNow;
	return accum;
}


long int my_atol ( const char* str, const char** endptr )
{
	int sign = 0;
	const char* pszNow = str;

	//process optional sign
	while ( ' ' == *pszNow ) ++pszNow;	//skip spaces
	if ( '+' == *pszNow )	//leading '+'? just skip ahead
	{
		++pszNow;
	}
	else if ( '-' == *pszNow )	//leading '-'? remember
	{
		sign = 1;
		++pszNow;
	}
	while ( ' ' == *pszNow ) ++pszNow;	//skip spaces

	//process digits
	const char* pszEnd;
	long int accum = (long int)my_atoul ( pszNow, &pszEnd );
	int digits = pszEnd - pszNow;
	pszNow = pszEnd;

	//finish up with the results
	if ( 0 == digits )	//failure
	{
		if ( NULL != endptr )	//at the beginning
			*endptr = str;
		return 0L;
	}
	if ( sign )	//adjust sign
		accum = -accum;
	if ( NULL != endptr )	//at the end
		*endptr = pszNow;
	return accum;
}


//we don't need a full strtof implementation, and avoiding the one in the
//stdlib saves about 5.5 k that we can put to other use.
//Note:  don't put ridiculous string in, e.g. long mantissas/fractions, as
//these will overflow the int's used in parsing those components.
float my_strtof ( const char* str, const char** endptr )
{
	const char* pszNow = str;
	const char* pszEnd;
	//get mantissa, if possible
	long int mant = my_atol ( pszNow, &pszEnd );
	if ( pszEnd == pszNow )	//failuer?
	{
		if ( NULL != endptr )	//at the beginning
			*endptr = str;
		return 0.0F;
	}
	pszNow = pszEnd;
	unsigned long int frac = 0;
	int fraclen = 0;
	//is there a decimal separator for fraction?
	if ( '.' == *pszNow )
	{
		++pszNow;
		//next /must/ be a digit; we cannot tolerate spaces or signs here
		if ( isdigit ( *pszNow ) )
		{
			frac = my_atoul ( pszNow, &pszEnd );
			//(failure is not a problem in this case)
			fraclen = pszEnd - pszNow;
			pszNow = pszEnd;
		}
	}
	float ret = frac;
	while ( 0 != fraclen-- ) ret /= 10.0F;	//shift fraction down as needed
	if ( mant < 0 )
	{
		ret = -ret;
	}
	ret += mant;
	if ( NULL != endptr )	//note endptr
		*endptr = pszNow;
	return ret;
}
