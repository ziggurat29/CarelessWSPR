//the task that handles incoming GPS NMEA data
//This is part of the BluepillSi5351Exp001 project.


#include "task_gps.h"
#include "task_notification_bits.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lamps.h"
#include "util_altlib.h"
#include "stm32f1xx_hal.h"

#ifndef COUNTOF
#define COUNTOF(arr) (sizeof(arr)/sizeof(arr[0]))
#endif


//the task that runs an interactive monitor on the USB data
osThreadId g_thGPS = NULL;
uint32_t g_tbGPS[ 128 ];
osStaticThreadDef_t g_tcbGPS;


const IOStreamIF* g_pGPSIOIf = NULL;	//the IO device to which the gps is attached


volatile int g_bLock = 0;

volatile int g_nGPSHour;
volatile int g_nGPSMinute;
volatile int g_nGPSSecond;
volatile int g_nGPSMonth;
volatile int g_nGPSDay;
volatile int g_nGPSYear;

volatile float g_fLat;	//+ is N, - is S
volatile float g_fLon;	//+ is E, - is W



//====================================================
//GPS task
//The GPS sends NMEA sentences over the serial port.  We interpret them and
//store interesting information in variables.



//sentence buffer
char g_achNMEA0183Sentence[82];	//abs max len is 82


static char _gpsGetChar ( const IOStreamIF* pio )
{
	char ret;
	pio->_receiveCompletely ( pio, &ret, 1, TO_INFINITY );
	return ret;
}


//this gets characters from the input stream until line termination occurs.
static void _getSentence ( const IOStreamIF* pio )
{
	int nIdxSentence;

	int bCont = 1;

	//pull characters into sentence buffer until full or line terminated
	nIdxSentence = 0;
	while ( bCont && nIdxSentence < COUNTOF(g_achNMEA0183Sentence) )
	{
		char chNow = _gpsGetChar ( pio );
		switch ( chNow )
		{
		case '\r':	//CR is a line terminator
		case '\n':	//LF is a line terminator
			memset ( &g_achNMEA0183Sentence[nIdxSentence], '\0', COUNTOF(g_achNMEA0183Sentence) - nIdxSentence );	//clear rest of buffer
			++nIdxSentence;
			bCont = 0;
		break;

		default:
			//everything else simply accumulates the character
			g_achNMEA0183Sentence[nIdxSentence] = chNow;
			++nIdxSentence;
		break;
		}
	}
	g_achNMEA0183Sentence[COUNTOF(g_achNMEA0183Sentence)-1] = '\0';	//just for safety
}



/*
http://aprs.gids.nl/nmea

$GPRMC
Recommended minimum specific GPS/Transit data

eg2. $GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68

           225446       Time of fix 22:54:46 UTC
           A            Navigation receiver warning A = OK, V = warning
           4916.45,N    Latitude 49 deg. 16.45 min North
           12311.12,W   Longitude 123 deg. 11.12 min West
           000.5        Speed over ground, Knots
           054.7        Course Made Good, True
           191194       Date of fix  19 November 1994
           020.3,E      Magnetic variation 20.3 deg East
           *68          mandatory checksum
*/


typedef enum GPSProcRetval GPSProcRetval;
enum GPSProcRetval
{
	GPSPROC_SUCCESS = 0,	//a command was dispatched successfully
	GPSPROC_ERROR = 1,		//a command was received, but there were problems
	GPSPROC_QUIT = 2,		//it is time to exit the command processor
};


//Advance the pointer to the next string in the message.  Note, no error
//checking for beyond end of string, so this is suitable only for internal
//use.
static void nextStr ( char** ppszThisStr )
{
	while ( '\0' != **ppszThisStr )
	{
		++(*ppszThisStr);
	}
	++(*ppszThisStr);
}



