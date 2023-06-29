#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/gl3w.h>    // This example is using gl3w to access OpenGL functions (because it is small). You may use glew/glad/glLoadGen/etc. whatever already works for you.
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <shellscalingapi.h>
#else
#include <sys/stat.h>
#endif
#include <thread>
#include "pffft.h"
#include <portaudio.h>
#include "../Core/Src/config.h"

#ifdef _WIN32

#pragma comment(lib,"winmm.lib") // midi
#pragma comment(lib,"shcore.lib") // dpi
#pragma comment(lib,"opengl32.lib") // for wglGetProcAddress
#ifdef _WIN64
#pragma comment(lib,"portaudio/lib/portaudio_x64.lib") 
#pragma comment(lib,"imgui/glfw/lib-vc2010-64/glfw3.lib") // for imgui rendering
#else
#pragma comment(lib,"portaudio/lib/portaudio_x86.lib") 
#pragma comment(lib,"imgui/glfw/lib-vc2010-32/glfw3.lib") // for imgui rendering
#endif

#endif  // _WIN32

//#define STRETCH_PROTO


bool enable_emu_audio = true;


#define USER_DEFAULT_SCREEN_DPI 96

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define FS 32000
#ifdef STRETCH_PROTO
#define BLOCK_SAMPLES 512
#else
#define BLOCK_SAMPLES 64
#endif

typedef unsigned char u8;
typedef unsigned int u32;
typedef short s16;
typedef signed char s8;

extern "C" {
	extern unsigned short expander_out[4];
	uint32_t emupixels[128 * 32];
	int16_t accel_raw[3];
	float accel_lpf[2];
	float accel_smooth[2];
	void resetspistate(void);
	void spi_update_dac(int chan) {
		resetspistate();
	}

	void emu_setadc(float araw, float braw, float pitchcv, float gatecv, float xcv, float ycv, float acv, float bcv, int gateforce, int pitchsense, int gatesense);

	void plinky_frame();
	void plinky_init();
	void uitick(u32* dst, const u32* src, int half);
	extern float gainhistoryrms[512];
	extern int ghi;
	extern float knobhistory[512];
	extern int khi;
	typedef unsigned short u16;
	extern volatile int encval;
	extern volatile u8 encbtn;

	extern float arpdebug[1024];
	extern int arpdebugi;

	extern int emucvouthist;
	extern float emucvout[6][256];
	extern float emupitchloopback;
	extern int emupitchsense, emugatesense;

	void ApplyUF2File(const char* fname);

	extern int samplelen;
	typedef struct FingerRecord {
		u8 pos[4];
		u8 pressure[8];
	} FingerRecord;
	typedef struct Preset {
		s16 params[96][8];
		u8 arpon;
		s8 loopstart_step;
		s8 looplen_step;
		u8 paddy[16 - 3];
	} Preset;
	typedef struct PatternQuarter {
		FingerRecord steps[16][8];
		s8 autoknob[16 * 8][2];
	} PatternQuarter;
	extern Preset rampreset;
	extern PatternQuarter rampattern[4];
	extern s8 cur_step;
#include "../Core/Src/adc.h"
#include "../Core/Src/wtenum.h"
	extern const short wavetable[WT_LAST][WAVETABLE_SIZE ];
}

static GLFWwindow* window;

static PaStream *stream;
static PaError err;
void paerror(const char *e) {
	printf(  "PortAudio error: %s-> %s\n", e, Pa_GetErrorText( err ) );
	exit(1);
}  


FILE* WriteWAVHeader(const char* fname) {
	FILE* f = fopen(fname, "wb");
	if (!f) {
		printf("Failed to open file %s %d (%s)\n", fname, errno, strerror(errno));
		return f;
	}
	fseek(f, 44, SEEK_SET);
	return f;
}

void UpdateWAVHeader(FILE* f, u32 fs, u32 chans) {
	fflush(f); // make sure we've written all the data
	u32 filelen = ftell(f);
	fseek(f, 0, SEEK_SET);
	u32 wavhdr[11] = { 0x46464952,filelen - 8,0x45564157,0x20746d66, 0x00000010,0x00000001 + (chans << 16), (u32)fs, (u32)fs * 4, 0x00100000 + (chans * 2),0x61746164,filelen - 44 };
	fwrite(wavhdr, 1, 44, f);
	fseek(f, filelen, SEEK_SET);
}

void FinishWAVHeader(FILE* f, u32 fs, u32 chans) {
	UpdateWAVHeader(f, fs, chans);
	fclose(f);
}
FILE* wavfile;
extern "C" int getheadphonevol(void);

#ifdef STRETCH_PROTO
#define WINDOWSIZE 16384
float inputtape[512 * 1024];
int inputpos = 0;
int inputjitter = 4096;
float pitches[4] = {0.f,0.f,12.f,-12.f};
float unison = 0.1f;
float attack = 0.6f, decay = 0.6f;
int inputdelay = 0;
PFFFT_Setup* fftsetup32768;
float outputtape[2][WINDOWSIZE];
#define PI 3.1415926535897932384626433832795f
float fftwindow(int i) {
	return 1.f - cosf(i * PI * 2.f / WINDOWSIZE);
}
void SplatFFT(float* dst, int dstpos) {
	static __declspec(align(32)) float tmp[WINDOWSIZE];
	static __declspec(align(32 )) float fft[WINDOWSIZE];
	static __declspec(align(32)) float mag[WINDOWSIZE/2];
	memset(tmp, 0, sizeof(tmp));
	float kattack = 1.f-attack;
	float kdecay = 1.f-decay;
	kattack *= kattack; kdecay *= kdecay;
	kattack *= kattack; kdecay *= kdecay;
	for (int chan = 0; chan < 4; ++chan) {
		float ptch = (chan - 1.5f) * unison + pitches[chan];
		ptch = exp2f(ptch * 1.f / 12.f);
		int basepos = int(inputpos - ptch * WINDOWSIZE - inputdelay - 1 - (rand() % inputjitter)) & (512 * 1024 - 1);
		for (int i = 0; i < WINDOWSIZE; ++i) {

			float fpos = (i*ptch);
			int pos = int(floor(fpos) + basepos)& (512 * 1024 - 1);
			fpos -= floor(fpos);
			float a = inputtape[pos];
			pos = (pos + 1) & (512 * 1024 - 1);
			float b = inputtape[pos];
			a += (b - a) * fpos;
			tmp[i] += fftwindow(i) * a * (1.f/(1.f+chan));
		}
	}
	pffft_transform_ordered(fftsetup32768, tmp, fft, nullptr, PFFFT_FORWARD);
	for (int i = 2; i < WINDOWSIZE; i+=2) {
		float phase = rand() * (PI * 2.f / RAND_MAX);
		float ss = sinf(phase) / float(WINDOWSIZE), cc=cosf(phase) / float(WINDOWSIZE);
		float re = fft[i], im = fft[i + 1];
		float mg = sqrtf(re * re + im * im);
		float& mgsmooth = mag[i / 2];
		mgsmooth += (mg - mgsmooth) * ((mg > mgsmooth) ? kattack : kdecay);
		fft[i] = mgsmooth * cc; //  re* cc - im * ss;
		fft[i + 1] = mgsmooth * ss; //  im* cc + re * ss;
	}
	fft[0] *= 0.f / WINDOWSIZE;
	fft[1] *= 0.f / WINDOWSIZE;
	pffft_transform_ordered(fftsetup32768, fft, tmp, nullptr, PFFFT_BACKWARD);
	for (int i = 0; i < WINDOWSIZE; ++i)
		dst[(i+dstpos)&(WINDOWSIZE-1)] += fftwindow(i) * tmp[i] * (float(BLOCK_SAMPLES)/WINDOWSIZE);
}

bool recording = false;
#endif

