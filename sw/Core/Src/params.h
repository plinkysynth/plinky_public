
static inline u8 lfohashi(u16 step) {
	return rndtab[step];
}
static inline float lfohashf(u16 step) {
	return (float) (lfohashi(step) * (2.f / 256.f) - 1.f);
}

float EvalTri(float t, u32 step) {
	return 1.f - (t + t);
}
float EvalEnv(float t, u32 step) {
	// unipolar pseudo exponential up/down
	if (step & 1) {
		t *= t;
		t *= t;
		return t;
	}
	else {
		t = 1.f - t;
		t *= t;
		t *= t;
		return 1.f-t;
	}
}
float EvalSin(float t, u32 step) {
	t = t * t * (3.f - t - t);
	return 1.f - (t + t);
}
float EvalSaw(float t, u32 step) {
	return (step &1) ? t-1.f : 1.f-t;
}
float EvalSquare(float t, u32 step) {
	return (step & 1) ? 0.f : 1.f;
}
float EvalBiSquare(float t, u32 step) {
	return (step & 1) ? -1.f : 1.f;
}
float EvalSandcastle(float t, u32 step) {
	return (step & 1) ? ((t<0.5f) ? 0.f : -1.f) : ((t<0.5f) ? 1.f : 0.f);
}
static inline float triggy(float t) {
	t=1.f-(t+t);
	t=t*t;
	return t*t;
}
float EvalTrigs(float t, u32 step) {
	return (step & 1) ? ((t<0.5f) ? 0.f : triggy(1.f-t)) : ((t<0.5f) ? triggy(t) : 0.f);
}
float EvalBiTrigs(float t, u32 step) {
	return (step & 1) ? ((t<0.5f) ? 0.f : -triggy(1.f-t)) : ((t<0.5f) ? triggy(t) : 0.f);
}
float EvalStepNoise(float t, u32 step) {
	return lfohashf(step);
}
float EvalSmoothNoise(float t, u32 step) {
	float n0 = lfohashf(step + (step&1)), n1 = lfohashf(step | 1);
	return n0 + (n1 - n0) * t;
}

float (*lfofuncs[LFO_LAST])(float t, u32 step) = {
		[LFO_TRI]=EvalTri, [LFO_SIN]=EvalSin, [LFO_SMOOTHNOISE]=EvalSmoothNoise, [LFO_STEPNOISE]=EvalStepNoise,
		[LFO_BISQUARE]=EvalBiSquare, [LFO_SQUARE]=EvalSquare, [LFO_SANDCASTLE]=EvalSandcastle, [LFO_BITRIGS]=EvalBiTrigs, [LFO_TRIGS]=EvalTrigs, [LFO_ENV]=EvalEnv,
		[LFO_SAW]=EvalSaw,
};

float lfo_eval(u32 ti, float warp, unsigned int shape) {
	int step = (ti >> 16)<<1;
	float t = (ti & 65535) * (1.f / 65536.f);
	if (t < warp)
		t /= warp;
	else {
		step++;
		t = (1.f - t) / (1.f-warp);
	}
	if (shape>=LFO_LAST) shape=0;
	return (*lfofuncs[shape])(t, step);
}




