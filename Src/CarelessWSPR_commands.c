//==============================================================
//This provides implementation for the commands relevant for the
//CarelessWSPR project.
//impl

#include "CarelessWSPR_commands.h"
#include "CarelessWSPR_settings.h"
#include "maidenhead.h"
#include "util_altlib.h"
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#include "si5351a.h"

#include "task_gps.h"
#include "task_wspr.h"


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern RTC_HandleTypeDef hrtc;	//in main.c

#ifndef COUNTOF
#define COUNTOF(arr) (sizeof(arr)/sizeof(arr[0]))
#endif



//forward decl command handlers
static CmdProcRetval cmdhdlHelp ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlSet ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlPerist ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlDeperist ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlReboot ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlDump ( const IOStreamIF* pio, const char* pszszTokens );

#ifdef DEBUG
static CmdProcRetval cmdhdlDiag ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlExp001 ( const IOStreamIF* pio, const char* pszszTokens );
#endif

static CmdProcRetval cmdhdlGps ( const IOStreamIF* pio, const char* pszszTokens );


//the array of command descriptors our application supports
const CmdProcEntry g_aceCommands[] = 
{
	{ "set", cmdhdlSet, "set a setting value, or list all settings" },
	{ "persist", cmdhdlPerist, "persist settings to flash" },
	{ "depersist", cmdhdlDeperist, "depersist settings from flash" },
	{ "reboot", cmdhdlReboot, "restart the board" },
	{ "dump", cmdhdlDump, "dump memory; dump {addr} {count}" },
#ifdef DEBUG
	{ "diag", cmdhdlDiag, "show diagnostic info (DEBUG build only)" },
	{ "exp001", cmdhdlExp001, "experimental command (DEBUG build only)" },
#endif
	{ "gps", cmdhdlGps, "show GPS info (if any)" },

	{ "help", cmdhdlHelp, "get help on a command; help {cmd}" },
};
const size_t g_nAceCommands = COUNTOF(g_aceCommands);



//========================================================================
//command helpers (XXX probably break out for general use)


static void _cmdPutChar ( const IOStreamIF* pio, char c )
{
	pio->_transmitCompletely ( pio, &c, 1, TO_INFINITY );
}


static void _cmdPutString ( const IOStreamIF* pio, const char* pStr )
{
	size_t nLen = strlen ( pStr );
	pio->_transmitCompletely ( pio, pStr, nLen, TO_INFINITY );
}


static void _cmdPutCRLF ( const IOStreamIF* pio )
{
	_cmdPutString ( pio, "\r\n" );
}


static void _cmdPutInt ( const IOStreamIF* pio, long val, int padding )
{
	char ach[16];
	my_itoa_sortof ( ach, val, padding );
	_cmdPutString ( pio, ach );
}


static void _cmdPutFloat ( const IOStreamIF* pio, float val )
{
	char ach[20];
	my_ftoa ( ach, val );
	_cmdPutString ( pio, ach );
}


//simple parser of an integer value (can be hex with '0x' prefix)
static uint32_t _parseInt ( const char* pszToken )
{
	uint32_t val;

	val = 0;
	//see if it starts with 0x meaning 'hex'
	if ( '0' == pszToken[0] && ( 'x' == pszToken[1] || 'X' == pszToken[1] ) )
	{
		pszToken += 2;
		while ( '\0' != *pszToken )
		{
			val <<= 4;
			if ( *pszToken <= '9' )
			{
				val += (*pszToken - '0');
			}
			else if ( *pszToken <= 'F' )
			{
				val += (*pszToken - 'A' + 10);
			}
			else
			{
				val += (*pszToken - 'a' + 10);
			}
			++pszToken;
		}
	}
	else
	{
		//otherwise, interpret it as decimal
		while ( '\0' != *pszToken )
		{
			val *= 10;
			val += (*pszToken - '0');
			++pszToken;
		}
	}

	return val;
}



//========================================================================
//simple command handlers


