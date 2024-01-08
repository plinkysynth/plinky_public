#if defined(_WIN32) || defined(__APPLE__)
#define EMU
#pragma warning(disable:4244)
#endif

#ifdef WASM
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE 
#endif

#ifndef EMU
#include <main.h>

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;

extern DAC_HandleTypeDef hdac1;
extern DMA_HandleTypeDef hdma_dac_ch1;
extern DMA_HandleTypeDef hdma_dac_ch2;

extern I2C_HandleTypeDef hi2c2;

extern SAI_HandleTypeDef hsai_BlockA1;
extern SAI_HandleTypeDef hsai_BlockB1;
extern DMA_HandleTypeDef hdma_sai1_a;
extern DMA_HandleTypeDef hdma_sai1_b;

extern SPI_HandleTypeDef hspi2;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern DMA_HandleTypeDef hdma_spi2_rx;

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim6;

extern TSC_HandleTypeDef htsc;

extern UART_HandleTypeDef huart3;


#endif

#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#define IMPL
#define BLOCK_SAMPLES 64
#ifdef WASM
#define ASSERT(...)
#else
#define ASSERT assert
#endif
#include "core.h"
#include "oled.h"
#include "codec.h"
#include "leds.h"
#include "adc.h"
#include "dac.h"
#include "gfx.h"
#include "spi.h"
#include "tables.h"
#include "audiointrin.h"
#include "lfo.h"
#include "enums.h"


const static float table_interp(const float *table, int x) { // 16 bit unsigned input, looked up in a 1024 entry table and linearly interpolated
	x=SATURATEU16(x);
	table+=x>>6;
	x&=63;
	return table[0]+(table[1]-table[0])*(x*(1.f/64.f));
}

#define TWENTY_OVER_LOG2_10 6.02059991328f // (20.f/log2(10.f));

static inline float lin2db(float lin) { return log2f(lin) * TWENTY_OVER_LOG2_10; }
static inline float db2lin(float db) { return exp2f(db * (1.f / TWENTY_OVER_LOG2_10)); }

typedef struct knobsmoother {
	float y1, y2;
} knobsmoother;


void knobsmooth_reset(knobsmoother* s, float ival) { s->y1 = s->y2 = ival; }

float knobsmooth_update_knob(knobsmoother* s, float newval, float max_scale) {
	// inspired by  https ://cytomic.com/files/dsp/DynamicSmoothing.pdf
	float band = fabsf(s->y2 - s->y1);
	float sens = 8.f / max_scale;
	float g = minf(1.f, 0.05f + band * sens);
	s->y1 += (newval - s->y1) * g;
	s->y2 += (s->y1 - s->y2) * g;
	return s->y2;
}
float knobsmooth_update_cv(knobsmoother* s, float newval) { // same as update but with faster constants
	// inspired by  https ://cytomic.com/files/dsp/DynamicSmoothing.pdf
	float band = fabsf(s->y2 - s->y1);
	const static float sens = 10.f;
	float g = minf(1.f, 0.1f + band * sens);
	s->y1 += (newval - s->y1) * g;
	s->y2 += (s->y1 - s->y2) * g;
	return s->y2;
}

typedef struct Osc {
	u32 phase, prevsample;
	s32 dphase;
	s32 targetdphase;
	int pitch;
} Osc;

typedef struct GrainPair {
	int fpos24;
	int pos[2];
	int vol24;
	int dvol24; 
	int dpos24;
	float grate_ratio;
	float multisample_grate;
	int bufadjust; // for reverse grains, we adjust the dma buffer address by this many samples
	int outflags;
} GrainPair;

typedef struct Voice {
	float vol;
	float y[4];
	Osc theosc[4];
	GrainPair thegrains[2];
	// grain synth state
	int playhead8;
	u8 sliceidx;
	int initialfingerpos;
	knobsmoother fingerpos;

	u8 decaying;
	int env_cur16;
	float noise;
	float env_level;
#ifdef NEW_LAYOUT
	int env_decaying;
#else
	uint64_t env_phase;
#endif
} Voice;

TickCounter _tc_budget;
TickCounter _tc_all;
TickCounter _tc_fx;
TickCounter _tc_audio;
TickCounter _tc_touch;
TickCounter _tc_led;
TickCounter _tc_osc;
TickCounter _tc_filter;

knobsmoother adc_smooth[8];
volatile int encval = 0;
volatile u8 encbtn = 0;
float encaccel;
u8 prevsynthfingerdown = 0;
u8 prevsynthfingerdown_nogatelen = 0; // same as above, but without gatelen applied
u8 prevprevsynthfingerdown_nogatelen = 0; // same as above, but without gatelen applied
u8 synthfingerdown = 0; // bit set when finger is down
u8 synthfingerdown_nogatelen = 0;
u8 synthfingertrigger = 0; // bit set on frame finger goes down
s8 shift_down = -1;//-1 means up; -2 means ghosted (supressed) touch; 0-7 means down
int shift_down_time = 0;
s8 editmode = EM_PLAY;
int last_time_shift_was_untouched = 0;
u32 tick = 0; // increments every 64 samples

s32 bpm10x = 120 * 10;

static inline bool isgrainpreview(void) {
	return editmode == EM_SAMPLE;
}


enum {
	PLAY_STOPPED,
	PLAY_PREVIEW,
	PLAY_WAITING_FOR_CLOCK_START,
	PLAY_WAITING_FOR_CLOCK_STOP,
	PLAYING,
};

u8 playmode = PLAY_STOPPED;
bool recording = false;
u8 pending_loopstart_step = 255; // set when we want to jump on next loop

static inline bool isplaying(void) {
	return playmode == PLAYING || playmode == PLAY_WAITING_FOR_CLOCK_STOP;
}


u32 bpm_clock_phase = 0;
int ticks_since_clock = 0;
int ticks_since_arp = 0;
int last_clock_period = 0;
int last_step_period = 0;
int last_arp_period = 0;
int ticks_since_step = 0; // this counts up, along with the seq_divide_counter, even when not playing, in order to give a sense of time eg for recording
bool external_clock_enable = false;

int seq_divide_counter = 0;
int arp_divide_counter = 0;
int seqdiv = 0; // what we count up to , to get seq division
uint64_t seq_used_bits=0;
u8 seq_dir=0;

static inline int calcseqsubstep(int tick_offset, int maxsubsteps) { // where are we within a recorded step? 
	if (last_step_period <= 0)
		return 0;
	if (ticks_since_step + tick_offset >= last_step_period)
		return maxsubsteps-1;
	if (ticks_since_step + tick_offset <= 0)
		return 0;
	int s = ((ticks_since_step+tick_offset) * maxsubsteps) / last_step_period;
	if (s < 0) s = 0;
	s=mini(maxsubsteps - 1, s);
	return s;
}
static inline int calcarpsubstep(int tick_offset, int maxsubsteps) { // where are we within a recorded step? 
	if (last_arp_period <= 0)
		return 0;
	if (ticks_since_arp + tick_offset >= last_arp_period)
		return maxsubsteps-1;
	if (ticks_since_arp + tick_offset <= 0)
		return 0;
	return mini(maxsubsteps - 1, ((ticks_since_arp + tick_offset) * maxsubsteps) / last_arp_period);
}

u8 edit_mod=0,edit_param=0xff;
//u8 ui_edit_param_prev[2][4] = { {P_LAST,P_LAST,P_LAST,P_LAST},{P_LAST,P_LAST,P_LAST,P_LAST} }; // push to front history
static float surf[2][8][8];

Voice voices[8];
#ifdef EMU
short delaybuf[DLMASK + 1];
short reverbbuf[RVMASK + 1];
int emupitchsense;
int emugatesense;
#else
//__attribute__((section(".dlram"))) short delaybuf[DLMASK + 1];
//__attribute__((section(".rvram"))) short reverbbuf[RVMASK + 1];
short *reverbbuf=(short*)0x10000000; // use ram2 :)
short *delaybuf= (short*)0x20008000; // use end of ram1 :)

#endif
static int reverbpos = 0;

static int k_reverb_fade = 240;
static int k_reverb_shim = 240;
static float k_reverb_wob = 0.5f;
static int k_reverbsend=0;
static int shimmerpos1 = 2000;
static int shimmerpos2 = 1000;
static int shimmerfade = 0;
static int dshimmerfade = 32768/4096;

static lfo aplfo = LFOINIT(1.f / 32777.f * 9.4f);
static lfo aplfo2= LFOINIT(1.3f / 32777.f * 3.15971f);


/*

update 
kick fetch for this pos
*/

u32 scope[128];


static inline u32 ticks(void) { return tick; }

enum {
	EA_OFF = 0,
	EA_PASSTHRU = -1,
	EA_PLAY = 1,
	EA_PREERASE = 2,
	EA_MONITOR_LEVEL = 3,
	EA_ARMED = 4,
	EA_RECORDING = 5,
	EA_STOPPING1 = 6, // we stop for 4 cycles to write 0s at the end
	EA_STOPPING2 = 7,
	EA_STOPPING3 = 8,
	EA_STOPPING4 = 9,
};
s8 enable_audio = EA_OFF;


#include "flash.h"
#include "params.h"
#include "touch.h"
#include "calib.h"
#include "arp.h"
#include "edit.h"

#include "webusb.h"


bool gatecv_trig = false;

// lpf_k = 1-exp(-0.707/t) where t is in samples to get to half
static inline float lpf_k(int x) {
	return table_interp(lpf_ks, x);
}


float param_eval_float(u8 paramidx, int rnd, int env16, int pressure16) {
	return param_eval_int(paramidx, rnd, env16, pressure16) * (1.f / 65536.f);
}

int param_eval_finger(u8 paramidx, int fingeridx, Finger* f) {
	return param_eval_int(paramidx, finger_rnd[fingeridx], voices[fingeridx].env_cur16, f->pressure * 32);
}

extern int16_t accel_raw[3];
extern float accel_lpf[2];
extern float accel_smooth[2];
s16 accel_sens=0;

//extern int debuga[4];
//int debuga[4];


void update_params(int fingertrig, int fingerdown) {

	//	DebugLog("%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",adcbuf[0],adcbuf[1],adcbuf[2],adcbuf[3],adcbuf[4],adcbuf[5],adcbuf[6],adcbuf[7],adcbuf[8]);

		// update envelopes
#ifdef NEW_LAYOUT
	param_eval_premod(P_ENV_LEVEL);
	param_eval_premod(P_A2);
	param_eval_premod(P_D2);
	param_eval_premod(P_S2);
	param_eval_premod(P_R2);
#else
	param_eval_premod(P_ENV_RATE);
	param_eval_premod(P_ENV_WARP);
	param_eval_premod(P_ENV_LEVEL);
	param_eval_premod(P_ENV_REPEAT);
#endif
	for (int vi = 0; vi < 8; ++vi) {
		int bit = 1 << vi;
		Voice* v = &voices[vi];
		Finger* f = touch_synth_getlatest(vi);
#ifdef NEW_LAYOUT
		if (fingertrig & bit) {
			v->env_level = 0.f;
			v->env_decaying = false;
		}
		int down = (fingerdown & bit);
		float target = down ? (v->env_decaying) ? 2.f*(param_eval_finger(P_S2, vi, f)*(1.f/65536.f)) : 2.2f : 0.f;
		float dlevel = target - v->env_level;
		float k = lpf_k(param_eval_finger((dlevel > 0.f) ? P_A2 : (v->env_decaying && down) ? P_D2 : P_R2, vi, f));
	// update v->env_level
		v->env_level += (target - v->env_level) * k;
		if (v->env_level >= 2.f && down)
			v->env_decaying = true;
		v->env_cur16 = SATURATE17(v->env_level * param_eval_finger(P_ENV_LEVEL, vi, f));
#else
		if (fingertrig & bit) {
			v->env_phase = (uint64_t)(65536.f * 65536.f * 2.f * (0.5f - 0.4999f));
			v->env_level = 2.f; // so that it clips!
		}
		int lfofreq = param_eval_finger(P_ENV_RATE, vi, f);
		u32 dlfo = (u32)(table_interp(pitches, 32768 + (lfofreq >> 1)) * (1 << 24));
		//u32 dlfo=(u32)exp2f(24.f+lfofreq*10.f);
		float lfowarp = param_eval_finger(P_ENV_WARP, vi, f) * (0.4999f / 65536.f) + 0.5f;
		u32 prev_cycle = v->env_phase >> 32;
		float lfoval = lfo_eval((u32)((v->env_phase) >> 16), lfowarp, LFO_ENV);
		v->env_phase += dlfo;
		u32 cur_cycle = v->env_phase >> 32;
		if (cur_cycle != prev_cycle)
			v->env_level *= param_eval_finger(P_ENV_REPEAT, vi, f) * (1.f / 65536.f);
		lfoval *= v->env_level;
		lfoval *= param_eval_finger(P_ENV_LEVEL, vi, f);
		v->env_cur16 = SATURATE17((int)lfoval);
#endif
	}
	// update average tilt + pressure
	int totw = 256;
	int tottilt = tilt16 * 256;
	int maxp = 0;
	int maxenv = 0;
	for (int fi = 0; fi < 8; ++fi) {
		Finger* f = touch_synth_getlatest(fi);
		int p = f->pressure;
		if (p < 0) p = 0;
		totw += p;
		maxp = maxi(maxp, p);
		maxenv = maxf(maxenv, voices[fi].env_cur16);
		tottilt += index_to_tilt16(fi) * p;
		if (fingertrig & (1 << fi)) {
			finger_rnd[fi] += 4813;
		}
	}
	if (fingertrig)
		any_rnd += 4813;
	tilt16 = tottilt / totw;
	env16 = maxenv;
	pressure16 = maxp * (65536 / 2048);
	// update lfos on modulation sources

	float aknob = clampf(GetADCSmoothed(ADC_AKNOB), -1.f, 1.f);
	float bknob = clampf(GetADCSmoothed(ADC_BKNOB), -1.f, 1.f);
	float acv = clampf(GetADCSmoothed(ADC_ACV) * IN_CV_SCALE, -1.f, 1.f);
	float bcv = clampf(GetADCSmoothed(ADC_BCV) * IN_CV_SCALE, -1.f, 1.f);
	float xcv = clampf(GetADCSmoothed(ADC_XCV) * IN_CV_SCALE, -1.f, 1.f);
	float ycv = clampf(GetADCSmoothed(ADC_YCV) * IN_CV_SCALE, -1.f, 1.f);

	//	accelerometer
	static int accel_counter;
	float accel_sens_f = (2.f/16384.f/32768.f) * abs(accel_sens);
	accel_counter++;
	int axisswap = accel_raw[2] > 4000; // run 2 plinkys have the accelerometer rotated 90 degrees and upside down from the addon... detect it via z direction
		for (int j=0;j<2;++j) {
			float f=accel_raw[j^axisswap]*accel_sens_f;
			if (!j) {
				if (!axisswap)
					f = -f; // reverse x
			} else if (accel_sens < 0)
				f = -f; // reverse y if accel sens negative
			accel_lpf[j]+=(f-accel_lpf[j])*0.0001f;
			accel_smooth[j]+=(f-accel_smooth[j])*0.1f;
			if(accel_counter<1000)
				accel_lpf[j]=accel_smooth[j]=f;
		}
//		int t1=1000*accel_lpf[0];
//		int t2=1000*accel_lpf[1];
//		int t3=1000*accel_smooth[0];
//		int t4=1000*accel_smooth[1];

	int gatesense = getgatesense();
	int pitchsense = getpitchsense();
	float pitchcv = pitchsense ? GetADCSmoothed(ADC_PITCH) : 0.f;
	float gatecv = gatesense ? clampf(GetADCSmoothed(ADC_GATE)*1.15f-0.05f, 0.f, 1.f) : 1.f;
	knobsmooth_update_cv(adc_smooth + 0, acv);
	knobsmooth_update_cv(adc_smooth + 1, bcv);
	knobsmooth_update_cv(adc_smooth + 2, xcv);
	knobsmooth_update_cv(adc_smooth + 3, ycv);
	knobsmooth_update_knob(adc_smooth + 4, aknob, 1.f);
	knobsmooth_update_knob(adc_smooth + 5, bknob, 1.f);
	knobsmooth_update_cv(adc_smooth + 6, pitchcv);
	knobsmooth_update_cv(adc_smooth + 7, gatecv);
	u8 prevlfohp = (lfo_history_pos >> 4) & 15;
	lfo_history_pos++;
	u8 lfohp = (lfo_history_pos >> 4) & 15;
	if (lfohp != prevlfohp)
		lfo_history[lfohp][0] =
		lfo_history[lfohp][1] =
		lfo_history[lfohp][2] =
		lfo_history[lfohp][3] = 0;
	//compute new mod_cur for each mod source
	int phase0 = calcseqsubstep(0, 65536);
	int phase1 = phase0 + (65536 / 8);
	int nextstep = cur_step;
	if (phase1 >= 65536) {
		phase1 &= 65535;
		nextstep++;
		u8 loopstart_step = (rampreset.loopstart_step_no_offset + step_offset) & 63;
		if (nextstep >= loopstart_step + rampreset.looplen_step)
			nextstep -= rampreset.looplen_step;
		nextstep &= 63;
	}
	int q1 = (cur_step >> 4) & 3;
	int q2 = (nextstep >> 4) & 3;
	s8* autoknob1 = rampattern[q1].autoknob[(cur_step & 15) * 8 + (phase0>>13)];
	s8* autoknob2 = rampattern[q2].autoknob[(nextstep & 15) * 8 + (phase1>>13)];
	float autoknobinterp = (phase0 & (65536 / 8 - 1)) * (1.f/(65536/8));
	for (int i = 0; i < 4; ++i) {
		float adc = adc_smooth[i].y2;
		float adcknob = 0.f;
		if (i < 2) {
			adcknob = adc_smooth[i + 4].y2;
			if (!(recordingknobs&(1<<i)))
				adcknob +=  (autoknob1[i]+(autoknob2[i]-autoknob1[i])* autoknobinterp )*(1.f/127.f);
		} else
			adcknob=accel_smooth[i-2]-accel_lpf[i-2];
		
		int i6 = i * 6;
		mod_cur[M_A + i] = (int)((adc) * 65536.f); // modulate yourself with the raw input!
		param_eval_premod(P_AFREQ + i6);
		param_eval_premod(P_AWARP + i6);
		param_eval_premod(P_ASHAPE + i6);
		param_eval_premod(P_ADEPTH + i6);
		param_eval_premod(P_AOFFSET + i6);
		param_eval_premod(P_ASCALE + i6);

		int lfofreq = param_eval_int(P_AFREQ + i6, any_rnd, env16, pressure16);
		u32 dlfo = (u32)(table_interp(pitches, 32768 + (lfofreq >> 1)) * (1 << 24));
		//u32 dlfo=(u32)exp2f(24.f+lfofreq*10.f);
		float lfowarp = param_eval_float(P_AWARP + i6, any_rnd, env16, pressure16) * 0.49f + 0.5f;
		int lfoshape = param_eval_int(P_ASHAPE + i6, any_rnd, env16, pressure16);
		float lfoval = lfo_eval((u32)((lfo_pos[i] += dlfo) >> 16), lfowarp, lfoshape);

		lfoval *= param_eval_float(P_ADEPTH + i6, any_rnd, env16, pressure16);
		//expander_out[i] = clampi(EXPANDER_ZERO - (int)(lfoval * (float)(EXPANDER_RANGE)), 0, EXPANDER_MAX);

		int cvval = param_eval_int(P_AOFFSET + i6, any_rnd, env16, pressure16);
		//if (i == 0) debuga[0] = cvval>>8;
		cvval += (int)(adc * (param_eval_int(P_ASCALE + i6, any_rnd, env16, pressure16)<<1));
		cvval += (int)(adcknob * 65536.f); // knob is not scaled by the cv bias/scale parameters. I think thats useful.
		mod_cur[M_A + i] = ((int)(lfoval * 65536.f)) + cvval;
		//if (i == 0) {
		//	debuga[1] = adc * 256.f;
		//	debuga[2] = adcknob * 256.f;
		//	debuga[3] = lfoval * 256.f;
		//}

		float expander_val = mod_cur[M_A + i] * (EXPANDER_GAIN * EXPANDER_RANGE / 65536.f);
		expander_out[i] = clampi(EXPANDER_ZERO - (int)(expander_val), 0, EXPANDER_MAX);


		int scopey = (-(mod_cur[M_A + i] * 7 + (1<<16)) >> 17) + 4;
		if (scopey >= 0 && scopey < 8)
			lfo_history[lfohp][i] |= 1 << scopey;
		

	}



	for (int i = 0; i < P_LAST; ++i) {
		int pg = i / 6;
		if (pg == PG_A || pg == PG_B || pg == PG_X || pg == PG_Y) {
			i += 5;
			continue;
		}
		param_eval_premod(i);
	}

	accel_sens = clampi(param_eval_int(P_ACCEL_SENS, any_rnd, env16, pressure16)/2, -32767, 32767);

	//	DebugLog("%d,%d,%d,%d\r\n",mod_cur[0]/256,mod_cur[1]/256,mod_cur[2]/256,mod_cur[3]/256);
	//	HAL_UART_Transmit(&huart3, (u8*) mod_cur, 4*4, 1000);

}





