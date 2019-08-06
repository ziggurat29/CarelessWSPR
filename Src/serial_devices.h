//==============================================================
//This realizes a unified, interrupt-driven, buffered, API to the various
//serial devices in the system.
//This module is part of the CarelessWSPR project.
//Some parts of this logical component are realized in main.c, as a
//consequence of the STM32CubeMX code generator.

#ifndef __SERIAL_DEVICES_H
#define __SERIAL_DEVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "system_interfaces.h"


//the stream interface objects we expose.
extern const IOStreamIF g_pifUART1;


//these init methods are intended to be called once; they initialize internal
//structures (e.g. queues).  Because of the nature of STM32CubeMX, there is
//also some other init that is done in main.c that is generated code.
void UART1_Init ( void );


//these are optional callbacks that you can implement to catch these events.
//Note, these are generally called at ISR time.
void UART1_DataAvailable ( void );
void UART1_TransmitEmpty ( void );


//these are debug methods for tuning buffer sizes
#ifdef DEBUG
unsigned int UART1_txbuff_max ( void );
unsigned int UART1_rxbuff_max ( void );
#endif


#ifdef __cplusplus
}
#endif

#endif
