//==============================================================
//This provides a lat/long to maidenhead coordinate translation facility.

#ifndef __MAIDENHEAD_H
#define __MAIDENHEAD_H

#ifdef __cplusplus
extern "C" {
#endif


//north latitude is positive, south is negative
//east longitude is positive, west is negative
//the desired length is an arbitrary EVEN number, but achMaidenhead is assumed
//to be nDesiredLen + 1 for the nul terminator.
//returns 0 on failure (e.g. lat lon out-of-bounds)
int toMaidenhead ( float lat, float lon, char* achMaidenhead, unsigned int nDesiredLen );


#ifdef __cplusplus
}
#endif

#endif