static inline void putscopepixel(unsigned int x, unsigned int y) {
	if (y>=32) return;
	scope[x]|=(1<<y);
}



bool trigout = false;


float UpdateEnvelope(Voice *v, int fingeridx, float targetvol) {
	Finger *f=touch_synth_getlatest(fingeridx);
	float sens = param_eval_finger(P_SENS, fingeridx, f);
	sens *= (2.f / 65536.f);
	targetvol *= sens*sens;
	bool gp = isgrainpreview();
	const float sustain = gp ? 1.f : squaref(param_eval_finger(P_S, fingeridx, f) * (1.f/65536.f));  // lerp(0.f,1.f,pot1);
	const float attack = gp ? 0.5f : lpf_k((param_eval_finger(P_A, fingeridx, f) ));
	const float decay =  gp ? 1.f : lpf_k((param_eval_finger(P_D, fingeridx, f) ));
	const float release =gp ? 0.5f : lpf_k((param_eval_finger(P_R, fingeridx, f) ));

	if (targetvol < 0.f)
		targetvol = 0.f;
	targetvol *= targetvol;
	// pitch compensation
	if (!ramsample.samplelen) {
		targetvol *= 1.f + ((v->theosc[2].pitch - 43000) * (1.f / 65536.f));
	}
	
	int bit=1<<fingeridx;
	if (arpmode>=0) {
		if (!(arpbits & bit))
			targetvol = 0.f;
		/*else if (targetvol > 0.f) {
			// gate len for arp here!
			int gatelen = param_eval_finger(P_GATE_LENGTH, fingeridx, f) >> 8;
			if (gatelen < 256) {
				int phase = calcarpsubstep(0, 256);
				if (phase > gatelen || !arp_rhythm.did_a_retrig)
					targetvol = 0.f;
			}
		}*/
	}

	
	float vol = v->vol;
	if (arpretrig || (synthfingertrigger & bit)) {
		//if (vol>sustain*0.5f)
		//	vol = sustain*0.5f; // make sure you hear the retrig :) not sure... legato would be nice. maybe use sustain as a hint?
		vol *= sustain;
		v->decaying = false;
		trigout = true;
	}

	float decay_or_release = decay;
	if (targetvol <= 0.f) {
		v->decaying = 0; // release phase
		decay_or_release = release;
	} else if (v->decaying) {
		targetvol *= sustain;
	}

	float attack_threshvol = targetvol;
	float dvol = (targetvol - vol);
	dvol *= (dvol > 0.f) ? attack : decay_or_release; // scale delta back by release time
	targetvol = vol + dvol; // new target
	if (targetvol > attack_threshvol * 0.95f)
		v->decaying = 1; // we hit the peak! time to decay.
	if (targetvol > 1.f) {
		targetvol=1.f;
		v->decaying = 1;
	}
	return targetvol;
}

inline s32 trifold(u32 x) {
	if (x > 0x80000000)
		x = 0xffffffff - x;
	return (s32)(x >> 4);
}

static inline int sample_slice_pos8(int pos16) {
	pos16 = clampi(pos16,0,65535);
	int i = pos16 >> 13;
	int p0 = ramsample.splitpoints[i];
	int p1 = ramsample.splitpoints[i+1];
	return (p0<<8) + (((p1 - p0) * (pos16 & 0x1fff)) >> (13-8));
}

static inline int calcloopstart(u8 sliceidx) {
	int all = ramsample.loop & 2;
	return (all) ? 0 : ramsample.splitpoints[sliceidx];
}
static inline int calcloopend(u8 sliceidx) {
	int all = ramsample.loop & 2;
	return (all || sliceidx>=7) ? ramsample.samplelen - 192 : ramsample.splitpoints[sliceidx+1];
}

static inline int doloop(int playhead, u8 sliceidx) {
	if (!(ramsample.loop & 1)) return playhead;
	int loopstart = calcloopstart(sliceidx);
	int loopend = calcloopend(sliceidx);
	int looplen = loopend - loopstart;
	if (looplen > 0 && (playhead < loopstart || playhead >= loopstart + looplen)) {
		playhead = (playhead - loopstart) % looplen;
		if (playhead < 0) playhead += looplen;
		playhead += loopstart;
	}
	return playhead;
}
static inline int doloop8(int playhead, u8 sliceidx) {
	if (!(ramsample.loop & 1)) return playhead;
	int loopstart = calcloopstart(sliceidx)<<8;
	int loopend = calcloopend(sliceidx)<<8;
	int looplen = loopend - loopstart;
	if (looplen > 0 && (playhead < loopstart || playhead >= loopstart + looplen)) {
		playhead = (playhead - loopstart) % looplen;
		if (playhead < 0) playhead += looplen;
		playhead += loopstart;
	}
	return playhead;
}

#ifdef EMU
float arpdebug[1024];
int arpdebugi;
#endif

#ifdef EMU
#define SMUAD(o, a, b) o=(int)(((s16)(a))*((s16)(b))+((s16)(a>>16))*((s16)(b>>16)))
#else
#define SMUAD(o, a, b) 		asm("smuad %0, %1, %2" : "=r" (o) : "r" (a), "r" (b))
#endif



