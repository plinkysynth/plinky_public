

#include <math.h>

uint8_t wmcodec_write( uint8_t reg, uint16_t data )
{
#ifndef EMU
    unsigned char d[2];
    d[0] = (reg << 1) | ((data & 0x100) >> 8);
    d[1] = (u8)data;
//    i2c_write(0x34, d, 2);
    int attempt;
	HAL_Delay(1);
    for (attempt=0;attempt<10;++attempt) {
		HAL_StatusTypeDef r = HAL_I2C_Master_Transmit(&hi2c2, 0x34, d, 2, I2C_TIMEOUT);
		if (r==HAL_OK)
			break;
		HAL_Delay(10);
    }
    if (attempt==100) {
		DebugLog("error in wmcodec_write reg %d data %d\r\n", reg , data);
    } else {
//    	DebugLog("codec write ok after %d attempts!\r\n", attempt+1);
    }
#endif
	return 0;
}


// datasheet constants transcribed via https://github.com/mguentner/rockbox/blob/master/firmware/drivers/audio/wm8758.c

#define RESET                   0x00
#define RESET_RESET             0x0

#define PWRMGMT1                0x01 /* default 000 */
#define PWRMGMT1_VMIDSEL_OFF    (0 << 0)
#define PWRMGMT1_VMIDSEL_100K   (1 << 0)
#define PWRMGMT1_VMIDSEL_500K   (2 << 0)
#define PWRMGMT1_VMIDSEL_10K    (3 << 0)
#define PWRMGMT1_BUFIOEN        (1 << 2)
#define PWRMGMT1_BIASEN         (1 << 3)
#define PWRMGMT1_MICBEN         (1 << 4)
#define PWRMGMT1_PLLEN          (1 << 5)
#define PWRMGMT1_OUT3MIXEN      (1 << 6)
#define PWRMGMT1_OUT4MIXEN      (1 << 7)

#define PWRMGMT2                0x02 /* default 000 */
#define PWRMGMT2_ADCENL         (1 << 0)
#define PWRMGMT2_ADCENR         (1 << 1)
#define PWRMGMT2_INPGAENL       (1 << 2)
#define PWRMGMT2_INPGAENR       (1 << 3)
#define PWRMGMT2_BOOSTENL       (1 << 4)
#define PWRMGMT2_BOOSTENR       (1 << 5)
#define PWRMGMT2_SLEEP          (1 << 6)
#define PWRMGMT2_LOUT1EN        (1 << 7)
#define PWRMGMT2_ROUT1EN        (1 << 8)

#define PWRMGMT3                0x03 /* default 000 */
#define PWRMGMT3_DACENL         (1 << 0)
#define PWRMGMT3_DACENR         (1 << 1)
#define PWRMGMT3_LMIXEN         (1 << 2)
#define PWRMGMT3_RMIXEN         (1 << 3)
#define PWRMGMT3_ROUT2EN        (1 << 5)
#define PWRMGMT3_LOUT2EN        (1 << 6)
#define PWRMGMT3_OUT3EN         (1 << 7)
#define PWRMGMT3_OUT4EN         (1 << 8)

#define AINTFCE                 0x04 /* default 050 */
#define AINTFCE_MONO            (1 << 0)
#define AINTFCE_ALRSWAP         (1 << 1)
#define AINTFCE_DLRSWAP         (1 << 2)
#define AINTFCE_FORMAT_MSB_RJUST (0 << 3)
#define AINTFCE_FORMAT_MSB_LJUST (1 << 3)
#define AINTFCE_FORMAT_I2S      (2 << 3) /* default */
#define AINTFCE_FORMAT_DSP      (3 << 3)
#define AINTFCE_FORMAT_MASK     (3 << 3)
#define AINTFCE_IWL_16BIT       (0 << 5)
#define AINTFCE_IWL_20BIT       (1 << 5)
#define AINTFCE_IWL_24BIT       (2 << 5) /* default */
#define AINTFCE_IWL_32BIT       (3 << 5)
#define AINTFCE_IWL_MASK        (3 << 5)
#define AINTFCE_LRP             (1 << 7)
#define AINTFCE_BCP             (1 << 8)

