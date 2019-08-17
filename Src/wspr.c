//==============================================================
//This provides the WSPR codec.
//This is part of the CarelessWSPR project.

//Details of the encoding process can be found in
//WSPR_Coding_Process.pdf
//Andy Talbot G4JNT June 2009

#include "wspr.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "util_bitfiddle.h"




//condition the inputs for WSPR.
int wspr_condition ( char* pszCall, char* pszLoc, uint8_t* pPwr )
{
	//the call sign must have the 3rd char be the digit, padding if needed
	if ( isdigit ( pszCall[1] ) )
	{
		memmove ( &pszCall[1], pszCall, 5 );
		pszCall[0] = ' ';
	}
	int nIdx;
	for ( nIdx = 0; nIdx < 6; ++nIdx )	//ensure chars are upper
	{
		pszCall[nIdx] = toupper(pszCall[nIdx]);
		if ( '\0' == pszCall[nIdx] )	//early term?
		{
			break;
		}
	}
	for ( ; nIdx < 6; ++nIdx )	//right-pad if needed
	{
		pszCall[nIdx] = ' ';
	}

	//the location must have letter values in range A-R
	for ( nIdx = 0; nIdx < 4; ++nIdx )
	{
		char ch = toupper(pszLoc[nIdx]);	//ensure upper if needed
		pszLoc[nIdx] = ch;
		if ( nIdx & 0x02 )	//digit validation
		{
			if ( ! isdigit ( ch ) )
				return 0;	//horror
		}
		else	//letter validation
		{
			if ( ch < 'A' || ch > 'R' )
				return 0;	//horror
		}
	}

	//the power can only take on certain values
	if ( *pPwr > 60 )
	{
		*pPwr = 60;
	}
	uint8_t nTens = *pPwr / 10;
	uint8_t nOnes = *pPwr % 10;
	//ones of 0, 3, 7 slip through; others round down
	switch ( nOnes )
	{
		case 1:
		case 2:
			*pPwr = nTens * 10 + 0;
		break;

		case 4:
		case 5:
		case 6:
			*pPwr = nTens * 10 + 3;
		break;

		case 8:
		case 9:
			*pPwr = nTens * 10 + 7;
		break;
	}
	return 1;
}



//base-37 encoding: 0-9,A-Z,space
uint8_t base37_encode ( uint8_t in )
{
	if ( isdigit ( in ) )
	{
		return in - '0';
	}
	else if ( isalpha ( in ) )
	{
		return toupper ( in ) - 'A' + 10;
	}
	else if ( ' ' == in )
	{
		return 36;
	}
	else
	{
		//horror
		return 0xff;
	}
}



//WSPR_Coding_Process.pdf; p. 1,2
//Source Coding
//Bit Packing


void wspr_pack ( uint8_t* packed, const char* pszCall, const char* pszLoc, uint8_t nPwr )
{
	uint32_t N, M;	//the infamous M and N registers...

	//these computations are textbook, as per WSPR_Coding_Process.pdf
	N = base37_encode(pszCall[0]);
	N = N * 36 + base37_encode(pszCall[1]);
	N = N * 10 + base37_encode(pszCall[2]);
	N = N * 27 + (base37_encode(pszCall[3]) - 10);
	N = N * 27 + (base37_encode(pszCall[4]) - 10);
	N = N * 27 + (base37_encode(pszCall[5]) - 10);

	M = ((179 - 10 * (pszLoc[0] - 'A') - (pszLoc[2] - '0')) * 180) +
		(10 * (pszLoc[1] - 'A')) + (pszLoc[3] - '0');
	M = (M * 128) + nPwr + 64;

	/*
	The two integers N and M are truncated and combined so the 50 bits sit end-
	to-end as callsign-locator-power.  These are placed into an array of eleven
	8-bit bytes c[0] to c[10], so the first element c[0] contains the most 
	significant 8 bits part of the callsign, c[1] the next 8 bits and so on.
	(Note that c[3] contains both the 4 LSBs of the callsign and 4 MSBs of the
	locator, and that c[6] contains just the two LSBs of M occupying the most
	significant bit positions.  The lowest six bits of c[6] are set to 0, with
	the remaining bytearray elements [c7] to c[10] set to zero.)  Only the
	left-most 81 of these 88 total bits are subsequently used.
	*/
	packed[3] = (uint8_t)((N & 0x0f) << 4);
	N >>= 4;
	packed[2] = (uint8_t)(N & 0xff);
	N >>= 8;
	packed[1] = (uint8_t)(N & 0xff);
	N >>= 8;
	packed[0] = (uint8_t)(N & 0xff);

	packed[6] = (uint8_t)((M & 0x03) << 6);
	M = M >> 2;
	packed[5] = (uint8_t)(M & 0xff);
	M = M >> 8;
	packed[4] = (uint8_t)(M & 0xff);
	M = M >> 8;
	packed[3] |= (uint8_t)(M & 0x0f);

	packed[7] = 0;
	packed[8] = 0;
	packed[9] = 0;
	packed[10] = 0;
}