//s16 wavetable[WAVETABLE_SIZE*WT_LAST];
#ifndef EMU
__attribute__((section(".wavetableSection")))
#endif
#include "wavetable.h"
#ifdef _WIN32
//#define clz __lzcnt
u32 clz(u32 val) {
	u8 res = 0;
	if (!val) return 32;
	while (!(val & 0x80000000))
	{
		res++;
		val <<= 1;
	}
	return res;
}
#else
#define clz __builtin_clz
#endif
void RunVoice(Voice *v, int fingeridx, float targetvol, u32 *outbuf) {

//	if (fingeridx == 0) targetvol = 1.f;
	//if (fingeridx > 0) return;
	//float otargetvol = targetvol;
	targetvol = UpdateEnvelope(v, fingeridx, targetvol);
	tc_start(&_tc_osc);	
	float noise;
#ifdef EMU
	if (fingeridx == 4) {
		//bool click = otargetvol > 0.5f && (arpbits & 16) && arpretrig ;
		arpdebug[arpdebugi] = targetvol;// +(click ? 0.5 : 0);
		//if (click && targetvol < 0.001f) {
		//	arpdebug[arpdebugi] = 1.f;
		//}
	}
#endif
	Finger *f=touch_synth_getlatest(fingeridx);

	/* the touch.h version should handle this damn it
	if (targetvol > 0.01f && v->vol < 0.01f) {
		// trigger! reset pitches?
		for (int c1 = 0; c1 < 8; ++c1) {
			fingers_synth_sorted[fingeridx][c1] = *f;
		}
	}*/

	float glide =lpf_k(param_eval_finger(P_GLIDE, fingeridx, f)>>2) * (0.5f/BLOCK_SAMPLES);
	int drivelvl=param_eval_finger(P_DRIVE, fingeridx, f);
	float fdrive = table_interp(pitches, ((32768-2048)+drivelvl/2));
	if (drivelvl<-65536+2048)
		fdrive*=(drivelvl+65536)*(1.f/2048.f); // ensure drive goes right to 0 when full minimum
	float drive = fdrive * (0.75f/65536.f);

	float targetnoise = param_eval_finger(P_NOISE, fingeridx, f) * (1.f / 65536.f);
	targetnoise *= targetnoise;
	if (drivelvl > 0)
		targetnoise *= fdrive;
	float dnoise = (targetnoise - v->noise) * (1.f / BLOCK_SAMPLES);

	int resonancei = 65536 - param_eval_finger(P_MIXRESO, fingeridx, f);
	float resonance = 2.1f - (table_interp(pitches, resonancei) * (2.1f / pitches[1024]));

	drive *= 2.f / (resonance + 2.f);

	//glide=0.25f/BLOCK_SAMPLES;

	Finger* synthf = touch_synth_getlatest(fingeridx);
	float timestretch = 1.f;
	float posjit = 0.f;
	float sizejit = 1.f;
	float gsize = 0.125f;
	float grate = 1.f;
	float gratejit = 0.f;
	int smppos = 0;
	if (ramsample.samplelen) {
		if (!isgrainpreview()) {
			timestretch = param_eval_finger(P_SMP_TIME, fingeridx, synthf) * (2.f / 65536.f);
			gsize = param_eval_finger(P_SMP_GRAINSIZE, fingeridx, synthf)* (1.414f / 65536.f);
			grate = param_eval_finger(P_SMP_RATE, fingeridx, synthf) * (2.f / 65536.f);
			smppos = (param_eval_finger(P_SMP_POS, fingeridx, synthf) * ramsample.samplelen) >> 16;
			posjit = param_eval_finger(P_JIT_POS, fingeridx, synthf) * (1.f / 65536.f);
			sizejit = param_eval_finger(P_JIT_GRAINSIZE, fingeridx, synthf) * (1.f / 65536.f);
			gratejit = param_eval_finger(P_JIT_RATE, fingeridx, synthf) * (1.f / 65536.f);
		}
	}
	int trig = synthfingertrigger & (1 << fingeridx);
	
	int prevsliceidx = v->sliceidx;
	if (ramsample.samplelen)
	{
		bool gp = isgrainpreview();

		// decide on the sample for the NEXT frame
		if (trig) {// on trigger frames, we FADE out the old grains! then the next dma fetch will be the new sample and we can fade in again 
			EmuDebugLog("!!!!!!!!!!!!TRIG! %d\n", arpretrig);
			targetvol = 0.f;
	//		DebugLog("\r\n%d", fingeridx);
			int ypos = 0;
			if (ramsample.pitched && !gp) {
				/// / / / ////////////////////// multisample choice
				int best = fingeridx;
				int bestdist = 0x7fffffff;
				int mypitch = (v->theosc[1].pitch + v->theosc[2].pitch) / 2;
				int mysemi = (mypitch) >> 9;
				static u8 multisampletime[8];
				static u8 trigcount = 0;
				trigcount++;
				for (int i = 0; i < 8; ++i) {
					int dist = abs(mysemi - ramsample.notes[i]) * 256 - (u8)(trigcount - multisampletime[i]);
					if (dist < bestdist) {
						bestdist = dist;
						best = i;
					}
				}
				multisampletime[best] = trigcount; // for round robin
				v->sliceidx = best;
				if (grate < 0.f)
					ypos = 8;
			}
			else {
				v->sliceidx = fingeridx;
				ypos = (f->pos / 256);
				if (gp)
					ypos = 0;
				if (grate < 0.f)
					ypos++;
			}
			v->initialfingerpos = gp ? 128 : f->pos;
			v->playhead8 = sample_slice_pos8(((v->sliceidx * 8) + ypos) << (16 - 6)) ;
			if (grate < 0.f) {
				v->playhead8 -= 192 << 8;
				if (v->playhead8 < 0)
					v->playhead8 = 0;
			}
			knobsmooth_reset(&v->fingerpos, 0);
		}
		else { // not trigger - just advance playhead
			float ms2 = (v->thegrains[0].multisample_grate + v->thegrains[1].multisample_grate); // double multisample rate
			int delta_playhead8 = (int)(grate * ms2 * timestretch * (BLOCK_SAMPLES * 0.5f * 256.f) + 0.5f);
			v->playhead8 = doloop8(v->playhead8 + delta_playhead8, v->sliceidx);

			float gdeadzone = clampf(minf(1.f - posjit, timestretch * 2.f), 0.f, 1.f); // if playing back normally and not jittering, add a deadzone
			float fpos = deadzone(f->pos - v->initialfingerpos, gdeadzone * 32.f);
			if (gp)
				fpos = 0.f;
//			EmuDebugLog("scrub pos %0.2f\n", fpos);
			knobsmooth_update_knob(&v->fingerpos, fpos, 2048.f);
		}
	} // sampler prep

#define OSC_COUNT 2 // DO NOT CHECK IN 1 // XXX 2
	s32 pwm_base = rampreset.params[P_PWM][0];
	s32 pwm = param_eval_finger(P_PWM, fingeridx, synthf);
	if (pwm_base >=-8 && pwm_base<8)
		pwm = 0;
	else if (pwm_base > 0)
		pwm = clampi(pwm, 1, 65535);
	else
		pwm = clampi(pwm, -65535, -1);

	

	for (int osci = 0; osci < OSC_COUNT; osci++) {
		s16 *dst = ((s16*) outbuf) + (osci & 1);
		noise = v->noise;
		float y1 = v->y[0 + osci] , y2 = v->y[2 + osci];
		int randtabpos = rand() & 16383;
		if (ramsample.samplelen) {
			// mix grains
			GrainPair* g = &v->thegrains[osci];
			int grainidx = fingeridx * 4 + osci * 2;
			int g0start = 0;
			if (grainidx) g0start = grainbufend[grainidx - 1];
			int g1start = grainbufend[grainidx];
			int g2start = grainbufend[grainidx + 1];

			int64_t posa = g->pos[0];
			int64_t posb = g->pos[1];
			int loopstart = calcloopstart(prevsliceidx);
			int loopend = calcloopend(prevsliceidx);
			bool outofrange0 = posa < loopstart || posa >= loopend;
			bool outofrange1 = posb < loopstart || posb >= loopend;
			int gvol24 = g->vol24;
			int dgvol24 = g->dvol24;
			int dpos24 = g->dpos24;
			int fpos24 = g->fpos24;
			float vol = v->vol;
			float dvol = (targetvol - vol) * (1.f / BLOCK_SAMPLES);
			outofrange0 |= g1start - g0start <= 2;
			outofrange1 |= g2start - g1start <= 2;
			g->outflags = (outofrange0 ? 1 : 0) + (outofrange1 ? 2 : 0);
			if ((g1start - g0start <= 2 && g2start - g1start <= 2)) {
				// fast mode :) emulate side effects without doing any work
				vol += dvol * BLOCK_SAMPLES;
				noise += dnoise * BLOCK_SAMPLES;
				gvol24 -= dgvol24 * BLOCK_SAMPLES;
				fpos24 += dpos24 * BLOCK_SAMPLES;
				int id = fpos24 >> 24;
				g->pos[0] += id;
				g->pos[1] += id;
				fpos24 &= 0xffffff;
			}
			else {
				const s16* src0 = (outofrange0 ? (const s16*)zero : &grainbuf[g0start + 2]) + g->bufadjust;
				const s16* src0_backup = src0;
				const s16* src1 = (outofrange1 ? (const s16*)zero : &grainbuf[g1start + 2]) + g->bufadjust;
				if (spistate && spistate <= grainidx + 2) {
					//DebugLog("!"); // spidebug
					while (spistate && spistate <= grainidx + 2);
				}
				for (int i = 0; i < BLOCK_SAMPLES; ++i) {
					int o0, o1;
#ifdef EMU
					ASSERT(outofrange0 || (src0 >= &grainbuf[g0start + 2] && src0 + 1 < &grainbuf[g1start]));
					ASSERT(outofrange1 || (src1 >= &grainbuf[g1start + 2] && src1 + 1 < &grainbuf[g2start]));
#endif
					u32 ab0 = *(u32*)(src0); // fetch a pair of 16 bit samples to interpolate between
					u32 mix = (fpos24 << (16 - 9)) & 0x7fff0000; 
					mix |= 32767 - (mix >> 16); // mix is now the weights for the linear interpolation
					SMUAD(o0, ab0, mix); // do the interpolation, result is *32768
					o0 >>= 16;

					u32 ab1 = *(u32*)(src1); // fetch a pair for the other grain in the pair
					SMUAD(o1, ab1, mix); // linear interp by same weights
					o1 >>= 16;

					fpos24 += dpos24;			// advance fractional sample pos
					int bigdpos = (fpos24 >> 24); 
					fpos24 &= 0xffffff;
					src0 += bigdpos; // advance source pointers by any whole sample increment
					src1 += bigdpos;

					mix = (gvol24 >> 9) & 0x7fff; // blend between the two grain results
					mix |= (32767 - mix) << 16;	
					u32 o01 = STEREOPACK(o0, o1);
					int ofinal;
					SMUAD(ofinal, o01, mix);
					gvol24 -= dgvol24;
					if (gvol24 < 0)
						gvol24 = 0;

					s16 n = ((s16*)rndtab)[randtabpos++]; // mix in a white noise source
					noise += dnoise;	// volume ramp for noise

					vol += dvol;		// volume ramp for grain signal
					float input = (ofinal * drive + n * noise) ; // input to filter
					float cutoff = 1.f - squaref(maxf(0.f, 1.f - vol * 1.1f)); // filter cutoff for low pass gate
					y1 += (input - y1) * cutoff; // do the lowpass
					
					int yy = FLOAT2FIXED(y1 * vol, 0); // for granular, we include an element of straight VCA
					*dst = SATURATE16(*dst + yy); // write to output
					dst += 2;
					
				}
				int bigposdelta = src0 - src0_backup;
				g->pos[0] += bigposdelta;
				g->pos[1] += bigposdelta;
			} // grain mix
			g->fpos24 = fpos24;
			g->vol24 = gvol24;
			
			if (gvol24 <= dgvol24 || trig) { // new grain trigger! this is for the *next* frame
//				if (targetvol > 0.01f)
//					DebugLog("%c", 'a' + (int)(targetvol * 25));
				if (targetvol > 0.01f) {
					int i = 1;
				}
				if (!trig) {
					int i = 1;
				}
				int ph = v->playhead8 >> 8;
				int slicelen = ramsample.splitpoints[v->sliceidx + 1] - ramsample.splitpoints[v->sliceidx];
				if (editmode != EM_SAMPLE) {
					ph += ((int)(v->fingerpos.y2 * slicelen)) >> (10);
					ph += smppos; // scrub input
				}
				g->vol24 = ((1 << 24) - 1);
				int grainsize = ((rand() & 127) * sizejit + 128.f) * (gsize * gsize) + 0.5f;
				grainsize *= BLOCK_SAMPLES;
				int jitpos = (rand() & 255) * posjit;
				ph += ((grainsize+8192) * jitpos) >> 8;
				g->dvol24 = g->vol24 / grainsize;

				float grate2 = 1.f + ((rand() & 255) * (gratejit * gratejit)) * (1.f / 256.f);
				//float revprob = (0.125f - timestretch) * (4.f * 256.f);
				//if ((rand() & 255) < (int)revprob)
				if (timestretch<0.f)
					grate2 = -grate2;
				g->grate_ratio = grate2;
#ifdef EMU
//				if (osci==0 && (targetvol > 0.01f || trig))
//					EmuDebugLog("%s grain at ph %d, other at %d, volume went from %f to %f, next to %f \n",
//						trig ? "trigger" : "new",
//						ph, (int)(g->pos[1]), v->vol, targetvol);
#endif
				g->pos[0] = trig ? ph : g->pos[1];
				g->pos[1] = ph;
			}
		}
		else
		{
			// synth 
			float vol = v->vol;
			float dvol = (targetvol - vol) * (1.f / BLOCK_SAMPLES);

			Osc* o = &v->theosc[osci];

			u32 flippity = 0;
			if (pwm!=0) {
				flippity = ~0;
				//if (abs(pwm) 
				{
					//u32 pp = pwm >> 16;
					//if (pp > 1024) pp = 1024;
					u32 avgdp = (o[0].dphase + o[2].dphase) / 2;
					o[0].dphase = avgdp;//(s32)((avgdp - o[0].dphase) * pp) >> 10;
					o[2].dphase = avgdp;// (s32)((avgdp - o[2].dphase)* pp) >> 10;
					avgdp = (o[0].targetdphase + o[2].targetdphase) / 2;
					o[0].targetdphase = avgdp;// (s32)((avgdp - o[0].targetdphase)* pp) >> 10;
					o[2].targetdphase = avgdp;// (s32)((avgdp - o[2].targetdphase)* pp) >> 10;

					if (pwm < 0) {
						s32 phase0fix = (s32)(o[2].phase - o[0].phase - (pwm<<16) + (1 << 31)) / (BLOCK_SAMPLES);
						o[0].dphase += phase0fix;
						o[0].targetdphase += phase0fix;
					}
				}
			}
			//		o->dphase=o->targetdphase;// XXXX REMOVE GLIDE
			int ddphase1 = (int)((o->targetdphase - o->dphase) * glide);
			u32 phase1 = o->phase;
			s32 dphase1 = o->dphase;
			u32 prevsample1 = o->prevsample;
			o += 2;
			//		o->dphase=o->targetdphase;// XXXX REMOVE GLIDE
			int ddphase2 = (int)((o->targetdphase - o->dphase) * glide);
			u32 phase2 = o->phase;
			s32 dphase2 = o->dphase;
			u32 prevsample2 = o->prevsample;
			o -= 2;

			if (pwm>0) {
				//pwm -= 4096;
				// we need to choose the shift so that, after shifting right, there are 16 bits of fractional part in each cycle
				// if the increment was say, 1<<(32-9) = ((1<<23)-1), we would take just over 512 steps to cycle, so we want to shift by 7
				//dphase1 = (1 << 22) - 1;
				int shift1 = 16-clz(maxi(dphase1,1<<22)); 
				const static u16 wavetable_octave_offset[17] = { 0,0,0,0,0,0,0,
					0,// 7 - 9 bits
					513, // 8 - 8 bits
					513 + 257, // 9 - 7 bits
					513 + 257 + 129, // 10 - 6 bits
					513 + 257 + 129 + 65,
					513 + 257 + 129 + 65 + 33,
					513 + 257 + 129 + 65 + 33 + 17,
					513 + 257 + 129 + 65 + 33 + 17 + 9,
					513 + 257 + 129 + 65 + 33 + 17 + 9 + 5,
					513 + 257 + 129 + 65 + 33 + 17 + 9 + 5 + 3, // 1031
				};
				// 16 wave shapes
//				pwm = 2<<12;
				u32 subwave = (pwm & 4095) << (1);
				//subwave = 0; DISABLE BLENDING
				subwave = subwave | ((8191 - subwave) << 16);
				int wtbase = (pwm) >> (12);
				const s16* table1 = wavetable[wtbase] + wavetable_octave_offset[shift1];
				for (int i = 0; i < BLOCK_SAMPLES; ++i) {
					unsigned int i,i2;
					int s0, s1;
					dphase1 += ddphase1;
					i = (phase1 += dphase1) >> shift1;
					i2 = i >> 16;
					s0 = table1[i2], s1 = table1[i2 + 1];
					s32 out0 = (s0<<16) + ((s1 - s0) * (u16)(i));
					i2 += WAVETABLE_SIZE;
					s0 = table1[i2], s1 = table1[i2 + 1];
					s32 out1 = (s0<<16) + ((s1 - s0) * (u16)(i));
					u32 packed=STEREOPACK(out1>>16, out0>>16);
					s32 out;
					SMUAD(out,packed,subwave);
					//////////////////////////////////////////////////
					// rest is same as polyblep
					s16 n = ((s16*)rndtab)[randtabpos++];
					noise += dnoise;

					vol += dvol;
					y1 += (out * drive + n * noise - (y2 - y1) * resonance - y1) * vol; // drive
	//				y1 *= 16383.f / (16384.f + fabsf(y2));
					y1 *= 0.999f;
					y2 += (y1 - y2) * vol;

					y2 *= 0.999f;

					int yy = FLOAT2FIXED(y2, 0);
					*dst = SATURATE16(*dst + yy);
					dst += 2;
				}
			}
			else {
				for (int i = 0; i < BLOCK_SAMPLES; ++i) {
					dphase1 += ddphase1;
					phase1 += dphase1;
					u32 newsample1 = phase1;

					if (unlikely(phase1 < (u32)dphase1)) {
						// edge! polyblep it.
						u32 fractime = mini(65535, phase1 / (dphase1 >> 16));
						prevsample1 -= (fractime * fractime) >> 1;
						fractime = 65535 - fractime;
						newsample1 += (fractime * fractime) >> 1;
					}
					s32 out = (s32)(prevsample1 >> 4); // (s32)prevsample1 >> 4
					prevsample1 = newsample1;

					dphase2 += ddphase2;
					phase2 += dphase2;
					u32 newsample2 = phase2;
					if (unlikely(phase2 < (u32)dphase2)) {
						// edge! polyblep it.
						u32 fractime = mini(65535, phase2 / (dphase2 >> 16));
						prevsample2 -= (fractime * fractime) >> 1;
						fractime = 65535 - fractime;
						newsample2 += (fractime * fractime) >> 1;

					}

#ifdef EMU
					/*
					if (fingeridx == 3 && targetvol>0.f && osci==1) {
						static FILE* testy = 0;
						if (!testy)
							testy = fopen("testy.raw", "wb");
							s16 aa = (out >> 16) - 32678 / 16;
						s16 bb = (((prevsample2 ^ flippity) >> 4) >> 16) - 32768 / 16;
						fwrite(&aa, 1, 2, testy);
						fwrite(&bb, 1, 2, testy);
					}*/
#endif

					out += (s32)((prevsample2 ^ flippity) >> 4) - (2 << (31 - 4));
					prevsample2 = newsample2;

					s16 n = ((s16*)rndtab)[randtabpos++];
					noise += dnoise;

					vol += dvol;
					y1 += (out * drive + n * noise - (y2 - y1) * resonance - y1) * vol; // drive
	//				y1 *= 16383.f / (16384.f + fabsf(y2));
					y1 *= 0.999f;
					y2 += (y1 - y2) * vol;

					y2 *= 0.999f;

					int yy = FLOAT2FIXED(y2, 0);
					*dst = SATURATE16(*dst + yy);
					dst += 2;
				} // samples
			}
			o[0].phase = phase1;
			o[0].dphase = dphase1;
			o[0].prevsample = prevsample1;

			o[2].phase = phase2;
			o[2].dphase = dphase2;
			o[2].prevsample = prevsample2;
		} // synth
		v->y[osci] = y1, v->y[osci + 2] = y2;
	}// osc loop
	
	v->vol = targetvol;
	v->noise = noise;

	if (ramsample.samplelen) {
		// update pitch (aka dpos24) for next time!
		for (int gi = 0; gi < 2; ++gi) {
			float multisample_grate;
			if (ramsample.pitched && !isgrainpreview()) {
				int relpitch = v->theosc[1 + gi].pitch - ramsample.notes[v->sliceidx] * 512;
				if (relpitch < -512 * 12 * 5) {
					multisample_grate = 0.f;
				}
				else {
					multisample_grate = // exp2f(relpitch / (512.f * 12.f));
						table_interp(pitches, relpitch+32768);

				}
			}
			else {
				multisample_grate = 1.f;
			}
//			multisample_grate = 1.f; // debug - force multisample to original pitch
			v->thegrains[gi].multisample_grate = multisample_grate;
			int dpos24 = (1 << 24) * (grate * v->thegrains[gi].grate_ratio * multisample_grate);
			while (dpos24 > (2 << 24))
				dpos24 >>= 1;
			v->thegrains[gi].dpos24 = dpos24;
		}
	}

	tc_stop(&_tc_osc);

}