static CmdProcRetval cmdhdlHelp ( const IOStreamIF* pio, const char* pszszTokens )
{
	//get next token; we will get help on that
	int nIdx;
	if ( NULL != pszszTokens && '\0' != *pszszTokens &&
		-1 != ( nIdx = CMDPROC_findProcEntry ( pszszTokens, g_aceCommands, g_nAceCommands ) ) )
	{
		//emit help information for this one command
		_cmdPutString ( pio, g_aceCommands[nIdx]._pszHelp );
		_cmdPutCRLF(pio);
	}
	else
	{
		//if unrecognised command
		if ( NULL != pszszTokens && '\0' != *pszszTokens )
		{
			_cmdPutString ( pio, "The command '" );
			_cmdPutString ( pio, pszszTokens );
			_cmdPutString ( pio, "' is not recognized.\r\n" );
		}

		//list what we've got
		_cmdPutString ( pio, "help is available for:\r\n" );
		for ( nIdx = 0; nIdx < g_nAceCommands; ++nIdx )
		{
			_cmdPutString ( pio, g_aceCommands[nIdx]._pszCommand );
			_cmdPutCRLF(pio);
		}
	}

	return CMDPROC_SUCCESS;
}


static CmdProcRetval cmdhdlSet ( const IOStreamIF* pio, const char* pszszTokens )
{
	PersistentSettings* psettings = Settings_getStruct();

	const char* pszSetting = pszszTokens;
	if ( NULL == pszSetting )
	{
		//list all settings and their current value

		//RTC date time
		RTC_TimeTypeDef sTime;
		RTC_DateTypeDef sDate;
		HAL_RTC_GetTime ( &hrtc, &sTime, RTC_FORMAT_BIN );
		HAL_RTC_GetDate ( &hrtc, &sDate, RTC_FORMAT_BIN );
		_cmdPutString ( pio, "datetime:  " );
		_cmdPutInt ( pio, sDate.Year + 2000, 4 );
		_cmdPutChar ( pio, '-' );
		_cmdPutInt ( pio, sDate.Month, 2 );
		_cmdPutChar ( pio, '-' );
		_cmdPutInt ( pio, sDate.Date, 2 );
		_cmdPutChar ( pio, ' ' );
		_cmdPutInt ( pio, sTime.Hours, 2 );
		_cmdPutChar ( pio, ':' );
		_cmdPutInt ( pio, sTime.Minutes, 2 );
		_cmdPutChar ( pio, ':' );
		_cmdPutInt ( pio, sTime.Seconds, 2 );
		_cmdPutCRLF(pio);

		_cmdPutString ( pio, "freq:  " );
		_cmdPutInt ( pio, psettings->_dialFreqHz, 0 );
		_cmdPutCRLF(pio);
		_cmdPutString ( pio, "band:  " );
		if ( psettings->_nSubBand < 0 )
		{
			_cmdPutString ( pio, "random" );
		}
		else
		{
			_cmdPutInt ( pio, psettings->_nSubBand, 0 );
		}
		_cmdPutCRLF(pio);
		_cmdPutString ( pio, "duty:  " );
		_cmdPutInt ( pio, psettings->_nDutyPct, 0 );
		_cmdPutCRLF(pio);

		_cmdPutString ( pio, "callsign:  " );
		_cmdPutString ( pio, psettings->_achCallSign );
		_cmdPutCRLF(pio);
		_cmdPutString ( pio, "maidenhead:  " );
		_cmdPutString ( pio, psettings->_achMaidenhead );
		_cmdPutCRLF(pio);
		_cmdPutString ( pio, "power:  " );
		_cmdPutInt ( pio, psettings->_nTxPowerDbm, 0 );
		_cmdPutCRLF(pio);

		_cmdPutString ( pio, "gpson:  " );
		_cmdPutInt ( pio, psettings->_bUseGPS, 0 );
		_cmdPutCRLF(pio);
		_cmdPutString ( pio, "gpsrate:  " );
		_cmdPutInt ( pio, psettings->_nGPSbitRate, 0 );
		_cmdPutCRLF(pio);

		return CMDPROC_SUCCESS;
	}

	//next, get the value
	const char* pszValue;
	pszValue = CMDPROC_nextToken ( pszSetting );
	if ( NULL == pszValue )
	{
		_cmdPutString ( pio, "set " );
		_cmdPutString ( pio, pszSetting );
		_cmdPutString ( pio, " requires a setting value\r\n" );
		return CMDPROC_ERROR;
	}

	if ( 0 == strcmp ( "datetime", pszSetting ) )
	{
		const char* pszTime;
		pszTime = CMDPROC_nextToken ( pszValue );
		if ( NULL == pszTime )
		{
			_cmdPutString ( pio, "datetime requires yyyy-mm-dd hh:mm:ss\r\n" );
			return CMDPROC_ERROR;
		}
		int nDateLen = strlen ( pszValue );
		int nTimeLen = strlen ( pszTime );
		if ( 10 != nDateLen || 8 != nTimeLen || 
				'-' != pszValue[4] || '-' != pszValue[7] || 
				':' != pszTime[2] || ':' != pszTime[5] )
		{
			_cmdPutString ( pio, "datetime requires yyyy-mm-dd hh:mm:ss\r\n" );
			return CMDPROC_ERROR;
		}
		//set the RTC right now
		RTC_TimeTypeDef sTime;
		RTC_DateTypeDef sDate;
		sTime.Hours = my_atoul ( &pszTime[0], NULL );
		sTime.Minutes = my_atoul ( &pszTime[3], NULL );
		sTime.Seconds = my_atoul ( &pszTime[6], NULL );
		sDate.WeekDay = RTC_WEEKDAY_SUNDAY;	//(arbitrary)
		sDate.Date = my_atoul ( &pszValue[8], NULL );
		sDate.Month = my_atoul ( &pszValue[5], NULL );
		sDate.Year = my_atoul ( &pszValue[0], NULL ) - 2000;
		HAL_RTC_SetTime ( &hrtc, &sTime, RTC_FORMAT_BIN );
		HAL_RTC_SetDate ( &hrtc, &sDate, RTC_FORMAT_BIN );
	}
	else if ( 0 == strcmp ( "freq", pszSetting ) )
	{
		long unsigned int freq = my_atoul ( pszValue, NULL );
		if ( freq < 7200 || freq > 200000000 )
		{
			_cmdPutString ( pio, "freq must be in range 7200 - 200000000\r\n" );
			return CMDPROC_ERROR;
		}
		else
		{
			psettings->_dialFreqHz = freq;
		}
	}
	else if ( 0 == strcmp ( "band", pszSetting ) )
	{
		long int band = my_atol ( pszValue, NULL );
		if ( band < 0 )
			band = -1;
		if ( band > 32 )
		{
			_cmdPutString ( pio, "band must be -1, or 0 - 32\r\n" );
			return CMDPROC_ERROR;
		}
		else
		{
			psettings->_nSubBand = band;
		}
	}
	else if ( 0 == strcmp ( "duty", pszSetting ) )
	{
		long int duty = my_atol ( pszValue, NULL );
		if ( duty < 0 || duty > 100 )
		{
			_cmdPutString ( pio, "duty must be 0 - 100\r\n" );
			return CMDPROC_ERROR;
		}
		else
		{
			psettings->_nDutyPct = duty;
		}
	}
	else if ( 0 == strcmp ( "callsign", pszSetting ) )
	{
		int nLen = strlen ( pszValue );
		if ( nLen < 4 || nLen > 6 )
		{
			_cmdPutString ( pio, "callsign must be 4-6 characters\r\n" );
			return CMDPROC_ERROR;
		}
		else
		{
			strcpy ( psettings->_achCallSign, pszValue );
			//cause WSPR message to be re-computed
			WSPR_ReEncode();
		}
	}
	else if ( 0 == strcmp ( "maidenhead", pszSetting ) )
	{
		int nLen = strlen ( pszValue );
		if ( nLen != 4 )
		{
			_cmdPutString ( pio, "maidenhead must always be 4 characters\r\n" );
			return CMDPROC_ERROR;
		}
		else
		{
			strcpy ( psettings->_achMaidenhead, pszValue );
			//cause WSPR message to be re-computed
			WSPR_ReEncode();
		}
	}
	else if ( 0 == strcmp ( "power", pszSetting ) )
	{
		long int power = my_atol ( pszValue, NULL );
		if ( power < 0 || power > 60 )
		{
			_cmdPutString ( pio, "power must be 0 - 60\r\n" );
			return CMDPROC_ERROR;
		}
		else
		{
			psettings->_nTxPowerDbm = power;
			//cause WSPR message to be re-computed
			WSPR_ReEncode();
		}
	}
	else if ( 0 == strcmp ( "gpson", pszSetting ) )
	{
		long int gpson = my_atol ( pszValue, NULL );
		psettings->_bUseGPS = gpson ? 1 : 0;
//XXX activate/deactivate GPS
	}
	else if ( 0 == strcmp ( "gpsrate", pszSetting ) )
	{
		long int rate = my_atol ( pszValue, NULL );
		if ( rate < 300 || rate > 115200 )
		{
			_cmdPutString ( pio, "rate must be 200 - 115200\r\n" );
			return CMDPROC_ERROR;
		}
		else
		{
			psettings->_nGPSbitRate = rate;
//XXX reconfigure USART1
		}
	}
	else
	{
		_cmdPutString ( pio, "error:  the setting " );
		_cmdPutString ( pio, pszSetting );
		_cmdPutString ( pio, "is not a valid setting name\r\n" );
		return CMDPROC_ERROR;
	}

	_cmdPutString ( pio, "done\r\n" );

	return CMDPROC_SUCCESS;
}



