//==============================================================
//This realizes an interactive command line interface operating on a serial
//stream.
//This module is part of the CarelessWSPR project.

#ifndef __COMMAND_PROCESSOR_H
#define __COMMAND_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "system_interfaces.h"


typedef enum CmdProcRetval CmdProcRetval;
enum CmdProcRetval
{
	CMDPROC_SUCCESS = 0,	//a command was dispatched successfully
	CMDPROC_ERROR = 1,		//a command was received, but there were problems
	CMDPROC_QUIT = 2,		//it is time to exit the command processor
};



//make an array of these structs to define your command set
typedef struct CmdProcEntry CmdProcEntry;
struct CmdProcEntry
{
	const char*	_pszCommand;
	CmdProcRetval (*_pfxnHandler) ( const IOStreamIF* pio, const char* pszszTokens );
	const char* _pszHelp;
};


//process data from input stream, dispatching commands
CmdProcRetval CMDPROC_process ( const IOStreamIF* pio, const CmdProcEntry* acpe, size_t nAcpe );


//helpers for command handlers
//given a command token, find the associated entry, or -1 if none found
int CMDPROC_findProcEntry ( const char* pszCommand, const CmdProcEntry* acpe, size_t nAcpe );
//given a token list, get the pointer to the next token, or NULL if none left
const char* CMDPROC_nextToken ( const char* pszszTokens );


#ifdef __cplusplus
}
#endif

#endif
