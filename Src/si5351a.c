//==============================================================
//This provides a simplified interface to the Si5351A3 device.
//This is part of the CarelessWSPR project.
//This was derived from code found at 
//https://www.qrp-labs.com/synth/si5351ademo.html

// 
// Author: Hans Summers, 2015
// Website: http://www.hanssummers.com
//
// A very very simple Si5351a demonstration
// using the Si5351a module kit http://www.hanssummers.com/synth
// Please also refer to SiLabs AN619 which describes all the registers to use
//
#include "si5351a.h"
#include "main.h"
#include "stm32f1xx_hal.h"

//address 0x60 or 0x61
#define SI3251_ADDR 0x60



extern I2C_HandleTypeDef hi2c1;


//Note:  first byte must be starting register address
int impl_writeSeveral ( const uint8_t* data, size_t len )
{
	//XXX calc timeout based on len? we will never be sending anything that big, though (~50 by / ms)
	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit ( &hi2c1, SI3251_ADDR<<1, (uint8_t*)data, len, 50 );
	if ( HAL_OK != ret )
	{
		return 0;	//XXX horror
	}
	return 1;
}


int impl_writeOne ( uint8_t reg, uint8_t data )
{
	uint8_t img[2];
	img[0] = reg;
	img[1] = data;
	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit ( &hi2c1, SI3251_ADDR<<1, img, 2, 2 );
	if ( HAL_OK != ret )
	{
		return 0;	//XXX horror
	}
	return 1;
}


uint8_t impl_ReadOne ( uint8_t reg )
{
	uint8_t val;
	HAL_StatusTypeDef ret;

	ret = HAL_I2C_Master_Transmit ( &hi2c1, SI3251_ADDR<<1, &reg, 1, 2 );
	if ( HAL_OK != ret )
	{
		return 0;	//XXX horror
	}
	ret = HAL_I2C_Master_Receive ( &hi2c1, SI3251_ADDR<<1, &val, sizeof(val), 2 );
	if ( HAL_OK != ret )
	{
		return 0;	//XXX horror
	}

	return val;
}



int si5351aIsPresent ( void )
{
	HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady (&hi2c1, SI3251_ADDR<<1, 2, 2);
	return ( HAL_OK == ret );	//someone ack'ed address SI3251_ADDR
}



uint8_t si5351aStatus ( void )
{
	return impl_ReadOne ( SI_STATUS );
}



// Set up specified PLL with mult, num and denom
// mult is 15..90
// num is 0..1,048,575 (0xFFFFF)
// denom is 0..1,048,575 (0xFFFFF)
void setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
	uint32_t P1;	// PLL config register P1
	uint32_t P2;	// PLL config register P2
	uint32_t P3;	// PLL config register P3

	P1 = (uint32_t)(128 * ((float)num / (float)denom));
	P1 = (uint32_t)(128 * (uint32_t)(mult) + P1 - 512);
	P2 = (uint32_t)(128 * ((float)num / (float)denom));
	P2 = (uint32_t)(128 * num - denom * P2);
	P3 = denom;

	impl_writeOne(pll + 0, (P3 & 0x0000FF00) >> 8);
	impl_writeOne(pll + 1, (P3 & 0x000000FF));
	impl_writeOne(pll + 2, (P1 & 0x00030000) >> 16);
	impl_writeOne(pll + 3, (P1 & 0x0000FF00) >> 8);
	impl_writeOne(pll + 4, (P1 & 0x000000FF));
	impl_writeOne(pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
	impl_writeOne(pll + 6, (P2 & 0x0000FF00) >> 8);
	impl_writeOne(pll + 7, (P2 & 0x000000FF));
}



