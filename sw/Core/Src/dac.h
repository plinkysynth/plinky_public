
enum {
	OUT_TRIGGER,
	OUT_CLOCK,
	OUT_PRESSURE,
	OUT_GATE,
	OUT_PITCHLO,
	OUT_PITCHHI,
};

#ifdef EMU
int emucvouthist;
float emucvout[6][256];
float emupitchloopback;
#endif

#ifdef EMU
void SetOutputCVEmu(int chan, int data) {
	emucvout[chan][emucvouthist/4] = maxf(emucvout[chan][emucvouthist/4],data * (1.f/65535.f));
}
#else
#define SetOutputCVEmu(chan, data)
#endif

void SetOutputCVTrigger(int data) {
#ifdef EMU
	SetOutputCVEmu(OUT_TRIGGER, data);
#else
//	HAL_GPIO_WritePin(CVOUTTRIG_GPIO_Port, CVOUTTRIG_Pin, data ? GPIO_PIN_SET : GPIO_PIN_RESET);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, clampi(data,0,65535)>>8);
#endif
}
void SetOutputCVClk(int data) {
#ifdef EMU
	SetOutputCVEmu(OUT_CLOCK, data);
#else
//	HAL_GPIO_WritePin(CVOUTCLK_GPIO_Port, CVOUTCLK_Pin, data ? GPIO_PIN_SET : GPIO_PIN_RESET);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, clampi(data,0,65535)>>8);
#endif
}
u8 maxpressure_out;
void SetOutputCVPressure(int data) {
	maxpressure_out = clampi(data >> 6,0,255);
#ifdef EMU
	SetOutputCVEmu(OUT_PRESSURE, data);
#else
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, clampi(data,0,65535)>>8);
#endif
}
void SetOutputCVGate(int data) {
#ifdef EMU
	SetOutputCVEmu(OUT_GATE, data);
#else
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, clampi(data, 0, 65535) >> 8);
#endif
}
void SetOutputCVPitchLo(int data, bool applycalib) {
	if (applycalib) {
		data = (int)((data * cvcalib[8].scale) + cvcalib[8].bias);
		int step=abs((int)(cvcalib[8].scale*(2048.f*12.f)));
		for (int k=0;k<3;++k) if (data>65535) data-=step; else break;
		for (int k=0;k<3;++k) if (data<0) data+=step; else break;
	}
#ifdef EMU
	SetOutputCVEmu(OUT_PITCHLO, data);
	emupitchloopback = (data - 52100.f) / -9334.83f;
#else
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_L, clampi(data,0,65535) );
#endif
}
int pitchhi_out;
void SetOutputCVPitchHi(int data, bool applycalib) {
	pitchhi_out = data ;
	if (applycalib) {
		data = (int)((data * cvcalib[9].scale) + cvcalib[9].bias);
		int step=abs((int)(cvcalib[9].scale*(2048.f*12.f)));
		for (int k=0;k<3;++k) if (data>65535) data-=step; else break;
		for (int k=0;k<3;++k) if (data<0) data+=step; else break;
	}
#ifdef EMU
	SetOutputCVEmu(OUT_PITCHHI, data);
#else
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_L, clampi(data,0,65535) );
#endif
}

void dac_init(void) {
#ifndef EMU
	HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
	HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);

	// must start timer to actually have it work

	// also start pwm outputs
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
#endif
}

extern int arpdebugi;

void AdvanceCVOut() {
#ifdef EMU
	emucvouthist++;
	emucvouthist &= 1023;
	if ((emucvouthist & 3) == 0) {
		for (int c = 0; c < 6; ++c) emucvout[c][emucvouthist / 4] = 0;
	}
	arpdebugi = (arpdebugi + 1) & 1023;
#endif
}