s32 Reverb2(s32 input, s16 *buf) {
	int i = reverbpos;
//	int fb = buf[i];
	int outl = 0, outr = 0;
	float wob = lfo_next(&aplfo) * k_reverb_wob;
	int apwobpos = FLOAT2FIXED((wob + 1.f), 12 + 6);
	wob = lfo_next(&aplfo2)  * k_reverb_wob;
	int delaywobpos = FLOAT2FIXED((wob + 1.f), 12 + 6);
#define RVDIV /2
#define CHECKACC // assert(acc>=-32768 && acc<32767);
#define AP(len) { \
		int j = (i + len RVDIV) & RVMASK; \
		s16 d = buf[j]; \
		acc -= d >> 1; \
		buf[i] = SATURATE16(acc); \
		acc = (acc >> 1) + d; \
		i = j; \
		CHECKACC \
	}
#define AP_WOBBLE(len, wobpos) { \
		int j = (i + len RVDIV) & RVMASK;\
		s16 d = LINEARINTERPRV(buf, j, wobpos); \
		acc -= d >> 1; \
		buf[i] = SATURATE16(acc); \
		acc = (acc >> 1) + d; \
		i = j; \
		CHECKACC \
	}
#define DELAY(len) { \
		int j = (i + len RVDIV) & RVMASK; \
		buf[i] = SATURATE16(acc); \
		acc = buf[j]; \
		i = j; \
		CHECKACC \
	}
#define DELAY_WOBBLE(len, wobpos) { \
		int j = (i + len RVDIV) & RVMASK; \
		buf[i] = SATURATE16(acc); \
		acc = LINEARINTERPRV(buf, j, wobpos); \
		i=j; \
		CHECKACC \
	}

	// Griesinger according to datorro does 142, 379, 107, 277 on the way in - totoal 905 (20ms)
	// then the loop does 672+excursion, delay 4453, (damp), 1800, delay 3720 - total 10,645 (241ms)
	// then decay, and feed in
	// and on the other side 908+excursion,	delay 4217, (damp), 2656, delay 3163 - total 10,944 (248 ms)

	// keith barr says:
	// I really like 2AP, delay, 2AP, delay, in a loop.
	// I try to set the delay to somewhere a bit less than the sum of the 2 preceding AP delays, 
	// which are of course much longer than the initial APs(before the loop)
	// Yeah, the big loop is great; you inject input everywhere, but take it out in only two places
	// It just keeps comin� newand fresh as the thing decays away.�If you�ve got the memoryand processing!

	// lets try the 4 greisinger initial Aps, inject stereo after the first AP,
	
	int acc = ((s16)(input)) * k_reverbsend >> 17;
	AP(142);
	AP(379);
	acc += (input >> 16) * k_reverbsend >> 17;
	AP(107);
//	int reinject2 = acc;
	AP(277);
	int reinject = acc;
	static int fb1 = 0;
	acc += fb1;
	AP_WOBBLE(672, apwobpos);
	AP(1800);
	DELAY(4453);

	if (1) {
		// shimmer - we can read from up to about 2000 samples ago

		// Brief shimmer walkthrough:
		// - We walk backwards through the reverb buffer with 2 indices: shimmerpos1 and shimmerpos2.
		//   - shimmerpos1 is the *previous* shimmer position.
		//   - shimmerpos2 is the *current* shimmer position.
		//   - Note that we add these to i (based on reverbpos), which is also walking backwards
		//     through the buffer.
		// - shimmerfade controls the crossfade between the shimmer from shimmerpos1 and shimmerpos2.
		//   - When shimmerfade == 0, shimmerpos1 (the old shimmer) is chosen.
		//   - When shimmerfade == SHIMMER_FADE_LEN - 1, shimmerpos2 (the new shimmer) is chosen.
		//   - For everything in-between, we linearly interpolate (crossfade).
		//   - When we hit the end of the fade, we reset shimmerpos2 to a random new position and set
		//     shimmerpos1 to the old shimmerpos2.
		// - dshimmerfade controls the speed at which we fade.

		#define SHIMMER_FADE_LEN 32768
		shimmerfade += dshimmerfade;

		if (shimmerfade >= SHIMMER_FADE_LEN) {
			shimmerfade -= SHIMMER_FADE_LEN;

			shimmerpos1 = shimmerpos2;
			shimmerpos2 = (rand() & 4095) + 8192;
			dshimmerfade = (rand() & 7) + 8; // somewhere between SHIMMER_FADE_LEN/2048 and SHIMMER_FADE_LEN/4096 ie 8 and 16
		}

		// L = shimmer from shimmerpos1, R = shimmer from shimmerpos2
		u32 shim1 = STEREOPACK(buf[(i + shimmerpos1) & RVMASK], buf[(i + shimmerpos2) & RVMASK]);
		u32 shim2 = STEREOPACK(buf[(i + shimmerpos1 + 1) & RVMASK], buf[(i + shimmerpos2 + 1) & RVMASK]);
		u32 shim = STEREOADDAVERAGE(shim1, shim2);

		// Fixed point crossfade:
#ifdef CORTEX
		u32 a = STEREOPACK((SHIMMER_FADE_LEN - 1) - shimmerfade, shimmerfade);
		s32 shimo;
		asm("smuad %0, %1, %2" : "=r" (shimo) : "r" (a), "r" (shim));
#else
		STEREOUNPACK(shim);
		s32 shimo = shiml * ((SHIMMER_FADE_LEN - 1) - shimmerfade) +
								shimr * shimmerfade;
#endif
		shimo >>= 15;  // Divide by SHIMMER_FADE_LEN

		// Apply user-selected shimmer amount.
		shimo *= k_reverb_shim;
		shimo >>= 8;

		// Tone down shimmer amount.
		shimo >>= 1;

		acc += shimo;
		outl = shimo;
		outr = shimo;

		shimmerpos1--;
		shimmerpos2--;
	}


	const static float k_reverb_color = 0.95f;
	static float lpf = 0.f, dc = 0.f;
	lpf += (((acc * k_reverb_fade) >> 8) - lpf) * k_reverb_color;
	dc += (lpf - dc) * 0.005f;
	acc = (int)(lpf - dc);
	outl += acc;

	acc += reinject;
	AP_WOBBLE(908, delaywobpos);
	AP(2656);
	DELAY(3163);
	static float lpf2 = 0.f;
	lpf2+= (((acc * k_reverb_fade) >> 8) - lpf2) * k_reverb_color;
	acc = (int)(lpf2);

	outr += acc;

//	acc += reinject2;
//	AP(4931);
//	AP(3713);
//	DELAY(6137);
	/*
	int acc=((s16) (input))*k_reverbsend >> 17;
	AP(142);
	acc += (input >> 16) * k_reverbsend >> 17;
	AP(107);
	AP_WOBBLE(379, delaywobpos);
	AP(277);
	static int fb1=0;
	// first leg
	acc+=fb1;
	AP_WOBBLE(672, apwobpos);
	AP(4453);

	if (1) {
		// shimmer - we can read from up to about 2000 samples ago
		shimmerfade += dshimmerfade;
		if (shimmerfade >= 32768) {
			shimmerpos1 = shimmerpos2;
			shimmerpos2 = (rand() & 1023) + 1024;
			shimmerfade -= 32768;
			dshimmerfade = (rand() & 31) + 32; // somewhere between 65536/1024 and 65536/512 ie 64 and 128
		}
		u32 shim1 = STEREOPACK(buf[(i + shimmerpos1) & RVMASK], buf[(i + shimmerpos2) & RVMASK]);
		u32 shim2 = STEREOPACK(buf[(i + shimmerpos1 + 1) & RVMASK], buf[(i + shimmerpos2 + 1) & RVMASK]);
		u32 shim = STEREOADDAVERAGE(shim1, shim2);
#ifdef CORTEX
		u32 a = STEREOPACK(32767 - shimmerfade, shimmerfade);
		s32 shimo;
		asm ("smuad %0, %1, %2" : "=r" (shimo) : "r" (a), "r" (shim));
#else
		STEREOUNPACK(shim);
		s32 shimo=(shimr*shimmerfade + shiml*(32767-shimmerfade));
#endif
		shimo >>= 16;
		shimo*=k_reverb_shim;
		shimo>>=8;
		acc += shimo>>1;
		outl = shimo;
		outr = shimo;
		shimmerpos1--;
		shimmerpos2--;
		//	outl=outr=shimo;
	}


	outr+=acc;

	static float lpf = 0.f, dc=0.f;
	const static float k_reverb_color = 0.95f;
	lpf += (((acc*k_reverb_fade)>>8) - lpf) * k_reverb_color;
	dc+=(lpf-dc)*0.005f;
	acc = (int) (lpf - dc);
	AP(1800);
	DELAY(3270);
	outl+=acc;
	*/
	reverbpos = (reverbpos - 1) & RVMASK;
	fb1=(acc*k_reverb_fade)>>8;
	return STEREOPACK(SATURATE16(outl), SATURATE16(outr));
	

}





u32 STEREOSIGMOID(u32 in) {
	u16 l = sigmoid[(u16) in];
	u16 r = sigmoid[in >> 16];
	return STEREOPACK(l, r);
}

s16 MONOSIGMOID(int in) {
	in=SATURATE16(in);
	return sigmoid[(u16) in];
}

void update_params(int fingertrig, int fingerdown);

	

#ifdef EMU
float powerout; // squared power 
float gainhistoryrms[512];
int ghi;
#endif

int stride(u32 scale, int stride_semitones, int fingeridx) { // returns the number of scale steps up from 0 for finger index fi
	static u8 stridetable[8]; // memoise the result indexed by fi
	static u8 stridehash[8];
	//XXX TEST stride_semitones =2;
	u8 newhash = stride_semitones + (scale * 16);
	if (newhash != stridehash[fingeridx]) {
		// memoise this man
		stridehash[fingeridx] = newhash;
		int pos = 0;
		u8 usedsteps[16] = { 1, 0 };
		const u16* scaleptr = scaletab[scale];
		u8 numsteps = *scaleptr++;
		for (int fi = 0; fi < fingeridx; ++fi) {
			pos += stride_semitones;
			int step = pos % 12;
			int best = 0, bestdist = 0;
			int bestscore = 9999;
			for (int i = 0; i < numsteps; ++i) {
				int qstep = scaleptr[i]/512; // candidate step in the scale
				int dist = qstep - step;
				if (dist < -6) {
					dist += 12; qstep += 12;
				}
				else if (dist > 6) {
					dist -= 12; qstep -= 12;
				}
				int score = abs(dist) * 16 + usedsteps[i];  // penalise steps we have used many times
				if (score < bestscore) {
					bestscore = score;
					best = i;
					bestdist = dist;
				}
			}
			usedsteps[best]++;
			pos += bestdist;
			stridetable[fingeridx] = best + (pos / 12) * numsteps;
		}
	}
	return stridetable[fingeridx];
}

#ifdef EMU
float m_compressor,m_dry, m_audioin, m_dry2wet, m_delaysend, m_delayreturn, m_reverbin, m_reverbout, m_fxout, m_output;
void MONITORPEAK(float *mon, u32 stereoin) {
	STEREOUNPACK(stereoin);
	float peak = (1.f/32768.f)*maxi(abs(stereoinl), abs(stereoinr));
	if (peak > *mon) *mon = peak;
	else *mon += (peak - *mon) * 0.0001f;
}
#else
#define MONITORPEAK(mon,stereoin)
#endif
u8 midi24ppqncounter;
const s8 midicctable[128] = {
	//					0			1			2			3			4			5			6			7
	/*   0 */			-1,			-1,			P_NOISE,	P_SENS,		P_DRIVE,	P_GLIDE,	-1,			P_MIXSYNTH,
	/*   8 */			P_MIXWETDRY,P_PITCH,	-1,			P_GATE_LENGTH,P_DLTIME,	P_PWM,		P_INTERVAL,	P_SMP_POS,
	/*  16 */			P_SMP_GRAINSIZE,P_SMP_RATE,P_SMP_TIME,P_ENV_LEVEL,P_A2,		P_D2,		P_S2,		P_R2,
	/*  24 */			P_AFREQ,	P_ADEPTH,	P_AOFFSET,	P_BFREQ,	P_BDEPTH,	P_BOFFSET,	-1,			P_MIXHPF,
	/*  32 */			-1,			-1,			-1,			-1,			-1,			-1,			-1,			-1,
	/*  40 */			-1,			-1,			-1,			-1,			-1,			-1,			-1,			-1,
	/*  48 */			-1,			-1,			-1,			-1,			-1,			-1,			-1,			-1,
	/*  56 */			-1,			-1,			-1,			-1,			-1,			-1,			-1,			-1,
	/*  64 */			-1,			-1,			-1,			-1,			-1,			-1,			-1,			P_MIXRESO,
	/*  72 */			P_R,		P_A,		P_S,		P_D,		P_XFREQ,	P_XDEPTH,	P_XOFFSET,	P_YFREQ,
	/*  80 */			P_YDEPTH,	P_YOFFSET,	P_SAMPLE,	P_SEQPAT,	-1,			P_SEQSTEP,	-1,			-1,
	/*  88 */			-1,			P_MIXINPUT,	P_MIXINWETDRY,P_RVSEND,	P_RVTIME,	P_RVSHIM,	P_DLSEND,	P_DLFB,
	/*  96 */			-1,			-1,			-1,			-1,			-1,			-1,			P_ARPONOFF,	P_ARPMODE,
	/* 104 */			P_ARPDIV,	P_ARPPROB,	P_ARPLEN,	P_ARPOCT,	P_SEQMODE,	P_SEQDIV,	P_SEQPROB,	P_SEQLEN,
	/* 112 */			P_DLRATIO,	P_DLWOB,	P_RVWOB,	-1,			P_JIT_POS,	P_JIT_GRAINSIZE, P_JIT_RATE, P_JIT_PULSE,
	/* 120 */		-1,			-1,			-1,			-1,			-1,			-1,			-1,			-1,
};
bool midi_receive(unsigned char packet[4]); // usb midi poll
void processmidimsg(u8 msg, u8 d1, u8 d2);
bool processusbmidi(void) {
	u8 midipacket[4];
	if (!midi_receive(midipacket))
		return false;
	// 09 90 30 64
	// 08 80 30 40
	//DebugLog("got midi %02x %02x %02x %02x\r\n", midipacket[0], midipacket[1], midipacket[2], midipacket[3]);
	u8 msg = midipacket[1];
	u8 d1 = midipacket[2];
	u8 d2 = midipacket[3];
	processmidimsg(msg,d1,d2);
	return true;
}


void midi_panic(void) {
	midi_pressure_override=0, midi_pitch_override=0;
	memset(midi_notes,0,sizeof(midi_notes));
	memset(midi_velocities, 0, sizeof(midi_velocities));
	memset(midi_aftertouch, 0, sizeof(midi_aftertouch));
	memset(midi_channels, 255, sizeof(midi_channels));
	memset(midi_chan_aftertouch, 0, sizeof(midi_chan_aftertouch));
	memset(midi_chan_pitchbend, 0, sizeof(midi_chan_pitchbend));
	midi_next_finger = 0;
}

