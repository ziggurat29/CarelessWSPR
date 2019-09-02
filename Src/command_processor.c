//==============================================================
//This realizes an interactive command line interface operating on a serial
//stream.
//impl

#include "command_processor.h"
#include <string.h>


#ifndef COUNTOF
#define COUNTOF(arr) (sizeof(arr)/sizeof(arr[0]))
#endif


//command line buffer
char g_achCmdLine[128];



static char _cmdGetChar ( const IOStreamIF* pio )
{
	char ret;
	pio->_receiveCompletely ( pio, &ret, 1, TO_INFINITY );
	return ret;
}


static void _cmdPutChar ( const IOStreamIF* pio, char c )
{
	pio->_transmitCompletely ( pio, &c, 1, TO_INFINITY );
}


static void _cmdPutString ( const IOStreamIF* pio, const char* pStr )
{
	size_t nLen = strlen ( pStr );
	pio->_transmitCompletely ( pio, pStr, nLen, TO_INFINITY );
}



//this gets characters from the input stream until line termination occurs.
//this supports echoing and backspace.
static void _getCommandLine2 ( const IOStreamIF* pio )
{
	int nIdxCmd;

	int bCont = 1;

	//pull characters into cmdline buffer until full or line terminated
	nIdxCmd = 0;
	while ( bCont && nIdxCmd < COUNTOF(g_achCmdLine) )
	{
		char chNow = _cmdGetChar ( pio );
		switch ( chNow )
		{
		case '\r':	//CR is a line terminator
			_cmdPutChar ( pio, '\r' );
			_cmdPutChar ( pio, '\n' );
			memset ( &g_achCmdLine[nIdxCmd], '\0', COUNTOF(g_achCmdLine) - nIdxCmd );	//clear rest of buffer
			++nIdxCmd;
			bCont = 0;
		break;

		case '\n':	//LF is a line terminator
			_cmdPutChar ( pio, '\r' );
			_cmdPutChar ( pio, '\n' );
			memset ( &g_achCmdLine[nIdxCmd], '\0', COUNTOF(g_achCmdLine) - nIdxCmd );	//clear rest of buffer
			++nIdxCmd;
			bCont = 0;
		break;

		case '\b':	//backspace
		case '\x7f':	//rubout
			if ( nIdxCmd > 0 )	//avoid silly case of backspacing past the beginning
			{
				_cmdPutChar ( pio, chNow );	//echo it back
				--nIdxCmd;
			}
		break;

		default:
			//everything else simply accumulates the character
			_cmdPutChar ( pio, chNow );	//echo it back
			g_achCmdLine[nIdxCmd] = chNow;
			++nIdxCmd;
		break;
		}
	}
}



//this gets characters from the input stream until line termination occurs.
//this supports echoing and backspace.
//This is a non-blocking version; it returns true if the line is complete and
//can be processed, and false if the line is incomplete and this should be
//invoked again later.
static int _getCommandLine3 ( const IOStreamIF* pio )
{
	static int nIdxCmd = 0;

	int bCont = 1;

	//pull characters into cmdline buffer until full or line terminated
	while ( bCont && nIdxCmd < COUNTOF(g_achCmdLine) )
	{
		char chNow;
		size_t nGot = pio->_receive ( pio, &chNow, 1 );
		if ( 0 == nGot )	//wanted more, but no more
		{
			return 0;	//try again later
		}

		switch ( chNow )
		{
		case '\r':	//CR is a line terminator
			_cmdPutChar ( pio, '\r' );
			_cmdPutChar ( pio, '\n' );
			memset ( &g_achCmdLine[nIdxCmd], '\0', COUNTOF(g_achCmdLine) - nIdxCmd );	//clear rest of buffer
			nIdxCmd = 0;	//next time start new line
			bCont = 0;
		break;

		case '\n':	//LF is a line terminator
			_cmdPutChar ( pio, '\r' );
			_cmdPutChar ( pio, '\n' );
			memset ( &g_achCmdLine[nIdxCmd], '\0', COUNTOF(g_achCmdLine) - nIdxCmd );	//clear rest of buffer
			nIdxCmd = 0;	//next time start new line
			bCont = 0;
		break;

		case '\b':	//backspace
		case '\x7f':	//rubout
			if ( nIdxCmd > 0 )	//avoid silly case of backspacing past the beginning
			{
				_cmdPutChar ( pio, chNow );	//echo it back
				--nIdxCmd;
			}
		break;

		default:
			//everything else simply accumulates the character
			_cmdPutChar ( pio, chNow );	//echo it back
			g_achCmdLine[nIdxCmd] = chNow;
			++nIdxCmd;
		break;
		}
	}
	return 1;
}