static CmdProcRetval cmdhdlPerist ( const IOStreamIF* pio, const char* pszszTokens )
{
	if ( Settings_persist() )
	{
		_cmdPutString( pio, "settings persisted\r\n" );
	}
	else
	{
		_cmdPutString ( pio, "Failed to persist settings!\r\n" );
	}
	return CMDPROC_SUCCESS;
}



static CmdProcRetval cmdhdlDeperist ( const IOStreamIF* pio, const char* pszszTokens )
{
	if ( Settings_depersist() )
	{
		_cmdPutString( pio, "settings depersisted\r\n" );
	}
	else
	{
		_cmdPutString ( pio, "Failed to depersist settings!\r\n" );
	}
	return CMDPROC_SUCCESS;
}



static CmdProcRetval cmdhdlReboot ( const IOStreamIF* pio, const char* pszszTokens )
{
	si5351aOutputOff(SI_CLK0_CONTROL);	//extinguish signal; if any
	_cmdPutString( pio, "rebooting\r\n" );
	osDelay ( 500 );	//delay a little to let all that go out before we reset
	NVIC_SystemReset();
	return CMDPROC_SUCCESS;
}



#ifdef DEBUG

//diagnostic variables in main.c
extern volatile size_t g_nHeapFree;
extern volatile size_t g_nMinEverHeapFree;
extern volatile int g_nMaxGPSRxQueue;
extern volatile int g_nMaxCDCTxQueue;
extern volatile int g_nMaxCDCRxQueue;
extern volatile int g_nMinStackFreeDefault;
extern volatile int g_nMinStackFreeMonitor;
extern volatile int g_nMinStackFreeGPS;
extern volatile int g_nMinStackFreeWSPR;

