//==============================================================
//This various task notification values used for IPC in the system
//This module is part of the CarelessWSPR project.

#ifndef __TASK_NOTIFCATION_BITS_H
#define __TASK_NOTIFCATION_BITS_H

#ifdef __cplusplus
extern "C" {
#endif



//These bits are used for task notifications of events.
//In FreeRTOS, task notifications are much more resource-friendly than the
//heavyweights, like semaphores, though not as general.  But for most cases the
//task notifications will work fine.  (They can only support notification to a
//single process, and communicate via bitfield.  They are most similar to
//an 'event group', but can used to emulate other primitives.)

//There could be different definitions specific to the related processes, but I
//am just going to use one common definition since there are only a few
//distinct events.
typedef enum TaskNotificationBits TaskNotificationBits;
enum TaskNotificationBits
{
	//these are generally used for byte streams events
	TNB_DAV = 0x00000001,	//data is available (serial receive)
	TNB_TBMT = 0x00000002,	//transmit buffer is empty (serial send)
	//0x00000004,	//reserved; maybe for errors?
	//0x00000008,	//reserved; maybe for errors?

	//bits for the default process
	TNB_LIGHTSCHANGED = 0x00010000,		//the lights have changed

	//bits for the WSPR process
	TNB_WSPRSTART = 0x00010000,		//start the transmission
	TNB_WSPRNEXTBIT = 0x00020000,	//send next bit in transmission
	TNB_WSPR_GPSLOCK = 0x00040000,	//GPS lock status changed
};



#ifdef __cplusplus
}
#endif

#endif