int pacb( const void *input,
	void *output,
	unsigned long frameCount,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData )  {

#ifdef STRETCH_PROTO
	if (recording) {
		for (int i = 0; i < BLOCK_SAMPLES; ++i) {
			inputtape[inputpos] = ((const float*)input)[i*2];
			inputpos = (inputpos + 1) & (1024 * 512 - 1);
		}
	}
	static int side = 0;
	static int dstpos = 0;
	SplatFFT(outputtape[side], dstpos);
	for (int i = 0; i < BLOCK_SAMPLES; ++i) {
		((float*)output)[i * 2 + 0] = outputtape[0][(dstpos + i) & (WINDOWSIZE-1)]; outputtape[0][(dstpos + i) & (WINDOWSIZE - 1)] = 0.f;
		((float*)output)[i * 2 + 1] = outputtape[1][(dstpos + i) & (WINDOWSIZE - 1)]; outputtape[1][(dstpos + i) & (WINDOWSIZE - 1)] = 0.f;
	}
	dstpos += BLOCK_SAMPLES;
	side = 1 - side;

#else
	static u32 audioin[BLOCK_SAMPLES];
	u32 temp[BLOCK_SAMPLES];
	static int half;
	half = 1 - half;
	float hpvol = getheadphonevol() * (1.f/32768.f/63.f);
#define PI 3.1415926535897932384626433832795f
	{
		float* src = (float*)input;
		short* dst = (short*)audioin;
		static float theta = 0.f;
		for (int i = 0; i < BLOCK_SAMPLES; ++i) {
			float l = src ? *src++:0;
			float r = src ? *src++:0;
			if (0) {
				r = l = sinf(theta) * sinf(theta * 0.001f) * 0.125f;
				theta += 440.f * PI * 2.f / 32000.f;
				if (theta >= PI * 2.f * 1000.f)
					theta -= PI * 2.f * 1000.f;
			}
			*dst++ = (short)(l * (32767.f));
			*dst++ = (short)(r * (32767.f));
		}
	}
	uitick(temp, audioin, half);
	float *dst=(float*)output;
	short* src = (short*)temp;
	for (int i=0;i<BLOCK_SAMPLES;++i) {
		short l = *src++;
		short r = *src++;
		*dst++ = l*hpvol;
		*dst++ = r*hpvol;
	}
	if (wavfile) {
		fwrite(temp, 4, BLOCK_SAMPLES, wavfile);
		static int counter;
		if (counter++ > 100) {
			counter = 0;
			UpdateWAVHeader(wavfile, FS, 2);

		}

	}
#endif
	return 0;
}

static void glfw_error_callback(int error, const char* description){    fprintf(stderr, "Error %d: %s\n", error, description);}
#undef min
#undef max

int clampi(int x, int mn, int mx) { return (x<mn)?mn:(x>mx)?mx:x; }

extern "C" u8 emuleds[9][8];
u8 emuleds[9][8];

int buttonsw, buttonsh, buttonscomp;
GLuint oledtex,buttonstex;
//#define ROTATE_OLED
#ifdef ROTATE_OLED
const static int oledw = 32, oledh = 128;
#else
const static int oledw=128,oledh=32;
#endif
int Knob(const char *label, float &curval, float *randamount, float minval, float maxval, unsigned int encodercol, float size);

extern "C" void EmuStartSound(void) {
	if (enable_emu_audio) {
		err = Pa_StartStream(stream);
		if (err != paNoError)
			paerror("start");
	}
}

float GRandom() {
	return rand()/float(RAND_MAX);
}
extern "C" short _flashram[8 * 2 * 1024 * 1024];
extern "C" int emutouch[9][2];
int emutouch[9][2];


typedef struct Sample {
	// fmt
	u16 formattag, nchannels;
	u32 samplerate, bytespersec;
	u16 blockalign, bitspersample;
	int datastart;
	int dataend;
	int numsamples;
	int firstsample;
	short *samples;
} Sample;
Sample s;


static inline short read_sample(Sample* s, int pos, int chan) { // resample to 32khz crudely
	if (chan >= s->nchannels) chan = 0;
	int64_t p64 = (int64_t(pos) * s->samplerate * 65536) / 32000;
	int si = int(p64 >> 16);
	if (si<0 || si >= s->numsamples - 1)
		return 0;
	int f = (int)(p64 & 65535);
	return (s->samples[si*s->nchannels] * (65536 - f) + s->samples[si * s->nchannels + s->nchannels] * f) >> 16;
}

const char* ParseWAV(Sample* s, const char* fname) {
	memset(s, 0, sizeof(*s));
	FILE* f = fopen(fname, "rb");
	if (!f)
		return "cant open";
	u32 wavhdr[3] = { 0 };
	fread(wavhdr, 4 , 3, f);
	if (wavhdr[0] != 0x46464952) return "bad header 1";
	if (wavhdr[2] != 0x45564157) return "bad header 2";
	while (!feof(f)) {
		if (fread(wavhdr, 1, 8,f)<8) break;
		int nextchunk = ftell(f) + wavhdr[1] + (wavhdr[1] & 1);
		if (wavhdr[0] == 0x61746164) { // 'data'
			s->datastart = ftell(f);
			s->dataend = s->datastart + wavhdr[1];
			int bytespersample = s->bitspersample / 8 * s->nchannels;
			s->numsamples = wavhdr[1] / (bytespersample);
			s->firstsample = s->datastart / bytespersample;
			printf("%d channels, %d samplerate, %d bits, %d samples\r\n", s->nchannels, s->samplerate, s->bitspersample, s->numsamples);
			s->samples = new short[(s->numsamples * s->nchannels * s->bitspersample / 8+3)/2];
			s->numsamples =(int) fread(s->samples, s->nchannels * s->bitspersample/8, s->numsamples, f);
			if (s->bitspersample == 24) {
				for (int i = 0; i < s->numsamples * s->nchannels; ++i) {
					short sh = *(short*) (((char*)(s->samples)) + (i * 3+1));
					s->samples[i] = sh;
				}
				s->bitspersample = 16;
			}
			fclose(f);
			return 0;
		}
		else if (wavhdr[0] == 0x20746d66) { // 'fmt '
			fread(s, 1,16, f);
			if (s->formattag != 1)
				return fclose(f), "bad formattag";
			if (s->nchannels < 1 || s->nchannels>2)
				return fclose(f), "bad channel count";
			if (s->bitspersample < 16)
				return fclose(f),"bad bits per sample";
		//	if (s->samplerate < 8000 || s->samplerate>48000)
		//		return fclose(f),"bad samplerate";
		}
		fseek(f, nextchunk, SEEK_SET);
	}
	fclose(f);
	return "error";
}

void EmuFrame();
extern "C" float life_damping;
extern "C" float life_force;
extern "C" float p_grainpos ;
extern "C" float p_grainsize ;
extern "C" float p_timestretch;
extern "C" float p_pitchy;
extern "C" int64_t p_playhead;
extern "C" float m_compressor, m_dry, m_audioin, m_dry2wet, m_delaysend, m_delayreturn, m_reverbin, m_reverbout, m_fxout, m_output;

extern "C" float lfo_eval(u32 ti, float warp, unsigned int shape);


float dflux[4096];
static float keys[64][128];
int slicepos[64];
u8 slicenote[64];
float slicepeak[64];
float sliceflux[64];
bool snappos=true;
int maxstep;
float maxdflux;
inline float maxf(float a, float b) {
	return (a > b) ? a : b;
}
inline int maxi(int a, int b) {
	return (a > b) ? a : b;
}
inline int mini(int a, int b) {
	return (a < b) ? a : b;
}
inline float clampf(float a, float mn, float mx) {
	return (a < mn) ? mn : (a > mx) ? mx : a;
}