#define USE_FREERTOS_HEAP_IMPL 1
#if USE_FREERTOS_HEAP_IMPL
//we implemented a 'heapwalk' function
typedef int (*CBK_HEAPWALK) ( void* pblk, uint32_t nBlkSize, int bIsFree, void* pinst );
extern int vPortHeapWalk ( CBK_HEAPWALK pfnWalk, void* pinst );

int fxnHeapwalk ( void* pblk, uint32_t nBlkSize, int bIsFree, void* pinst )
{
//	const IOStreamIF* pio = (const IOStreamIF*) pinst;
	//XXX heapwalk suspends all tasks, so cannot do io here
//	"%p %lu, %u\r\n", pblk, nBlkSize, bIsFree
//	_cmdPutString ( pio, ach );
	return 1;	//keep walking
}


#endif

static CmdProcRetval cmdhdlDiag ( const IOStreamIF* pio, const char* pszszTokens )
{
	//list what we've got
	_cmdPutString ( pio, "diagnostic vars:\r\n" );

	_cmdPutString ( pio, "Heap: free now: " );
	_cmdPutInt ( pio, g_nHeapFree, 0 );
	_cmdPutString ( pio, ", min free ever: " );
	_cmdPutInt ( pio, g_nMinEverHeapFree, 0 );
	_cmdPutCRLF(pio);

	_cmdPutString ( pio, "GPS max RX queue: " );
	_cmdPutInt ( pio, g_nMaxGPSRxQueue, 0 );
	_cmdPutCRLF(pio);

	_cmdPutString ( pio, "Monitor max RX queue: " );
	_cmdPutInt ( pio, g_nMaxCDCRxQueue, 0 );
	_cmdPutString ( pio, ", max TX queue: " );
	_cmdPutInt ( pio, g_nMaxCDCTxQueue, 0 );
	_cmdPutCRLF(pio);

	_cmdPutString ( pio, "Task: Default: min stack free: " );
	_cmdPutInt ( pio, g_nMinStackFreeDefault*sizeof(uint32_t), 0 );
	_cmdPutCRLF(pio);

	_cmdPutString ( pio, "Task: Monitor: min stack free: " );
	_cmdPutInt ( pio, g_nMinStackFreeMonitor*sizeof(uint32_t), 0 );
	_cmdPutCRLF(pio);

	_cmdPutString ( pio, "Task: GPS: min stack free: " );
	_cmdPutInt ( pio, g_nMinStackFreeGPS*sizeof(uint32_t), 0 );
	_cmdPutCRLF(pio);

	_cmdPutString ( pio, "Task: WSPR: min stack free: " );
	_cmdPutInt ( pio, g_nMinStackFreeWSPR*sizeof(uint32_t), 0 );
	_cmdPutCRLF(pio);

#if USE_FREERTOS_HEAP_IMPL
//heapwalk suspends all tasks, so not good here
//	_cmdPutString ( pio, "Heapwalk:\r\n" );
//	vPortHeapWalk ( fxnHeapwalk, (void*)pio );
#endif

	return CMDPROC_SUCCESS;
}
#endif