const char * const paramnames[P_LAST]={
#ifdef NEW_LAYOUT
	[P_A2] = I_ADSR_A "Attack2",
	[P_D2] = I_ADSR_D "Decay2",
	[P_S2] = I_ADSR_S "Sustain2",
	[P_R2] = I_ADSR_R "Release2",
	[P_SWING] = I_TEMPO "Swing",
#else
	[P_ENV_RATE] = I_PERIOD "Env Rate",
	[P_ENV_WARP] = I_WARP "Env Warp",
	[P_ENV_REPEAT] = I_FEEDBACK "Env Repeat",
#endif

	[P_SENS]=I_TOUCH "Sensitivity",
	[P_DRIVE]=I_DISTORT "Distort",
	[P_A]=I_ADSR_A "Attack",
	[P_D]=I_ADSR_D "Decay",
	[P_S]=I_ADSR_S "Sustain",
	[P_R]=I_ADSR_R "Release",


	[P_MIXSYNTH] = I_WAVE "Synth Lvl",
	[P_MIXINPUT] = I_JACK "Input Lvl",
	[P_MIXINWETDRY] = I_JACK "In Wet/Dry",
	[P_MIXWETDRY] = I_REVERB "Main Wet/Dry",
	[P_MIXHPF] = I_HPF "High Pass",
	[P_MIXRESO] = I_DISTORT "Resonance",

	[P_OCT]=I_OCTAVE "Octave",
	[P_PITCH]=I_PIANO "Pitch",
	[P_GLIDE]=I_GLIDE "Glide",
	[P_INTERVAL]=I_INTERVAL "Interval",
	[P_GATE_LENGTH] = I_INTERVAL "Gate Len",
	[P_ENV_LEVEL] = I_AMPLITUDE "Env Level",
	

	
	[P_PWM] = "Shape",
	[P_RVUNUSED] = "<unused>",

	[P_SCALE]=I_PIANO "Scale",
	[P_ROTATE]=I_FEEDBACK "Degree",
	[P_MICROTUNE]=I_MICRO "Microtone",
	[P_STRIDE]=I_INTERVAL "Stride",

	[P_ARPONOFF] = I_NOTES "Arp On/Off",
	[P_LATCHONOFF] = "Latch On/Off",

	[P_ARPMODE]=I_ORDER "Arp",
	[P_ARPDIV]=I_DIVIDE "Divide",
	[P_ARPPROB]=I_PERCENT "Prob %",
	[P_ARPLEN]=I_LENGTH "Euclid Len",
	[P_ARPOCT]=I_OCTAVE "Octaves",
	[P_TEMPO]= "BPM",

	[P_SEQMODE]=I_ORDER "Seq",
	[P_SEQDIV]=I_DIVIDE "Divide",
	[P_SEQPROB]=I_PERCENT "Prob %",
	[P_SEQLEN]=I_LENGTH "Euclid Len",
	[P_SEQSTEP]=I_SEQ "Step Ofs",
	[P_SEQPAT]=I_PRESET "Pattern",

	[P_DLSEND]=I_SEND "Send",
	[P_DLTIME]=I_TIME "Time",
	[P_DLFB]=I_FEEDBACK "Feedback",
	//[P_DLCOLOR]=I_COLOR "Colour",
	[P_DLWOB]=I_AMPLITUDE "Wobble",
	[P_DLRATIO]=I_DIVIDE "2nd Tap",

	[P_RVSEND]=I_SEND "Send",
	[P_RVTIME]=I_TIME "Time",
	[P_RVSHIM]=I_FEEDBACK "Shimmer",
	//[P_RVCOLOR]=I_COLOR "Colour",
	[P_RVWOB]=I_AMPLITUDE "Wobble",
	//[P_RVUNUSED]=" ",

	[P_SAMPLE] = I_WAVE "Sample",
	[P_SMP_POS] = "Scrub",
	[P_SMP_RATE] = I_NOTES "Rate",
	[P_SMP_GRAINSIZE] = I_PERIOD "Grain Sz",
	[P_SMP_TIME] = I_TIME "Timestretch",
	[P_CV_QUANT] = I_JACK "CV Quantise",

	[P_ACCEL_SENS] = I_AMPLITUDE "Accel Sens",
	[P_MIX_WIDTH] = I_AMPLITUDE "Stereo Width",
	[P_NOISE] = I_WAVE "Noise",
	[P_JIT_POS] = "Scrub",
	[P_JIT_RATE] = I_NOTES "Rate",
	[P_JIT_GRAINSIZE] = I_PERIOD "Grain Sz",
	[P_JIT_PULSE] = I_ENV "<unused>",
	[P_HEADPHONE] = "'Phones Vol",

	[P_AOFFSET]=I_OFFSET "CV Offset",
	[P_ASCALE]= I_TIMES "CV Scale",
	[P_ADEPTH]=I_AMPLITUDE "LFO Depth",
	[P_AFREQ]= I_PERIOD "LFO Rate",
	[P_ASHAPE]=I_SHAPE "LFO Shape",
	[P_AWARP]= I_WARP "LFO Warp",

	[P_BOFFSET]=I_OFFSET "CV Offset",
	[P_BSCALE]=I_TIMES "CV Scale",
	[P_BDEPTH]=I_AMPLITUDE "LFO Depth",
	[P_BFREQ]=I_PERIOD "LFO Rate",
	[P_BSHAPE]=I_SHAPE "LFO Shape",
	[P_BWARP]=I_WARP "LFO Warp",

	[P_XOFFSET]=I_OFFSET "CV Offset",
	[P_XSCALE]=I_TIMES "CV Scale",
	[P_XDEPTH]=I_AMPLITUDE "LFO Depth",
	[P_XFREQ]=I_PERIOD "LFO Rate",
	[P_XSHAPE]=I_SHAPE "LFO Shape",
	[P_XWARP]=I_WARP "LFO Warp",

	[P_YOFFSET]=I_OFFSET "CV Offset",
	[P_YSCALE]=I_TIMES "CV Scale",
	[P_YDEPTH]=I_AMPLITUDE "LFO Depth",
	[P_YFREQ]=I_PERIOD "LFO Rate",
	[P_YSHAPE]=I_SHAPE "LFO Shape",
	[P_YWARP]=I_WARP "LFO Warp",

	[P_MIDI_CH_IN]=I_PIANO "MIDI In Ch",
	[P_MIDI_CH_OUT]=I_PIANO "MIDI Out Ch",


};

#define FLAG_SIGNED 128
#define FLAG_MASK 127


