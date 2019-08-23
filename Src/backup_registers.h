//==============================================================
//This device has 10 16-bit backup registers.
//This module is part of the CarelessWSPR project.

#ifndef __BACKUP_REGISTERS_H
#define __BACKUP_REGISTERS_H

#ifdef __cplusplus
extern "C" {
#endif



//We use register 10 for our runtime flags that persist across warm boots, but
//not cold boots.
#define FLAGS_REGISTER RTC_BKP_DR10

//the bits in the FLAGS_REGISTER
#define FLAG_HAS_CONFIGED_CLOCKS 0x8000	//system clocks have been config'ed
#define FLAG_HAS_SET_RTC 0x4000			//the RTC was set to a value



#ifdef __cplusplus
}
#endif

#endif