#ifdef _WIN32
HMIDIIN hmidiin;
HMIDIOUT hmidiout;
u32 midiq[256];
u32 midiqw, midiqr;
void CALLBACK midicb(
	HMIDIIN   hMidiIn,
	UINT      wMsg,
	DWORD_PTR dwInstance,
	DWORD_PTR dwParam1,
	DWORD_PTR dwParam2
) {
	// midi in looks like dwParam1 is what we want - 00643090 00403080
//	printf("%08x %08x %08x midi\n", wMsg, dwParam1, dwParam2);
	if (midiqw-midiqr<256) 
		midiq[(midiqw++) & 255] = dwParam1;
}
extern "C" bool midi_receive(unsigned char packet[4]) {
	if (midiqr == midiqw) return false;
	u32 mm = midiq[(midiqr++) & 255];
	packet[0] = (mm >> 4)&15;
	packet[1] = mm >> 0;
	packet[2] = mm >> 8;
	packet[3] = mm >> 16;
	return true;
}
extern "C" bool send_midimsg(u8 status, u8 data1, u8 data2) {
	//if (status == 0x90) printf("------------- DOWN %d (%d)\n", data1, data2);
	//else if (status == 0x80) printf("------------- UP %d\n", data1);
	//else printf("%02x %02x %02x\n", status, data1, data2);
	if (hmidiout)
		if (MIDIERR_NOTREADY == midiOutShortMsg(hmidiout, status + (data1 << 8) + (data2 << 16)))
			return false;
	return true;
}
#else  // _WIN32
// TODO: Add MIDI support on non-Windows platforms.
extern "C" bool midi_receive(unsigned char packet[4]) {
	return false;
}
extern "C" bool send_midimsg(u8 status, u8 data1, u8 data2) {
	return false;
}
#endif

typedef struct CalibResult {
	u16 pressure[8];
	s16 pos[8];
} CalibResult;
extern "C" CalibResult calibresults[18];
extern "C" int flash_readcalib(void);

bool plinky_inited = false;

