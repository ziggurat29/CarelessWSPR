//==============================================================
//This provides a simplified interface to the Si5351A3 device.
//This is part of the CarelessWSPR project.
//This was derived from code found at 
//https://www.qrp-labs.com/synth/si5351ademo.html
#ifndef __SI5351A_H
#define __SI5351A_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

// Register definitions
#define SI_STATUS			0
#define SI_CLK_DISABLE		3	//1 in bit position disables respective clock
#define SI_CLK0_CONTROL		16	//register starts for the clock control registers
#define SI_CLK1_CONTROL		17
#define SI_CLK2_CONTROL		18

#define SI_CLK30_DISSTAT	24	//the disabled state of the first 4 clocks
#define SI_CLKDISSTAT_LOW	0
#define SI_CLKDISSTAT_HIGH	1
#define SI_CLKDISSTAT_FLOAT	2
#define SI_CLKDISSTAT_NEVER	3

#define SI_SYNTH_PLL_A		26	//register starts for the plls
#define SI_SYNTH_PLL_B		34
#define SI_SYNTH_MS_0		42	//register starts for the multisynths
#define SI_SYNTH_MS_1		50
#define SI_SYNTH_MS_2		58
#define SI_PLL_RESET		177	//

#define SI_XTAL_LOAD		183	//

#define SI_R_DIV_1			0b00000000	// R-division ratio definitions
#define SI_R_DIV_2			0b00010000
#define SI_R_DIV_4			0b00100000
#define SI_R_DIV_8			0b00110000
#define SI_R_DIV_16			0b01000000
#define SI_R_DIV_32			0b01010000
#define SI_R_DIV_64			0b01100000
#define SI_R_DIV_128		0b01110000

#define SI_CLK_SRC_PLL_A	0b00000000	//
#define SI_CLK_SRC_PLL_B	0b00100000

#define XTAL_FREQ	25000000			// Crystal frequency


void si5351aInit ( void );
int si5351aIsPresent ( void );
uint8_t si5351aStatus ( void );
void si5351aOutputOff ( uint8_t clk );


typedef struct 
{
	//these are for the PLL
	uint8_t mult;
	uint32_t num;
	uint32_t denom;

	//these are for the multisynth
	uint32_t divider;
	uint8_t rDiv;
} SYNTH_PARAMS;


void si5351aCalcParams ( SYNTH_PARAMS* pparams, uint64_t freqCentiHz, int32_t nSynthCorrPPM );


void si5351aSetFrequency ( uint64_t freqCentiHz, int32_t nSynthCorrPPM, int bResetPLL );


#ifdef __cplusplus
}
#endif

#endif //__SI5351A_H