//this processes the command line into tokens.  this supports escaping and
//quoting.
void _parseCommandLine ( void )
{
	int nIdxSrc;
	int nIdxDst;

	int bCont = 1;
	int bQuoting = 0;
	int bEscaping = 0;
	int bSkipping = 1;

	//parse characters
	nIdxSrc = 0;
	nIdxDst = 0;
	while ( bCont && nIdxSrc < COUNTOF(g_achCmdLine) )
	{
		//transfer from src to dst
		char chNow = g_achCmdLine[nIdxSrc];
		++nIdxSrc;
		switch ( chNow )
		{
		case ' ':	//include whitespace if quoting/escaping, else token is done
		case '\t':
			if ( bQuoting )
			{
				//if quoting; accumulate this whitespace
				g_achCmdLine[nIdxDst] = chNow;
				++nIdxDst;
			}
			else if ( bEscaping )
			{
				//if escaping; accumulate this whitespace and reset escaping state
				g_achCmdLine[nIdxDst] = chNow;
				++nIdxDst;
				bEscaping = 0;
			}
			else if ( bSkipping )
			{
				//leading whitespace is skipped
			}
			else
			{
				//otherwise whitespace is a token delimiter
				g_achCmdLine[nIdxDst] = '\0';
				++nIdxDst;
				bSkipping = 1;
			}
		break;

		case '\\':	//maybe enter escaping mode
			if ( bQuoting )
			{
				//if quoting; accumulate this char
				g_achCmdLine[nIdxDst] = chNow;
				++nIdxDst;
			}
			else if ( bEscaping )
			{
				//if escaping; accumulate this char and reset escaping state
				g_achCmdLine[nIdxDst] = chNow;
				++nIdxDst;
				bEscaping = 0;
			}
			else
			{
				//otherwise go into escaping state
				bEscaping = 1;
			}
		break;

		case '"':	//toggle quoting
		case '\'':
			if ( bEscaping )
			{
				//if escaping; accumulate this char and reset escaping state
				g_achCmdLine[nIdxDst] = chNow;
				++nIdxDst;
				bEscaping = 0;
			}
			else
			{
				//XXX any fancier logic? like ensuring quoting at beginning of token, or remembering the quote char used?
				bQuoting = ! bQuoting;
			}
			break;

		case '\0':	//nul is line terminator
			//XXX support escaping of this?
			memset ( &g_achCmdLine[nIdxDst], '\0', COUNTOF(g_achCmdLine) - nIdxDst );	//clear rest of buffer
			++nIdxDst;
			bCont = 0;
		break;

		default:
			//everything else simply accumulates the character
			g_achCmdLine[nIdxDst] = chNow;
			++nIdxDst;
			bEscaping = 0;
			bSkipping = 0;
		break;
		}
	}

	//ensure cmdline is properly (double) terminated
	g_achCmdLine[COUNTOF(g_achCmdLine)-1] = '\0';
	g_achCmdLine[COUNTOF(g_achCmdLine)-2] = '\0';
}



//========================================================================



static CmdProcRetval _parseAndDispatch ( const IOStreamIF* pio, const CmdProcEntry* acpe, size_t nAcpe )
{
	CmdProcRetval retval;
	int nCmdEntry;

	_parseCommandLine();	//tokenize the command line

	//dispatch command
	nCmdEntry = CMDPROC_findProcEntry ( g_achCmdLine, acpe, nAcpe );
	if ( -1 != nCmdEntry )
	{
		const char* pszRest = CMDPROC_nextToken ( g_achCmdLine );
		retval = acpe[nCmdEntry]._pfxnHandler ( pio, pszRest );
	}
	else
	{
		//no valid command found
		_cmdPutString ( pio, "The command '" );
		_cmdPutString ( pio, g_achCmdLine );
		_cmdPutString ( pio, "' is not recognized.\r\n" );
		retval = CMDPROC_ERROR;
	}

	return retval;
}


//process data from input stream, dispatching commands
CmdProcRetval CMDPROC_process ( const IOStreamIF* pio, const CmdProcEntry* acpe, size_t nAcpe )
{
	//get a tokenized series of strings
	do
	{
		_getCommandLine2 ( pio );	//get the command line from the IO stream
	} while ( '\0' == g_achCmdLine[0] );

	return _parseAndDispatch ( pio, acpe, nAcpe );
}



CmdProcRetval CMDPROC_process_nb ( const IOStreamIF* pio, const CmdProcEntry* acpe, size_t nAcpe )
{
	//get a tokenized series of strings
	do
	{
		if ( ! _getCommandLine3 ( pio ) )
			return CMDPROC_INCOMPLETE;
	} while ( '\0' == g_achCmdLine[0] );	//skip empty lines

	return _parseAndDispatch ( pio, acpe, nAcpe );
}



//given a command token, find the associated entry, or -1 if none found
int CMDPROC_findProcEntry ( const char* pszCommand, const CmdProcEntry* acpe, size_t nAcpe )
{
	size_t nIdx;

	for ( nIdx = 0; nIdx < nAcpe; ++nIdx )
	{
		if ( 0 == strcmp ( acpe[nIdx]._pszCommand, pszCommand ) )
			return nIdx;
	}

	return -1;	//nothing found
}



//given a token list, get the pointer to the next token, or NULL if none left
const char* CMDPROC_nextToken ( const char* pszszTokens )
{
	if ( NULL == pszszTokens || '\0' == *pszszTokens )
		return NULL;
	//advance past this token
	while ( '\0' != *pszszTokens )
		++pszszTokens;
	//get to start of next token
	++pszszTokens;
	//see if nothing left
	if ( '\0' == *pszszTokens )
		return NULL;
	//otherwise return start of next token
	return pszszTokens;
}