//WSPR_Coding_Process.pdf; p. 3
//Convolutional Encoding
//
//The data is now expanded to add FEC with a rate ½, constraint length 32, 
//convolutional encoder.
//
//The 81 bits (including the 31 trailing zeros) are read out MSB first in the 
//order:
//    c[0] MSB… c[0] LSB., c[1] MSB…c[1] LSB ………… c[11]
//(or adopting the alternative view, one-at-a-time from the left hand end of 
//the string)
//
//The bits are clocked simultaneously into the right hand side, or least 
//significant position, of two 32 bit shift registers [Reg 0] and [Reg 1]. 
//Each shift register feeds an ExclusiveOR parity generator from feedback taps 
//described respectively by the 32 bit values 0xF2D05351 and 0xE4613C47.  
//Parity generation starts immediately the first bit appears in the registers 
//(which must be initially cleared) and continues until the registers are 
//flushed by the final 31st zero being clocked into them.
//
//Each of the 81 bits shifted in generates a parity bit from each of the 
//generators, a total of 162 bits in all.  For each bit shifted in, the 
//resulting two parity bits are taken in turn, in the order the two feedback 
//tap positions values are given, to give a stream of 162 output bits.



void wspr_convencode ( uint8_t* convolved, const uint8_t* packed )
{
	uint32_t Reg0, Reg1;	//the infamous Reg0 and Reg1 registers...

	Reg0 = Reg1 = 0;
	unsigned int nIdxOutBit = 0;
	//for each input byte (we won't use all the bits, though)
	for ( unsigned int nIdxInByte = 0; nIdxInByte < 11; nIdxInByte++)
	{
		//for each bit in that byte
		for ( unsigned int nIdxInBit = 0; nIdxInBit < 8; ++nIdxInBit )
		{
			//feed MSB first into LSB of the two stream registers
			Reg0 <<= 1;
			Reg1 <<= 1;
			if ( (packed[nIdxInByte] << nIdxInBit) & 0x80)	//shifting in a 1?
			{
				Reg0 |= 1;
				Reg1 |= 1;
			}

			//Layland-Lushbaugh
			//do Reg0 parity computation; this is essentially the xor of
			//specific bits (as per generator polynomial), and this parity bit
			//gets emitted to the output data.
			//We have a pre-computed parity table that makes this a little
			//easier.
			//Also, we are going to set the value to 2 or 0 (rather than 1 or
			//0) because we would otherwise be shifting it up later, anyway.
			uint32_t conv;
			unsigned int parity;
			conv = Reg0 & 0xf2d05351;
			parity = 
					g_abyEvenParityTableByte[((uint8_t*)&conv)[0]] ^ 
					g_abyEvenParityTableByte[((uint8_t*)&conv)[1]] ^
					g_abyEvenParityTableByte[((uint8_t*)&conv)[2]] ^
					g_abyEvenParityTableByte[((uint8_t*)&conv)[3]];
			convolved[nIdxOutBit] = parity ? 2 : 0;
			nIdxOutBit++;

			//do Reg1 parity computation; this is the same thing, just with a
			//different polynomial.
			conv = Reg1 & 0xe4613c47;
			parity = 
					g_abyEvenParityTableByte[((uint8_t*)&conv)[0]] ^ 
					g_abyEvenParityTableByte[((uint8_t*)&conv)[1]] ^
					g_abyEvenParityTableByte[((uint8_t*)&conv)[2]] ^
					g_abyEvenParityTableByte[((uint8_t*)&conv)[3]];
			convolved[nIdxOutBit] = parity ? 2 : 0;
			nIdxOutBit++;

			if ( nIdxOutBit >= 162 )	//we are done (will be sooner than all bits in packed)
			{
				break;
			}
		}
	}
}



//WSPR_Coding_Process.pdf; p. 4
//Interleaving
//The interleaving process is performed by taking the block of 162 starting 
//bits labelled S[0] to S[161] and using a bit reversal of the address to 
//reorder them, to give a pattern of destination bits referred to as 
//D[0] to D[161].