// Set up MultiSynth with integer divider and R divider
// R divider is the bit value which is OR'ed onto the appropriate register, it is a #define in si5351a.h
void setupMultisynth(uint8_t synth, uint32_t divider, uint8_t rDiv)
{
	uint32_t P1;	// Synth config register P1
	uint32_t P2;	// Synth config register P2
	uint32_t P3;	// Synth config register P3

	P1 = 128 * divider - 512;
	P2 = 0;			// P2 = 0, P3 = 1 forces an integer value for the divider
	P3 = 1;

	impl_writeOne(synth + 0,   (P3 & 0x0000FF00) >> 8);
	impl_writeOne(synth + 1,   (P3 & 0x000000FF));
	impl_writeOne(synth + 2,   ((P1 & 0x00030000) >> 16) | rDiv);
	impl_writeOne(synth + 3,   (P1 & 0x0000FF00) >> 8);
	impl_writeOne(synth + 4,   (P1 & 0x000000FF));
	impl_writeOne(synth + 5,   ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
	impl_writeOne(synth + 6,   (P2 & 0x0000FF00) >> 8);
	impl_writeOne(synth + 7,   (P2 & 0x000000FF));
}



// Switches off Si5351a output
// Example: si5351aOutputOff(SI_CLK0_CONTROL);
// will switch off output CLK0
void si5351aOutputOff(uint8_t clk)
{
	//disable the clock so we get the desired output level
	uint8_t val = impl_ReadOne ( SI_CLK_DISABLE );
	val |= (1<<(clk-SI_CLK0_CONTROL));
	impl_writeOne ( SI_CLK_DISABLE, val );

	impl_writeOne ( clk, 0x80 );	// Refer to SiLabs AN619 to see bit values - 0x80 turns off the output stage
}



void si5351aCalcParams ( SYNTH_PARAMS* pparams, uint64_t freqCentiHz, int32_t nSynthCorrPPM )
{
	//these are intermediates for computing the params
	uint64_t pllFreq;
	uint32_t xtalFreq = XTAL_FREQ;
	uint32_t l;
	float f;

	//see if our chosen frequency is too low, and if we need to use some
	//of the final output 'R' dividers
	if ( freqCentiHz < 800000ULL )	//< 8 KHz
	{
		pparams->rDiv = SI_R_DIV_128;
		freqCentiHz *= 128ULL;
	}
	else if ( ( freqCentiHz >= 800000ULL ) && ( freqCentiHz < 1600000ULL ) )	//8-16 KHz
	{
		pparams->rDiv = SI_R_DIV_64;
		freqCentiHz *= 64ULL;
	}
	else if ( ( freqCentiHz >= 1600000ULL ) && ( freqCentiHz < 3200000ULL ) )	//16-32 KHz
	{
		pparams->rDiv = SI_R_DIV_32;
		freqCentiHz *= 32ULL;
	}
	else if ( ( freqCentiHz >= 3200000ULL ) && ( freqCentiHz < 6400000ULL ) )	//32-64 KHz
	{
		pparams->rDiv = SI_R_DIV_16;
		freqCentiHz *= 16ULL;
	}
	else if ( ( freqCentiHz >= 6400000ULL ) && ( freqCentiHz < 12800000ULL ) )	//64-128 KHz
	{
		pparams->rDiv = SI_R_DIV_8;
		freqCentiHz *= 8ULL;
	}
	else if ( ( freqCentiHz >= 12800000ULL ) && ( freqCentiHz < 25600000ULL ) )	//128-256 KHz
	{
		pparams->rDiv = SI_R_DIV_4;
		freqCentiHz *= 4ULL;
	}
	else if ( ( freqCentiHz >= 25600000ULL ) && ( freqCentiHz < 51200000ULL ) )	//256-512 KHz
	{
		pparams->rDiv = SI_R_DIV_2;
		freqCentiHz *= 2ULL;
	}
	else
	{
		pparams->rDiv = SI_R_DIV_1;
	}

	// Calculate the division ratio. 900,000,000 is the maximum internal 
	pparams->divider = (uint32_t)(90000000000ULL / freqCentiHz);
	// PLL frequency: 900MHz
	if (pparams->divider % 2)
	{
		pparams->divider--;		// Ensure an even integer division ratio
	}

	// Calculate the pllFrequency: the divider * desired output frequency
	pllFreq = pparams->divider * freqCentiHz / 100;

	// Determine the multiplier to get to the required pllFrequency
	pparams->mult = pllFreq / xtalFreq;
	l = pllFreq % xtalFreq;		// It has three parts:
	f = l;						// mult is an integer that must be in the range 15..90
	f *= 1048575;				// num and denom are the fractional parts, the numerator and denominator
	f /= xtalFreq;				// each is 20 bits (range 0..1048575)

f += nSynthCorrPPM * 1048575.0F / 1000000.0F;
if ( f > 1048575 )
{
	f -= 1048575;
	++pparams->mult;
}
else if ( f < 0 )
{
	f += 1048575;
	--pparams->mult;
}

	pparams->num = f;			// the actual multiplier is  mult + num / denom
	pparams->denom = 1048575;	// For simplicity we set the denominator to the maximum 1048575
}


/*
//XXX
int32_t _nSynthCorrPPM = 0;

	uint32_t xtalFreq = XTAL_FREQ;
	// Determine the multiplier to get to the required pllFrequency
	// It has three parts:
	// mult is an integer that must be in the range 15..90
	// num and denom are the fractional parts, the numerator and denominator
	// each is 20 bits (range 0..1048575)
	// the actual multiplier is  mult + num / denom
	// For simplicity we set the denominator to the maximum 1048575
	pparams->mult = pllFreq / xtalFreq;		//'a'
	float f = (pllFreq % xtalFreq) * 1048575.0F / xtalFreq;
	if ( _nSynthCorrPPM < 0 )
	{
		f += _nSynthCorrPPM;
	}
	else
	{
		f -= _nSynthCorrPPM;
		--pparams->mult;
	}
	pparams->num = f;
	pparams->denom = 1048575;
*/


void si5351aInit ( void )
{
	uint8_t val;
	
	//setup the crystal oscillator
	impl_writeOne ( SI_XTAL_LOAD, (2<<6)|0b010010 );	//8pf
//	impl_writeOne ( SI_XTAL_LOAD, (3<<6)|0b010010 );	//10pf

	//set the disabled state for CLK0 to 'low'
	val = impl_ReadOne ( SI_CLK30_DISSTAT );
	val &= ~(3 << (0 * 2));
	val |= SI_CLKDISSTAT_LOW << (0 * 2);
	impl_writeOne ( SI_CLK30_DISSTAT, val );

	//disable the clock so we get the desired output level
	val = impl_ReadOne ( SI_CLK_DISABLE );
	val |= (1<<(SI_CLK0_CONTROL-SI_CLK0_CONTROL));
	impl_writeOne ( SI_CLK_DISABLE, val );
}



// Set CLK0 output ON and to the specified frequency (in cHz)
//
// This example sets up PLL A
// and MultiSynth 0
// and produces the output on CLK0
void si5351aSetFrequency ( uint64_t freqCentiHz, int32_t nSynthCorrPPM, int bResetPLL )
{
	SYNTH_PARAMS params;
	si5351aCalcParams ( &params, freqCentiHz, nSynthCorrPPM );

	// Set up PLL A with the calculated multiplication ratio
	setupPLL ( SI_SYNTH_PLL_A, params.mult, params.num, params.denom );

	// Set up MultiSynth divider 0, with the calculated divider. 
	// The final R division stage can divide by a power of two, from 1..128. 
	// represented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
	// If you want to output frequencies below 0.512 MHz, you have to use the 
	// final R division stage
	setupMultisynth ( SI_SYNTH_MS_0, params.divider, params.rDiv );

	// Reset the PLL. This causes a glitch in the output. For small changes to 
	// the parameters, you don't need to reset the PLL, and there is no glitch
	if ( bResetPLL )	//only if requested
	{
		impl_writeOne ( SI_PLL_RESET, 0xA0 );
	}

	//set the disabled state for CLK0 to 'low'
	uint8_t val = impl_ReadOne ( SI_CLK30_DISSTAT );
	val &= ~(3 << (0 * 2));
	val |= SI_CLKDISSTAT_LOW << (0 * 2);
	impl_writeOne ( SI_CLK30_DISSTAT, val );

	//enable the clock
	val = impl_ReadOne ( SI_CLK_DISABLE );
	val &= ~(1<<(SI_CLK0_CONTROL-SI_CLK0_CONTROL));
	impl_writeOne ( SI_CLK_DISABLE, val );

	// Finally switch on the CLK0 output (0x4F)
	// and set the MultiSynth0 input to be PLL A
	impl_writeOne ( SI_CLK0_CONTROL, 0x4F | SI_CLK_SRC_PLL_A );
}