const static u8 param_flags[P_LAST] = {
	[P_PWM]=FLAG_SIGNED,
	[P_ARPMODE] = ARP_LAST,
	[P_ARPDIV] = FLAG_SIGNED,
	[P_ARPPROB] =0, // FLAG_SIGNED, - used to have this signed, but its nice to slam it into 0
	[P_ARPLEN] = 17 + FLAG_SIGNED,
	[P_ARPOCT] = 4,
	[P_TEMPO] = FLAG_SIGNED,

#ifdef NEW_LAYOUT
	[P_A2] = 0,
	[P_D2] = 0,
	[P_S2] = 0,
	[P_R2] = 0,
	[P_SWING] = FLAG_SIGNED,

	[P_ACCEL_SENS] = FLAG_SIGNED,
	[P_MIX_WIDTH] = FLAG_SIGNED,
#else
	[P_ENV_WARP] = FLAG_SIGNED,
	[P_ENV_RATE] = FLAG_SIGNED,
	[P_ENV_REPEAT] = FLAG_SIGNED,
#endif
	[P_MIXWETDRY] = FLAG_SIGNED,
	[P_MIXINWETDRY] = FLAG_SIGNED,

	[P_SEQMODE] = SEQ_LAST,
	[P_SEQDIV] = DIVISIONS_MAX+1,
	[P_SEQPROB] =FLAG_SIGNED,
	[P_SEQLEN] = 17 + FLAG_SIGNED,
	[P_SEQPAT]=24,
	[P_SEQSTEP] = FLAG_SIGNED + 64,

	[P_DLSEND]=0,
	[P_DLTIME]=FLAG_SIGNED,
	[P_DLFB]=0,
	//[P_DLCOLOR]=0,
	[P_DLWOB]=0,
	[P_DLRATIO]=0,

	[P_RVSEND]=0,
	[P_RVTIME]=0,
	[P_RVSHIM]=0,
	//[P_RVCOLOR]=0,
	[P_RVWOB]=0,
	//[P_RVUNUSED]=0,

	[P_SENS]=0,
	[P_DRIVE]=FLAG_SIGNED,
	[P_A]=0,
	[P_D]=0,
	[P_S]=0,
	[P_R]=0,


	[P_OCT]=4+FLAG_SIGNED,
	[P_PITCH]=FLAG_SIGNED,
	[P_GLIDE]=0,
	[P_INTERVAL]=FLAG_SIGNED,
	
	[P_SCALE]=S_LAST,
	[P_ROTATE]=FLAG_SIGNED+24,
	[P_MICROTUNE]=0,
	[P_STRIDE]=13,
	
	[P_ASCALE ]=FLAG_SIGNED,
	[P_AOFFSET]=FLAG_SIGNED,
	[P_ADEPTH ]=FLAG_SIGNED,
	[P_AFREQ  ]=FLAG_SIGNED,
	[P_ASHAPE ]=LFO_LAST,
	[P_AWARP  ]=FLAG_SIGNED,

	[P_BSCALE ]=FLAG_SIGNED,
	[P_BOFFSET]=FLAG_SIGNED,
	[P_BDEPTH ]=FLAG_SIGNED,
	[P_BFREQ  ]=FLAG_SIGNED,
	[P_BSHAPE ]=LFO_LAST,
	[P_BWARP  ]=FLAG_SIGNED,

	[P_XSCALE ]=FLAG_SIGNED,
	[P_XOFFSET]=FLAG_SIGNED,
	[P_XDEPTH ]=FLAG_SIGNED,
	[P_XFREQ  ]=FLAG_SIGNED,
	[P_XSHAPE ]=LFO_LAST,
	[P_XWARP  ]=FLAG_SIGNED,

	[P_YSCALE ]=FLAG_SIGNED,
	[P_YOFFSET]=FLAG_SIGNED,
	[P_YDEPTH ]=FLAG_SIGNED,
	[P_YFREQ  ]=FLAG_SIGNED,
	[P_YSHAPE ]=LFO_LAST,
	[P_YWARP  ]=FLAG_SIGNED,

	[P_SAMPLE] = 9,
	[P_SMP_RATE] = FLAG_SIGNED,
	[P_SMP_TIME] = FLAG_SIGNED,
	[P_CV_QUANT] = CVQ_LAST,
	[P_JIT_PULSE] = FLAG_SIGNED,

	[P_MIDI_CH_IN] = 16,
	[P_MIDI_CH_OUT] = 16
};

#define C  ( 0*512)
#define Cs ( 1*512)
#define D  ( 2*512)
#define Ds ( 3*512)
#define E  ( 4*512)
#define F  ( 5*512)
#define Fs ( 6*512)
#define G  ( 7*512)
#define Gs ( 8*512)
#define A  ( 9*512)
#define As (10*512)
#define B  (11*512)

#define Es F
#define Bs C

#define Ab Gs
#define Bb As
#define Cb B
#define Db Cs
#define Eb Ds
#define Fb E
#define Gb Fs

#define CENTS(c) (((c)*512)/100)

const static u16 scaletab[S_LAST][16] = {
[S_CHROMATIC]		= {12, C,Cs,D,Ds,E,F,Fs,G,Gs,A,As,B},
[S_MAJOR]			= {7, C,D,E,F,G,A,B},
[S_MINOR]			= {7, C,D,Eb,F,G,Ab,Bb},
[S_HARMMINOR]		= {7,C,D,Ds,F,G,Gs,B},
[S_PENTA]			= {5, C,D,E,G,A},
[S_PENTAMINOR]		= {5, C,Ds,F,G,As},
[S_HIRAJOSHI]		= {5, C,D,Ds,G,Gs},
[S_INSEN]			= {5, C,Cs,F,G,As},
[S_IWATO]			= {5, C,Cs,F,Fs,As},
[S_MINYO]			= {5, C,D,F,G,A},

[S_FIFTHS]			= {2, C,G},
[S_TRIADMAJOR]		= {3, C,E,G},
[S_TRIADMINOR]		= {3, C,Eb,G},

// these are dups of major/minor/rotations thereof, but lets throw them in anyway
[S_DORIAN]			= {7, C,D,Ds,F,G,A,As},
[S_PHYRGIAN]		= {7, C,Db,Eb,F,G,Ab,Bb},
[S_LYDIAN]			= {7, C,D,E,Fs,G,A,B},
[S_MIXOLYDIAN]		= {7, C,D,E,F,G,A,Bb},
[S_AEOLIAN]			= {7, C,D,Eb,F,G,Ab,Bb},
[S_LOCRIAN]			= {7, C,Db,Eb,F,Gb,Ab,Bb},

[S_BLUESMAJOR]		= {6,C,D,Ds,E,G,A},
[S_BLUESMINOR] = {6,C,Ds,F,Fs,G,As},

[S_ROMANIAN]		= {7,C,D,Ds,Fs,G,A,As},
[S_WHOLETONE]		= {6,C,D,E,Fs,Gs,As},

// microtonal stuff
[S_HARMONICS] = {4, C, E - CENTS(14), G + CENTS(2), Bb - CENTS(31)},
[S_HEXANY] = { 5, CENTS(0), CENTS(386), CENTS(498), CENTS(702), CENTS(814)}, // kinda C,E,F,G,G# but the E is quite flat

[S_JUST] = {7, CENTS(0),	CENTS(204), CENTS(386), CENTS(498), CENTS(702), CENTS(884), CENTS(1088)},
[S_DIMINISHED]		= {8,C,D,Ds,F,Fs,Gs,A,B},
};

#undef C
#undef D
#undef E
#undef F
#undef G
#undef A
#undef B
#undef Cs
#undef Ds
#undef Es
#undef Fs
#undef Gs
#undef As
#undef Bs
#undef Cb
#undef Db
#undef Eb
#undef Fb
#undef Gb
#undef Ab
#undef Bb



// returns number of notes in scale
static inline u8 scalelen(int scale) {
	return scaletab[scale][0];
}

