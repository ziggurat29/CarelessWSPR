//==============================================================
//This provides a lat/long to maidenhead coordinate translation facility.

#include "maidenhead.h"
#include <math.h>


//north latitude is positive, south is negative
//east longitude is positive, west is negative
int toMaidenhead ( float lat, float lon, 
		char* achMaidenhead, unsigned int nDesiredLen )
{
	if ( nDesiredLen < 2 || nDesiredLen & 0x01 )	//silly cases
	{
		return 0;
	}
	int maxprec = nDesiredLen / 2;

	//bounds check lon[-180, +180]
	//bounds check lat[-90, +90]
	if (lon < -180.0F || lon > 180.0F)
	{
		return 0;
	}
	if (lat < -90.0F || lat > 90.0F)
	{
		return 0;
	}

	int lonquo, latquo;
	float lonrem, latrem;

	//18 zones of long of 20 deg; 18 zones of lat of 10 deg
	lonquo = (int)((lon + 180.0F)/20.0F);
	lonrem = (float) fmod ( lon + 180.0F, 20.0F );
	latquo = (int)((lat + 90.0F)/10.0F);
	latrem = (float) fmod ( lat + 90.0F, 10.0F );

	char* pchOut = achMaidenhead;

	(*pchOut++) = ('A' + lonquo);
	(*pchOut++) = ('A' + latquo);

	lonrem /= 2.0F;

	int prec = 1;
	while ( prec < maxprec )
	{
		++prec;
		lonquo = (int)(lonrem/1.0F);
		lonrem = (float) fmod ( lonrem,1.0F );
		latquo = (int)(latrem/1.0F);
		latrem = (float) fmod ( latrem,1.0F );
		if (prec & 0x01)
		{
			(*pchOut++) = ('a' + lonquo);
			(*pchOut++) = ('a' + latquo);
			lonrem *= 10.0F;
			latrem *= 10.0F;
		}
		else
		{
			(*pchOut++) = ('0' + lonquo);
			(*pchOut++) = ('0' + latquo);
			lonrem *= 24.0F;
			latrem *= 24.0F;
		}
	}

	(*pchOut) = '\0';

	return 1;
}


