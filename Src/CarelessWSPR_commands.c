//==============================================================
//This provides implementation for the commands relevant for the
//CarelessWSPR project.
//impl

#include "CarelessWSPR_commands.h"
#include "maidenhead.h"
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#ifndef COUNTOF
#define COUNTOF(arr) (sizeof(arr)/sizeof(arr[0]))
#endif



//forward decl command handlers
static CmdProcRetval cmdhdlHelp ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlReboot ( const IOStreamIF* pio, const char* pszszTokens );
static CmdProcRetval cmdhdlDump ( const IOStreamIF* pio, const char* pszszTokens );

#ifdef DEBUG
static CmdProcRetval cmdhdlDiag ( const IOStreamIF* pio, const char* pszszTokens );
#endif

static CmdProcRetval cmdhdlGps ( const IOStreamIF* pio, const char* pszszTokens );


//the array of command descriptors our application supports
const CmdProcEntry g_aceCommands[] = 
{
	{ "reboot", cmdhdlReboot, "restart the board" },
	{ "dump", cmdhdlDump, "dump memory; dump {addr} {count}" },
#ifdef DEBUG
	{ "diag", cmdhdlDiag, "show diagnostic info (DEBUG build only)" },
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
		_cmdPutString ( pio, "\r\n" );
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
			_cmdPutString ( pio, "\r\n" );
		}
	}

	return CMDPROC_SUCCESS;
}



static CmdProcRetval cmdhdlReboot ( const IOStreamIF* pio, const char* pszszTokens )
{
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
	char ach[80];
	sprintf ( ach, "Heap: free now: %u, min free ever: %u\r\n", g_nHeapFree, g_nMinEverHeapFree );
	_cmdPutString ( pio, ach );

	sprintf ( ach, "GPS max RX queue: %u\r\n", g_nMaxGPSRxQueue );
	_cmdPutString ( pio, ach );

	sprintf ( ach, "Monitor max RX queue %u, max TX queue %u\r\n", g_nMaxCDCRxQueue, g_nMaxCDCTxQueue );
	_cmdPutString ( pio, ach );

	sprintf ( ach, "Task: Default: min stack free %u\r\n", g_nMinStackFreeDefault*sizeof(uint32_t) );
	_cmdPutString ( pio, ach );

	sprintf ( ach, "Task: Monitor: min stack free %u\r\n", g_nMinStackFreeMonitor*sizeof(uint32_t) );
	_cmdPutString ( pio, ach );

	sprintf ( ach, "Task: GPS: min stack free %u\r\n", g_nMinStackFreeGPS*sizeof(uint32_t) );
	_cmdPutString ( pio, ach );

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
	char ach[80];

	if ( g_bLock )//0 != g_nGPSYear )
	{
		//emit gps timestamp
		sprintf ( ach, "GPS TS:  %04d-%02d-%02d %02d:%02d:%02d\r\n",
				g_nGPSYear, g_nGPSMonth, g_nGPSDay, 
				g_nGPSHour, g_nGPSMinute, g_nGPSSecond );
		_cmdPutString ( pio, ach );

		//emit location
		sprintf ( ach, "GPS Pos:  lat %f, lon %f", g_fLat, g_fLon );
		_cmdPutString ( pio, ach );
		char ach[8];
		if ( ! toMaidenhead ( g_fLat, g_fLon, ach, 6 ) )
		{
			_cmdPutString ( pio, "  toMaidenhead() failed\r\n" );
		}
		else
		{
			_cmdPutString ( pio, ", maidenhead " );
			_cmdPutString ( pio, ach );
			_cmdPutString ( pio, "\r\n" );
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
		_cmdPutString ( pio, "\r\n" );
		
		nIdx += nToDo;
	}

	return CMDPROC_SUCCESS;
}