bool send_midimsg(u8 status, u8 data1, u8 data2);
void processmidimsg(u8 msg, u8 d1, u8 d2) {
	u8 chan = msg & 15;
	u8 type = msg >> 4;
	if ((chan != 0)&&(type != 0xF)) //LPZW KAY Fix for MIDI Sync type == F continue
		return; 
	if (type < 8)
		return;

//	send_midimsg(msg, d1, d2); // midi echo!

	if (type == 9 && d2 == 0)
		type = 8;
	
	switch (type) {
	case 0xc: // program change
		if (d1<32)
			SetPreset(d1, false);
		break;
	case 8: { // note up 
	   // find the voice for this note up
		u8 fi = find_midi_note(chan, d1);
		if (fi < 8) {
			midi_pressure_override &= ~(1 << fi);
			midi_channels[fi] = 255;
		}
	}
	break;
	case 9: { // note down
		u8 fi = find_midi_note(chan, d1);
		if (fi == 255)
			fi = find_midi_free_channel();
		if (fi < 8) {
			midi_notes[fi] = d1;
			midi_channels[fi] = chan;
			midi_velocities[fi] = d2;
			midi_aftertouch[fi] = 0;
			midi_pressure_override |= 1 << fi;
			midi_pitch_override |= 1 << fi;
		}
	}
		  break;
	case 0xe: // pitchbend
		midi_chan_pitchbend[chan] = (d1 + (d2 << 7)) - 0x2000;
		break;
	case 0xa: { // polyphonic aftertouch
			u8 fi = find_midi_note(chan, d1);
			if (fi < 8) {
				midi_aftertouch[fi] = d2;
			}
		}
		break;
	case 0xd: { // channel aftertouch
			midi_chan_aftertouch[chan] = d2;
		}
		break;
	case 0xb: // cc param
	{
		if (d1 >= 32 && d1 < 64)
			midi_lsb[d1 - 32] = d2;
		s8 param = (d1 < 128) ? midicctable[d1] : -1;
		if (param >= 0 && param < P_LAST) {
			int val;
			if (d1 < 32)
				val = (d2 << 7) + midi_lsb[d1];
			else
				val = (d2 << 7) + d2;
			val = (val * FULL) / (127 * 128 + 127);
			if (param_flags[param] & FLAG_SIGNED)
				val = val * 2 - FULL;
			EditParamNoQuant(param, M_BASE, val);
		}
		break;
	}
	case 0xf: // system
		if (msg == 0xfa) { // start
			got_ui_reset = true;
			midi24ppqncounter = 5; // 2020-02-26: Used to be 0, changed to 5: https://discord.com/channels/784856175937585152/784884878994702387/814951459581067264
			playmode = PLAYING;
			//		seq_step(1);
		}
		else if (msg == 0xfb) { // continue
			playmode = PLAYING;
		}
		else if (msg == 0xfc) { // stop
			midi24ppqncounter = 0;
			playmode = PLAY_STOPPED;
			OnLoop();
		}
		else if (msg == 0xf8) {
			// midi clock! 24ppqn, we want 4, so divide by 6.		
			midi24ppqncounter++;
			if (midi24ppqncounter == 6) {
				gotclkin++;
				midi24ppqncounter = 0;
			}

		}
		break;
	}

}

void DoRecordModeAudio(u32* dst, u32* audioin) {
	// recording or level testing
	int newaudiorec_gain = 65535 - GetADCSmoothedNoCalib(ADC_AKNOB);
	if (abs(newaudiorec_gain - audiorec_gain_target) > 256) // hysteresis
		audiorec_gain_target = newaudiorec_gain;
	knobsmooth_update_knob(&recgain_smooth, audiorec_gain_target, 65536.f);
	int audiorec_gain = (int)(recgain_smooth.y2)/2;


	cur_sample1 = edit_sample0 + 1;
	CopySampleToRam(false);
	if (enable_audio == EA_ARMED) {
		if (audioin_peak > 1024) {
			recording_trigger();
		}
	}
	if (enable_audio > EA_PREERASE && enable_audio < EA_STOPPING4) {
		s16* dldst = delaybuf + (recpos & DLMASK);
		const s16* asrc = (const s16*)audioin;
		s16* adst = (s16*)dst;
		if (enable_audio >= EA_STOPPING1) {
			memset(dldst, 0, BLOCK_SAMPLES * 2);
			enable_audio++;
		}
		else {
			for (int i = 0; i < BLOCK_SAMPLES; ++i) {
				s16 smp = *dldst++ = SATURATE16((((int)(asrc[0] + asrc[1])) * audiorec_gain) >> 14);
#ifdef EMU
				smp = 0; // disable loopback
#endif
				adst[0] = adst[1] = smp;
				adst += 2;
				asrc += 2;
			}
		}
		recpos += BLOCK_SAMPLES;
	}
}

s16 audioin_is_stereo = 0;
s16 noisegate=0;
#ifdef DEBUG
//#define NOISETEST
#endif
#ifdef NOISETEST
float noisetestl=0, noisetestr=0,noisetest=0;
#endif
void PreProcessAudioIn(u32* audioin) {
	int newpeak = 0, newpeakr=0;
#ifdef NOISETEST
	int newpeakl=0;
#endif
	static float dcl, dcr;
	int ng=mini(256, noisegate);
	// dc remover from audio in, and peak detector while we're there.
	for (int i = 0; i < BLOCK_SAMPLES; ++i) {
		u32 inp = audioin[i];
		STEREOUNPACK(inp);
		dcl += (inpl - dcl) * 0.0001f;
		dcr += (inpr - dcr) * 0.0001f;
		inpl -= dcl;
		inpr -= dcr;
		newpeakr = maxi(newpeakr, abs(inpr));
#ifdef NOISETEST
		newpeakl = maxi(newpeakl, abs(inpl));
#endif
		if (!audioin_is_stereo)
			inpr = inpl;
		newpeak = maxi(newpeak, abs(inpl + inpr));
		inpl=(inpl*ng) >> 8;
		inpr=(inpr*ng) >> 8;

		audioin[i] = STEREOPACK(inpl, inpr);
	}
	if (newpeak > 400)
		noisegate=1000;
	else
		if (noisegate > 0)
			noisegate--;

	if (newpeakr > 300)
		audioin_is_stereo = 1000;
	else
		if (audioin_is_stereo > 0)
			audioin_is_stereo--;

#ifdef NOISETEST
	if(newpeakl>noisetestl) noisetestl=newpeakl; else noisetestl+=(newpeakl-noisetestl)*0.01f;
	if(newpeakr>noisetestr) noisetestr=newpeakr; else noisetestr+=(newpeakr-noisetestr)*0.01f;
	if(newpeak>noisetest) noisetest=newpeak; else noisetest+=(newpeak-noisetest)*0.01f;
#endif
	int audiorec_gain = (int)(recgain_smooth.y2)/2;


	newpeak = SATURATE16((newpeak * audiorec_gain) >> 14);
	audioin_peak = maxi((audioin_peak * 220) >> 8, newpeak);
	if (audioin_peak > audioin_hold || audioin_holdtime++ > 500) {
		audioin_hold = audioin_peak;
		audioin_holdtime = 0;
	}
}
static s16 scopex = 0;

void usb_midi_update(void);
void serial_midi_update(void);

u8 midi_last_pitch[8];
u8 midi_first_vol[8];
u8 midi_last_at[8];
u8 midi_last_pos[8];
u8 midi_last_pressure[8];
u8 midi_send_chan;
u8 midi_desired_note[8];
void kick_midi_send(void);
bool serial_midi_ready(void);

void midi_send_update(void) {
	if (!serial_midi_ready())
		return;
	for (int i = 0; i < 8; ++i) {
		Finger* synthf = touch_synth_getlatest(midi_send_chan);
		Finger* prevsynthf = touch_synth_getprev(midi_send_chan);

		bool pressurestable = abs(prevsynthf->pressure - synthf->pressure) < 100;
		bool posstable = abs(prevsynthf->pos - synthf->pos) < 32;
		bool pressure_significant = synthf->pressure > 200;

		int desired_pitch = midi_desired_note[midi_send_chan];
		int desired_vol = clampi((synthf->pressure-100)/48, 0, 127);
		//int prev_vol = clampi((prevsynthf->pressure-100) / 48, 0, 127);
		u8 desired_pos = clampi(127 - (synthf->pos/13-16), 0, 127);
		if (desired_pitch == 0)
			desired_vol = 0;
		if (arpmode >= 0 && !(arpbits & (1 << midi_send_chan))) {
			desired_vol = 0;
		}
		if (desired_vol <= 0)
			desired_pitch = 0;
		bool sent = false;
		u8 cur_pitch = midi_last_pitch[midi_send_chan];

		if (desired_pitch != cur_pitch && (desired_pitch==0 || (posstable && pressurestable))) {
			// send note up
			if (cur_pitch != 0) {
				//if (midi_send_chan == 0) printf("note up because %d vs %d\n", desired_pitch, cur_pitch);
				if (!send_midimsg(0x80, cur_pitch, 0)) break;
				midi_last_pitch[midi_send_chan] = 0;
				midi_last_at[midi_send_chan] = 0;
				sent=true;
			}
			// send new note down
			if (desired_pitch != 0) {
				//if (midi_send_chan == 0) printf("note down because %d vs %d %d\n", desired_pitch, cur_pitch, desired_vol);
				if (!send_midimsg(0x90, desired_pitch, desired_vol)) break;
				midi_last_pitch[midi_send_chan] = desired_pitch;
				midi_first_vol[midi_send_chan] = desired_vol;
				midi_last_at[midi_send_chan] = 0;
				sent=true;
			}
		}
		
		u8 cur_at = midi_last_at[midi_send_chan];
		int desired_at = desired_vol - midi_first_vol[midi_send_chan];
		if (desired_at < 0) desired_at = 0;
		if (abs(desired_at -cur_at) > 4) {
			if (!send_midimsg(0xa0, cur_pitch, desired_at)) break;
			midi_last_at[midi_send_chan] = desired_at;
			sent=true;
		}
		
		u8 cur_pos = midi_last_pos[midi_send_chan];
		if (abs(desired_pos - cur_pos) > 1 && pressure_significant && pressurestable) {
			if (!send_midimsg(0xb0, 32 + midi_send_chan, desired_pos)) break;
			midi_last_pos[midi_send_chan] = desired_pos;
			sent=true;
		}
		u8 cur_pressure = midi_last_pressure[midi_send_chan];
		if (abs(desired_vol - cur_pressure) > 1) {
			if (!send_midimsg(0xb0, 40 + midi_send_chan, desired_vol)) break;
			midi_last_pressure[midi_send_chan] = desired_vol;
			sent=true;
		}
		
		midi_send_chan = (midi_send_chan + 1) & 7;
		if (sent) {
			kick_midi_send();
			break;
		}
	}
}