static float fwavetable[WT_LAST][WAVETABLE_SIZE];
float softtri(float x) {
	x *= PI*0.5f;
	float y=sinf(x)
		- sinf(x * 3.f) / 9.f
		+ sinf(x * 5.f) / 25.f
		- sinf(x * 7.f) / 49.f
		+ sinf(x * 9.f) / 81.f
		- sinf(x * 11.f) / 121.f
		+ sinf(x * 13.f) / 169.f
		- sinf(x * 15.f) / 225.f;
	return y * (8.f / PI / PI);
}
uint32_t wang_hash(uint32_t seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}
float noisey(int i) {
	return (wang_hash(i) & 65535) / 32768.f - 1.f;
}
Sample cycles[16];
const char* cyclenames[16] = {
//	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"c:/temp/ss.wav"
	0,0,
"c:/temp/waves/saw_1024.wav",
"c:/temp/waves/saw_2048.wav",
"c:/temp/waves/saw2_1024.wav",
"c:/temp/waves/saw2_2048.wav",
"c:/temp/waves/wave1_1024.wav",
"c:/temp/waves/wave1_2048.wav",
"c:/temp/waves/wave2_1024.wav",
"c:/temp/waves/wave2_2048.wav",
"c:/temp/waves/wave3_1024.wav",
"c:/temp/waves/wave3_2048.wav",
"c:/temp/waves/wave4_1024.wav",
"c:/temp/waves/wave4_2048.wav",
"c:/temp/waves/wave5_1024.wav",
"c:/temp/waves/wave5_2048.wav",
};
float cyclegain[16];
bool cycledone[16];
float eval_wave(int shape, int i) {
	i &= 65535;
	if (cyclenames[shape]) {
		if (!cycledone[shape]) {
			if (const char* err = ParseWAV(&cycles[shape], cyclenames[shape]))
				printf("ERROR READING WAV %d %s %s \n", shape, cyclenames[shape], err);
			cycledone[shape] = true;
			short max = 0;
			for (int i = 0; i < cycles[shape].numsamples; ++i) {
				short s = cycles[shape].samples[i * cycles[shape].nchannels];
				if (s < 0) s = -s;
				if (s > max) max = s;
			}
			cyclegain[shape] = 1.f / max;
		}
		if (cycles[shape].numsamples > 100) {
			int i0 = (i * cycles[shape].numsamples) / 65536;
			int i1 = (i0 + 1); 
			if (i1 >= cycles[shape].numsamples) i1 = 0;
			float s0 = cycles[shape].samples[i0 * cycles[shape].nchannels]  *cyclegain[shape];
			float s1 = cycles[shape].samples[i1 * cycles[shape].nchannels] * cyclegain[shape];
			float t = ((i * cycles[shape].numsamples) & 65535) * (1.f / 65536.f);
			return s0 + (s1 - s0) * t;
		}
	}
	float ph = i * (PI*2.f / 65536.f);
	float ns = 0.f;
	int seed = 0;
	float f = 1.f;
	if (shape>4) for (int oct = 15; oct >= 2; --oct) {
		ns += noisey((i >> oct) + seed) * f;
		f *= 0.5f;
		seed += 232532;
	}
	switch (shape) {
	case WT_SAW:
		return (i - 32768) * (1.f/32768.f); // saw
	case WT_SQUARE:
		return (i < 32768) ? -1.f : 1.f; // square
	case WT_SIN:
		return cosf(ph); // sin
	case WT_SIN_2:
		return cosf(ph)*0.75f+cosf(ph*3.f)*0.5f; // sin
	case WT_FM:
		return cosf(ph + sinf(ph * 7.f + sinf(ph * 11.f) * 0.1f) * 0.7f); // fm thing
	case WT_SIN_FOLD1:
		return softtri(sinf(ph) * 2.f); // folded sin
	case WT_SIN_FOLD2:
		return softtri(sinf(ph) * 6.f); // folded sin
	case WT_SIN_FOLD3:
		return softtri(sinf(ph) * 12.f); // folded sin
	case WT_NOISE_FOLD1:
		return softtri(sinf(ph) + 2.f * ns); // folded noise
	case WT_NOISE_FOLD2:
		return softtri(sinf(ph) + 8.f * ns); // folded noise
	case WT_NOISE_FOLD3:
		return softtri(sinf(ph) + 32.f * ns); // folded noise
	case WT_WHITE_NOISE:
		return ((wang_hash(i)&65535)-32768)*(1.f/32768.f);
	default:
		return 0.f;
	}
	
//	return ns; // noise
	
}
int main(int argc, char** argv) {
	const char* midiin_name_to_match = "";
	const char* midiout_name_to_match = "";

	for (int i = 1; i < argc; ++i) if (strcmp(argv[i], "-q") == 0)
		enable_emu_audio = false;
	else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
		midiin_name_to_match = argv[++i];
	else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
		midiout_name_to_match = argv[++i];


	if (0)
	{
		float kernel[256];
		float totk = 0.f;
		for (int i = 0; i < 256; ++i) {
			float x = i * PI / 28.f; // 32 is good. 24 may be a bit more open
			kernel[i] = i ? (0.5f + 0.5f * cosf(i * PI / 256.f)) * sinf(x) / x : 1.f;
			totk += kernel[i] * (i ? 2.f : 1.f);
		}
		for (int i = 0; i < 256; ++i) {
			kernel[i] /= totk;
		}
		// memoize the shapes :)
		static float wshape[WT_LAST][65536];
		for (int shape = 0; shape < WT_LAST; ++shape)
			for (int i = 0; i < 65536; ++i)
				wshape[shape][i] = eval_wave(shape, i);
		FILE* fh = fopen("../Core/Src/wavetable.h", "w");
		if (fh) {
			fprintf(fh, "// generated for WT_LAST %d WAVETABLE_SIZE %d \nconst short wavetable[%d][%d]={\n", WT_LAST,WAVETABLE_SIZE, WT_LAST, WAVETABLE_SIZE);
		}
		for (int shape = 0; shape < WT_LAST; ++shape) {
			//s16* dst = wavetable + shape * WAVETABLE_SIZE;
			float* fdst = fwavetable[shape];
			if (fh) fprintf(fh, "\t// %s\n\t{", wavetablenames[shape]);
			for (int octave = 0; octave < 9; ++octave) {
				int n = (512 >> octave);
				for (int i = 0; i <= n; ++i) {
					float x = 0.f;
					for (int j = -255; j < 256; ++j) {
						x += kernel[abs(j)] * wshape[shape][65535 & ( (i << (octave + 7)) + (j << (octave+2)))];
					}
					*fdst++ = x;	
					short s = /**dst++ = */ x * (16384.f);
					if (fh) fprintf(fh, "%d,", s);
				}
				if (fh) fprintf(fh, "\n\t");
			}
			if (fh) fprintf(fh, "},");

		}
		if (fh) {
			fprintf(fh, "\n};\n");
			fclose(fh);
		}

	}

	// lpzw text converter
	if (0)
	{
		int w, h, comp;
		u8* bmp = stbi_load("c:/temp/lpzwtext_bw.png", &w, &h, &comp, 1);
		int xpos[128] = { 0 };
		int x = 0, y = 0;
		int ox = 0;
		uint8_t obmp[24][1024] = {};
		printf("static const uint16_t font24_xpos[] PROGMEM ={\n");
		for (int c = 33; c<0x60; ++c) {
			while (1) { for (y = 0; y < h; ++y) if (bmp[x + y * w] < 128) break; if (y < h) break; ++x;  } // skip white
			int x1 = x;
			while (1) { for (y = 0; y < h; ++y) if (bmp[x + y * w] < 128) break; if (y == h) break;++x;  } // skip black
			int x2 = x;
			if (c == '"') {
				while (1) { for (y = 0; y < h; ++y) if (bmp[x + y * w] < 128) break; if (y < h) break; ++x; } // skip white
				while (1) { for (y = 0; y < h; ++y) if (bmp[x + y * w] < 128) break; if (y == h) break; ++x; } // skip black
				x2 = x;
			}
			printf("%d,", ox);
			for (int xx = x1; xx < x2; ++xx) for (int yy = 0; yy < 24; ++yy)
				obmp[yy][ox + (x2-x1-1) - (xx - x1)] = bmp[xx + yy * w] ^ 255; // flipped in x
			ox += (x2 - x1);
		}
		printf("%d};\n", ox);
		ox += 7; ox &= ~7;
		int nb = ox / 8;
		printf("static const uint8_t font24[24][%d] PROGMEM = {\n", nb);
		for (int y = 0; y < 24; ++y) {
			for (int x = 0; x < nb; ++x) {
				u8 b = 0;
				for (int xx = 0; xx < 8; ++xx) {
					if (obmp[y][xx + x * 8] > 128) 
						b |= 1 << xx;
				}
				printf("0x%02x,", b);
			}
			printf("\n");
		}
		printf("};\n");
	}




	/* dart throwing logo generator :) *
	int w, h, comp;
	u8* bmp = stbi_load("plinkydots4k.png", &w, &h, &comp, 1);

	float* sdf = new float[w * h];
	for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) sdf[y * w + x] = (bmp[y * w + x] < 128) ? 0.f : w*2;
	int iters = 0;
	while (iters<8) {
		printf("%d\n", iters++);
		bool more = false;
		int x1 = (iters & 1) ? 0 : w-1;
		int x2 = (iters & 1) ? w : -1;
		int y1 = (iters & 2) ? 0 : h-1;
		int y2 = (iters & 2) ? h : -1;
		for (int y = y1; y != y2; y+=(y1<y2)?1:-1) for (int x = x1; x !=x2 ; x+=(x1<x2)?1:-1) {
			float d = sdf[y * w + x];
			float od = w * 2;
			if (y > 0 && x > 0 && (od = sdf[(y - 1) * w + (x - 1)] + 1.414f) < d) sdf[y * w + x] = d = od;
			if (y > 0 && (od = sdf[(y - 1) * w + (x)] + 1.000f) < d) sdf[y * w + x] = d = od;
			if (y > 0 && x < w - 1 && (od = sdf[(y - 1) * w + (x + 1)] + 1.414f) < d) sdf[y * w + x] = d = od;

			if (x > 0 && (od = sdf[(y + 0) * w + (x - 1)] + 1.f) < d) sdf[y * w + x] = d = od;
			if (x < w - 1 && (od = sdf[(y + 0) * w + (x + 1)] + 1.f) < d) sdf[y * w + x] = d = od;

			if (y < h - 1 && x > 0 && (od = sdf[(y + 1) * w + (x - 1)] + 1.414f) < d) sdf[y * w + x] = d = od;
			if (y < h - 1 && (od = sdf[(y + 1) * w + (x)] + 1.000f) < d) sdf[y * w + x] = d = od;
			if (y < h - 1 && x < w - 1 && (od = sdf[(y + 1) * w + (x + 1)] + 1.414f) < d) sdf[y * w + x] = d = od;
			if (od < w * 2)
				more = true;
		}
		if (!more)
			break;
	}
	//for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) bmp[y * w + x] = sdf[y * w + x]*(255.f/w);
	//stbi_write_png("plinkysdf.png", w, h, 1, bmp, w);
	memset(bmp, 255, w * h);
	for (int j = 0;j < 2000000; ++j) {
		if ((j%100000)==0)
			printf("%d\n", j);
		int x = rand() % w;
		int y = rand() % h;
		float rmax = sdf[y * w + x] ;
		rmax *= 0.125f * 0.75f;
		rmax += 5.f;
		if (rmax <= 5.5f) continue;
		int i = int(rmax + 1.f);
		bool ohno = false;
		for (int y2 = -i; y2 <= i; ++y2) {
			int xr = 1.f + sqrtf(rmax * rmax - y2 * y2);
			int yy = y + y2;
			if (yy < 0 || yy >= h) continue;
			for (int x2 = maxi(0, x - xr); x2 <= x + xr && x2 < w; ++x2) if (bmp[yy * w + x2]<128 && (x2 - x) * (x2 - x) + y2 * y2 < rmax * rmax) {
				ohno = true;
				break;
			}
			if (ohno)
				break;
		}
		if (ohno)
			continue;
		float rr = 4.f + rmax * 0.05f;
		if (rr > 6.f) rr = 6.f;
		for (int x2 = -6; x2 <= 6; ++x2) for (int y2 = -6; y2 <= 6; ++y2) {
			int xx = x + x2;
			int yy = y + y2;
			int dd = x2 * x2 + y2 * y2;
			if (dd<rr*rr && xx>=0 && yy>=0 && xx<w && yy<h)
				bmp[yy * w + xx] = mini(bmp[yy * w + xx],255-255*clampf(rr-sqrtf(dd),0.f,1.f));
		}
		
	}
	stbi_write_png("plinkydots.png", w, h, 1, bmp, w);

	*/

	if (0) {
		// alpha channel fiddler
		int w, h, comp;
		u8* bmp = stbi_load("../../docs/web/plinky_black_small.png", &w, &h, &comp, 4);
		u8* pix = bmp;
		for (int i = 0; i < w * h; ++i) {
			pix[3] = mini(pix[3], 255 - pix[0]);
			pix[0] = pix[1] = pix[2] = 255;
			pix += 4;
		}
		stbi_write_png("../../docs/web/plinky_alpha_small.png", w, h, 4, bmp, w * 4);
		pix = bmp;
		for (int i = 0; i < w * h; ++i) {
			pix[0] = pix[1] = pix[2] = 0;
			pix += 4;
		}
		stbi_write_png("../../docs/web/plinky_alpha_small_black.png", w, h, 4, bmp, w * 4);
		free(bmp);
	}
	//SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	/*
	// print out lfo shapes for excel sheet
	for (int i = 0; i < 256; ++i) {
		float warp = (i >= 128) ? (i >= 192) ? 0.875f : 0.125f : 0.5f;
		for (int j=0;j<10;++j) 
			printf("%0.2f ", lfo_eval(i * 65536 / 64, warp, j));
		printf("\n");
	}*/

//	PFFFT_Setup* fftsetup256 = pffft_new_setup(256, PFFFT_REAL);
//	PFFFT_Setup* fftsetup2048 = pffft_new_setup(2048, PFFFT_REAL);
#ifdef STRETCH_PROTO
	fftsetup32768 = pffft_new_setup(WINDOWSIZE, PFFFT_REAL);
#endif

	const char* fname =
	//	"C:\\Users\\mmalex\\Dropbox\\think.wav";
	//"C:\\Users\\mmalex\\Dropbox\\pv\\samples\\amen.wav";
	//	"C:\\Users\\mmalex\\Dropbox\\pianohome.wav";
	//			"C:\\Users\\mmalex\\Dropbox\\pianohome2.wav";
	"C:\\Users\\mmalex\\Dropbox\\pv\\samples\\164718__bradovic__piano.wav";

/*
	if (const char* err = ParseWAV(&s, fname))
		printf("Can't load wav file %s\n", err);
	else {
		short* dst = (short*)_flashram;
		for (int i = 0; i < 8 * 2 * 1024 * 1024; ++i) {
			if (1) {// sample
				*dst++ = (read_sample(&s, i, 0) + read_sample(&s, i, 1)) >> 1;
			} else // test tone
				*dst++ = (short)(30000.f * sinf(i / 32000.f * (440.f) * PI * 2.f));
		}
		s.numsamples = (int64_t(s.numsamples) * 32000) / s.samplerate;
	}
	samplelen = s.numsamples;
	*/

	/*
	static float inp[2048], fft2[2][2048];
	maxstep = s.numsamples / 256;
	maxdflux = 0.f;
	for (int i = 0; i < 1024 * 1024; i += 256) {
		for (int j = 0; j < 256; ++j) inp[j] = _flashram[i + j] * 1.f/32768.f;
		int step = i / 256;
		float* fft = fft2[step & 1];
		float* prevfft = fft2[(step & 1) ^ 1];
		pffft_transform_ordered(fftsetup256, inp, fft, nullptr, PFFFT_FORWARD);
		float df = 0.f;
		for (int bucket = 4; bucket < 128; bucket++) {
			int j = bucket * 2;
			float re = fft[j], im = fft[j + 1];
			float pwr = re * re + im * im;
			fft[j] = pwr;
			float prevpwr = prevfft[j];
			if (pwr > prevpwr)
				df += pwr - prevpwr;
		}
		dflux[step] = df;
		if (df > maxdflux) maxdflux = df;
	}
	// slice into 64 bits
	for (int i = 0; i < 64; ++i) {
		int pos = (maxstep * i) / 64;
		int minpos = maxi(0, (maxstep * (i - 0.5f)) / 64);
		int maxpos = mini(maxstep, (maxstep * (i + 0.5f)) / 64);
		float maxf = maxdflux*1.f/20.f;
		for (int j = minpos; j <= maxpos; ++j) if (dflux[j] > maxf) {
			maxf = dflux[j];
			pos = j;
		}
		
		slicepos[i] = maxi(0,pos * 256-128);
		sliceflux[i] = maxf;
	}
	// analyse pitch of each slice
	float peak = 0.f;
	int peakkey = 0;
	for (int si = 0; si < 64; ++si) {
		int firstsamp = (slicepos[si])-1024;
		int lastsamp = (si==63)?s.numsamples : (slicepos[si+1])-1024;
		if (firstsamp < 0) firstsamp = 0;
		memset(fft2, 0, sizeof(fft2));
		int step = 0;
		peak *= 0.5f;
		while (firstsamp+1024 <= lastsamp) {
			// zero pad top half, parabola window
			float* fft = fft2[step & 1];
			float* prevfft = fft2[(step & 1) ^ 1];
			for (int j = 0; j < 1024; ++j) inp[j] = _flashram[firstsamp + j] * (1.f / 322768.f) * (j * (1024 - j)) * (4.f / (1024.f * 1024.f));
			pffft_transform_ordered(fftsetup2048, inp, fft, nullptr, PFFFT_FORWARD);
			static float keys[64];
			memset(keys, 0, sizeof(keys));
			for (int bucket = 4; bucket < 1024; ++bucket) {
				int j = bucket * 2;
				float re = fft[j], im = fft[j + 1];
				float pwr = re * re + im * im;
				// convert to polar
				float phase = atan2f(re, im) * (1.f / PI); // measured in half-cycles, not radians
				fft[j] = pwr;
				fft[j + 1] = phase;
				if (step > 0) {
					float cycle_delta = (prevfft[j + 1] - phase);
					cycle_delta -= floorf(cycle_delta);
					if (cycle_delta > 0.5f)
						cycle_delta -= 1.f;
					// for debugging float bucket_freq = 32000.f/2048.f * bucket;
					float expected_half_cycles = bucket; // we do this in half cycles, because we overlap the ffts by half
					float actual_freq = (32000.f / 2048.f) * (expected_half_cycles + cycle_delta);
					pwr = sqrtf(pwr);
					//p /= f * 0.01f + 2.f; // 1/f, but dont boost the bass too much!
					for (int overtone = 1; overtone < 10; overtone++) {
						float p = pwr * ((overtone > 1) ? 0.875f : 1.f);
						float f = actual_freq / overtone;
						float key = log2f(f * (1.f / 261.63f)) * 12.f + 24.f ;
						//key = fmodf(key, 12.f);
						if (key >= 0 && key < 63) {
							int ik = (int)key;
							float fk = key - ik;
							keys[(ik)] += p * (1.f - fk);
							keys[(ik + 1)] += p * fk;
						}
					} // overtone
				} // step>0
			} // bucket
			// find strongest note
			for (int i = 0; i < 128; ++i) if (keys[i] > peak) {
				peak = keys[i];
				peakkey = i;
			}
			step++;
			firstsamp += 1024;
		} // steps
		slicepeak[si] = peak;
		slicenote[si] = peakkey;
	} // slice
	*/
	
if (enable_emu_audio) {
	if (Pa_Initialize() != paNoError)
		paerror("init");
//		wavfile = fopen("out.wav", "wb");
	err = Pa_OpenDefaultStream(&stream, 2, 2, paFloat32, FS, BLOCK_SAMPLES, pacb, nullptr);
	if (err != paNoError)
	{
		// second attempt.. now without input! 
		err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, FS, BLOCK_SAMPLES, pacb, nullptr);
		if (err != paNoError)
		{
			paerror("open");
		}
	}
#ifdef STRETCH_PROTO

	static bool first = true;
	if (first) {
		first = false;
		Sample s = {};
		//ParseWAV(&s, "C:\\Users\\blues\\Dropbox\\pv\\samples\\164718__bradovic__piano.wav");
		//if (!s.samples)
			ParseWAV(&s, "piano.wav");
		int l = s.numsamples;
		if (l > 512 * 1024) l = 512 * 1024;
		for (int i = 0; i < l; ++i)
			inputtape[i] = read_sample(&s, i, 0) / 32768.f;

	}
#endif
}