#define COMPCTRL                0x05 /* default 000 unused */

#define CLKCTRL                 0x06 /* default 140 */
#define CLKCTRL_MS              (1 << 0)
#define CLKCTRL_BCLKDIV_1       (0 << 2)
#define CLKCTRL_BCLKDIV_2       (1 << 2)
#define CLKCTRL_BCLKDIV_4       (2 << 2)
#define CLKCTRL_BCLKDIV_8       (3 << 2)
#define CLKCTRL_BCLKDIV_16      (4 << 2)
#define CLKCTRL_BCLKDIV_32      (5 << 2)
#define CLKCTRL_MCLKDIV_1       (0 << 5)
#define CLKCTRL_MCLKDIV_1_5     (1 << 5)
#define CLKCTRL_MCLKDIV_2       (2 << 5) /* default */
#define CLKCTRL_MCLKDIV_3       (3 << 5)
#define CLKCTRL_MCLKDIV_4       (4 << 5)
#define CLKCTRL_MCLKDIV_6       (5 << 5)
#define CLKCTRL_MCLKDIV_8       (6 << 5)
#define CLKCTRL_MCLKDIV_12      (7 << 5)
#define CLKCTRL_MCLKDIV_MASK    (7 << 5)
#define CLKCTRL_CLKSEL          (1 << 8) /* default */

#define ADDCTRL                 0x07 /* default 000 */
#define ADDCTRL_SLOWCLKEN       (1 << 0)
#define ADDCTRL_SR_48kHz        (0 << 1)
#define ADDCTRL_SR_32kHz        (1 << 1)
#define ADDCTRL_SR_24kHz        (2 << 1)
#define ADDCTRL_SR_16kHz        (3 << 1)
#define ADDCTRL_SR_12kHz        (4 << 1)
#define ADDCTRL_SR_8kHz         (5 << 1)
#define ADDCTRL_SR_MASK         (7 << 1)
#define ADDCTRL_M128ENB         (1 << 8)

#define GPIOCTRL                0x08 /* default 000 unused */
#define JACKDETECTCTRL1         0x09 /* default 000 unused */

#define DACCTRL                 0x0a /* default 000 */
#define DACCTRL_DACLPOL         (1 << 0)
#define DACCTRL_DACRPOL         (1 << 1)
#define DACCTRL_AMUTE           (1 << 2)
#define DACCTRL_DACOSR128       (1 << 3)
#define DACCTRL_SOFTMUTE        (1 << 6)

#define LDACVOL                 0x0b /* default 0ff */
#define LDACVOL_MASK            0xff
#define LDACVOL_DACVU           (1 << 8)

#define RDACVOL                 0x0c /* default 0ff */
#define RDACVOL_MASK            0xff
#define RDACVOL_DACVU           (1 << 8)

#define JACKDETECTCTRL2         0x0d /* default 000 unused */

#define ADCCTRL                 0x0e /* default 100 */
#define ADCCTRL_ADCLPOL         (1 << 0)
#define ADCCTRL_ADCRPOL         (1 << 1)
#define ADCCTRL_ADCOSR128       (1 << 3)
#define ADCCTRL_HPFCUT_MASK     (7 << 4)
#define ADCCTRL_HPFAPP          (1 << 7)
#define ADCCTRL_HPFEN           (1 << 8) /* default */

#define LADCVOL                 0x0f /* default 0ff */
#define LADCVOL_MASK            0xff
#define LADCVOL_ADCVU           (1 << 8)

#define RADCVOL                 0x10 /* default 0ff */
#define RADCVOL_MASK            0xff
#define RADCVOL_ADCVU           (1 << 8)