int audiotime = 0;
void DoAudio(u32 *dst, u32 *audioin) {
	audiotime += BLOCK_SAMPLES;
	tc_start(&_tc_audio);
	memset(dst, 0, 4 * BLOCK_SAMPLES);
	PreProcessAudioIn(audioin); // dc remover; peak detector
//	enable_audio = EA_PASSTHRU;
	if (enable_audio <= 0) {
		if (enable_audio == EA_PASSTHRU) {
		//	memcpy(dst, audioin, 4 * BLOCK_SAMPLES);
			for (int i=0;i<BLOCK_SAMPLES;++i) {
				float t=(i-BLOCK_SAMPLES/2)*(2.f/BLOCK_SAMPLES);
				if (t<0) t=-t;
				t=EvalSin(t,0);
				s16 ss=t*32767.f;
				t=(((i*2)&(BLOCK_SAMPLES-1))-BLOCK_SAMPLES/2)*(2.f/BLOCK_SAMPLES);
				if (t<0) t=-t;
				t=EvalSin(t,0);
				s16 tt=t*32767.f;
				dst[i]=STEREOPACK(tt,ss);
			}
		}
		return;
	}

	if (enable_audio > EA_PLAY) {
		DoRecordModeAudio(dst, audioin);
		return;
	} 
	int ainlvl = param_eval_int(P_MIXINPUT, any_rnd, env16, pressure16);
	int audiorec_gain_target = ainlvl; // we want to update recgain_smooth as it is used to maintain the pretty audio gain history
	knobsmooth_update_knob(&recgain_smooth, audiorec_gain_target, 65536.f);

	//////////////////////////////////////////////////////////
	// PLAYMODE

	CopyPresetToRam(false);
	// a few midi messages per tick. WCGW 	
	if (1) {
		midi_send_update();
#ifndef EMU
		usb_midi_update();
		serial_midi_update();
#endif
		for (int i = 0; i < 2; ++i) if (!processusbmidi())
			break;
	}
	// do the clock first so we can update the sequencer step etc
	bool gotclock = update_clock();
	static u8 whichhalf = 0;
	for (int fi = whichhalf; fi < whichhalf + 4; ++fi) {
		finger_synth_update(fi);
		if (fi == 7) {

			if (total_ui_pressure<=0 && prev_total_ui_pressure <= 0 && prev_prev_total_ui_pressure > 0 && recording && playmode != PLAYING) {
				// you've released your fingers, you're recording in step mode - let's advance!
				set_cur_step(cur_step + 1, false);
			}
			prev_prev_total_ui_pressure = prev_total_ui_pressure;
			prev_total_ui_pressure = total_ui_pressure;
			//  rather than incrementing, we let finger_frame_synth shadow the ui. that way we get the full variability of the 
			// ui input (due to it tikcing slowly), but we dont accidentally read ahead
			finger_frame_synth = finger_frame_ui;
			//(finger_frame_synth + 1) & 7;
		}
	}
	whichhalf ^= 4;

	prevsynthfingerdown = synthfingerdown;
	synthfingerdown = 0;
	prevprevsynthfingerdown_nogatelen = prevsynthfingerdown_nogatelen;
	prevsynthfingerdown_nogatelen = synthfingerdown_nogatelen;
	synthfingerdown_nogatelen = synthfingerdown_nogatelen_internal;
	for (int fi = 0; fi < 8; ++fi) {
		Finger* synthf = touch_synth_getlatest(fi);
		int thresh = (prevsynthfingerdown & (1 << fi)) ? -50 : 1;
		if (synthf->pressure > thresh) {
			synthfingerdown |= 1 << fi;
		}
	}
	synthfingertrigger = (synthfingerdown & ~prevsynthfingerdown);
	// needs finger_down to be set

	bool latchon = ((rampreset.flags & FLAGS_LATCH));

	if (!isplaying() && !read_from_seq && !latchon && synthfingerdown_nogatelen != 0 && prevsynthfingerdown_nogatelen == 0) {
		// they have put their finger down after no arp playing, trigger it immediately!
		arp_reset_partial();
		/* -- this caused missed clock steps! what!
		if (!external_clock_enable) {
			bpm_clock_phase = 0; 
			ticks_since_clock = 0;
			gotclock = true;
		}
		*/
		seq_reset(); // so that gate length works
	}
	/* this causes random restarts when jamming, I dont like it
	else if (rampreset.arpon && arp_rhythm.did_a_retrig && !arp_rhythm.supress && synthfingerdown_nogatelen && !(arpbits & synthfingerdown_nogatelen)) {
		// oh no! suddenly the arp bits dont overlap with the fingers. maybe the sequencer moved on. do a partial reset of the arp
		if (!isarpmode8step(arpmode)) {
			arp_reset_partial();
			gotclock = true;
		}
	}
	*/

	update_arp(gotclock);
	update_params(synthfingertrigger, synthfingerdown);
	int seqdivi = param_eval_int(P_SEQDIV, any_rnd, env16, pressure16);
	seqdiv = (seqdivi==DIVISIONS_MAX) ? -1 : divisions[ clampi(seqdivi, 0, DIVISIONS_MAX-1) ]-1;
	
	cur_sample1 = param_eval_int(P_SAMPLE, any_rnd, env16, pressure16);
	cur_pattern = param_eval_int(P_SEQPAT, any_rnd, env16, pressure16);
	step_offset = param_eval_int(P_SEQSTEP, any_rnd, env16, pressure16);
	check_curstep();
	CopySampleToRam(false);
	CopyPatternToRam(false);

	//static int fr = 0;
	//DebugLog("%02x - %d - frame synth %d\n", synthfingerdown, touch_synth_getlatest(7)->pressure, finger_frame_synth);
	//if (synthfingertrigger)
	//	DebugLog("%d %2x\r\n", fr,synthfingertrigger);
	//fr++;

	

	// decide on pitches & run the dry synth for the 8 fingers!
	int maxpressure = 0;
	int pitchhi = 0;
	int maxvol = 0;
	bool gotlow = false, gothi = false;
	trigout = false;


	int cvpitch = (int)(adc_smooth[6].y2 * (512.f * 12.f)); // pitch cv input 
	int cvquant = param_eval_int(P_CV_QUANT, any_rnd, env16, pressure16);
	if (cvquant) {
		cvpitch = (cvpitch + 256) & (~511);
	}
	for (int fi = 0; fi < 8; ++fi) {
		Finger* synthf = touch_synth_getlatest(fi);
		float vol = (synthf->pressure) * 1.f / 2048.f ; // sensitivity
		{
			// pitch table is (64*8) steps per semitone, ie 512 per semitone
			int octave = arpoctave + param_eval_finger(P_OCT, fi, synthf);
			// so heres my maths, this comes out at 435
			// 8887421 comes from the value of pitch when playing a C
			// the pitch of middle c in plinky as written is (4.0/(65536.0*65536.0/8887421.0/31250.0f))
			// which is 1.0114729530400526 too low
			// which is 0.19749290999 semitones flat
			// *512 = 101. so I need to add 101 to pitch_base

#define PITCH_BASE ((32768-(12<<9)) + (1*512) + 101)
			int pitchbase =12*((octave<<9) + (param_eval_finger(P_PITCH, fi, synthf)>>7)); // +- one octave
			int root = param_eval_finger(P_ROTATE, fi, synthf);
			int interval = (param_eval_finger(P_INTERVAL, fi, synthf) * 12) >> 7;
			int totpitch = 0;
			if (midi_pitch_override & (1 << fi)) {
				Finger* f = fingers_synth_sorted[fi] + 2;
				int midinote = ((midi_notes[fi]-12*2) << 9) + midi_chan_pitchbend[midi_channels[fi]]/8;
				for (int i = 0; i < 4; ++i) {
					int pitch = pitchbase + ((i & 1) ? interval : 0) + (i-2)*64 + midinote; //  (lookupscale(scale, ystep + root)) + +((fine * microtune) >> 14);
					totpitch += pitch;
					voices[fi].theosc[i].pitch = pitch;
					voices[fi].theosc[i].targetdphase = maxi(65536, (int)(table_interp(pitches, pitch + PITCH_BASE) * (65536.f * 128.f)));
					++f;
				}
			}
			else {
				u32 scale = param_eval_finger(P_SCALE, fi, synthf);
				if (scale >= S_LAST) scale = 0;
				if (cvquant != CVQ_SCALE)
					pitchbase += cvpitch;
				else {
					// remap the 12 semitones input to the scale steps, evenly, with a slight offset so white keys map to major scale etc
					int steps = ((cvpitch / 512) * scaletab[scale][0] + 1) / 12;
					root += steps;
				}
				int stride_semitones = maxi(0,param_eval_finger(P_STRIDE, fi, synthf));
				root += stride(scale, stride_semitones, fi);
				int microtune = 64 + param_eval_finger(P_MICROTUNE, fi, synthf);  // really, micro-tune amount

				Finger* f = fingers_synth_sorted[fi] + 2;
				for (int i = 0; i < 4; ++i) {
					//				if (ramsample.samplelen)
					//					f = synthf; // XXX FORCE LATEST
					int ystep = 7 - (f->pos >> 8);
					int fine = 128 - (f->pos & 255);
					int pitch = pitchbase + (lookupscale(scale, ystep + root)) + ((i & 1) ? interval : 0) + ((fine * microtune) >> 14);
					totpitch += pitch;
					voices[fi].theosc[i].pitch = pitch;
					voices[fi].theosc[i].targetdphase = maxi(65536, (int)(table_interp(pitches, pitch + PITCH_BASE) * (65536.f * 128.f)));
					++f;
				}
			}
#ifdef DEBUG
			if (fi == 0)
#endif			
			//if (fi<6)
				RunVoice(&voices[fi], fi, vol, dst);
			if (vol > 0.001f) {
				if (!gotlow) {
					SetOutputCVPitchLo(totpitch, true);
					gotlow = true;
				}
				if (arpmode < 0 || (arpbits & (1<<fi))) {
					pitchhi = totpitch;
					gothi = true;
				}
				midi_desired_note[fi] = clampi((totpitch + 1024) / 2048 + 24, 0, 127);
			}
			maxvol = maxi(maxvol, (int)(vol * 65536.f));
		}
		if (synthf->pressure > maxpressure)
			maxpressure = synthf->pressure;
	}
	SetOutputCVPressure(maxpressure * 8);
	SetOutputCVTrigger(trigout ? 65535 : 0);
	if (gothi)
		SetOutputCVPitchHi(pitchhi, true);
	SetOutputCVGate(maxvol);
	AdvanceCVOut();
	tc_stop(&_tc_audio);

	if (ramsample.samplelen > 0) {
		// decide on a priority for 8 voices
		int gprio[8];
		u32 sampleaddr = ((cur_sample1 - 1) & 7) * MAX_SAMPLE_LEN;

		for (int i = 0; i < 8; ++i) {
			GrainPair* g = voices[i].thegrains;
			int glen0 = ((abs(g[0].dpos24) * (BLOCK_SAMPLES/2) + g[0].fpos24/2 + 1) >> 23) + 2; // +2 for interpolation
			int glen1 = ((abs(g[1].dpos24) * (BLOCK_SAMPLES/2) + g[1].fpos24/2 + 1) >> 23) + 2; // +2 for interpolation

			// TODO - if pos at end of next fetch will be out of bounds, negate dpos24 and grate_ratio so we ping pong back for the rest of the grain!
			int glen = maxi(glen0, glen1);
			glen = clampi(glen, 0, AVG_GRAINBUF_SAMPLE_SIZE*2);
			g[0].bufadjust = (g[0].dpos24< 0) ? maxi(glen-2,0): 0;
			g[1].bufadjust = (g[1].dpos24< 0) ? maxi(glen-2, 0) : 0;
			grainpos[i * 4 + 0] = (int)(g[0].pos[0]) - g[0].bufadjust + sampleaddr ; 
			grainpos[i * 4 + 1] = (int)(g[0].pos[1]) - g[0].bufadjust + sampleaddr;
			grainpos[i * 4 + 2] = (int)(g[1].pos[0]) - g[1].bufadjust + sampleaddr;
			grainpos[i * 4 + 3] = (int)(g[1].pos[1]) - g[1].bufadjust + sampleaddr;
			glen += 2; // 2 extra 'samples' for the SPI header
		//	if (i==0) EmuDebugLog("%d %d %d %d\n", grainpos[0], grainpos[1], grainpos[2], grainpos[3]);
			gprio[i]=((int)(voices[i].vol * 65535.f) << 12) + i + (glen << 3);
		}
		sort8(gprio, gprio);
		u8 lengths[8];
		int pos = 0,i; 
#if defined DEBUG
#define MAX_SAMPLE_VOICES 1
#else
#define MAX_SAMPLE_VOICES 6
#endif
		for (i = 7; i >= 0; --i) {
			int prio = gprio[i];
			int fi = prio & 7;
			int len = (prio >> 3) & 255;
			if (i < 8 - MAX_SAMPLE_VOICES)
				len = 0; // we only budget for MAX_SPI_STATE transfers. so after that, len goes to 0. also helps CPU load.
			else if (voices[fi].vol <= 0.01f && !(synthfingerdown & (1 << fi)))
				len = 0; // if your finger is up and the volume is 0, we can just skip this one.
			lengths[fi] = (pos + len * 4 > GRAINBUF_BUDGET) ? 0 : len;
			pos += len*4;
		}
		// cumulative sum
		pos = 0;
		for (int i = 0; i < 32; ++i) {
			pos += lengths[i / 4];
			grainbufend[i] = pos;
		}
		if (spistate == 0)
			spi_readgrain_dma(0); // kick off the dma for next time
		else {
			//DebugLog("?"); // spidebug
		}
	} else {
		// just update dac when not in sampler mode
		if (spistate == 0)
			spi_update_dac(0);
	}
	tc_start(&_tc_fx);
	// /// ////////////////////////////////////////////////////////////////////////
	// half rate fx
	static u16 delaypos = 0;
	static u32 wetlr;
	const float k_target_fb = param_eval_float(P_DLFB, any_rnd, env16,pressure16) * (0.35f); // 3/4
	static float k_fb=0.f;
	//const int k_delay2out = param_eval_int(P_MIXDL, any_rnd, env16,pressure16) >> 8; // 1
	int k_target_delaytime = param_eval_int(P_DLTIME, any_rnd, env16, pressure16);
	if (k_target_delaytime > 0) {
		// free timing
		k_target_delaytime = (((k_target_delaytime+255) >> 8) * k_target_delaytime) >> 8;
		k_target_delaytime = (k_target_delaytime * (DLMASK-64))>>16;
	}
	else {
		k_target_delaytime = divisions[clampi((-k_target_delaytime*13)>>16,0,12)]; // results in a number 1-32
		// figure out how samples we can have, max, in a beat synced scenario
		int max_delay = (32000 * 600 * 4) / (maxi(150, bpm10x));
		while (max_delay > DLMASK - 64)
			max_delay >>= 1;
		k_target_delaytime = (max_delay * k_target_delaytime) >> 5;

	}
	k_target_delaytime = clampi(k_target_delaytime, BLOCK_SAMPLES, DLMASK - 64) << 12;
	int k_delaysend=(param_eval_int(P_DLSEND, any_rnd, env16, pressure16)>>9);

	static int wobpos=0;
	static int dwobpos=0;
	static int wobcount=0;
	if (wobcount<=0) {
		const int wobamount =param_eval_int(P_DLWOB, any_rnd, env16,pressure16); // 1/2
		int newwobtarget=((rand()&8191)*wobamount)>>8;
		if (newwobtarget>k_target_delaytime/2)
			newwobtarget=k_target_delaytime/2;
		wobcount=((rand()&8191)+8192)&(~(BLOCK_SAMPLES-1));
		dwobpos=(newwobtarget-wobpos+wobcount/2)/wobcount;
	}
	wobcount-=BLOCK_SAMPLES;


	
	/*
	for (int i=0;i<64;++i) {
		s16 input = ((s16*)spibigrx[1])[i+2];
		input>>=3; 
		dst[i]=STEREOADDSAT(STEREOPACK(input,input), dst[i]);
	}
	if (spistate==0)
		spi_read256_dma(0,0);
		*/
	///////////////// ok lets do hpf earlier!
	static float peak = 0.f;
	peak *= 0.99f;
	static float power = 0.f;
	// at sample rate, lpf k 0.002 takes 10ms to go to half; .0006 takes 40ms; k=.0002 takes 100ms;
// at buffer rate, k=0.13 goes to half in 10ms; 0.013 goes to half in 100ms; 0.005 is 280ms

float g = param_eval_float(P_MIXHPF, any_rnd, env16, pressure16); // tanf(3.141592f * 8000.f / 32000.f); // highpass constant // TODO PARAM 0 -1
g *= g;
g *= g;
g += (10.f / 32000.f);
//const float k = 2.f -2.f * param_eval_float(P_HPF_RESO, any_rnd, env16, pressure16); //  2.f - 2.f * res;
const static float k = 2.f;
float a1 = 1.f / (1.f + g * (g + k));
float a2 = g * a1;


#ifdef DEBUG
#define ENABLE_HPF 0
#else
#define ENABLE_HPF 1
#endif

if (ENABLE_HPF) for (int i = 0; i < BLOCK_SAMPLES; ++i) {
		u32 input = STEREOSIGMOID(dst[i]);
		STEREOUNPACK(input);
		static float ic1l, ic2l, ic1r, ic2r;
		float l = inputl, r = inputr;
		float v1l = a1 * ic1l + a2 * (l - ic2l);
		float v2l = ic2l + g * v1l;
		ic1l = v1l + v1l - ic1l;
		ic2l = v2l + v2l - ic2l;
		l -= k * v1l + v2l;

		float v1r = a1 * ic1r + a2 * (r - ic2r);
		float v2r = ic2r + g * v1r;
		ic1r = v1r + v1r - ic1r;
		ic2r = v2r + v2r - ic2r;
		r -= k * v1r + v2r;

		power *= 0.999f;
		power += l * l + r * r;
		peak = maxf(peak, l + r);

		s16 li = (s16)SATURATE16(l);
		s16 ri = (s16)SATURATE16(r);
		dst[i] = STEREOPACK(li, ri);
	}

	u32 *src = (u32*) dst;

	float f=1.f-clampf(param_eval_float(P_RVTIME, any_rnd, env16, pressure16),0.f,1.f);
	f*=f; f*=f;
	k_reverb_fade=(int)(250*(1.f-f));
	k_reverb_shim=(param_eval_int(P_RVSHIM, any_rnd, env16, pressure16)>>9);
	k_reverb_wob=(param_eval_float(P_RVWOB, any_rnd, env16, pressure16));
	//k_reverb_color=(param_eval_float(P_RVCOLOR, any_rnd, env16, pressure16));
	k_reverbsend=(param_eval_int(P_RVSEND, any_rnd, env16, pressure16));
	
	int synthlvl_ = param_eval_int(P_MIXSYNTH, any_rnd, env16, pressure16);
	int synthwidth = param_eval_int(P_MIX_WIDTH, any_rnd, env16, pressure16);
	int asynthwidth = abs(synthwidth);
	int synthlvl_mid;
	int synthlvl_side;
	if (asynthwidth <= 32768) { // make more narrow
		synthlvl_mid = synthlvl_;
		synthlvl_side = (synthwidth * synthlvl_) >> 15;
	}
	else {
		synthlvl_side = (synthwidth < 0) ? -synthlvl_ : synthlvl_;
		asynthwidth = 65536 - asynthwidth;
		synthlvl_mid = (asynthwidth * synthlvl_) >> 15;
	}

	int ainwetdry= param_eval_int(P_MIXINWETDRY, any_rnd, env16, pressure16);
	int wetdry = param_eval_int(P_MIXWETDRY, any_rnd, env16, pressure16);
	int wetlvl = 65536 - maxi(-wetdry, 0);
	int drylvl = 65536 - maxi(wetdry, 0);

	int ainwetlvl = 65536 - maxi(-ainwetdry, 0);
	int aindrylvl = 65536 - maxi(ainwetdry, 0);


	//int ainlvl = param_eval_int(P_MIXINPUT, any_rnd, env16, pressure16);
	ainwetlvl = ((ainwetlvl >>4) * (ainlvl>>4)) >> 8;

	ainlvl = ((ainlvl >> 4) * (drylvl >> 4)) >> 8; // prescale by dry level
	ainlvl = ((ainlvl >> 4) * (aindrylvl >>4)) >> 8; // prescale by fx dry level
	
	int delayratio=param_eval_int(P_DLRATIO,any_rnd, env16, pressure16)>>8;
	static int delaytime = BLOCK_SAMPLES<<12;
	int scopescale = (65536 * 24) / maxi(16384, (int)peak);
	//int scopetrig = (65536 / 2) / (1 + scopex);

#ifdef DEBUG
#define ENABLE_FX 0
#else
#define ENABLE_FX 1
#endif
	if (ENABLE_FX) for (int i = 0; i < BLOCK_SAMPLES / 2; ++i) {
		//float lfopos1 = lfo_next(&delaylfo1) * wob;
		//int wobpos1 = FLOAT2FIXED(lfopos1, 12 + 6);
		int targetdt=k_target_delaytime+2048-(int)wobpos;
		wobpos+=dwobpos;
		delaytime+=(targetdt-delaytime)>>10;
		s16 delayreturnl = LINEARINTERPDL(delaybuf, delaypos, delaytime);
		s16 delayreturnr = LINEARINTERPDL(delaybuf, delaypos, ((delaytime>>4)*delayratio)>>4);
		// soft clipper due to drive; reduces range to half also giving headroom on tape & output
		u32 drylr0 = STEREOSIGMOID(src[0]);
		u32 drylr1 = STEREOSIGMOID(src[1]);

		/////////////////////////////////// COMPRESSOR
		u32 drylr01 = STEREOADDAVERAGE(drylr0, drylr1); // this is gonna have absolute max +-32768
		STEREOUNPACK(drylr01);
		static float peaktrack=1.f;
		float peaky = (float)((1.f/4096.f/65536.f)*(maxi(maxi(drylr01l, -drylr01l), maxi(drylr01r, -drylr01r)) * synthlvl_)); 
		if (peaky > peaktrack) peaktrack += (peaky - peaktrack) *0.01f;
		else {
			peaktrack += (peaky - peaktrack) *0.0002f;
			peaktrack = maxf(peaktrack, 1.f);
		}
		float recip = (2.5f / peaktrack);
		int lvl_mid = synthlvl_mid * recip;
		int lvl_side = synthlvl_side * recip;
#ifdef EMU
		m_compressor = synthlvl_ * recip /65536.f;
#endif	
		///////////////////////////////////////////////////////////////////////////////////////////
		drylr0 = MIDSIDESCALE(drylr0, lvl_mid, lvl_side);
		drylr1 = MIDSIDESCALE(drylr1, lvl_mid, lvl_side);
		
		MONITORPEAK(&m_dry, drylr0);
		MONITORPEAK(&m_dry, drylr1);

		u32 ain0 = audioin[i * 2 + 0];
		u32 ain1 = audioin[i * 2 + 1];

		u32 audioinwet = STEREOSCALE(STEREOADDAVERAGE(ain0, ain1), ainwetlvl);
		u32 dry2wetlr = STEREOADDAVERAGE(drylr0, drylr1);
		dry2wetlr = STEREOADDSAT(dry2wetlr,audioinwet);

		MONITORPEAK(&m_dry2wet, dry2wetlr);


		int delaysend = (int)((delayreturnl+(delayreturnr>>1)) * k_fb);
		delaysend += (((s16) (dry2wetlr) + (s16) (dry2wetlr >> 16)) * k_delaysend ) >> 8;
		static float lpf=0.f,dc=0.f;
		lpf+=(delaysend-lpf)*0.75f;
		dc+=(lpf-dc)*0.05f;
		delaysend=(int)(lpf-dc);
		//- compressor in feedback of delay
		delaysend=MONOSIGMOID(delaysend);
		
		MONITORPEAK(&m_delaysend, delaysend);

		
		/*int peak=abs(delaysend);
		 if (unlikely(peak>32000)) {
		 // adjust feedback down as by 32000/peak
			 k_fb*=16000.f/peak;
		 } else */
		 {
		 // adjust feedback up again
			 k_fb+=(k_target_fb-k_fb)*0.001f;
		 }
		 delaypos &= DLMASK;
		 delaybuf[delaypos] = delaysend;
		delaypos++;
		//s16 l=(delayreturnl*k_delay2out)>>9;
		//s16 r=(delayreturnr*k_delay2out)>>9;
		//if (i & 1) 
		{
			s16 li = dry2wetlr;
			s16 ri = dry2wetlr>>16;
			static s16 prevli = 0;
			static s16 prevprevli = 0;
			static u16 bestedge = 0;
			static s16 antiturningpointli = 0;
			bool turningpoint = (prevli>prevprevli && prevli>li);
			bool antiturningpoint = (prevli < prevprevli && prevli < li);
			if (antiturningpoint)
				antiturningpointli = prevli; // remember the last turning point at the bottom
			if (turningpoint) { // we are at a peak!
				int edgesize = prevli- antiturningpointli;
				if (scopex >= 256 || (scopex<0 && edgesize>bestedge)) {
					scopex = -256;
					bestedge = edgesize;
				}
			}
			prevprevli = prevli;
			prevli = li;
			if (scopex < 256 && scopex>=0) {
				int x = scopex / 2;
				if (!(scopex&1))
					scope[x]= 0;
				putscopepixel(x, (li * scopescale >> 16) + 16);
				putscopepixel(x, (ri * scopescale >> 16) + 16);
			}
			scopex++;
			if (scopex > 1024)
				scopex = -256;
			//scopex &= 4095;
		}

		u32 newwetlr = STEREOPACK(delayreturnl, delayreturnr);
		MONITORPEAK(&m_delayreturn, newwetlr);

		// reverb
#ifndef DEBUG
		if (1)
		{
			u32 reverbin=STEREOADDAVERAGE(newwetlr,dry2wetlr);
			MONITORPEAK(&m_reverbin, reverbin);
			u32 reverbout=Reverb2(reverbin, reverbbuf);
			MONITORPEAK(&m_reverbout, reverbout);
			newwetlr=STEREOADDSAT(newwetlr, reverbout);
			MONITORPEAK(&m_fxout, newwetlr);

		}
#endif
		// output upsample
		newwetlr = STEREOSCALE(newwetlr, wetlvl);
		u32 midwetlr = STEREOADDAVERAGE(newwetlr, wetlr);
		wetlr = newwetlr;
		
		u32 audioin0 = STEREOSIGMOID(STEREOSCALE(ain0, ainlvl)); // ainlvl already scaled by drylvl
		u32 audioin1 = STEREOSIGMOID(STEREOSCALE(ain1, ainlvl));
		MONITORPEAK(&m_audioin, audioin0);
		MONITORPEAK(&m_audioin, audioin1);

		src[0] = STEREOADDSAT(STEREOADDSAT(STEREOSCALE(drylr0,drylvl), audioin0), midwetlr);
		src[1] = STEREOADDSAT(STEREOADDSAT(STEREOSCALE(drylr1,drylvl), audioin1), newwetlr);

		MONITORPEAK(&m_output, src[0]);
		MONITORPEAK(&m_output, src[1]);


		src += 2;
	}

#ifdef EMU
	powerout = power / (BLOCK_SAMPLES * 2.f * 32768.f * 32768.f);
	gainhistoryrms[ghi] = lin2db(powerout+1.f/65536.f)*0.5f;
	ghi = (ghi + 1) & 511;
#endif
	tc_stop(&_tc_fx);
}