#ifdef _WIN32
	int numin = midiInGetNumDevs();
	int numout = midiOutGetNumDevs();
	printf("%d midi in devices, %d midi out devices\n", numin, numout);
	int midiindev = -1;
	for (int i = 0; i < numin; ++i) {
		MIDIINCAPSA caps = {};
		midiInGetDevCapsA(i, &caps, sizeof(caps));
		if (midiin_name_to_match && midiindev < 0 && strstr(caps.szPname, midiin_name_to_match))
			midiindev = i;
		printf("%d: %s %c\n", i, caps.szPname, (midiindev==i)?'*':' ');
	}
	if (midiindev < 0)
		midiindev = 0; // default to first one
	if (numin >= 0) {
		midiInOpen(&hmidiin, midiindev, (DWORD_PTR)midicb, 0, CALLBACK_FUNCTION);
		midiInStart(hmidiin);
	}
	int midioutdev = -1;
	for (int i = 0; i < numout; ++i) {
		MIDIOUTCAPSA caps = {};
		midiOutGetDevCapsA(i, &caps, sizeof(caps));
		if (midiout_name_to_match && midioutdev < 0 && strstr(caps.szPname, midiout_name_to_match))
			midioutdev = i;
		printf("%d: %s %c\n", i, caps.szPname, (midioutdev == i) ? '*' : ' ');
	}
	if (midioutdev < 0)
		midioutdev = 0; // default to first one
	if (numout >= 0) {
		midiOutOpen(&hmidiout, midioutdev, (DWORD_PTR)0, 0, CALLBACK_NULL);
	}