#define EQ1                     0x12 /* default 12c */
#define EQ2                     0x13 /* default 02c */
#define EQ3                     0x14 /* default 02c */
#define EQ4                     0x15 /* default 02c */
#define EQ5                     0x16 /* default 02c */
/* note: WM8758 curruently runs on low power mode. 3 peaking filters
 * and 3D will work when M128ENB is enabled + proper code. */
#define EQ1_EQ3DMODE            (1 << 8) /* default */
#define EQ_GAIN_MASK            0x1f
#define EQ_CUTOFF_MASK          (3 << 5)
#define EQ_GAIN_VALUE(x)        (((-x) + 12) & 0x1f)
#define EQ_CUTOFF_VALUE(x)      ((((x) - 1) & 0x03) << 5)

#define DACLIMITER1             0x18 /* default 032 unused */
#define DACLIMITER2             0x19 /* default 000 unused */
#define NOTCHFILTER1            0x1b /* default 000 unused */
#define NOTCHFILTER2            0x1c /* default 000 unused */
#define NOTCHFILTER3            0x1d /* default 000 unused */
#define NOTCHFILTER4            0x1e /* default 000 unused */
#define ALCCONTROL1             0x20 /* default 038 unused */
#define ALCCONTROL2             0x21 /* default 00b unused */
#define ALCCONTROL3             0x22 /* default 032 unused */
#define NOISEGATE               0x23 /* default 000 unused */
#if 0
#define PLLN                    0x24 /* default 008 */
#define PLLN_PLLN_MASK          0x0f
#define PLLN_PLLPRESCALE        (1 << 4)

#define PLLK1                   0x25 /* default 00c */
#define PLLK1_MASK              0x3f

#define PLLK2                   0x26 /* default 093 */
#define PLLK3                   0x27 /* default 0e9 */
#endif

#define THREEDCTRL              0x29 /* default 000 */
#define THREEDCTRL_DEPTH3D_MASK 0x0f

#define OUT4TOADC               0x2a /* default 000 */
#define OUT4TOADC_OUT1DEL       (1 << 0)
#define OUT4TOADC_DELEN         (1 << 1)
#define OUT4TOADC_POBCTRL       (1 << 2)
#define OUT4TOADC_OUT2DEL       (1 << 3)
#define OUT4TOADC_VMIDTOG       (1 << 4)
#define OUT4TOADC_OUT4_2LNR     (1 << 5)
#define OUT4TOADC_OUT4_ADCVOL_MASK (7 << 6)

#define BEEPCTRL                0x2b /* default 000 */
#define BEEPCTRL_DELEN2         (1 << 2)
#define BEEPCTRL_BYPR2LMIX      (1 << 7)
#define BEEPCTRL_BYPL2RMIX      (1 << 8)

#define INCTRL                  0x2c /* default 003 */
#define INCTRL_LIP2INPGA        (1 << 0) /* default */
#define INCTRL_LIN2INPGA        (1 << 1) /* default */
#define INCTRL_L2_2INPGA        (1 << 2)
#define INCTRL_RIP2INPGA        (1 << 4)
#define INCTRL_RIN2INPGA        (1 << 5)
#define INCTRL_R2_2INPGA        (1 << 6)
#define INCTRL_MBVSEL           (1 << 8)

#define LINPGAVOL               0x2d /* default 010 */
#define LINPGAVOL_INPGAVOL_MASK 0x3f
#define LINPGAVOL_INPGAMUTEL    (1 << 6)
#define LINPGAVOL_INPGAZCL      (1 << 7)
#define LINPGAVOL_INPGAVU       (1 << 8)

#define RINPGAVOL               0x2e /* default 010 */
#define RINPGAVOL_INPGAVOL_MASK 0x3f
#define RINPGAVOL_INPGAMUTER    (1 << 6)
#define RINPGAVOL_INPGAZCR      (1 << 7)
#define RINPGAVOL_INPGAVU       (1 << 8)