/////////////////////////////////////////////////////////


#ifdef EMU
uint32_t emupixels[128*32];
void OledFlipEmu(const u8 * vram) {
	if (!vram)
		return;
	const u8* src = vram + 1;
	for (int y = 0; y < 32; y += 8) {
		for (int x = 0; x < 128; x++) {
			u8 b = *src++;
			for (int yy = 0; yy < 8; ++yy) {
				u32 c = (b & 1) ? 0xffffffff : 0xff000000;
				int y2 = y + yy;
#ifdef ROTATE_OLED
				//pixels[(y2 + (127-x) * 32)] = c; // rotated, pins at bottom
				emupixels[((31 - y2) + x * 32)] = c; // rotated, pins at top
#else
				emupixels[(y2 * 128 + x)] = c;
#endif
				b >>= 1;
			}
		}
	}
}

 int * EMSCRIPTEN_KEEPALIVE getemubitmap(void) {
	return (int*)emupixels;
}
 uint8_t *EMSCRIPTEN_KEEPALIVE getemuleds() { 
	 return (uint8_t*)led_ram;
}

#endif

void EMSCRIPTEN_KEEPALIVE uitick(u32 *dst, const u32 *src, int half) {
	tc_stop(&_tc_budget);
	tc_start(&_tc_budget);

	tc_start(&_tc_all);
//	if (half)
	{
		tc_start(&_tc_touch);
		touch_update();
		tc_stop(&_tc_touch);
	}
//	else
	{
		tc_start(&_tc_led);
		led_update();
		tc_stop(&_tc_led);
	}

	// clear some scope pixels

	
	// pass thru: memcpy(dst,src,64*4);

	DoAudio((u32*)dst, (u32*)src);
	/* triangle wave test
	static u16 foo;
	for (int i=0;i<BLOCK_SAMPLES;++i) {
		s16 s=(foo<32768) ? foo*2-32768 : 65536+32767-foo*2;
		foo+=256;

		dst[i] = STEREOPACK(s,s);
	}
	*/
	/*
	static int th=0;
	th+=16;

	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R,  adcbuf[ADC_IN5]>>4);
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R,  adcbuf[ADC_IN6]>>4);

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (th>>0)&255);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, (th >> 1) & 255);
*/

	
	tc_stop(&_tc_all);



}

void bootswish(void);
void cv_calib(void);


void reflash(void) {
	clear();
	drawstr(0, 0, F_16_BOLD, "Re-flash");
	drawstr(0, 16, F_16, "over USB DFU");
	oled_flip(vrambuf);
	HAL_Delay(100);
	jumptobootloader();
}

void set_test_mux(int c) {
#ifndef EMU
	GPIOD->ODR &= ~(GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4); // rgb led off
	if (c & 1)
		GPIOD->ODR |= GPIO_PIN_3;
	if (c & 2)
		GPIOD->ODR |= GPIO_PIN_1;
	if (c & 4)
		GPIOD->ODR |= GPIO_PIN_4;
	if (c & 8)
		GPIOA->ODR |= GPIO_PIN_8;
	else
		GPIOA->ODR &= ~GPIO_PIN_8;
#endif
}
void set_test_rgb(int c) {
	set_test_mux(c^7);
}

short *getrxbuf(void);

#undef ERROR
#define ERROR(msg, ...) do { errorcount++; DebugLog("\r\n" msg "\r\n", __VA_ARGS__);  } while (0)

void test_jig(void) {
	// pogo pin layout:
	//GND DEBUG = GND / PA8 - 67
	//GND GND
	//MISO SPICLK = PD3 - 84 / PD1 - 82
	//MOSI GND = PD4 - 85 / GND
	// rgb led is hooked to PD1,PD3,PD4. configure it for output
	// mux address hooked to LSB=PD3, PD1, PD4, PA8=MSB
#ifndef EMU
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	// we also use debug as an output now!
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	GPIOA->ODR &= ~GPIO_PIN_8;
#endif
	clear();
	drawstr(0, 0, F_32_BOLD, "TEST JIG");
	oled_flip(vrambuf);
	HAL_Delay(100);
	enable_audio=EA_PASSTHRU;
	SetOutputCVTrigger(0);
	SetOutputCVClk(0);
	SetOutputCVGate(0);
	SetOutputCVPressure(0);
	int gndcalib[ADC_CHANS]={0};
	int refcalib[ADC_CHANS]={0};
	float pdac[2][2]={0};
#define PITCH_1V_OUT (43000 - 8500)// about 8500 per volt; 43000 is zero ish.
#define PITCH_4V_OUT (43000 - 8500 * 4)
	static int const expected_mvolts[11][2]={
			{0,0},// gnd
			{2500,2500}, // 2.5 ref
			{3274,3274}, // 3.3 supply
			{4779,4779}, // 5v supply
			{950,950}, // 1v from 12v supply
			{1039,4230}, // pitch lo 1v/4v
			{1039,4230}, // pitch hi 1v/4v
			{0,4700}, // clock
			{0,4700}, // trig,
			{0,4700}, // gate,
			{0,4700}, // pressure
	};
	static int const tol_mvolts[11]={
			100, // gnd
			10, // ref
			300, // 3.3
			500, // 5
			100, // 1v
			100,100,// pitch
			150,150,150,150, // outputs
	};
	const char * const names[11][2]={
			{"gnd",0},
			{"2.5v",0},
			{"3.3v",0},
			{"5v",0},
			{"1v from 12v",0},
			{"plo (1v)","plo (4v)"},
			{"phi (1v)","phi (4v)"},
			{"clk (0v)","clk (4.6v)"},
			{"trig (0v)","trig (4.6v)"},
			{"gate (0v)","gate (4.6v)"},
			{"pressure (0v)","pressure (4.6v)"}
			};
	while (1) {
		DebugLog("mux in:  pitch     gate      x        y        a        b   | mux:\r\n");
		int errorcount=0;
		int rangeok=0,zerook=0;
		gotclkin=0;
#ifndef EMU
		if (!update_accelerometer_raw()) {
			drawstr(0, 0, F_32_BOLD, "BAD ACCEL");
			oled_flip(vrambuf);
			HAL_Delay(1000);
			errorcount++;
		}
#endif
		for (int iter=0;iter<4;++iter) {
			SetOutputCVClk(0);
			HAL_Delay(3);
			SetOutputCVClk(65535);
			HAL_Delay(3);
		}
		if (gotclkin!=4)
			ERROR("expected clkin of 4, got %d", gotclkin);
		for (int mux=0;mux<11;++mux ){
			set_test_mux(mux);
			int numlohi = (mux<5) ? 1 : 2;
			for (int lohi=0;lohi<numlohi;++lohi) {
				int data=lohi?49152:0;
				int pitch=lohi? PITCH_4V_OUT : PITCH_1V_OUT;
				SetOutputCVTrigger(data);
				SetOutputCVClk(data);
				SetOutputCVGate(data);
				SetOutputCVPressure(data);
				SetOutputCVPitchLo(pitch,0);
				SetOutputCVPitchHi(pitch,0);

				HAL_Delay(3);
				int tot[ADC_CHANS] = {0};
				clear();

#define NUMITER 32
				for (int iter = 0; iter < NUMITER; ++iter) {
					HAL_Delay(2);
					short *rx=getrxbuf();
					for (int x=0;x<128;++x) {
						putpixel(x,16+rx[x*2]/1024,1);
						putpixel(x,16+rx[x*2+1]/1024,1);
					}
					for (int j = 0; j < ADC_SAMPLES; ++j)
						for (int ch=0;ch<ADC_CHANS;++ch)
							tot[ch] += adcbuf[j * ADC_CHANS + ch];
				}
				if (lohi)
					invertrectangle(0,0,128,32);
				oled_flip(vrambuf);
				for (int ch=0;ch<ADC_CHANS;++ch)
					tot[ch] /= ADC_SAMPLES * NUMITER;

					DebugLog("-----\nmux = %d lohi = %d\n", mux, lohi);
					for (int ch=0;ch<ADC_CHANS;++ch) {
						DebugLog("adc ch reads %d\n",tot[ch]);
					}
					DebugLog("-----\n");
				switch(mux) {
				case 0:
					for (int ch=0;ch<ADC_CHANS;++ch) {
						gndcalib[ch]=tot[ch];
						int expected = (ch==0) ? 43262 : (ch>=6) ? 31500 : 31035 ;
						int error=abs(expected-tot[ch]);
						if (error>2000)
							ERROR("ADC Channel %d zero point is %d, expected %d", ch, tot[ch], expected);
						else
							zerook|=(1<<ch);
					}
					break;
				case 1:
					for (int ch=0;ch<ADC_CHANS;++ch) {
						refcalib[ch]=tot[ch];
						int range=gndcalib[ch]-refcalib[ch];
						int expected=(ch==0) ? 21600 : (ch>=6) ? 0 : 14386;
						int error=abs(expected-range);
						if (error>2000)
							ERROR("ADC Channel %d range is %d, expected %d", ch, range, expected);
						else
							rangeok|=(1<<ch);
					}
					break;
				case 5:
				case 6: {
					int range0 = (refcalib[0] - gndcalib[0]);
					if (range0 == 0) range0 = 1;
					pdac[mux - 5][lohi] = (tot[0] - gndcalib[0]) * (2.5f / range0);
					break;
					}
				}
				DebugLog("%4d: ",mux);
				for (int ch=0;ch<6;++ch) {
					int range=(refcalib[ch]-gndcalib[ch]);
					if (range==0) range=1;
					float gain = 2500.f / range;
					int mvolts=(tot[ch]-gndcalib[ch])*gain;
					int exp_mvolts=expected_mvolts[mux][lohi];
					int error=abs(exp_mvolts-mvolts);
					int tol=tol_mvolts[mux];
					bool ok=true;
					if (error>tol) {
						ok=false;
						ERROR("ADC channel %d was %dmv, expected %dmv, outside tolerence of %dmv", ch, mvolts, exp_mvolts, tol);
					}
					DebugLog("%6dmv%c ", mvolts, ok ? ' ' : '*');
				}
				DebugLog("| %s. clocks=%d\r\n",names[mux][lohi],gotclkin);
			}
		}
		DebugLog("zero: ");
		for (int ch=0;ch<8;++ch)
			DebugLog("%6d%c   ",gndcalib[ch], (zerook&(1<<ch)) ? ' ':'*');
		DebugLog("\r\nrange ");
		for (int ch=0;ch<6;++ch)
			DebugLog("%6d%c   ",gndcalib[ch]-refcalib[ch], (rangeok&(1<<ch)) ? ' ':'*');
		DebugLog("\r\n%d errors\r\n\r\n", errorcount);
		set_test_rgb(errorcount ? 4 : 2);
		clear();
		if (errorcount==0)
			drawstr(0,0,F_32_BOLD,"GOOD!");
		else
			fdrawstr(0,0,F_32_BOLD,"%d ERRORS", errorcount);
		oled_flip(vrambuf);
		/* fill in cv calib - bias and scale x 10 
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
	*/
		for (int ch = 0; ch < 8; ++ch) {
			int zero = gndcalib[ch];
			int range = gndcalib[ch] - refcalib[ch];
			if (range == 0) range = 1;
			cvcalib[ch].bias = zero;
			if (ch >= 6)
				cvcalib[ch].scale = -1.01f / (zero+1);
			else if (ch==0)
				cvcalib[ch].scale = -2.5f / range; // range is measured off 2.5; so this scales it so that we get true volts out
			else 
				cvcalib[ch].scale = (-2.5f/5.f) / range; // range is measured off 2.5; so this scales it so that we get 1 out for 5v in
		}
		// ok pdac[k][0] tells us what we got from the ADC when we set the DAC to PITCH_1V_OUT, and pdac[k][1] tells us what we got when we output PITCH_4V_OUT
		//so we have dacb + dacs * plo0 = PITCH_1V_OUT
		// and       dacb + dacs * plo1 = PITCH_4V_OUT
		// dacs = (PITCH_1V_OUT-PITCH_4V_OUT) / (plo0-plo1)
		// dacb = PITCH_1V_OUT - dacs*plo0
		for (int dacch = 0; dacch < 2; ++dacch) {
			float range = (pdac[dacch][0] - pdac[dacch][1]);
			if (range == 0) range = 1.f;
			float scale_per_volt = (PITCH_1V_OUT - PITCH_4V_OUT) / range;
			float zero = PITCH_1V_OUT - scale_per_volt * pdac[dacch][0];
			DebugLog("dac channel %d has zero at %d and %d steps per volt, should be around 42500 and -8000 ish\r\n", dacch, (int)zero, (int)scale_per_volt);
			cvcalib[dacch + 8].bias = zero;
			cvcalib[dacch + 8].scale = scale_per_volt * (1.f / (2048.f * 12.f)); // 2048 per semitone
		}

		flash_writecalib(2);


		HAL_Delay(errorcount ? 2000 : 4000);
	}
}