void wspr_interleave ( uint8_t* scrambled, const uint8_t* convolved )
{
	int nIdxOutBit = 0;
	//try all possible indices
	for ( unsigned int nIndex = 0; nIndex < 255; ++nIndex )
	{
		//bit-reverse this candidate index; we have a convenient table
		unsigned int nIndexReversed = g_nBitReverseByte[nIndex];

		//if the bit-reversal refers to a location that is not in the input
		//data, we simply skip it.
		if ( nIndexReversed >= 162 )
		{
			continue;
		}
		//otherwise we transfer it to the bit-reversed position
		scrambled[nIndexReversed] = convolved[nIdxOutBit];
		++nIdxOutBit;

		if ( nIdxOutBit >= 162 )	//see if we are done
		{
			break;
		}
	}
}



//WSPR_Coding_Process.pdf; p. 4
//Merge With Sync Vector
//The 162 bits of data are now merged with 162 bits of a pseudo random 
//synchronisation word having good autocorrelation properties.  Each source 
//bit is combined with a sync bit taken in turn from the table below to give a 
//four-state symbol value:
//    Symbol[n] = Sync[n] + 2 * Data[n]
const uint8_t sync[162] =
{
	1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 
	0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 
	1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 
	0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 
	0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 
	1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 
	0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 
	0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 
	0, 0 
};



void wspr_merge_sync ( uint8_t* scrambled )
{
	//we were careful in wspr_convencode to have pre-shifted the data, so we
	//can simply xor (or add) here
	for ( unsigned int nIdx = 0; nIdx < 162; ++nIdx )
	{
		scrambled[nIdx] ^= sync[nIdx];
	}
}



//pbyBuffer must be 162 bytes
//achCall is the call sign, and must be six chars max
//achMaiden is the maidenhead locator, and must be four chars
//nPwr is the power level, dbm, and must be 0 to 60
int wspr_encode ( uint8_t* pbyBuffer, const char* pszCall, 
		const char* pszMaiden, const uint8_t nPwr )
{
	//condition the input parameters
	char call_cond[7];
	char loc_cond[5];
	uint8_t pwr_cond = nPwr;
	strncpy ( call_cond, pszCall, 6 );
	call_cond[6] = '\0';
	strncpy ( loc_cond, pszMaiden, 4 );
	loc_cond[4] = '\0';
	if ( ! wspr_condition ( call_cond, loc_cond, &pwr_cond ) )
		return 0;	//horror

	//do the bit-packing step
	uint8_t* packed = malloc ( 11 );	//avoid excessive stack
	wspr_pack ( packed, call_cond, loc_cond, pwr_cond );

	//do the convolutional encoding step
	uint8_t* convolved = malloc ( 162 );	//avoid excessive stack
	wspr_convencode ( convolved, packed );
	free ( packed );

	//scramble the data via bit-reversed addressing
	wspr_interleave ( pbyBuffer, convolved );
	free ( convolved );

	//merge in the sync data
	wspr_merge_sync ( pbyBuffer );

	return 1;
}



//test vector created from 'official' wspr.exe program thusly:
//wspr.exe Tx 10.1386 10.140100 0 K1JT FN20 30 11
const uint8_t wspr_test_K1JT_FN20_30[162] =
{
	3, 3, 2, 0, 2, 0, 2, 2, 1, 2, 2, 2, 3, 3, 1, 0, 
	2, 2, 3, 2, 0, 3, 0, 1, 1, 3, 1, 2, 0, 2, 2, 0, 
	0, 2, 1, 2, 0, 1, 2, 3, 2, 2, 2, 0, 0, 0, 3, 2, 
	1, 3, 2, 0, 3, 3, 0, 3, 0, 0, 0, 1, 1, 0, 3, 0, 
	2, 0, 0, 3, 3, 0, 3, 2, 3, 0, 1, 0, 1, 0, 0, 3, 
	2, 2, 1, 0, 1, 1, 0, 2, 0, 1, 1, 2, 3, 0, 3, 2, 
	2, 2, 3, 2, 2, 2, 2, 2, 1, 0, 0, 1, 2, 0, 3, 1, 
	1, 2, 1, 3, 2, 0, 1, 1, 2, 1, 2, 0, 2, 1, 1, 1, 
	2, 0, 2, 2, 0, 3, 2, 3, 2, 0, 3, 1, 0, 0, 2, 2, 
	2, 2, 0, 3, 3, 2, 1, 2, 3, 1, 0, 2, 2, 1, 3, 2, 
	2, 2,
};


int wspr_test ( void )
{
	uint8_t* wspr = malloc ( 162 );
	wspr_encode ( wspr, "K1JT", "FN20", 30 );
	int nRet = memcmp ( wspr, wspr_test_K1JT_FN20_30, 162 );
	free ( wspr );
	return 0 == nRet;
}