// returns pitch, step is relative to C1
static inline int lookupscale(int scale, int step) {
	int oct = step / scalelen(scale);
	step -= oct * scalelen(scale);
	if (step < 0) {
		step += scalelen(scale); 
		oct--; 
	}
	return oct * (12 * 512) + scaletab[scale][step+1];
}


int params_premod[P_LAST]; // parameters with the lfos/inputs pre-mixed in
#define FULLBITS 10
#define FULL 1024
#define HALF (FULL/2)
#define QUARTER (FULL/4)
#define EIGHTH (FULL/8)
#define QUANT(v,maxi) ( ((v)*FULL+FULL/2)/(maxi) )

#define FIRST_PRESET_IDX 0
#define LAST_PRESET_IDX 32
#define FIRST_PATTERN_IDX LAST_PRESET_IDX
#define LAST_PATTERN_IDX 128 // 24 patterns x 4 quarters = 96 pages starting from page 32
#define FIRST_SAMPLE_IDX LAST_PATTERN_IDX
#define LAST_SAMPLE_IDX 136
#define LAST_IDX LAST_SAMPLE_IDX
#define OPS_IDX 0xfe
typedef struct PageFooter {
	u8 idx; // preset 0-31, pattern (quarters!) 32-127, sample 128-136, blank=0xff
	u8 version;
	u16 crc;
	u32 seq;
} PageFooter;
typedef struct SysParams {
	u8 curpreset;
	u8 paddy;
	u8 systemflags;
	s8 headphonevol;
	u8 pad[16 - 4];
} SysParams;
enum {
	SYS_DEPRACATED_ARPON=1,
	SYS_DEPRACATED_LATCHON=2,
};
enum {
	FLAGS_ARP=1,
	FLAGS_LATCH=2,
};
// preset version 1: ??
// preset version 2: add SAW lfo shape
#define CUR_PRESET_VERSION 2
typedef struct Preset {
	s16 params[96][8];
	u8 flags;
	s8 loopstart_step_no_offset;
	s8 looplen_step;
	u8 paddy[3];
	u8 version;
	u8 category;
	u8 name[8];
} Preset;
static_assert((sizeof(Preset)&15)==0,"?");
static_assert(sizeof(Preset)+sizeof(SysParams)+sizeof(PageFooter)<=2048, "?");
typedef struct SampleInfo {
	u8 waveform4_b[1024]; // 4 bits x 2048 points, every 1024 samples
	int splitpoints[8];
	int samplelen; // must be after splitpoints, so that splitpoints[8] is always the length.
	s8 notes[8];
	u8 pitched; 
	u8 loop; // bottom bit: loop; next bit: slice vs all
	u8 paddy[2]; 
} SampleInfo;
static_assert(sizeof(SampleInfo) + sizeof(SysParams) + sizeof(PageFooter) <= 2048, "?");
static_assert((sizeof(SampleInfo)&15)==0,"?");
typedef struct FingerRecord {
	u8 pos[4];
	u8 pressure[8];
} FingerRecord;
typedef struct PatternQuarter {
	FingerRecord steps[16][8];
	s8 autoknob[16*8][2];
} PatternQuarter;
static_assert(sizeof(PatternQuarter) + sizeof(SysParams) + sizeof(PageFooter) <= 2048, "?");
static_assert((sizeof(PatternQuarter)&15)==0,"?");
SampleInfo ramsample;
Preset rampreset;
PatternQuarter rampattern[4];
SysParams sysparams;
u8 ramsample1_idx=255;
u8 rampreset_idx=255;
u8 rampattern_idx=255;
u8 updating_bank2 = 0;
u8 edit_preset_pending = 255;
u8 edit_pattern_pending = 255;
u8 edit_sample1_pending = 255;

u8 prev_preset_pending = 255;
u8 prev_pattern_pending = 255;
u8 prev_sample1_pending = 255;

u8 cur_sample1; // this is the one we are playing, derived from param, can modulate. 0 means off, 1-8 is sample
u8 cur_pattern; // this is the current pattern, derived from param, can modulate.
s8 cur_step = 0; // current step
s8 step_offset = 0; // derived from param
u8 edit_sample0 = 0; // this is the one we are editing. no modulation. sample 0-7. not 1 based!
u8 copyrequest = 255;
u8 copyfrompreset = 0;
u8 copyfrompattern = 0;
u8 copyfromsample = 0;
u8 recordingknobs = 0;
s8 last_preset_selection_rotstep = 0; // the thing that gets cleared when you hold down X

float knobbase[2];

enum {
	GEN_PRESET,
	GEN_PAT0,
	GEN_PAT1,
	GEN_PAT2,
	GEN_PAT3,
	GEN_SYS,
	GEN_SAMPLE,
	GEN_LAST
};
u32 flashtime[GEN_LAST]; // for each thing we care about, what have we written to?
u32 ramtime[GEN_LAST]; //...and what has the UI set up? 

static u8 const zero[2048]={0};
typedef struct FlashPage {
	union {
		u8 raw[2048 - sizeof(SysParams) - sizeof(PageFooter)];
		Preset preset;
		PatternQuarter patternquarter;
		SampleInfo sampleinfo;
	};
	SysParams sysparams;
	PageFooter footer;
} FlashPage;
static_assert(sizeof(FlashPage) == 2048, "?");
u8 latestpagesidx[LAST_IDX];
u8 backuppagesidx[LAST_PRESET_IDX];
SysParams sysparams;
static inline FlashPage* GetFlashPagePtr(u8 page) { return (FlashPage*)(FLASH_ADDR_256 + page * 2048); }

static Preset const init_params;
int mod_cur[8]; // 16 bit fp


static inline int GetHeadphoneAsParam(void) {
	return (sysparams.headphonevol + 45) * (FULL / 64);
}