//stuff that the GPS task projects from task_gps.h
#include "task_gps.h"


static CmdProcRetval cmdhdlGps ( const IOStreamIF* pio, const char* pszszTokens )
{
	if ( g_bLock )//0 != g_nGPSYear )
	{
		//emit gps timestamp
		_cmdPutString ( pio, "GPS TS:  " );
		_cmdPutInt ( pio, g_nGPSYear, 4 );
		_cmdPutChar ( pio, '-' );
		_cmdPutInt ( pio, g_nGPSMonth, 2 );
		_cmdPutChar ( pio, '-' );
		_cmdPutInt ( pio, g_nGPSDay, 2 );
		_cmdPutChar ( pio, ' ' );
		_cmdPutInt ( pio, g_nGPSHour, 2 );
		_cmdPutChar ( pio, ':' );
		_cmdPutInt ( pio, g_nGPSMinute, 2 );
		_cmdPutChar ( pio, ':' );
		_cmdPutInt ( pio, g_nGPSSecond, 2 );
		_cmdPutCRLF(pio);

		//emit location
		_cmdPutString ( pio, "GPS Pos:  lat " );
		_cmdPutFloat ( pio, g_fLat );
		_cmdPutString ( pio, ", lon " );
		_cmdPutFloat ( pio, g_fLon );
		char ach[8];
		if ( ! toMaidenhead ( g_fLat, g_fLon, ach, 6 ) )
		{
			_cmdPutString ( pio, "toMaidenhead() failed\r\n" );
		}
		else
		{
			_cmdPutString ( pio, ", maidenhead " );
			_cmdPutString ( pio, ach );
			_cmdPutCRLF(pio);
		}
	}
	else
	{
		_cmdPutString ( pio, "(no lock yet)\r\n" );
	}

	return CMDPROC_SUCCESS;
}


//========================================================================
//'dump' command handler


static char _printableChar ( char ch )
{
	if ( ( ch < ' ' ) || ( ch > 0x7f ) ) ch='.';
	return ch;
}


static char _nybbleToChar ( uint8_t nyb )
{
	char ret = nyb + '0';
	if ( nyb > 9 )
		ret += 'a' - '9' - 1;
	return ret;
}