GPSProcRetval GPS_process ( const IOStreamIF* pio )
{
	GPSProcRetval retval;

	//get a tokenized series of strings
	do
	{
		_getSentence ( pio );	//get the command line from the IO stream
	} while ( '\0' == g_achNMEA0183Sentence[0] );	//skip the empties
	retval = GPSPROC_SUCCESS;

//_ledToggleGn();

	//The NMEA data is basically CSV, we can do a simple strchr/replace of all
	//the commas with nul, then treat it as a string sequence.  We will have to
	//conscientiously not run off the end of buffer.
	char* pszThisStr = g_achNMEA0183Sentence;
	for ( ; (pszThisStr = strchr(pszThisStr, ',')); ++pszThisStr )
	{
		*pszThisStr = '\0';
	}
	pszThisStr = g_achNMEA0183Sentence;
	//we parse only a couple well-known ones that we care about
	if ( 0 == strcmp ( pszThisStr, "$GPRMC" ) )
	{
		//$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
		//4807.038,N   Latitude 48 deg 07.038' N
		//01131.000,E  Longitude 11 deg 31.000' E
		int bLockedStateChanged = 0;
		char* pchPrevStr = pszThisStr;
		nextStr ( &pszThisStr );
		int nLen = pszThisStr - pchPrevStr;
		if ( nLen > 4 )	//must have enough stuff
		{
			char szHour[3], szMinute[3], * pszSecond;
			szHour[0] = pszThisStr[0];
			szHour[1] = pszThisStr[1];
			szHour[2] = '\0';
			szMinute[0] = pszThisStr[2];
			szMinute[1] = pszThisStr[3];
			szMinute[2] = '\0';
			pszSecond = &pszThisStr[4];

			nextStr ( &pszThisStr );
			char* pszStatus = pszThisStr;
			nextStr ( &pszThisStr );
			char* pszLat = pszThisStr;
			nextStr ( &pszThisStr );
			char* pszLatHemi = pszThisStr;
			nextStr ( &pszThisStr );
			char* pszLon = pszThisStr;
			nextStr ( &pszThisStr );
			char* pszLonHemi = pszThisStr;
			nextStr ( &pszThisStr );
			//char* pszSpeedKnots = pszThisStr;
			nextStr ( &pszThisStr );
			//char* pszTrackAngle = pszThisStr;

			if ( 'A' == *pszStatus )
			{
				//first two chars are deg
				//remainder is minutes
				g_fLat = pszLat[0]-'0';
				g_fLat *= 10;
				g_fLat += pszLat[1]-'0';
				float fmin = my_strtof(&pszLat[2], NULL);
				g_fLat += fmin/60;
				if ( 'S' == *pszLatHemi )	//+ is N, - is S
				{
					g_fLat *= -1;
				}

				//first three chars are deg
				//remainder is minute
				g_fLon = pszLon[0]-'0';
				g_fLon *= 10;
				g_fLon += pszLon[1]-'0';
				g_fLon *= 10;
				g_fLon += pszLon[2]-'0';
				fmin = my_strtof(&pszLon[3], NULL);
				g_fLon += fmin/60;
				if ( 'W' == *pszLonHemi )	//+ is E, - is W
				{
					g_fLon *= -1;
				}

				bLockedStateChanged = ( 0 == g_bLock );	//take note if we changed
				g_bLock = 1;
			}
			else
			{
				bLockedStateChanged = ( 0 != g_bLock );	//take note if we changed
				g_bLock = 0;
			}

			pchPrevStr = pszThisStr;
			nextStr ( &pszThisStr );
			nLen = pszThisStr - pchPrevStr;
			if ( nLen > 5 )	//must have enough stuff
			{
				char szDate[3], szMonth[3], * pszYear;
				szDate[0] = pszThisStr[0];
				szDate[1] = pszThisStr[1];
				szDate[2] = '\0';
				szMonth[0] = pszThisStr[2];
				szMonth[1] = pszThisStr[3];
				szMonth[2] = '\0';
				pszYear = &pszThisStr[4];

				nextStr ( &pszThisStr );
				//char* pszMagVar = pszThisStr;
				nextStr ( &pszThisStr );
				//char* pszMagVarHemi = pszThisStr;

				g_nGPSHour = my_atol(szHour,NULL);
				g_nGPSMinute = my_atol(szMinute,NULL);
				g_nGPSSecond = my_atol(pszSecond,NULL);
				g_nGPSDay = my_atol(szDate,NULL);
				g_nGPSMonth = my_atol(szMonth,NULL);
				g_nGPSYear = my_atol(pszYear,NULL) + 2000;	//y2.1k
			}
		}
	}

	else
	{
		//unknown sentence
	}

	return retval;
}


//XXX might want to have these direct to whatever device based on config
void UART1_DataAvailable ( void )
{
	//YYY you could use this opportunity to signal an event
	//Note, this is called at ISR time
	if ( NULL != g_thGPS )	//only if we have a notificand
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xTaskNotifyFromISR ( g_thGPS, TNB_DAV, eSetBits, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}


//XXX might want to have these direct to whatever device based on config
void UART1_TransmitEmpty ( void )
{
	//YYY you could use this opportunity to signal an event
	//Note, this is called at ISR time
	if ( NULL != g_thGPS )	//only if we have a notificand
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xTaskNotifyFromISR ( g_thGPS, TNB_TBMT, eSetBits, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}


//implementation for the gps processor



void thrdfxnGPSTask ( void const* argument )
{
	for(;;)
	{
		while ( GPSPROC_QUIT != GPS_process ( g_pGPSIOIf ) )
		{
			//XXX
		}
	}
}