static inline int param_eval_premod(u8 paramidx) {
	if (paramidx == P_HEADPHONE)
		return params_premod[paramidx] = GetHeadphoneAsParam() * 65536 ;
	s16* p = rampreset.params[paramidx];
	int tv = p[M_BASE] << 16;
	tv += (mod_cur[M_A] * p[M_A]);
	tv += (mod_cur[M_B] * p[M_B]);
	tv += (mod_cur[M_X] * p[M_X]);
	tv += (mod_cur[M_Y] * p[M_Y]);
	params_premod[paramidx] = tv;
	return tv;
}



static inline Preset* GetSavedPreset(u8 presetidx) {
#ifdef HALF_FLASH
	return (Preset*)&init_params;
#endif
	if (presetidx >= 32)
		return (Preset*)&init_params;
	FlashPage*fp=GetFlashPagePtr(latestpagesidx[presetidx]);
	if (fp->footer.idx!=presetidx || fp->footer.version!=2)
		return (Preset*)&init_params;
	return (Preset * )fp;
}
static inline PatternQuarter* GetSavedPatternQuarter(u8 patternq) {
#ifdef HALF_FLASH
	return (PatternQuarter*)zero;
#endif
	if (patternq >= 24*4)
		return (PatternQuarter*)zero;
	FlashPage* fp = GetFlashPagePtr(latestpagesidx[patternq + FIRST_PATTERN_IDX]);
	if (fp->footer.idx != patternq+ FIRST_PATTERN_IDX || fp->footer.version != 2)
		return (PatternQuarter*)zero;
	return (PatternQuarter*)fp;
}
static inline SampleInfo* GetSavedSampleInfo(u8 sample0) {
#ifdef HALF_FLASH
	return (SampleInfo*)zero;
#endif
	if (sample0 >= 8)
		return (SampleInfo*)zero;
	FlashPage* fp = GetFlashPagePtr(latestpagesidx[sample0 + FIRST_SAMPLE_IDX]);
	if (fp->footer.idx != sample0 + FIRST_SAMPLE_IDX || fp->footer.version != 2)
		return (SampleInfo*)zero;
	return (SampleInfo*)fp;
}

u16 computehash(const void* data, int nbytes) {
	u16 hash = 123;
	const u8* src = (const u8 * )data;
	for (int i=0;i<nbytes;++i)
		hash = hash* 23 + *src++;
	return hash;
}

const static bool IsGenDirty(int gen) {
	return ramtime[gen] != flashtime[gen];
}
void SwapParams(int a, int b) {
	for (int k = 0; k < 8; ++k) {
		int t = rampreset.params[a][k];
		rampreset.params[a][k] = rampreset.params[b][k];
		rampreset.params[b][k] = t;
	}
}
bool CopyPresetToRam(bool force) {
	if (rampreset_idx == sysparams.curpreset && !force)
		return true; // nothing to do
	if (updating_bank2 || IsGenDirty(GEN_PRESET)) return false; // not copied yet
	memcpy(&rampreset, GetSavedPreset(sysparams.curpreset), sizeof(rampreset));
	for (int m = 1; m < M_LAST; ++m)
		rampreset.params[P_HEADPHONE][m] = 0;
	// upgrade rampreset.version to CUR_PRESET_VERSION
	if (rampreset.version == 0) {
		rampreset.version = 1;
		// swappin these around ;)
		SwapParams(P_MIX_WIDTH,P_ACCEL_SENS);
		rampreset.params[P_MIX_WIDTH][0] = HALF; // set default
	}
	if (rampreset.version == 1) {
		rampreset.version = 2;
		// insert a new lfo shape at LFO_SAW
		for (int p = 0; p < 4; ++p) {
			s16* data = rampreset.params[P_ASHAPE + p * 6];
			*data = (*data * (LFO_LAST - 1)) / (LFO_LAST); // rescale to add extra enum entry
			if (*data >= (LFO_SAW * FULL) / LFO_LAST) // and shift high numbers up
				*data += (1 * FULL) / LFO_LAST;
		}
	}
	rampreset_idx = sysparams.curpreset;
	return true;
}
bool CopySampleToRam(bool force) {
	if (ramsample1_idx == cur_sample1 && !force)
		return true; // nothing to do
	if (updating_bank2 || IsGenDirty(GEN_SAMPLE)) return false; // not copied yet
	if (cur_sample1 == 0)
		memset(&ramsample, 0, sizeof(ramsample));
	else
		memcpy(&ramsample, GetSavedSampleInfo(cur_sample1 - 1), sizeof(ramsample));
	ramsample1_idx = cur_sample1;
	return true;
}
bool CopyPatternToRam(bool force) {
	if (rampattern_idx == cur_pattern && !force)
		return true; // nothing to do
	if (updating_bank2 || IsGenDirty(GEN_PAT0) || IsGenDirty(GEN_PAT1) || IsGenDirty(GEN_PAT2) || IsGenDirty(GEN_PAT3)) 
		return false; // not copied yet
	for (int i = 0; i < 4; ++i)
		memcpy(&rampattern[i], GetSavedPatternQuarter((cur_pattern) * 4 + i), sizeof(rampattern[0]));
	rampattern_idx = cur_pattern;
	return true;
}


