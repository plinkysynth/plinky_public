
static inline int led_gamma(int i){
	if (i<0) return 0;
	if (i>255) return 255;
	return (i*i)>>8;
}


#ifdef IMPL

//static u8 led_state_2=0;
u8 led_ram[9][8];

static u8 led_state=0;




/*
void led_init(void) {
#ifndef EMU
	  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
	  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
#ifdef NEW_PINOUT
	  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
#else
	  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
#endif
	  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
	  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
	  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
#endif
}
*/




#define P1 TIM1->CCR1
#define P2 TIM1->CCR2
#define P3 TIM1->CCR3
#define P4 TIM1->CCR4
#define P5 TIM4->CCR2
#define P6 TIM4->CCR3
#define P7 TIM2->CCR3
#define P8 TIM2->CCR4
//const uint32_t* pwmout[] = {P1, P2, P3, P4, P5, P6, P7, P8};
#define N1 2
#define N2 5
#define N3 7
#define N4 0
#define N5 15



#ifndef EMU
volatile uint8_t test_pin = 0;
typedef struct
{
	GPIO_TypeDef* gpio;
	uint32_t pin;
}pin_t;

const pin_t pintestlist[] =
{
		{GPIOD,  2}, //N4
		{GPIOD,  5}, //N3
		{GPIOD,  7}, //N2
		{GPIOD,  0}, //N1
		{GPIOD, 15}, //N5
		{GPIOE,  9}, //P1
		{GPIOA,  9}, //P2
		{GPIOA, 10}, //P3
		{GPIOE, 14}, //P4
		{GPIOD, 13}, //P5
		{GPIOD, 14}, //P6
		{GPIOA,  2}, //P7
		{GPIOA,  3}, //P8
};
#define PINLISTSIZE (sizeof(pintestlist) / sizeof(pin_t))
#endif


/*
void led_update(void) //noise test (failed)
{
	uint8_t i;
	for (i = 0; i < PINLISTSIZE; i++) //disable all outputs
	{
		pintestlist[i].gpio->MODER &= ~(0x3<<(pintestlist[i].pin*2));
	}
	pintestlist[test_pin].gpio->MODER |= 0x1<<(pintestlist[test_pin].pin*2); //set to output
	if (pintestlist[test_pin].gpio->ODR & (0x1<<pintestlist[test_pin].pin)) //toggle pin
		pintestlist[test_pin].gpio->ODR &= ~(0x1<<pintestlist[test_pin].pin);
	else
		pintestlist[test_pin].gpio->ODR &= ~(0x1<<pintestlist[test_pin].pin);
	test_pin++;
	if (test_pin > PINLISTSIZE) //add scan, need to perform test by led
		test_pin = 0;
}
// */


/*
 * double-edge interleaved mode?
 * ___-----____|___-----____|___-----____|___-----____|___-----____|  100% non inverted
 * -__________-|-__________-|-__________-|-__________-|-__________-|   20% inverted
 * _____-______|_____-______|_____-______|_____-______|_____-______|   20% norm
 * ---_____----|---_____----|---_____----|---_____----|---_____----|  100% inverted
 *
#define P1 TIM1->CCR1
#define P2 TIM1->CCR2
#define P3 TIM1->CCR3
#define P4 TIM1->CCR4
#define P5 TIM4->CCR2
#define P6 TIM2->CCR2
#define P7 TIM2->CCR3
#define P8 TIM2->CCR4
 */