static CmdProcRetval cmdhdlDump ( const IOStreamIF* pio, const char* pszszTokens )
{
	const char* pszStartAddress;
	const char* pszCount;
	uint32_t nStartAddr;
	uint32_t nCount;
	const uint8_t* pby;
	uint32_t nIdx;

	pszStartAddress = pszszTokens;
	if ( NULL == pszStartAddress )
	{
		_cmdPutString ( pio, "dump requires an address\r\n" );
		return CMDPROC_ERROR;
	}
	pszCount = CMDPROC_nextToken ( pszStartAddress );
	if ( NULL == pszCount )
	{
		_cmdPutString ( pio, "dump requires a count\r\n" );
		return CMDPROC_ERROR;
	}

	//parse address
	nStartAddr = _parseInt ( pszStartAddress );

	//parse count
	nCount = _parseInt ( pszCount );

	if ( nCount < 1 )
	{
		_cmdPutString ( pio, "too few bytes to dump.  1 - 8192.\r\n" );
		return CMDPROC_ERROR;
	}
	if ( nCount > 8192 )
	{
		_cmdPutString ( pio, "too many bytes to dump.  1 - 8192.\r\n" );
		return CMDPROC_ERROR;
	}

	//OK, now we do the hex dump
	_cmdPutString ( pio, "          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\r\n" );
	_cmdPutString ( pio, "--------  -----------------------------------------------  ----------------\r\n" );
	pby = (const uint8_t*)nStartAddr;
	for ( nIdx = 0; nIdx < nCount; )
	{
		int nIter;
		int nToDo = nCount - nIdx;
		if ( nToDo > 16 )
			nToDo = 16;

		//first, do the address
		uint32_t nThisAddr = nStartAddr + nIdx;
		for ( nIter = 0; nIter < 8; ++nIter )
		{
			_cmdPutChar ( pio, _nybbleToChar ( (uint8_t) ( nThisAddr >> 28 ) ) );
			nThisAddr <<= 4;
		}
		_cmdPutString ( pio, "  " );
		
		//now do the hex part
		for ( nIter = 0; nIter < nToDo; ++nIter )
		{
			_cmdPutChar ( pio, _nybbleToChar ( pby[nIdx+nIter] >> 4 ) );
			_cmdPutChar ( pio, _nybbleToChar ( pby[nIdx+nIter] & 0x0f ) );
			_cmdPutChar ( pio, ' ' );
		}
		for ( ; nIter < 16; ++nIter )
		{
			_cmdPutString ( pio, "   " );
		}
		_cmdPutChar ( pio, ' ' );
		
		//now do the text part
		for ( nIter = 0; nIter < nToDo; ++nIter )
		{
			_cmdPutChar ( pio, _printableChar ( pby[nIdx+nIter] ) );
		}
		for ( ; nIter < 16; ++nIter )
		{
			_cmdPutChar ( pio, ' ' );
		}

		//finished!
		_cmdPutCRLF(pio);

		nIdx += nToDo;
	}

	return CMDPROC_SUCCESS;
}



//========================================================================
//'exp001' command handler


#ifdef DEBUG

#include "task_wspr.h"
#include "wspr.h"

//this is used for doing experiments during development
static CmdProcRetval cmdhdlExp001 ( const IOStreamIF* pio, const char* pszszTokens )
{

	const char* pszArg1 = pszszTokens;
	if ( 0 == strcmp ( pszArg1, "on" ) )
	{
		//start (potentially) WSPR'ing at the next even minute
		PersistentSettings* psettings = Settings_getStruct();
		if ( wspr_encode ( g_abyWSPR, psettings->_achCallSign, 
				psettings->_achMaidenhead, 
				psettings->_nTxPowerDbm ) )
		{
			WSPR_StartWSPR();
			_cmdPutString ( pio, "WSPR'ing started\r\n" );
		}
		else
		{
			_cmdPutString ( pio, "Failed to compute WSPR message!\r\n" );
		}
	}
	else
	{
		//stop any WSPR'in
		WSPR_StopWSPR();
		_cmdPutString ( pio, "WSPR'ing stopped\r\n" );
	}

	return CMDPROC_SUCCESS;
}

#endif