u8 next_free_page=0;
u32 next_seq = 0;
void InitParamsOnBoot(void) {
	u8 dummypage = 0;
	memset(latestpagesidx, dummypage, sizeof(latestpagesidx));
	memset(backuppagesidx, dummypage, sizeof(backuppagesidx));
	u32 highest_seq = 0;
	next_free_page = 0;
	memset(&sysparams, 0, sizeof(sysparams));
#ifndef HALF_FLASH
	// scan for the latest page for each object
	for (int page = 0 ; page < 255; ++page) {
		FlashPage* p = GetFlashPagePtr(page);
		int i = p->footer.idx;
		if (i >= LAST_IDX)
			continue;// skip blank
		if (p->footer.version < 2)
			continue; // skip old
		u16 check = computehash(p, 2040);
		if (check != p->footer.crc) {
			DebugLog("flash page %d has a bad crc!\r\n", page);
			if (page == dummypage) {
				// shit, the dummy page is dead! move to a different dummy
				for (int i = 0; i < sizeof(latestpagesidx); ++i) if (latestpagesidx[i] == dummypage)
					latestpagesidx[i]++;
				for (int i = 0; i < sizeof(backuppagesidx); ++i) if (backuppagesidx[i] == dummypage)
					backuppagesidx[i]++;
				dummypage++;
			}
			continue;
		}
		if (p->footer.seq > highest_seq) {
			highest_seq = p->footer.seq;
			next_free_page = page + 1;
			sysparams = p->sysparams;
		}
		FlashPage* existing = GetFlashPagePtr(latestpagesidx[i]);
		if (existing->footer.idx!=i  || p->footer.seq > existing->footer.seq || existing->footer.version<2)
			latestpagesidx[i] = page;
	}
#endif
	next_seq = highest_seq + 1;
	memcpy(backuppagesidx, latestpagesidx, sizeof(backuppagesidx));
	
	// clear remaining state
	edit_preset_pending = -1;
	edit_pattern_pending = -1;
	edit_sample1_pending = -1;
	rampattern_idx = -1;
	ramsample1_idx = -1;
	rampreset_idx = -1;
	edit_sample0 = 0;
	// relocate the first preset and pattern into ram
	copyrequest = 255;
	for (int i = 0; i < GEN_LAST; ++i) {
		ramtime[i] = 0;
		flashtime[i] = 0;
	}
	codec_setheadphonevol(sysparams.headphonevol + 45);
	last_preset_selection_rotstep = sysparams.curpreset;
}

int getheadphonevol(void) { // for emu really
	return sysparams.headphonevol + 45;
}

u8 AllocAndEraseFlashPage(void) {
#ifdef HALF_FLASH
	return 255;
#endif
	while (1) {
		FlashPage* p = GetFlashPagePtr(next_free_page);
		bool inuse = next_free_page == 255;
		inuse |= (p->footer.idx < LAST_IDX&& latestpagesidx[p->footer.idx] == next_free_page);
		inuse |= (p->footer.idx < LAST_PRESET_IDX&& backuppagesidx[p->footer.idx] == next_free_page);
		if (inuse) {
			++next_free_page;
			continue;
		}
		DebugLog("erasing flash page %d\r\n", next_free_page);
		flash_erase_page(next_free_page);
		return next_free_page++;
	}
}


void ProgramPage(void* datasrc, u32 datasize, u8 index) {
#ifndef HALF_FLASH
	updating_bank2 = 1;
	HAL_FLASH_Unlock();
	u8 page = AllocAndEraseFlashPage();
	u8* dst = (u8*)(FLASH_ADDR_256 + page * 2048);
	flash_program_block(dst, datasrc, datasize);
	flash_program_block(dst + 2048 - sizeof(SysParams) - sizeof(PageFooter), &sysparams, sizeof(SysParams));
	PageFooter f;
	f.idx = index;
	f.seq = next_seq++;
	f.version = 2;
	f.crc = computehash(dst, 2040);
	flash_program_block(dst + 2040, &f, 8);
	HAL_FLASH_Lock();
	latestpagesidx[index] = page;
	updating_bank2 = 0;
#endif
}
void clearlatch(void);

void SetPreset(u8 preset, bool force) {
	if (preset >= 32)
		return;
	if (preset == sysparams.curpreset && !force)
		return;
	sysparams.curpreset = preset;
	clearlatch();
	CopyPresetToRam(force);
	ramtime[GEN_SYS]=millis();
}
/*
void SetPattern(u8 pattern, bool force) {
	if (pattern >= 24)
		return;
	if (pattern == cur_pattern && !force)
		return;
	cur_pattern = pattern;
	CopyPatternToRam(force);
	ramtime[GEN_SYS]=millis();
}*/


int GetParam(u8 paramidx, u8 mod) {
	if (paramidx == P_HEADPHONE)
		return mod ? 0 : GetHeadphoneAsParam();
	return rampreset.params[paramidx][mod];
}

void EditParamNoQuant(u8 paramidx, u8 mod, s16 data) {
	if (paramidx >= P_LAST || mod >= M_LAST)
		return;
	if (paramidx == P_HEADPHONE) {
		if (mod == M_BASE) {
			data = clampi(-45, ((data + (FULL/128)) / (FULL / 64)) - 45, 18);
			if (data == sysparams.headphonevol)
				return;
			sysparams.headphonevol = data;
			ramtime[GEN_SYS] = millis();
		}
		return;
	}
	if (!CopyPresetToRam(false))
		return; // oh dear we haven't backed up the previous one yet!
	int olddata = GetParam(paramidx, mod);
	if (olddata == data)
		return;
	rampreset.params[paramidx][mod] = data;
	param_eval_premod(paramidx);
	//if (paramidx == P_SEQSTEP && mod == M_BASE)
	//	return; // dont set dirty when you're just moving the current playhead pos.
	ramtime[GEN_PRESET]=millis();
}

void EditParamQuant(u8 paramidx, u8 mod, s16 data) {
	int max = param_flags[paramidx] & FLAG_MASK;
	if (max>0) {
		data %= max; 
		if (data < 0 && !(param_flags[paramidx] & FLAG_SIGNED))
			data += max;
		data = ((data * 2 + 1) * FULL) / (max * 2);
	}
	EditParamNoQuant(paramidx, mod, data);
}

