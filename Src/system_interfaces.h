//==============================================================
//'Interface' objects for various abstracted components in the system.
//This module is part of the CarelessWSPR project.
//By abstracting the API to various components of the system, we gain some
//flexibility in how we wire the components together at runtime.

#ifndef __SYSTEM_INTERFACES_H
#define __SYSTEM_INTERFACES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>


#define TO_INFINITY 0xffffffff

//These interface objects will typically be in read-only memory

//IO stream abstraction; typically for serial ports
typedef struct IOStreamIF IOStreamIF;
typedef struct IOStreamIF
{
	//transmit methods; non-blocking
	void (* _flushTransmit) ( const IOStreamIF* pthis );
	size_t (* _transmitFree) ( const IOStreamIF* pthis );
	size_t (* _transmit) ( const IOStreamIF* pthis, const void* pv, size_t nLen );

	//receive methods; non-blocking
	void (* _flushReceive) ( const IOStreamIF* pthis );
	size_t (* _receiveAvailable) ( const IOStreamIF* pthis );
	size_t (* _receive) ( const IOStreamIF* pthis, void* pv, const size_t nLen );

	//transmit/receive methods; blocking
	//0 on success, nRemaining on timeout (i.e nLen - nProcessed)
	int (* _transmitCompletely) ( const IOStreamIF* pthis, const void* pv, size_t nLen, uint32_t to );
	int (* _receiveCompletely) ( const IOStreamIF* pthis, void* pv, const size_t nLen, uint32_t to );

	//(constant) instance data
	void* huart;	//a little tacky; usually a UART_HandleTypeDef
} IOStreamIF;



#ifdef __cplusplus
}
#endif

#endif