#define LADCBOOST               0x2f /* default 100 */
#define LADCBOOST_L2_2BOOST_MASK  (7 << 4)
#define LADCBOOST_L2_2BOOST(x)  ((x) << 4)
#define LADCBOOST_PGABOOSTL     (1 << 8) /* default */

#define RADCBOOST               0x30 /* default 100 */
#define RADCBOOST_R2_2BOOST_MASK  (7 << 4)
#define RADCBOOST_R2_2BOOST(x)  ((x) << 4)
#define RADCBOOST_PGABOOSTR     (1 << 8) /* default */

#define OUTCTRL                 0x31 /* default 002 */
#define OUTCTRL_VROI            (1 << 0)
#define OUTCTRL_TSDEN           (1 << 1) /* default */
#define OUTCTRL_TSOPCTRL        (1 << 2)
#define OUTCTRL_OUT3ENDEL       (1 << 3)
#define OUTCTRL_OUT4ENDEL       (1 << 4)
#define OUTCTRL_DACR2LMIX       (1 << 5)
#define OUTCTRL_DACL2RMIX       (1 << 6)
#define OUTCTRL_LINE_COM        (1 << 7)
#define OUTCTRL_HP_COM          (1 << 8)

#define LOUTMIX                 0x32 /* default 001 */
#define LOUTMIX_DACL2LMIX       (1 << 0) /* default */
#define LOUTMIX_BYPL2LMIX       (1 << 1)
#define LOUTMIX_BYP2LMIXVOL_MASK (7 << 2)
#define LOUTMIX_BYP2LMIXVOL(x)  ((x) << 2)

#define ROUTMIX                 0x33 /* default 001 */
#define ROUTMIX_DACR2RMIX       (1 << 0) /* default */
#define ROUTMIX_BYPR2RMIX       (1 << 1)
#define ROUTMIX_BYP2RMIXVOL_MASK (7 << 2)
#define ROUTMIX_BYP2RMIXVOL(x)  ((x) << 2)

#define LOUT1VOL                0x34 /* default 039 */
#define LOUT1VOL_MASK           0x3f
#define LOUT1VOL_LOUT1MUTE      (1 << 6)
#define LOUT1VOL_LOUT1ZC        (1 << 7)
#define LOUT1VOL_OUT1VU         (1 << 8)

#define ROUT1VOL                0x35 /* default 039 */
#define ROUT1VOL_MASK           0x3f
#define ROUT1VOL_ROUT1MUTE      (1 << 6)
#define ROUT1VOL_ROUT1ZC        (1 << 7)
#define ROUT1VOL_OUT1VU         (1 << 8)

#define LOUT2VOL                0x36 /* default 039 */
#define LOUT2VOL_MASK           0x3f
#define LOUT2VOL_LOUT2MUTE      (1 << 6)
#define LOUT2VOL_LOUT2ZC        (1 << 7)
#define LOUT2VOL_OUT2VU         (1 << 8)

#define ROUT2VOL                0x37 /* default 039 */
#define ROUT2VOL_MASK           0x3f
#define ROUT2VOL_ROUT2MUTE      (1 << 6)
#define ROUT2VOL_ROUT2ZC        (1 << 7)
#define ROUT2VOL_OUT2VU         (1 << 8)

#define OUT3MIX                 0x38 /* default 001 */
#define OUT3MIX_LDAC2OUT3       (1 << 0) /* default */
#define OUT3MIX_LMIX2OUT3       (1 << 1)
#define OUT3MIX_BYPL2OUT3       (1 << 2)
#define OUT3MIX_OUT4_2OUT3      (1 << 3)
#define OUT3MIX_OUT3MUTE        (1 << 6)

#define OUT4MIX                 0x39 /* default 001 */
#define OUT4MIX_RDAC2OUT4       (1 << 0) /* default */
#define OUT4MIX_RMIX2OUT4       (1 << 1)
#define OUT4MIX_BYPR2OUT4       (1 << 2)
#define OUT4MIX_LDAC2OUT4       (1 << 3)
#define OUT4MIX_LMIX2OUT4       (1 << 4)
#define OUT4MIX_OUT4ATTN        (1 << 5)
#define OUT4MIX_OUT4MUTE        (1 << 6)
#define OUT4MIX_OUT3_2OUT4      (1 << 7)

