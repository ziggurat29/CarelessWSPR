//==============================================================
//This provides the WSPR message encoder.
//This is part of the CarelessWSPR project.
#ifndef __WSPR_H
#define __WSPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


//pbyBuffer must be 162 bytes
//achCall is the call sign, and must be six chars max
//achMaiden is the maidenhead locator, and must be four chars
//nPwr is the power level, dbm, and must be 0 to 60, though only values ending
//with 0, 1, 3, and 7 are valid.
int wspr_encode ( uint8_t* pbyBuffer, const char* pszCall, 
		const char* pszMaiden, const uint8_t nPwr );


#ifdef DEBUG
//unit test function against well known values
int wspr_test ( void );
#endif

#ifdef __cplusplus
}
#endif

#endif