#endif  // _WIN32

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return 1;
	// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 + GLSL 150
	const char* glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
#define WINDOW_WIDTH 1170
#define WINDOW_HEIGHT 820
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "plinky", NULL, NULL);
	glfwSetWindowSizeLimits(window, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync
	gl3wInit();
	// Setup ImGui binding
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
	//io.DisplayFramebufferScale = ImVec2(2.f, 2.f);
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	struct stat src = {}, dst = {};
	stat("../core/src/icons.h", &dst);
	stat("plinkyicons.png", &src);
	if (dst.st_mtime < src.st_mtime) {
		// regenerate icons table from square png sprite sheet
		const char* names[128] = {
			"KNOB",			"SEND",		 			"TOUCH",		"DISTORT",	 
			"ADSR_A",		"ADSR_D",	 			"ADSR_S",		"ADSR_R",	 
			"SLIDERS",		"FORK",		 			"PIANO",		"NOTES",	 
			"DELAY",		"REVERB",	 			"SEQ",			"RANDOM",	 
			"AB",			"A",		 			"B",			"ALFO",		 
			"BLFO",			"XY",		 			"X",			"Y",		 
			"XLFO",			"YLFO",		 			"REWIND",		"PLAY",		 
			"RECORD",		"LEFT",		 			"RIGHT",		"PREV",		 
			"NEXT",			"CROSS",	 			"PRESET",		"ORDER",	 
			"WAVE",			"MICRO",	 			"LENGTH",		"TIME",		 
			"FEEDBACK",		"TIMES",	 			"OFFSET",		"INTERVAL",	 
			"PERIOD",		"AMPLITUDE", 			"WARP",			"SHAPE",	 
			"TILT",			"GLIDE",	 			"COLOR",		"FM",		 
			"OCTAVE",		"HPF",		 			"DIVIDE",		"PERCENT",	 
			"TEMPO",		"PHONES",	 			"JACK",			"ENV",		 
		};
		FILE* f = fopen("../core/src/icons.h", "w");
		if (f) {
			int w, h, comp;
			u8* iconpixels = stbi_load("plinkyicons.png", &w, &h, &comp, 4);
			int nw = w / 15, nh = h / 15;
			fprintf(f, "const static u8 numicons=%d;\nconst static u16 icons[%d][16]={\n", nw * nh, nw * nh);
			for (int yi = 0; yi < nh; yi++) {
				for (int xi = 0; xi < nw; xi++) {
					fprintf(f, "\t{");
					for (int xx = 0; xx < 15; ++xx) {
						u16 bits = 0;
						for (int yy = 0; yy < 15; ++yy) {
							if (iconpixels[(xx + xi * 15) * 4 + (yy + yi * 15) * (w * 4) + 3] > 128)
								bits |= 1 << yy;
						}
						fprintf(f, "0x%04x,", bits);
					}
					fprintf(	f, "0},\n");
				}
			}
			fprintf(f, "};\n");
			for (int i = 0; i < 128; ++i) {
				if (!names[i]) break;
				fprintf(f, "#define I_%s \"\\x%02x\"\n", names[i], i + 0x80);
			}
			fclose(f);
		}
	}

	glGenTextures(1, &oledtex);
	glGenTextures(1, &buttonstex);
	u8*buttonspixels=stbi_load("buttons_v2.jpg", &buttonsw, &buttonsh, &buttonscomp, 3);
	glBindTexture(GL_TEXTURE_2D, buttonstex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, buttonsw, buttonsh, 0, GL_RGB, GL_UNSIGNED_BYTE, buttonspixels);

	glBindTexture(GL_TEXTURE_2D, oledtex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, oledw, oledh, 0, GL_RGBA, GL_UNSIGNED_BYTE, emupixels);

	for (int i=1;i<argc;++i) if (strstr(argv[i],".uf2") || strstr(argv[i], ".UF2"))
		ApplyUF2File(argv[i]);

#ifdef CALIB_TEST
	CalibResult calibset[20][18];
	for (int i = 1; i <= 20; ++i) {
		char fname[256];
		sprintf(fname, "c:/temp/calib20/CALIB.UF2 %d", i);
		if (i == 0)
			strcpy(fname, "c:/temp/pc/heavy_CALIB.UF2");
		if (i == 1)
			strcpy(fname, "c:/temp/pc/light_CALIB.UF2");

		ApplyUF2File(fname);
		int ok = flash_readcalib();
		if (ok == 0) {
			int i = 1;
		}
		memcpy(calibset[i - 1], calibresults, sizeof(calibresults));
	}
	FILE* f = fopen("c:/temp/heavylight.csv", "w");
	for (int fi = 0; fi < 18; ++fi) {
		fprintf(f,"finger index,%d\n", fi);
		for (int pad = 0; pad < 8; ++pad) {
			for (int k = 0; k < 2; ++k) {
				fprintf(f, "%d,", calibset[k][fi].pressure[pad]);
			}
			fprintf(f, ",");
			for (int k = 0; k < 2; ++k) {
				fprintf(f, "%d,", calibset[k][fi].pos[pad]);
			}
			fprintf(f, "\n");
		}
		fprintf(f,"\n");
	}
	fprintf(f, "now lets look at the 18 strips for each plinky in turn\n\n");
	for (int k = 0; k < 2; ++k) {
		fprintf(f,"plinky,%d\n", k);
		for (int pad = 0; pad < 8; ++pad) {
			for (int fi = 0; fi < 9; ++fi) {
				fprintf(f, "%d,", calibset[k][fi].pos[pad]);
			}
			fprintf(f, ",");
			for (int fi = 0; fi < 9; ++fi) {
				fprintf(f, "%d,", calibset[k][fi].pressure[pad]);
			}
			fprintf(f, "\n");
		}
	}
	return 0;
#endif	
	std::thread workthread([]() {
		plinky_init();
		plinky_inited = 1;
		while (1) {
			plinky_frame();
#ifdef _WIN32
			Sleep(33);
#else
			// TODO: Test on Windows. Maybe it works and we can kill the #ifdef!
			std::this_thread::sleep_for(std::chrono::milliseconds(33));
#endif
		};
		});
	while (1) {
		EmuFrame();
	}
	return 0;
}


void EmuFrame() {
	if (glfwWindowShouldClose(window)) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwTerminate();
		if (enable_emu_audio) {
			err = Pa_StopStream(stream);
			if (err != paNoError)
				paerror("stop");
			err = Pa_CloseStream(stream);
			if (err != paNoError)
				paerror("close");
			err = Pa_Terminate();
			if (err != paNoError)
				paerror("terminate");
		}
		exit(0);
	}
	glfwPollEvents();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	glBindTexture(GL_TEXTURE_2D, oledtex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, oledw, oledh, GL_RGBA, GL_UNSIGNED_BYTE, emupixels);

	ImGui::SetNextWindowPos(ImVec2(0, 0), 0); // ImGuiCond_FirstUseEver
	ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT), 0);
	
	ImGui::Begin("plinky 7", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);


	static float araw = 0.5f;
	static float braw = 0.5f;
	static float encraw = 0.f;
	static float pitchcv = 0.f;
	static float acv = 0.f;
	static float bcv = 0.f;
	static float xcv = 0.f;
	static float ycv = 0.f;
	static float gatecv = 0.f;
	bool gateforce = false;

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, 400.f);

