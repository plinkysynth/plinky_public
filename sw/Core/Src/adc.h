#pragma once

#define ADC_CHANS  8
#define ADC_SAMPLES 8
extern u16 adcbuf[ADC_CHANS*ADC_SAMPLES];
enum {
	ADC_IN1, // fifth jack - pitch
	ADC_IN2, // sixth jack - gate
	ADC_IN3,  //third jack -
	ADC_IN4,// fourth jack
	ADC_IN5, // top jack
	ADC_IN6, // second jack
	ADC_POT1,
	ADC_POT2,
	
	ADC_PITCH = ADC_IN1,
	ADC_GATE = ADC_IN2,
	ADC_XCV = ADC_IN3,
	ADC_YCV = ADC_IN4,
	ADC_ACV = ADC_IN5,
	ADC_BCV = ADC_IN6,
	ADC_AKNOB = ADC_POT2,
	ADC_BKNOB = ADC_POT1,
};
void adc_init(void);

typedef struct CVCalib {
	float bias, scale;
} CVCalib;

#ifdef IMPL
u16 adcbuf[ADC_CHANS*ADC_SAMPLES];
CVCalib cvcalib[10] = {
	// 6 input
	{52100.f, 1.f / -9334.833333f},
	{31716.f, 0.2f / -6548.1f},
	{31665.f, 0.2f / -6548.1f},
	{31666.f, 0.2f / -6548.1f},
	{31041.f, 0.2f / -6548.1f},
	{31712.f, 0.2f / -6548.1f},
	// 2 pots 
	{32768.f, 1.05f / -32768.f},
	{32768.f, 1.05f / -32768.f},
	// 2 output
	// 2048 per semitone, so...
	{42490.f, (26620-42490) * (1.f / (2048.f * 12.f * 2.f))},
	{42511.f, (26634-42511) * (1.f / (2048.f * 12.f * 2.f))},
};

static inline int GetADCSmoothedNoCalib(int chan) {
	u32 rv = 0;
	u16* src = adcbuf + chan;
	if (chan == ADC_GATE) { // get max not average, to respond to short gates better
		for (int i = 0; i < ADC_SAMPLES; ++i) {
			rv = maxi(rv,*src);
			src += ADC_CHANS;
		}
		return rv;
	}
	for (int i = 0; i < ADC_SAMPLES; ++i) {
		rv += *src;
		src += ADC_CHANS;
	}
	return rv / ADC_SAMPLES;
}

static inline float GetADCSmoothed(int chan) {
	return (GetADCSmoothedNoCalib(chan) - cvcalib[chan].bias) * cvcalib[chan].scale;
}

//volatile u32 adccounter;
//
//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
//	adccounter++;
//}

void adc_init(void) {
	for (int i = 0; i < ADC_CHANS * ADC_SAMPLES; ++i)
		adcbuf[i] = 32768;
#ifndef EMU
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*) &adcbuf, ADC_CHANS * ADC_SAMPLES);
#endif
}

void adc_start(void) {
#ifndef EMU
	HAL_TIM_Base_Start(&htim6);
#endif
}

#endif