#define BIASCTRL                0x3d /* default 000 */
#define BIASCTRL_HALFOPBIAS     (1 << 0)
#define BIASCTRL_HALFI_IPGA     (1 << 6)
#define BIASCTRL_BIASCUT        (1 << 8)



uint8_t wmcodec_write( uint8_t reg, uint16_t data );
void uitick(u32 *dst, const u32 *src, int half);

static short txbuf[BLOCK_SAMPLES*4];
static short rxbuf[BLOCK_SAMPLES*4];
short* getrxbuf(void) { return rxbuf; }

#ifndef EMU



void HAL_SAI_RxCpltCallback (SAI_HandleTypeDef * hi2s) {
	uitick(((u32*)txbuf)+BLOCK_SAMPLES, ((u32*)rxbuf)+BLOCK_SAMPLES, 1);
}
void HAL_SAI_RxHalfCpltCallback (SAI_HandleTypeDef * hi2s) {
	uitick((u32*)txbuf, ((u32*)rxbuf), 0);
}

#endif

static u8 curhpvol = -1;
void codec_setheadphonevol(int vol) {
	vol = clampi(vol, 0, 63);
	if (vol == curhpvol) return;
	curhpvol = vol;
	wmcodec_write(LOUT1VOL, 0x000 + vol);
	wmcodec_write(ROUT1VOL, 0x100 + vol);
}

void codec_init(void) {
	//  for (int i=0;i<N*4;++i)
	//	  txbuf[i]=32767.f * sinf(i*3.141592f/2.f/N);
#ifndef EMU
	if (HAL_OK != HAL_SAI_Receive_DMA(&hsai_BlockB1, (uint8_t*) rxbuf, sizeof(rxbuf) / 2)) {
		DebugLog("HAL_SAI_Receive_DMA fail 1\r\n");
		Error_Handler();
	}
		if (HAL_OK != HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*) txbuf, sizeof(txbuf) / 2)) {
			DebugLog("HAL_SAI_Transmit_DMA fail 1\r\n");
			Error_Handler();
		}
#endif
		HAL_Delay(1);
    wmcodec_write(0,0);
	    wmcodec_write(BIASCTRL, BIASCTRL_BIASCUT);
	    wmcodec_write(OUTCTRL, OUTCTRL_HP_COM | OUTCTRL_LINE_COM
	                         | OUTCTRL_TSOPCTRL | OUTCTRL_TSDEN | OUTCTRL_VROI);
	    wmcodec_write(LOUT1VOL, 0x140);
	    wmcodec_write(ROUT1VOL, 0x140);
	    wmcodec_write(LOUT2VOL, 0x140);
	    wmcodec_write(ROUT2VOL, 0x140);
	    wmcodec_write(OUT3MIX,  0x40);
	    wmcodec_write(OUT4MIX,  0x40);
	    // enable lout1 and rout1 via pwrmgmt2
	    wmcodec_write(PWRMGMT2, PWRMGMT2_ROUT1EN | PWRMGMT2_LOUT1EN
	    		| PWRMGMT2_BOOSTENL | PWRMGMT2_BOOSTENR
	    	//	| PWRMGMT2_INPGAENL  | PWRMGMT2_INPGAENL
				| PWRMGMT2_ADCENL | PWRMGMT2_ADCENR);
	    wmcodec_write(OUT4TOADC, OUT4TOADC_POBCTRL);
	    // enable out3 and out4 and the mixer and the dac
	    wmcodec_write(PWRMGMT3, PWRMGMT3_RMIXEN | PWRMGMT3_LMIXEN
	                          | PWRMGMT3_DACENR | PWRMGMT3_DACENL
							  | PWRMGMT3_LOUT2EN | PWRMGMT3_ROUT2EN
							  | PWRMGMT3_OUT3EN | PWRMGMT3_OUT4EN);
	    wmcodec_write(PWRMGMT1,  PWRMGMT1_BIASEN
	                          | PWRMGMT1_BUFIOEN | PWRMGMT1_VMIDSEL_10K
							  | PWRMGMT1_OUT3MIXEN | PWRMGMT1_OUT4MIXEN);

	    wmcodec_write(AINTFCE, AINTFCE_IWL_16BIT | AINTFCE_FORMAT_I2S);
	    wmcodec_write(CLKCTRL, 0); 
		
	  	wmcodec_write(ADDCTRL, ADDCTRL_SR_32kHz /*| ADDCTRL_SLOWCLKEN*/);

	    wmcodec_write(LOUTMIX, LOUTMIX_DACL2LMIX);
	    wmcodec_write(ROUTMIX, ROUTMIX_DACR2RMIX);

	    wmcodec_write(OUT4TOADC, 0);