#ifndef EMU
#include "core_cm4.h"
#endif
void led_init(void)
{
#ifndef EMU
	TIM1->CR1 = 0; //reset all
	TIM2->CR1 = 0;
	TIM4->CR1 = 0;
	TIM1->CR1 = TIM_CR1_CMS_0; //center aligned mode
	TIM2->CR1 = TIM_CR1_CMS_0;
	TIM4->CR1 = TIM_CR1_CMS_0;
	TIM1->ARR = 512; //reload
	TIM2->ARR = 512;
	TIM4->ARR = 512;
	TIM1->CCMR1 = 0x00007060; // 1 2 <-set mode to PWM positive or negative
	TIM1->CCMR2 = 0x00007060; // 3 4
	TIM1->BDTR = 0x00008000; //no breaks! (TIM1 got outputs protection HiZ state by default)
	TIM4->CCMR1 = 0x00006000; // 2
	TIM4->CCMR2 = 0x00000070; // 3
	TIM2->CCMR1 = 0x00000000; // -
	TIM2->CCMR2 = 0x00007060; // 3 4
	TIM1->CCER = 0x00001111; // enable PWM outputs
	TIM4->CCER = 0x00000110;
	TIM2->CCER = 0x00001100;
	__disable_irq();
	TIM1->CR1 |= TIM_CR1_CEN; //start! all timers must be in sync to prevent current spikes
	TIM4->CR1 |= TIM_CR1_CEN;
	TIM2->CR1 |= TIM_CR1_CEN;
	__enable_irq();
#endif
}
// u8 led_ram2[9][8] = {0};
void led_update(void)
{
#ifndef EMU
	//uint8_t i;
	GPIOD->MODER &= ~((3<<(N1*2))+(3<<(N2*2))+(3<<(N3*2))+(3<<(N4*2))+(3<<(N5*2))); //disable all outputs
	const static unsigned int nbits[5]={(1<<(N4*2)),(1<<(N3*2)),(1<<(N2*2)),(1<<(N1*2)),(1<<(N5*2))};

	const uint8_t* leds = led_ram[led_state];
	if (led_state & 1)
	{
		GPIOD->BSRR = ((1<<N1)+(1<<N2)+(1<<N3)+(1<<N4)+(1<<N5))<<16; //activation by low value
		P1 = leds[0];
		P2 = leds[1] ^ 0x1FF;
		P3 = leds[2];
		P4 = leds[3] ^ 0x1FF;
		P5 = leds[4];
		P6 = leds[5] ^ 0x1FF;
		P7 = leds[6];
		P8 = leds[7] ^ 0x1FF;
	}
	else
	{
		GPIOD->BSRR = ((1<<N1)+(1<<N2)+(1<<N3)+(1<<N4)+(1<<N5)); //activation by high value
		P1 = leds[0] ^ 0x1FF;
		P2 = leds[1];
		P3 = leds[2] ^ 0x1FF;
		P4 = leds[3];
		P5 = leds[4] ^ 0x1FF;
		P6 = leds[5];
		P7 = leds[6] ^ 0x1FF;
		P8 = leds[7];
	}
	//TIM1->EGR |= TIM_EGR_UG; //apply PWM state right now
	//TIM2->EGR |= TIM_EGR_UG; //CCR unbuffered mode
	//TIM4->EGR |= TIM_EGR_UG;

	GPIOD->MODER |= nbits[led_state>>1]; //activate row
#else
	if (led_state < 9) {
		const u8* leds = led_ram[led_state];
		extern u8 emuleds[9][8];
		for (int i = 0; i < 8; ++i) emuleds[led_state][i] = leds[i];
	}

#endif
	led_state++;
	if (led_state>9)
		led_state=0;
}






/*



void led_update(void) {
#define N1 2
#define N2 5
#define N3 7
#ifdef NEW_PINOUT
#define N4 0
#else
#define N4 14
#endif
#define N5 15

#ifndef EMU
	GPIOD->MODER &= ~((3<<(N1*2))+(3<<(N2*2))+(3<<(N3*2))+(3<<(N4*2))+(3<<(N5*2))); // all inputs
#endif
	  const static u8 led_state_remap[11]={0,2,4,6,8+128,8,1,3,5,7,0+128}; // bmp b D000XXXX D - don't enable
	  u8 led_state=led_state_remap[led_state_2++];
	  u8 dont_enable_leds = led_state & 128;             //extract enable
	  led_state&=127;                                    //delete enable (&0x7F)
	  if (led_state_2==11) led_state_2=0; //round
	int x=led_state;                                     //to position
#ifndef EMU
	if (x < 8) x ^= 7; //                                //why? data: 7,5,3,1,8+,8,6,4,2,0,7+ position and disabling
	const u8* leds = led_ram[x];                         //           + + + + +  +         + <inverted
//	for (int i=0;i<8;++i) led_ram[x][i]=255;// force leds on          3 2 1 0 4  4 3 2 1 0 3 <nbits

	const static unsigned int nbits[5]={(1<<(N1*2)),(1<<(N2*2)),(1<<(N3*2)),(1<<(N4*2)),(1<<(N5*2))}; // particular output of 5
	  unsigned char xor=0;
	  if (led_state&9) {
	  	  //GPIOD->ODR |=  ((1<<N1)+(1<<N2)+(1<<N3)+(1<<N4)+(1<<N5)); //set all
		  GPIOD->BSRR = ((1<<N1)+(1<<N2)+(1<<N3)+(1<<N4)+(1<<N5)); //set all
	  	  xor=255;
	  } else
		  //GPIOD->ODR &= ~((1<<N1)+(1<<N2)+(1<<N3)+(1<<N4)+(1<<N5)); //reset all
		  GPIOD->BSRR = ((1<<N1)+(1<<N2)+(1<<N3)+(1<<N4)+(1<<N5))<<16; //reset all
	  __HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_1 , leds[0]^xor);
	  __HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_2 , leds[1]^xor);
	  __HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_3 , leds[2]^xor);
	  __HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_4 , leds[3]^xor);
	  __HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_2 , leds[4]^xor);
#ifdef NEW_PINOUT
	  __HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_3, leds[5]^xor);
#else
	  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, leds[5] ^ xor);
#endif
	  __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3 , leds[6]^xor);
	  __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_4 , leds[7]^xor);
	 if (!dont_enable_leds)
		 GPIOD->MODER|=nbits[led_state/2];
#else
	const u8* leds = led_ram[x];
	extern u8 emuleds[9][8];
	  for (int i=0;i<8;++i) emuleds[led_state][i]=leds[i];
#endif
}

// */

#endif
