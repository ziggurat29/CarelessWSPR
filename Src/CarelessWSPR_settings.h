//==============================================================
//Persistent settings object for the CarelessWSPR project.
//This defines a struct containing the persistent (power on-to-power on)
//settings that will be persisted in the last page of flash.
//It also contains the logic for persisting a modified version of the struct,
//and an elementary wear avoidance algorithm.

#ifndef __CARELESSWSPR_SETTINGS_H
#define __CARELESSWSPR_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

//This serves as a signature of the version structure; you should change it
//when the structure changes so that the firmware can gracefully recognize
//old-formatted data.  Just don't use 0xffffffff, since that's how we test
//for an erased area.
#define PERSET_VERSION	1


//The persistent settings are stored in the last flash page.  It is simply a
//fixed-size structure.  Updates are written sequentially to that page, and
//the current version is the last version written.  Updating when there is
//no more room will trigger an erase of the page before starting over at the
//beginning.  This has some implications:
//1  wear is reduced by a factor of the number of structures that will fit on
//   that page (the page size for this device is 1 KiB).
//2  write time is non-deterministic.
//   It is not yet clear to me if there are synchronization aspects while a
//   flash erase or write cycle is being performed relative to instruction
//   fetches coming out of other areas of flash.
typedef struct
{
	//============================
	uint32_t	_version;	//should always be first, should be PERSET_VERSION

	//'dial' frequency for the WSPR channel.  WSPR works in a USB narrow
	//(200 Hz) band within a conventional USB channel.  The center of that
	//200 Hz band is 1.5 KHz above the dial frequency.
	uint32_t	_dialFreqHz;
	//the WSPR signal is extremely narrow-band (6 Hz).  The 200 Hz WSPR band
	//can accommodate 33 1/3 of these 6 Hz sub-bands.  We can be configured to
	//use a specific one, or a negative number means randomly pick one at
	//transmit time (the usual case).
	int32_t		_nSubBand;		//0-32; or < 0 to randomize
	//duty cycle (i.e. how often to try to transmit, randomized)
	uint32_t	_nDutyPct;		//percent

	//call sign
	char		_achCallSign[8];	//6 chars max
	//explicit grid locator
	char		_achMaidenhead[8];	//4 chars always
	//transmit power level
	int32_t		_nTxPowerDbm;		//0-60, though only 0, 1, 3, 7 endings

	//use GPS (i.e. auto time sync auto grid locator, and wait-for-lock)
	uint32_t	_bUseGPS;			//boolean
	//GPS bit rate
	int32_t		_nGPSbitRate;		//9600 default, but can be other

} PersistentSettings;



//get our RAM based persistent settings structure
PersistentSettings* Settings_getStruct ( void );

//depersist settings from flash, or default values if no persisted value
//return true on depersisted from flash, or false if defaulted
int Settings_depersist ( void );

//persist RAM based settings to flash
//return true on success or false if fail
int Settings_persist ( void );

//change the RAM-based settings to the out-of-box defaults.  Note, does not
//persist the changes; you must do that separately if desired.
void Settings_restoreDefaults ( void );



#ifdef __cplusplus
}
#endif

#endif