#ifdef STRETCH_PROTO
	ImGui::Checkbox("recording", &recording);
	ImGui::SliderInt("delay", &inputdelay, 0, 1024*512-32768);
	ImGui::SliderInt("jitter", &inputjitter, 1, 32767);
	ImGui::SliderFloat("unison detune", &unison, 0.f, 1.f);
	ImGui::SliderFloat("pitch 1", &pitches[0], -24.f, 24.f);
	ImGui::SliderFloat("pitch 2", &pitches[1], -24.f, 24.f);
	ImGui::SliderFloat("pitch 3", &pitches[2], -24.f, 24.f);
	ImGui::SliderFloat("pitch 4", &pitches[3], -24.f, 24.f);
	ImGui::SliderFloat("attack", &attack, 0.f, 1.f);
	ImGui::SliderFloat("decay", &decay, 0.f, 1.f);
#else
	ImGui::Image((void*)size_t(oledtex), ImVec2(oledw * 3.f, oledh * 3.f));
	//ImGui::SetCursorScreenPos(ImVec2(300.f, 0.f));
	Knob("A", araw, nullptr, 0.f, 1.f, 0, 96.f);
	ImGui::SameLine();
	Knob("B", braw, nullptr, 0.f, 1.f, 0, 96.f);
	//ImGui::SameLine();
	ImGui::SameLine();
	static float prevencraw = encraw;

	encbtn = Knob("Enc", encraw, nullptr, 0.f, 24.f * 4.f, ImGui::GetColorU32(ImGuiCol_FrameBg), 96.f);
	int encdelta = (encraw - prevencraw) + 0.5f;
	encval += encdelta;
	prevencraw += encdelta;
	//	printf("%d\n", encval);

	emu_setadc(araw, braw, pitchcv, gatecv, xcv, ycv, acv, bcv, gateforce, emupitchsense, emugatesense);

	ImGui::Spacing();




#define MONITOR(n)	ImGui::ProgressBar(n,ImVec2(200,0),#n)
	if (ImGui::CollapsingHeader("monitor levels / gain staging")) {
		MONITOR(m_dry);
		MONITOR(m_audioin);
		MONITOR(m_dry2wet);
		MONITOR(m_delaysend);
		MONITOR(m_delayreturn);
		MONITOR(m_reverbin);
		MONITOR(m_reverbout);
		MONITOR(m_fxout);
		MONITOR(m_output);
		MONITOR(m_compressor);
		ImGui::PlotLines("output", gainhistoryrms, 512, ghi, "output", -60.f, 0.f, ImVec2(200.f, 40.f));

	
	}
	if (ImGui::CollapsingHeader("output jacks")) {
		char strval[20] = { 0 };
		sprintf(strval, "%1.2f", emucvout[0][emucvouthist / 4]);
		ImGui::PlotHistogram("trigger", emucvout[0], 256, emucvouthist/4, strval, 0.f, 1.f, ImVec2(200.f,30.f));
		sprintf(strval, "%1.2f", emucvout[1][emucvouthist / 4]);
		ImGui::PlotHistogram("clock", emucvout[1], 256, emucvouthist/4, strval, 0.f, 1.f, ImVec2(200.f, 30.f));
		sprintf(strval, "%1.2f", emucvout[2][emucvouthist / 4]);
		ImGui::PlotHistogram("pressure", emucvout[2], 256, emucvouthist/4, strval, 0.f, 1.f, ImVec2(200.f, 30.f));
		sprintf(strval, "%1.2f", emucvout[3][emucvouthist / 4]);
		ImGui::PlotHistogram("gate", emucvout[3], 256, emucvouthist/4, strval, 0.f, 1.f, ImVec2(200.f, 30.f));
		sprintf(strval, "%1.2f", emucvout[4][emucvouthist / 4]);
		ImGui::PlotHistogram("pitchlo", emucvout[4], 256, emucvouthist/4, strval, 0.f, 1.f, ImVec2(200.f, 30.f));
		sprintf(strval, "%1.2f", emucvout[5][emucvouthist / 4]);
		ImGui::PlotHistogram("pitchhi", emucvout[5], 256, emucvouthist/4, strval, 0.f, 1.f, ImVec2(200.f, 30.f));
		ImGui::ProgressBar(expander_out[0] / 4096.f,ImVec2(200,0),"expander 0");
		ImGui::ProgressBar(expander_out[1] / 4096.f, ImVec2(200, 0), "expander 1");
		ImGui::ProgressBar(expander_out[2] / 4096.f, ImVec2(200, 0), "expander 2");
		ImGui::ProgressBar(expander_out[3] / 4096.f, ImVec2(200, 0), "expander 3");
	}
	static int wavetable_enable = 0;
	ImGui::Combo(
		"wavetable", &wavetable_enable,
		wavetablenames,WT_LAST
	);
//	ImGui::Checkbox("wavetable", (bool*)&wavetable_enable);
	ImGui::PlotLines("wave", fwavetable[wavetable_enable], WAVETABLE_SIZE, 0, "", -1.1f,1.1f, ImVec2(200.f, 100.f));
		

//	ImGui::PlotLines("arpdebug", arpdebug, 1024, arpdebugi, "arpdebug", 0.f, 1.f, ImVec2(1024.f, 40.f));
	//	ImGui::SliderFloat("damping", &life_damping, 0.f, 1.f);
