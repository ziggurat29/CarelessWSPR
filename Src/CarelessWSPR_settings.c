//==============================================================
//Persistent settings object for the CarelessWSPR project.
//impl

#include "CarelessWSPR_settings.h"

#include "main.h"
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#include <string.h>



static PersistentSettings g_settings;	//our settings!

//we round up the size of the persistent setting structure to the 32-bit bound.
#define SETTINGS_SIZE_ADJUSTED (((sizeof(PersistentSettings)+(sizeof(uint32_t)-1))/sizeof(uint32_t))*sizeof(uint32_t))

//YYY is there a define for the flash size?  couldn't find it.  64K on this device
//in this implementation, we are only going to use one page.  On this device,
//that is 1K.  It should be somewhat easy to support more than one page if the
//struct gets ridiculously large.
#ifndef FLASH_PAGE_SIZE
#error Unknown Flash Page Size
#endif
//HHH I modded this to get 128K #define FLASH_SETTINGS_END_ADDR 0x08010000
#define FLASH_SETTINGS_END_ADDR 0x08020000
#define FLASH_SETTINGS_START_ADDR (FLASH_SETTINGS_END_ADDR-FLASH_PAGE_SIZE)
#define MAX_SETTINGS_PER_PAGE (FLASH_PAGE_SIZE/SETTINGS_SIZE_ADJUSTED)




static const PersistentSettings* _findLastValidSettings ( void )
{
	const uint8_t* pbyThisSettings;
	const uint8_t* pbyLastSettings = NULL;
	for ( pbyThisSettings = (const uint8_t*)FLASH_SETTINGS_START_ADDR;
			pbyThisSettings <= (const uint8_t*)(FLASH_SETTINGS_END_ADDR-SETTINGS_SIZE_ADJUSTED);
			pbyThisSettings += SETTINGS_SIZE_ADJUSTED )
	{
		//is this erased area?
		if ( 0xffffffff == ((const PersistentSettings*)pbyThisSettings)->_version )
		{
			//ran out of settings; got to (presumably) erased area
			break;
		}
		if ( PERSET_VERSION != ((const PersistentSettings*)pbyThisSettings)->_version )
		{
			//invalid settings; old.  erase settings page
			pbyLastSettings = NULL;
			break;
		}
		//otherwise, we presume it's valid, so let's remember where we found it.
		pbyLastSettings = pbyThisSettings;
	}
	
	return (const PersistentSettings*) pbyLastSettings;
}




//========================================================================



//get our RAM based persistent settings structure
PersistentSettings* Settings_getStruct ( void )
{
	return &g_settings;
}



//depersist settings from flash, or default values if no persisted value
//return true on depersisted from flash, or false if defaulted
int Settings_depersist ( void )
{
	//scan for last valid settings
	const PersistentSettings* pLastSettings = _findLastValidSettings();

	//if we found it, copy it in.  otherwise default the values
	if ( NULL != pLastSettings )
	{
		memcpy ( &g_settings, pLastSettings, sizeof(g_settings) );
	}
	else
	{
		Settings_restoreDefaults();
	}

	return (NULL != pLastSettings);	//would be non-null if depersisted
}



//persist RAM based settings to flash
//return true on success or false if fail
int Settings_persist ( void )
{
	//first, scan for last valid settings
	const uint8_t* pNewSettings = (const uint8_t*) _findLastValidSettings();
	const uint8_t* pNewSettingsEnd = NULL;
	if ( NULL == pNewSettings )
	{
		//maybe because it is totally erased? verify all words erased
		const uint32_t* pnValue;
		for ( pnValue = (const uint32_t*)FLASH_SETTINGS_START_ADDR;
				pnValue < (const uint32_t*)FLASH_SETTINGS_END_ADDR;
				pnValue += sizeof(uint32_t) )
		{
			if ( 0xffffffff != *pnValue )
				break;
		}

		//if it's already erased, we can go straight to programming
		if ( (uint32_t)pnValue >= FLASH_SETTINGS_END_ADDR )
		{
			//new one will be at beginning
			pNewSettings = (const uint8_t*) FLASH_SETTINGS_START_ADDR;
			pNewSettingsEnd = pNewSettings + SETTINGS_SIZE_ADJUSTED;
		}
	}
	else
	{
		//new one will be one beyond (if it fits)
		pNewSettings = pNewSettings + SETTINGS_SIZE_ADJUSTED;
		pNewSettingsEnd = pNewSettings + SETTINGS_SIZE_ADJUSTED;
		if ( pNewSettingsEnd > (const uint8_t*)FLASH_SETTINGS_END_ADDR )
		{
			pNewSettings = NULL;	//last struct cannot fall off page
		}
	}

	//if we couldn't find a suitable place, we erase the settings page and
	//start at the beginning.
	if ( NULL == pNewSettings )
	{
		FLASH_EraseInitTypeDef EraseInitStruct;
		uint32_t PAGEError = 0;

		HAL_FLASH_Unlock();

		EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
		EraseInitStruct.PageAddress = FLASH_SETTINGS_START_ADDR;
		EraseInitStruct.NbPages = (FLASH_SETTINGS_END_ADDR - FLASH_SETTINGS_START_ADDR) / FLASH_PAGE_SIZE;
		if ( HAL_FLASHEx_Erase ( &EraseInitStruct, &PAGEError ) != HAL_OK )
		{
			HAL_FLASH_Lock();
			Error_Handler();	//won't return
			return 0;
		}

		HAL_FLASH_Lock();

		//new one will be at beginning
		pNewSettings = (const uint8_t*) FLASH_SETTINGS_START_ADDR;
		pNewSettingsEnd = pNewSettings + SETTINGS_SIZE_ADJUSTED;
	}

	//now we should have our programming location defined
	//assert NULL != pNewSettings
	{
		const uint8_t* pnVal = (const uint8_t*)&g_settings;
		HAL_FLASH_Unlock();

		while ( pNewSettings < pNewSettingsEnd )
		{
			if ( HAL_FLASH_Program ( FLASH_TYPEPROGRAM_WORD,
					(uint32_t)pNewSettings,
					*(const uint32_t*)pnVal ) == HAL_OK )
			{
				pNewSettings += sizeof(uint32_t);
				pnVal += sizeof(uint32_t);
			}
			else
			{
				HAL_FLASH_Lock();
				Error_Handler();	//won't return
				return 0;
			}
		}

		HAL_FLASH_Lock();
	}

	return 1;
}



const PersistentSettings g_defaultSettings = 
{
	._version = PERSET_VERSION,	//must be this
	._dialFreqHz = 14095600,	//the 20-meter conventional WSPR channel
	._nSubBand = -1,
	._nDutyPct = 20,
	._achCallSign = "",			//you must set this
	._achMaidenhead = "",		//you must set this
	._nTxPowerDbm = 10,			//10 mW
	._bUseGPS = 1,
	._nGPSbitRate = 9600,		//default for the ublox NEO-6M
	._nSynthCorrPPM = 0,		//initially uncorrected
};


//change the RAM-based settings to the out-of-box defaults.  Note, does not
//persist the changes; you must do that separately if desired.
void Settings_restoreDefaults ( void )
{
	g_settings = g_defaultSettings;
}