bool NeedWrite(int gen, u32 now) {
	if (ramtime[gen] == flashtime[gen])
		return false;
	if (gen == GEN_PRESET && sysparams.curpreset != rampreset_idx) {
		// the current preset is not equal to the ram preset, but the ram preset is dirty! WE GOTTA WRITE IT NOW!
		return true;
	}
	if (gen == GEN_SAMPLE && cur_sample1 != ramsample1_idx) {
		// the current sample is not equal to the ram preset, but the ram sample is dirty! WE GOTTA WRITE IT NOW!
		return true;
	}
	if (gen >= GEN_PAT0 && gen <= GEN_PAT3 && cur_pattern != rampattern_idx) {
		// the current pattern is not equal to the ram pattern, but the ram pattern is dirty! WE GOTTA WRITE IT NOW!
		return true;
	}
	
	u32 age = ramtime[gen] - flashtime[gen];
	if (age > 60000)
		return true;
	u32 time_since_edit = now - ramtime[gen];
	return time_since_edit > 5000;
}

void WritePattern(u32 now) {
#ifndef DISABLE_AUTOSAVE
	for (int i = 0; i < 4; ++i) if (NeedWrite(GEN_PAT0 + i, now)) {
		flashtime[GEN_SYS] = ramtime[GEN_SYS];
		flashtime[GEN_PAT0 + i] = ramtime[GEN_PAT0 + i];
		ProgramPage(&rampattern[i], sizeof(PatternQuarter), FIRST_PATTERN_IDX + (rampattern_idx) * 4 + i);
	}
#endif
}

void WriteSample(u32 now) {
	if (NeedWrite(GEN_SAMPLE, now)) {
		flashtime[GEN_SYS] = ramtime[GEN_SYS];
		flashtime[GEN_SAMPLE] = ramtime[GEN_SAMPLE];
		if (ramsample1_idx > 0)
			ProgramPage(&ramsample, sizeof(SampleInfo), FIRST_SAMPLE_IDX + ramsample1_idx - 1);
	}
}

void WritePreset(u32 now) {
#ifndef DISABLE_AUTOSAVE	
	if (NeedWrite(GEN_PRESET, now) || NeedWrite(GEN_SYS, now)) {
		flashtime[GEN_SYS] = ramtime[GEN_SYS];
		flashtime[GEN_PRESET] = ramtime[GEN_PRESET];
		ProgramPage(&rampreset, sizeof(Preset), rampreset_idx);
	}
#endif
}



void PumpFlashWrites(void) {
	if (enable_audio != EA_PLAY)
		return;
	u32 now = millis();

	if (copyrequest != 255) {
		// we want to copy TO copyrequest, FROM copyfrompreset
		if (copyrequest & 128) {
			// wipe!
			copyrequest &= 63; 
			if (copyrequest < 32) {
				memcpy(&rampreset, &init_params, sizeof(rampreset));
				ramtime[GEN_PRESET] = now;
			}
			else if (copyrequest < 64 - 8) {
				memset(&rampattern, 0, sizeof(rampattern));
				ramtime[GEN_PAT0] = now;
				ramtime[GEN_PAT1] = now;
				ramtime[GEN_PAT2] = now;
				ramtime[GEN_PAT3] = now;
			}
			else {
				memset(&ramsample, 0, sizeof(ramsample));
				ramtime[GEN_SAMPLE] = now;
			}
		}
		else if (copyrequest<64) {
			// copy!
			if (copyrequest < 32) {

#ifndef DISABLE_AUTOSAVE				
				if (copyrequest == copyfrompreset) {
					// toggle
					WritePreset(now + 100000); // flush any writes
					int t = backuppagesidx[copyfrompreset];
					backuppagesidx[copyfrompreset] = latestpagesidx[copyfrompreset];
					latestpagesidx[copyfrompreset] = t;
					memcpy(&rampreset, GetSavedPreset(sysparams.curpreset), sizeof(rampreset));

				}
				else {
					// copy preset
					ProgramPage(GetSavedPreset(copyfrompreset), sizeof(Preset), copyrequest);
				}
#endif

				SetPreset(copyrequest, true);
			} 
			else if (copyrequest < 64 - 8) {
				int srcpat = copyfrompattern;
				int dstpat = copyrequest - 32;
				/*if (srcpat == dstpat) { toggle not available for patterns
					// toggle
					WritePattern(now + 100000); // flush any writes
					for (int i = 0; i < 4; ++i) {
						int j = srcpat * 4 + i + 32;
						int t = backuppagesidx[j];
						backuppagesidx[j] = latestpagesidx[j];
						latestpagesidx[j] = t;
						memcpy(&rampattern[i], GetSavedPatternQuarter(srcpat*4+i), sizeof(PatternQuarter));
					}
				}
				else */
				{
#ifndef DISABLE_AUTOSAVE					
					// copy pattern
					for (int i = 0; i < 4; ++i)
						ProgramPage(GetSavedPatternQuarter(srcpat * 4 + i), sizeof(PatternQuarter), 32 + dstpat * 4 + i);
#endif
				}
				EditParamQuant(P_SEQPAT, M_BASE, dstpat);
			}
		}
		copyrequest = 255;
		editmode = EM_PLAY;
	}
	

	WritePattern(now);
	WriteSample(now);
	WritePreset(now);
	
}