//	ImGui::SliderFloat("force", &life_force, 0.f, 1.f);
	/*
	if (ImGui::SliderFloat("pos", &p_grainpos, 0.f, 1.f))
		p_playhead = 0;
	ImGui::SliderFloat("size", &p_grainsize, 0.f, 1.f);
	ImGui::SliderFloat("timestretch", &p_timestretch, 0.f, 1.f);
	ImGui::SliderFloat("pitchy", &p_pitchy, -1.f, 1.f);
	static float peaky= 40.f;
	static int smear = 6;
	ImGui::SliderFloat("peak max", &peaky, 10.f, 200.f);
	ImGui::SliderInt("smear", &smear, 1, 16);
	float temp[128];
	float* skeys = keys[(int)(p_grainpos * 1024.f)];
	for (int i = 0; i < 64; ++i) {
		float tt = 0.f;
		for (int j = 0; j < smear; ++j) {
			float t = skeys[i*2+j*128];
			tt += t;
		}
		tt /= smear;
		temp[i] = tt;
	}
	ImGui::PlotHistogram("keys", temp, 64, 0, 0, 0.f, peaky, ImVec2(1024.f,80.f));


	static int slice = 0;
	ImGui::Checkbox("snap positions", &snappos);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	for (int si = 0; si < 64; ++si) {
		float x = ((snappos?slicepos[si]:(si*s.numsamples)/64)) / (float)s.numsamples * 1024.f;
		draw_list->AddLine(ImVec2(p.x+x, p.y), ImVec2(p.x+x, p.y + 80.f), (si==slice)?0xff4080ff:0x80ffffff);
	}

	ImGui::PlotLines("flux", [](void* data, int idx) {return maxf(maxf(dflux[idx],dflux[idx+1]),dflux[idx+2]); }, nullptr, maxstep, 0, "flux", 0.f, maxdflux, ImVec2(1024.f, 80.f));
	
	ImGui::SliderInt("slice", &slice, 0, 64);
	ImGui::Text("slice %c%c%d peak %0.2f pitch, %0.2f tot", "CCDDEFFGGAAB"[slicenote[slice]%12], " # #  # # # "[slicenote[slice] % 12], (slicenote[slice] /12)+1,slicepeak[slice], slicepeak[slice]*sliceflux[slice]);
	ImGui::PlotLines("wave", [](void* data, int idx) {
		int slice = *(int*)data;
		int addr = (snappos ? slicepos[slice] : (slice * maxstep*256) / 64) ;
		addr=addr-512 * 8 + idx * 8;
		if (idx == 512) return 32768.f;
		if (idx == 511) return -32768.f;
		if (addr < 0 || addr >= 1024 * 1024) return 0.f;
		return (float)_flashram[addr];
		}, &slice, 1024, 0, "waveform", -32768,32768,ImVec2(1024.f,80.f));
		*/

		//	ImGui::PlotLines("knob", knobhistory, 512, khi, "knob", -1.f, 1.f, ImVec2(1024.f, 40.f));

	if (ImGui::CollapsingHeader("cv inputs")) {
		ImGui::SetNextItemWidth(200);
		ImGui::SliderFloat("A cv", &acv, -5.f, 5.f);
		ImGui::SetNextItemWidth(200);
		ImGui::SliderFloat("B cv", &bcv, -5.f, 5.f);
		ImGui::SetNextItemWidth(200);
		ImGui::SliderFloat("X cv", &xcv, -5.f, 5.f);
		ImGui::SetNextItemWidth(200);
		ImGui::SliderFloat("Y cv", &ycv, -5.f, 5.f);
		ImGui::SetNextItemWidth(200);
		ImGui::SliderFloat("Gate cv", &gatecv, 0.f, 5.f); //ImGui::SameLine(); 
		ImGui::Checkbox("plug##gate", (bool*)&emugatesense);
		ImGui::SameLine();
		ImGui::Button("5v!");
		gateforce = ImGui::IsItemActive();
		ImGui::RadioButton("no pitch plug", &emupitchsense, 0); 
		ImGui::RadioButton("pitch loopback (in only)", &emupitchsense, 1);
		ImGui::RadioButton("pitch loopback (cabled!)", &emupitchsense, 2);
		ImGui::RadioButton("pitch control", &emupitchsense, 3);
		if (emupitchsense == 2)
			pitchcv = emupitchloopback * 12.f; // pitchcv is in semitones, loopback is in volts aka octaves
		ImGui::SliderFloat("Pitch cv", &pitchcv, -1.5f * 12.f, 6.f * 12.f);
		
	}
	//ImVec2 debcont = ImGui::GetCursorScreenPos();
	ImGui::Spacing();

	float volhisto[9 * 8];
	float pitchhisto[5 * 8];
	float knobhisto[(65 * 2)];
	int step = cur_step;
	int q = (step / 16) & 3;
	for (int i = 0; i < 8; ++i) {
		FingerRecord* fr = &rampattern[q].steps[(step & 15)][i];
		for (int j = 0; j < 8; ++j) {
			volhisto[i * 9 + j] = (float)fr->pressure[j] + 10;
		}
		volhisto[i * 9 + 8] = 0;
		for (int j = 0; j < 4; ++j) {
			pitchhisto[i * 5 + j] = (float)fr->pos[j] + 10;
		}
		pitchhisto[i * 5 + 4] = 0;
	}
	for (int k = 0; k < 2; ++k) {
		for (int j = 0; j < 64; ++j) {
			knobhisto[k * 65 + j] = (float)rampattern[q].autoknob[(cur_step & 8) * 8 + j][k] + 128;
		}
	}
	ImGui::SetNextItemWidth(200);
	ImGui::PlotHistogram("velocity", volhisto, 9 * 8, 0, nullptr, 0.f, 265, ImVec2(0, 50));
	ImGui::SetNextItemWidth(200);
	ImGui::PlotHistogram("pitch", pitchhisto, 5 * 8, 0, nullptr, 0.f, 265, ImVec2(0, 50));
	ImGui::SetNextItemWidth(200);
	ImGui::PlotHistogram("knobs", knobhisto, 65 * 2, 0, nullptr, 0.f, 265, ImVec2(0, 50));
	/////////////////////////////////////////////////////////////////////////////////////////
	ImGui::Spacing();

	//ImGui::SetCursorScreenPos(ImVec2(400.f, 0.f));
	ImGui::NextColumn();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	p.y += 2.f;
	draw_list->AddImage((void*)size_t(buttonstex), ImVec2(p.x, p.y), ImVec2(p.x + buttonsw, p.y + buttonsh));
	// circles are 81x81, spacing is 96x91
	float pw = 80.f;//ImGui::GetWindowContentRegionWidth();	
	float xgutter=95.f-pw-6.8f, ygutter=90.2f-pw;
//	static int tt = GetTickCount(); printf("%dms\n", GetTickCount() - tt); tt = GetTickCount();
	// Draw + handle each column of the grid (except the shift row), then the final shift row separately (f==8).
	for (int f = 0; f < 9; ++f) {
		bool horiz = (f == 8);
		float ph = horiz ? (pw+xgutter+7.f)*8.f : (pw + ygutter) * 8.f;
		float jh = ph * 0.125f;
		const ImVec2 p = ImGui::GetCursorScreenPos();

		for (int j = 0; j < 8; ++j) {
			ImVec2 cen;
			if (horiz)
				cen = ImVec2(p.x + (j + 0.5f) * jh, p.y + pw*0.5f);
			else
				cen = ImVec2(p.x + (0.5f) * pw, p.y + jh * (0.5f + j));
			draw_list->AddCircle(cen, pw * 0.5f, 0xffcccccc, 32);
			// Draw LED.
			ImVec2 led_center(cen.x - pw * (horiz ? 0.0f : 0.4f), cen.y - jh * 0.4f);
			int led = clampi((int)(sqrtf(emuleds[f][j]) * 16.f), 0, 255)/2;
			draw_list->AddCircleFilled(led_center, pw * 0.1f, 0xff000000 + led * (horiz ? 0x20100 : 0x20202));
			draw_list->AddCircle(led_center, pw * 0.1f, 0xffcccccc, 16);
		}
		ImVec2 m = ImGui::GetIO().MousePos;
		int mb = ImGui::GetIO().MouseDown[0];
		m.x -= p.x; m.y -= p.y;
		if (horiz) { float t = m.x; m.x = pw - m.y; m.y = t; }
		if (mb && m.x >= 0 && m.y >= 0 && m.x < pw && m.y < ph) {
			emutouch[f][1] = clampi(int(((m.y) / ph) * 2047.f ), 0, 2047);
			int target = (clampi(int( (1.f-(fabsf(m.x-pw/2) / (pw/2) )) * 1024.f) + 1024, 1024, 2047));
			emutouch[f][0] += (target- emutouch[f][0])/4;
			//int j=emutouch[f][1]/256;
			//emuleds[f][j]=emutouch[f][0]/8;
		}
		else {
			if (!ImGui::GetIO().KeyShift && f<8)
				emutouch[f][0] = 0;
			if (!ImGui::GetIO().KeyCtrl && f == 8)
				emutouch[f][0] = 0;
		}
		ImGui::Dummy(horiz ? ImVec2(ph,pw) : ImVec2(pw+xgutter, ph+2.f));
		if (f < 7) ImGui::SameLine();
		else {
			ImVec2 nl = ImGui::GetCursorScreenPos();
//			nl.x += 390;
//			ImGui::SetCursorScreenPos(nl);
		}
	}
	ImGui::Spacing();
	//ImGui::SetCursorScreenPos(debcont);
	/*ImGui::Button("hello");
	for (int f = 0; f < 9; ++f) {
		ImGui::Text("%d-%d ", emutouch[f][0], emutouch[f][1]);
		ImGui::SameLine();
	}*/
#endif

	ImGui::End();
	// Rendering
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui::Render();


	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(window);

	if (!enable_emu_audio && plinky_inited) {
		for (int i = 0; i < 32000 / 33; i += 64) {
			static int half = 0;
			u32 temp[256];
			u32 audioin[256] = {};
			uitick(temp, audioin, half);
			half = 1 - half;
		}
	}
}