void plinky_frame(void);




void EMSCRIPTEN_KEEPALIVE emu_setadc(float araw, float braw, float pitchcv, float gatecv, float xcv, float ycv, float acv, float bcv, int gateforce, int pitchsense, int gatesense) {
#ifdef EMU
	emupitchsense = pitchsense;
	emugatesense = gatesense;
#endif
	u16* a = adcbuf;
	for (int i = 0; i < ADC_SAMPLES; ++i) {
		a[0] = clampi((int)(52100 - 9334.83f * pitchcv * 1.f / 12.f), 0, 65535);
		a[1] = gateforce ? 0 : clampi((int)(31716 - 6548.11f * gatecv), 0, 65535);
		a[2] = clampi((int)(31665 - 6548.11f * xcv), 0, 65535);
		a[3] = clampi((int)(31666 - 6548.11f * ycv), 0, 65535);
		a[4] = clampi((int)(31041 - 6548.11f * acv), 0, 65535);
		a[5] = clampi((int)(31712 - 6548.11f * bcv), 0, 65535);
		a[ADC_AKNOB] = (u16)((1.f - araw) * 65535);
		a[ADC_BKNOB] = (u16)((1.f - braw) * 65535);
		a += ADC_CHANS;
	}
}

//#define BITBANG
void SetExpanderDAC(int chan, int data) {
#ifndef EMU
	GPIOA->BRR = 1<<8; // cs low
	 u16 daccmd = (2<<14) + ((chan&3)<<12) + (data & 0xfff);
#ifdef BITBANG
	  for (int i=0;i<16;++i) {
		  if (daccmd&0x8000) GPIOD->BSRR=1<<4; else GPIOD->BRR=1<<4;
		  daccmd<<=1;
		  HAL_Delay(1);
		  GPIOD->BRR = 1<<1; // clock low
		  HAL_Delay(1);
		  GPIOD->BSRR = 1<<1; // clock high
	  }
	  HAL_Delay(1);
#else
	  spidelay();
	  daccmd=(daccmd>>8) | (daccmd<<8);
	  HAL_SPI_Transmit(&hspi2, (uint8_t*) &daccmd, 2, -1);
	  spidelay();
#endif
	  GPIOA->BSRR = 1<<8; // cs high
#endif
}


#ifdef WASM

bool send_midimsg(u8 status, u8 data1, u8 data2) {
	return true;
}
void spi_update_dac(int chan) {
	resetspistate();
}

void EmuStartSound(void) {
}

bool midi_receive(unsigned char packet[4]) {
	return false;// fill in packet and return true if midi found
}
int emutouch[9][2];
void EMSCRIPTEN_KEEPALIVE wasm_settouch(int idx, int pos, int pressure) {
	if (idx >= 0 && idx < 9)
		emutouch[idx][1] = pos, emutouch[idx][0] = pressure;
}

void EMSCRIPTEN_KEEPALIVE plinky_frame_wasm(void) {
	plinky_frame();
}
u32 wasmbuf[BLOCK_SAMPLES];
uint8_t* EMSCRIPTEN_KEEPALIVE get_wasm_audio_buf(void) {
	return (uint8_t*)wasmbuf;
}
uint8_t* EMSCRIPTEN_KEEPALIVE get_wasm_preset_buf(void) {
	return (uint8_t*)&rampreset;
}
void EMSCRIPTEN_KEEPALIVE wasm_audio(void) {
	static u8 half=0;
	u32 audioin[BLOCK_SAMPLES] = {0};
	uitick(wasmbuf, audioin, half);
	half = 1 - half;
}
void EMSCRIPTEN_KEEPALIVE wasm_pokepreset(int offset, int byte) {
	if (offset >= 0 && offset < sizeof(rampreset)) ((u8*)&rampreset)[offset] = byte;
}
int EMSCRIPTEN_KEEPALIVE wasm_peekpreset(int offset) {
	if (offset >= 0 && offset < sizeof(rampreset)) return ((u8*)&rampreset)[offset];
	return 0;
}
int EMSCRIPTEN_KEEPALIVE wasm_getcurpreset(void) {
	return sysparams.curpreset;
}
void EMSCRIPTEN_KEEPALIVE wasm_setcurpreset(int i) {
	SetPreset(i, false);
}
#endif

static u8 uartbuf[16];
void serial_midi_init(void) {
#ifndef EMU
  	HAL_UART_Receive_DMA(&huart3, uartbuf, sizeof(uartbuf));
#endif
}
void serial_midi(const u8*buf, u8 len) {
	static u8 state=0;
	static u8 msg[3];
	for (;len--;){
		u8 data=*buf++;
		if (data & 0x80) state = 0;
		else if (state == 3) state = 1; // running status
		if (state < 3) {
			msg[state++] = data;
			if (state == 2 && (msg[0] >= 0xc0 && msg[0] <= 0xdf)) {
				msg[state++] = 0; // two byte messages. wtf midi.//WE NEED TO DEBUG THIS COS IT SEEMS NOT TO WORK
			}
			if (state==3) {
				processmidimsg(msg[0], msg[1], msg[2]);
			}
		}
	}
}

#ifndef EMU
// from https://community.st.com/s/question/0D50X00009XkflR/haluartirqhandler-bug
// what a trash fire
// USART Error Handler
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
	__HAL_UART_CLEAR_OREFLAG(huart);
	__HAL_UART_CLEAR_NEFLAG(huart);
	__HAL_UART_CLEAR_FEFLAG(huart);
	/* Disable the UART Error Interrupt: (Frame error, noise error, overrun error) */
	__HAL_UART_DISABLE_IT(huart, UART_IT_ERR);
	//The most important thing when UART framing error occur/any error is restart the RX process 
	midi_panic();
  	HAL_UART_Receive_DMA(&huart3, uartbuf, sizeof(uartbuf));
}

typedef unsigned int uint;
u8 midisendbuf[16+16];
uint midisendhead,midisendtail;
bool usb_midi_write(const uint8_t packet[4]);
bool serial_midi_ready(void) {
	return huart3.TxXferCount == 0;
}
void kick_midi_send(void) {
	if (huart3.TxXferCount==0 && midisendhead!=midisendtail) {
		uint from=midisendtail&15;
		uint to = midisendhead&15;
		if (to>from) {
			midisendtail+=(to-from);
			HAL_UART_Transmit_DMA(&huart3, midisendbuf + from, to-from );

		} else if (to<from) {
			// wrapped! send from->16, and 0->to
			u8 sendlen = (16-from) + to;
			memcpy(midisendbuf+16,midisendbuf,to); // copy looped part to end so we can send it all in one go! good on us.
			midisendtail+=sendlen;
			HAL_UART_Transmit_DMA(&huart3, midisendbuf + from, sendlen);
		}
	}
}


bool send_midi_serial(const u8 *data, int len) {
	if (len<=0) return true;
	if (midisendhead-midisendtail+len > 16)
		return false; // full
	while (len--)
		midisendbuf[(midisendhead++)&15]=*data++;
	return true;
}

bool send_midimsg(u8 status, u8 data1, u8 data2) { // returns false if too full
	u8 len=3;
	if (status>=0xc0 && status<0xe0)
		len=2;
	if (!(status&0x80))
		return true;
	if (status == 0x80 && data1 == 0)
		return true; // er, no
	u8 buf[4]={status>>4, status,data1,data2};
	usb_midi_write(buf);
#ifdef DEBUG
//	DebugLog("%02x %02x %02x\r\n", status, data1, data2);
#endif
	send_midi_serial(buf + 1, len);
	return true;
}
#else
void kick_midi_send(void) {}
bool serial_midi_ready(void) {
	return true;
}
#endif

void serial_midi_update(void) {
	kick_midi_send();
	static u8 old_pos=0;
#ifdef EMU
	u8 pos = 0;
	#else
	u8 pos = 16 - __HAL_DMA_GET_COUNTER(huart3.hdmarx);
#endif
	if (pos != old_pos) {
		if (pos > old_pos) {
			serial_midi(&uartbuf[old_pos], pos - old_pos);
		} else {
			serial_midi(&uartbuf[old_pos], 16 - old_pos);
			serial_midi(&uartbuf[0], pos);
		}
	}
	old_pos = pos;
}


void EMSCRIPTEN_KEEPALIVE plinky_init(void) {
	denormals_init();
	touch_reset_calib();
	tc_init();

	adc_init();
#ifdef EMU
	emu_setadc(0.5f, 0.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, false, false, false);
#endif
	dac_init();
	reverb_clear(); // ram2 is not cleared by startup.s as written.
	delay_clear();
	HAL_Delay(100); // stablise power before bringing oled up
	oled_init();
	codec_init();
	adc_start(); // also dac_start effectively

#ifdef EMU
	void EmuStartSound(void);
	EmuStartSound();
#endif

	// see if were in the testjig - it pulls PA8 (pin 67) down 'DEBUG'
#ifndef EMU
	if (!(GPIOA->IDR & (1<<8))) {
		test_jig();
	}

	// turn debug pin to an output
	  GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	  GPIO_InitStruct.Pin = GPIO_PIN_8;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
      GPIOA->BSRR = 1<<8; // cs high
      HAL_Delay(1);

  	spi_setchip(0xffffffff);
  	int spiid = spi_readid();
  	DebugLog("SPI flash chip 1 id %04x\r\n", spiid);
  	spi_setchip(0);
  	spiid = spi_readid();
  	DebugLog("SPI flash chip 0 id %04x\r\n", spiid);
  	// kick off serial midi in!
  	serial_midi_init();

      /*
	 //  BIT BANG TEST
#ifdef BITBANG
	  // PD1 is spiclk, pd4 is mosi
	  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	  GPIOD->BSRR = (1<<1) + (1<<4); // clock, data high
#endif
	  int count=0;
	  while (1) {
		  SetExpanderDAC(0,(count&1)?0xfff:0);
		  SetExpanderDAC(1,(count&2)?0xfff:0);
		  SetExpanderDAC(2,(count&4)?0xfff:0);
		  SetExpanderDAC(3,(count&8)?0xfff:0);
		  count++;
		  HAL_Delay(250);
	  }
*/
#endif
	led_init();

	//enable_audio=EA_PASSTHRU; // DO NOT CHECK IN
	//while(1);


	int flashvalid = flash_readcalib();
	if (!(flashvalid & 1)) { // no calib at all
		touch_reset_calib();
		calib();
		flashvalid |= 1;
		flash_writecalib(flashvalid );
	}
	if (!(flashvalid &2)) {
		//cv_reset_calib();
		cv_calib();
		flashvalid |= 2;
		flash_writecalib(3);
	}
	HAL_Delay(80);
	int knoba= GetADCSmoothedNoCalib(ADC_AKNOB);
	int knobb= GetADCSmoothedNoCalib(ADC_BKNOB);
	bootswish();
	knoba=abs(knoba-(int)GetADCSmoothedNoCalib(ADC_AKNOB));
	knobb=abs(knobb-(int)GetADCSmoothedNoCalib(ADC_BKNOB));
	DebugLog("knob turned by %d,%d during boot\r\n", knoba,knobb);
	//knoba = 10000; // DO NOT CHECK IN - FORCE CALIBRATION
	//knobb = 10000; // DO NOT CHECK IN - FORCE CV CALIB
		// turn knobs during boot to force calibration
#ifndef WASM
	if (knoba>4096 || knobb>4096) {
		if (knoba > 4096 && knobb > 4096) {
			// both knobs twist on boot - jump to stm flash bootloader
			reflash();
		}
		if (knoba > 4096) {
			 // left knob twist on boot - full calib
			touch_reset_calib();
			calib();
		}
		else {
			// right knob twist on boot - cv calib only
			//cv_reset_calib();
			cv_calib();
		}
		flash_writecalib(3);
	}
#endif
	InitParamsOnBoot();


	

	/*
	DebugLog("erase test ...\r\n");
	spi_erase64k(0, 0);
	spi_read256(0);
	for (int i = 0; i < 256; ++i) if (spibigrx[i + 4] != 255) {
		DebugLog("erase fail at %d\r\n", i);
	}
	memset(spibigrx, 0, sizeof(spibigrx));
	for (int i = 0; i < 256; ++i) spibigtx[i + 4] = i * 23 + 72;
	spi_write256(0);
	memset(spibigrx, 0, sizeof(spibigrx));
	spi_read256(0);
	for (int i = 0; i < 256; ++i) if (spibigrx[i + 4] != (u8)(i*23+72)) {
		DebugLog("write fail at %d\r\n", i);
	}
	spi_erase64k(0, 0);
	spi_read256(0);
	for (int i = 0; i < 256; ++i) if (spibigrx[i + 4] != 255) {
		DebugLog("erase 2 fail at %d\r\n", i);
	}
	

	DebugSPIPage(65536+32768);
	*/

	enable_audio = EA_PLAY;

}



#include "ui.h"