static Preset const init_params = {
	.looplen_step=8,
	.version=CUR_PRESET_VERSION,
	.params=
	{
		[P_SENS] = {HALF},
		[P_DRIVE] = {0},
		[P_A] = {EIGHTH},
		[P_D] = {QUARTER},
		[P_S] = {FULL},
		[P_R] = {EIGHTH},

		[P_OCT] = {0,0,0},
		[P_PITCH] = {0,0,0},
		[P_SCALE] = {QUANT(S_MAJOR,S_LAST)},
		[P_MICROTUNE] = {EIGHTH},
		[P_STRIDE] = {QUANT(7,13)},
		[P_INTERVAL] = {(0 * FULL) / 12},
		[P_ROTATE] = {0,0,0},

		[P_NOISE] = {0,0,0},

		[P_SMP_RATE] = {HALF},
		[P_SMP_GRAINSIZE] = {HALF},
		[P_SMP_TIME] = {HALF},
		

		//		[P_ARPMODE]={QUANT(ARP_UP,ARP_LAST)},
				[P_ARPDIV] = {QUANT(2,DIVISIONS_MAX) },
				[P_ARPPROB] = {FULL},
				[P_ARPLEN] = {QUANT(8,17)},
				[P_ARPOCT] = {QUANT(0,4)},
				[P_GLIDE] = {0},

				[P_SEQMODE] = {QUANT(SEQ_FWD,SEQ_LAST)},
				[P_SEQDIV] = {QUANT(6,DIVISIONS_MAX+1)},
				[P_SEQPROB] = {FULL},
				[P_SEQLEN] = {QUANT(8,17)},
				[P_SEQPAT] = {QUANT(0,24)},
				[P_SEQSTEP] = {0},
				[P_TEMPO] = {0},

				[P_GATE_LENGTH] = {FULL},

				//[P_DLSEND]={HALF},
				[P_DLTIME] = {QUANT(3,8)},
				[P_DLFB] = {HALF},
				//[P_DLCOLOR]={FULL},
				[P_DLWOB] = {QUARTER},
				[P_DLRATIO] = {FULL},

				[P_RVSEND]={QUARTER},
				[P_RVTIME] = {HALF},
				[P_RVSHIM] = {QUARTER},
				//[P_RVCOLOR]={FULL-QUARTER},
				[P_RVWOB] = {QUARTER},
				//[P_RVUNUSED]={0},


				[P_MIXSYNTH] = {HALF},
				[P_MIX_WIDTH] = {(HALF * 7)/8},
				[P_MIXINWETDRY] = {0},
#ifdef EMU
				[P_MIXINPUT] = {0},
#else
				[P_MIXINPUT] = {HALF},
#endif
				[P_MIXWETDRY] = {0},
							
#ifdef NEW_LAYOUT
		[P_A2] = {EIGHTH},
		[P_D2] = {QUARTER},
		[P_S2] = {FULL},
		[P_R2] = {EIGHTH},
		[P_SWING] = {0},
#else
				[P_ENV_RATE] = {QUARTER},
				[P_ENV_REPEAT] = {0},
				[P_ENV_WARP] = {-FULL},
#endif
				[P_ENV_LEVEL] = {HALF},
				[P_CV_QUANT] = {QUANT(CVQ_OFF,CVQ_LAST)},

				[P_AOFFSET] = {0},
				[P_ASCALE] = {HALF},
				[P_ADEPTH] = {0},
				[P_AFREQ] = {0},
				//[P_ASHAPE] = {QUANT(LFO_ENV,LFO_LAST)},
				[P_AWARP] = {0},

				[P_BOFFSET] = {0},
				[P_BSCALE] = {HALF},
				[P_BDEPTH] = {0},
				[P_BFREQ] = {100},
				[P_BSHAPE] = {0},
				[P_BWARP] = {0},

				[P_XOFFSET] = {0},
				[P_XSCALE] = {HALF},
				[P_XDEPTH] = {0},
				[P_XFREQ] = {-123},
				[P_XSHAPE] = {0},
				[P_XWARP] = {0},

				[P_YOFFSET] = {0},
				[P_YSCALE] = {HALF},
				[P_YDEPTH] = {0},
				[P_YFREQ] = {-315},
				[P_YSHAPE] = {0},
				[P_YWARP] = {0},

				[P_ACCEL_SENS] = {HALF},

				[P_MIDI_CH_IN] = {0},
				[P_MIDI_CH_OUT] = {0},
				}
}; // init params


u8 lfo_history[16][4];
u8 lfo_history_pos;
uint64_t lfo_pos[4];
u16 finger_rnd[8] = { 0, 1 << 12, 2 << 12, 3 << 12, 4 << 12, 5 << 12, 6 << 12, 7 << 12 }; // incremented by a big prime each time the finger is triggered
u16 any_rnd = { 8 << 12 }; // incremented every time any finger goes down
int tilt16 = 0; // average tilt, 
int env16 = 0; // global attack/decay env - TODO
int pressure16 = 0; // max pressure
static inline int index_to_tilt16(int fingeridx) {
	return fingeridx * 16384 - (28672*2);
}
static inline int param_eval_int_noscale(u8 paramidx, int rnd, int env16, int pressure16) { // 16 bit fp
	s16* p = rampreset.params[paramidx];
	int tv = params_premod[paramidx]; // p[M_BASE] * 65538;
	if (p[M_RND]) {
		u16 ri = (u16)(rnd + paramidx);
		if (p[M_RND] > 0)
			// unsigned uniform distribution
			tv += (rndtab[ri] * p[M_RND]) << 8;
		else {
			// signed! triangular distribution
			ri += ri;
			tv += (((int)rndtab[ri] - (int)rndtab[ri - 1]) * p[M_RND]) << 8;
		}
	}
	//tv += (tilt16 * p[M_TILT]);
	tv += (env16 * p[M_ENV]);
	tv += (maxi(0,pressure16) * p[M_PRESSURE]);
	/*
	tv += (mod_cur[M_A] * p[M_A]);
	tv += (mod_cur[M_B] * p[M_B]);
	tv += (mod_cur[M_X] * p[M_X]);
	tv += (mod_cur[M_Y] * p[M_Y]);
	*/
	u8 flags = param_flags[paramidx];
	u8 maxi = flags & FLAG_MASK;
	return clampi(tv >> FULLBITS, (flags & FLAG_SIGNED) ? -65536 : 0, maxi ? 65535 : 65536);
}

static inline int param_eval_int(u8 paramidx, int rnd, int env16, int pressure16) { // 16 bit fp
	u8 flags = param_flags[paramidx];
	u8 maxi = flags & FLAG_MASK;
	int tv = param_eval_int_noscale(paramidx, rnd, env16, pressure16);
	if (maxi) {
		tv = (tv * maxi) >> 16;
	}
	return tv;
}