//	    wmcodec_write(OUT3MIX, OUT3MIX_LMIX2OUT3);
//	    wmcodec_write(OUT4MIX, OUT4MIX_RMIX2OUT4);
	    wmcodec_write(OUT3MIX, OUT3MIX_OUT3MUTE);
	    wmcodec_write(OUT4MIX, OUT4MIX_OUT4MUTE);


	    HAL_Delay(100);
	    wmcodec_write(LDACVOL, 255);
	    wmcodec_write(RDACVOL, 255 | RDACVOL_DACVU);

	    int voldb=0; // 0db is about +-8v (16v peak to peak!)
	    int voldb_hp=-12;
	    // voldb goes from -57 to +6
	    wmcodec_write(LOUT1VOL, 0x100+voldb_hp+57);
	    wmcodec_write(ROUT1VOL, 0x100+voldb_hp+57);
	    wmcodec_write(LOUT2VOL, 0x100+voldb+57);
	    wmcodec_write(ROUT2VOL, 0x100+voldb+57);

		wmcodec_write(PWRMGMT1, /*PWRMGMT1_PLLEN | */PWRMGMT1_BIASEN
	                          | PWRMGMT1_BUFIOEN | PWRMGMT1_VMIDSEL_500K
							  | PWRMGMT1_OUT3MIXEN | PWRMGMT1_OUT4MIXEN);
	    wmcodec_write(BIASCTRL, 0);
		wmcodec_write(DACCTRL, DACCTRL_DACOSR128 );

		wmcodec_write(ADCCTRL,ADCCTRL_ADCOSR128 );
		wmcodec_write(LADCVOL, 255);
		wmcodec_write(RADCVOL, 255 | RADCVOL_ADCVU);

		wmcodec_write(LINPGAVOL, LINPGAVOL_INPGAMUTEL + LINPGAVOL_INPGAVU);
		wmcodec_write(RINPGAVOL, RINPGAVOL_INPGAMUTER + RINPGAVOL_INPGAVU);
		wmcodec_write(LADCBOOST, LADCBOOST_L2_2BOOST(6)); // 0db = 5; 3db steps
		wmcodec_write(RADCBOOST, RADCBOOST_R2_2BOOST(6));
//		wmcodec_write(LADCBOOST, /*LADCBOOST_PGABOOSTL | */LADCBOOST_L2_2BOOST(0)); // 0db = 5; 3db steps
//		wmcodec_write(RADCBOOST, /*RADCBOOST_PGABOOSTR | */RADCBOOST_R2_2BOOST(0));

//		wmcodec_write(INCTRL, INCTRL_L2_2INPGA |INCTRL_R2_2INPGA  );


//		int invol=63; // 6 bits; 16=0db, .75db steps; 63=+35db
//		wmcodec_write(LINPGAVOL, invol + LINPGAVOL_INPGAVU);
//		wmcodec_write(RINPGAVOL, invol + RINPGAVOL_INPGAVU);

}



