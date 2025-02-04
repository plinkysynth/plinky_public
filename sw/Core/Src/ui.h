
#ifdef LPZW_TEST
 #define PROGMEM 

#include "lpzw.h"


void lpzwtest(void) {
	static const uint8_t logo32x32[] PROGMEM = {
0xE0, 0xF0, 0x38, 0x9C, 0xCE, 0xC6, 0x63, 0x31, 0xF9, 0xEF, 0x67, 0x33, 0x19, 0x1C, 0x0E, 0x07, 0x07, 0x0E, 0x1C, 0x19, 0x33, 0x67, 0xEF, 0xF9, 0x31, 0x63, 0xC6, 0xCE, 0x9C, 0x38, 0xF0, 0xE0,
0xFF, 0xFF, 0x07, 0xF3, 0xFF, 0x3F, 0x76, 0xE3, 0xCD, 0x8C, 0x1C, 0x3C, 0x3C, 0xF8, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3C, 0x3C, 0x1C, 0x8C, 0xCD, 0xE3, 0x76, 0x3F, 0xFF, 0xF3, 0x07, 0xFF, 0xFF,
0xFF, 0xFF, 0xE0, 0xCF, 0x9F, 0x30, 0x60, 0xC0, 0xFF, 0x3F, 0x30, 0xF0, 0xF0, 0x37, 0x36, 0x3E, 0x3E, 0x36, 0x37, 0xF0, 0xF0, 0x30, 0x3F, 0xFF, 0xC0, 0x60, 0x30, 0x9F, 0xCF, 0xE0, 0xFF, 0xFF,
0x07, 0x0F, 0x1C, 0x18, 0x31, 0x63, 0xC6, 0xCC, 0xD9, 0xF3, 0xE7, 0xEF, 0xDF, 0x3B, 0x73, 0xE3, 0xE3, 0x73, 0x3B, 0xDF, 0xEF, 0xE7, 0xF3, 0xD9, 0xCC, 0xC6, 0x63, 0x31, 0x18, 0x1C, 0x0F, 0x07,
	};
	

	u8 bmp[32][32];

	for (int y = 0; y < 32; ++y) {
		for (int x = 0; x < 32; ++x) {
			bmp[y][x] = (logo32x32[x + (y / 8) * 32] & (1 << (y & 7))) ? 255 : 0;
		}
	}
	u8 logorot[4][32];
	DebugLog("static const uint8_t logo32x32_rotated_pins_top[] PROGMEM = {\n");
	for (int y = 0; y < 4; ++y) {
		for (int x = 0; x < 32; ++x)
		{
			u8 b = 0;
			//for (int yy = 0; yy < 8; ++yy) if (bmp[31-x][yy + y * 8]) b |= 1 << yy; // pins at bottom
			for (int yy = 0; yy < 8; ++yy) if (bmp[x][31 - yy - y * 8]) b |= 1 << yy; // pins at top
			logorot[y][x] = b;
			DebugLog("0x%02x,", b);
		}
		DebugLog("\n");
	}
	DebugLog("};\n");
	for (int i = 33; i < 0x60; ++i) DebugLog("%c", i);
	DebugLog("\r\n");
	FILE* f = fopen("c:/temp/lpzw.pgm", "wb");
	fprintf(f, "P5 32 32 255\n");
	fwrite(bmp, 32, 32, f);
	fclose(f);
	int frame = 0;
	while (1) {
		++frame;
		clear();
		

		clearscreen();
		drawlpzwlogo(16.f * sinf(frame * 0.01f));
		drawstr24(-32.f - 32.f * sinf(frame * 0.02f), 32, "HELLO KAY");
		drawstr24(-32.f - 32.f * sinf(frame * 0.03f), 32+25, "HOW ARE YOU");
		drawstr24(-32.f - 32.f * sinf(frame * 0.04f), 32+50, "C#3 IS A NOTE");

		memcpy(vrambuf + 1, i2cbuf, 512);
		oled_flip(vrambuf);
		HAL_Delay(10);
	}
}

#endif

void bootswish(void) {
#ifdef WASM
	return;
#endif
#ifdef LPZW_TEST
	lpzwtest();
#endif
	for (int f = 0; f < 64; ++f) {
		HAL_Delay(20);
		for (int y = 0; y < 9; ++y) {
			int y1 = (y * 512) + 256;
			int dy = (y1 - 2048);
			dy *= dy;
			for (int x = 0; x < 8; ++x) {
				int x1 = (x * 512) + 256;
				int dx = (x1 - 2048);
				dx *= dx;
				int dist = (int)sqrtf((float)(dx + dy));
				dist -= f * 128;
				int k = -dist / 8;
				if (k > 255)
					k = 512 - k;
				if (k < 0)
					k = 0;
				led_ram[y][x] = ((k * k) >> 8);
			}
		}
	}
}


u8 audiohistpos = 0;
u8 audiopeakhistory[32];
u8 erasepos = 0;
u8 messagefnt;
const char* message = 0;
const char* submessage = 0;
u32 messagetime = 0;
void ShowMessage(EFont fnt, const char* msg, const char *submsg) {
	message = msg;
	submessage = submsg;
	messagefnt = fnt;
	messagetime = millis() + 500;
}

char 
#ifndef EMU
__attribute__((section (".endsection"))) 
#endif
version_tail[]=VERSION2;

void flip(void) {
	if (millis() > messagetime)
		message = 0;
	else if (message) {
		clear();
		int y = 0;
		if (submessage)
			drawstr(0, 0, F_12, submessage),y+=12;
		drawstr(0, y, messagefnt, message);
	}
	static u8 frame = 3;
	if (frame < 255) {
		int y = frame - 255 + 32;
		if (y < 32 - 12) y = 32 - 12;
		textcol = 3;
		fdrawstr(32, y, F_12, VERSION2
#ifdef DEBUG
			" DBG"
#endif
#ifndef NEW_LAYOUT
			" OL"
#endif
				, version_tail // version tail is passed to printf, so the linker cant remove it, haha, but we dont actually use the pointer
		);
		u8* v = getvram() - 1;
		const u8* l = getlogo() - 1;
		//u8* r = rndtab;
		int k = frame/2;
		const static u8 dither[16] = { 0, 8, 2, 10, 12,4,14,6,3,11,1,9,15,7,13,5 };
		for (int i = 0; i < 32 * 128 / 8; ++i) {
			u8 mask = 0;
#define RND(y) dither[(i&3)+((i/128+y)&3)*4]
			if (RND(0) < k) mask |= 1;
			if (RND(1) < k) mask |= 2;
			if (RND(2) < k) mask |= 4;
			if (RND(3) < k) mask |= 8;
			if (RND(4) < k) mask |= 16;
			if (RND(5) < k) mask |= 32;
			if (RND(6) < k) mask |= 64;
			if (RND(7) < k) mask |= 128;
			*v = (*v & mask) | (*l & ~mask);
			v++; l++;
		}
		frame+=4;
	}
	textcol = 1;
	oled_flip(vrambuf);
}



void draw_recording_ui(void) {
	clear();
	SampleInfo* s = getrecsample();

	int peak = maxi(0, audioin_peak / 128);
	int hold = maxi(0, audioin_hold / 128);
	if (enable_audio == EA_PREERASE) {
		drawstr(0, 0, F_32, "erasing...");
		invertrectangle(0, 0, erasepos * 2, 32);
		flip();
		return;
	}
	else {
		drawstr(-128, 0, F_12, (enable_audio == EA_ARMED) ? "armed!" : (enable_audio == EA_RECORDING) ? "recording" : "rec level" I_A);
		fdrawstr(-128, 32 - 12, F_12, (hold >= 254) ? "CLIP! %dms" : "%dms", s->samplelen / 32);
	}
	int full = s->samplelen / (MAX_SAMPLE_LEN / 128);
	if (enable_audio != EA_RECORDING)
		full = 0;
	for (int i = 0; i < full; ++i) {
		u16 avgpeak = getwaveform4zoom(s, i * 16, 4);
		u8 avg = avgpeak;
		u8 peak = avgpeak >> 8;
		vline(i, 15 - avg, 16 + avg, 1);
		vline(i, 15 - peak, 15 - avg, 2);
		vline(i, 16 + avg, 16 + peak, 2);
	}
	vline(full, 0, 32, 1);
	int srcpos = recpos;
	for (int i = 127; i > full; --i) {
		int pmx = 0, pmn = 0;
		for (int j = 0; j < 256; ++j) {
			int p = -delaybuf[--srcpos & DLMASK];
			pmx = maxi(p, pmx);
			pmn = mini(p, pmn);
		}
		vline(i, 15 + pmn / 1024, 16 + pmx / 1024, 2);
	}
	fillrectangle(hold - 1, 29, hold + 1, 32);
	halfrectangle(peak, 29, hold, 32);
	fillrectangle(0, 29, peak, 32);
	flip();
}

const char* notename(int note) {
	// blondi asked to octave-up the displayed notes
	note += 12;
	if (note < 0 || note>8 * 12) return "";
	static char buf[4];
	int octave = note / 12;
	note -= octave * 12;
	buf[0] = "CCDDEFFGGAAB"[note];
	buf[1] = " + +  + + + "[note];
	buf[2] = '0' + octave;
	return buf;
}

void DebugSPIPage(int addr) {
	while (spistate);
	spistate = 255;

	int spiid = spi_readid();
	DebugLog("SPI flash chip id %04x\r\n", spiid);

	for (int i = 0; i < 256; ++i) {
		memset(spibigrx, -2, sizeof(spibigrx));
		spi_read256(i * 256 + addr);
		memcpy(&delaybuf[i * 128], spibigrx + 4, 256);
	}
	DebugLog("first 256 samples of spi ram at addr %d:\r\n", addr);
	for (int i = 0; i < 256; ++i) {
		DebugLog("%5d ", delaybuf[i]);
		if ((i & 31) == 31) DebugLog("\r\n");
	}
	spistate = 0;
}

void DrawSample(SampleInfo *s, int sliceidx) {
	sliceidx &= 7;
	int ofs = s->splitpoints[sliceidx] / 1024;
	int maxx = s->samplelen / 1024;
	textcol = 3;
	for (int i = 0; i < 128; ++i) {
		int x = i - 64 + ofs;
		u8 h = getwaveform4(s, x);
		if (x >= 0 && x < maxx)
			vline(i, 15 - h, 16 + h, (i < 64) ? 2 : 1);
		if (i == 64) {
			vline(i, 0, 13 - h, 1);
			vline(i, 18 + h, 32, 1);
		}
	}
	fdrawstr(64 + 2, 0, F_12, "%d", sliceidx + 1);
	// draw other slices
	for (int si = 0; si < 8; ++si) if (si != sliceidx) {
		int x = (s->splitpoints[si] / 1024) - ofs + 64;
		char buf[2] = { '1' + si,0 };
		u8 h = getwaveform4(s, s->splitpoints[si] / 1024);
		if (x >= 0 && x < 128) {
			vline(x, 0, 13 - h, 2);
			vline(x, 18 + h, 32, 2);
		}
		drawstr_noright(x + 2, 0, F_8, buf);
	}
}


void DrawSamplePlayback(SampleInfo* s) {
	static int curofscenter = 0;
	static bool jumpable = false;
	int ofs = curofscenter / 1024;
	//int maxx = s->samplelen / 1024;
	textcol = 3;
	for (int i = 32; i < 128-16; ++i) {
		int x = i - 32 + ofs;
		u8 h = getwaveform4(s, x);
		vline(i, 15 - h, 16 + h, 2);
	}
	for (int si = 0; si < 8; ++si) {
		int x = (s->splitpoints[si] / 1024) - ofs + 32;
		u8 h = getwaveform4(s, s->splitpoints[si] / 1024);
		if (x >= 32 && x < 128-16) {
			vline(x, 0, 13 - h, 1);
			vline(x, 18 + h, 32, 1);
		}
	}
	s8 gx[8 * 4], gy[8 * 4], gd[8*4];
	int numtodraw = 0;
	int minpos = MAX_SAMPLE_LEN;
	int maxpos = 0;

	minpos = clampi(minpos, 0, s->samplelen);
	maxpos = clampi(maxpos, 0, s->samplelen);

	for (int i = 0; i < 8; ++i) {
		GrainPair* gr = voices[i].thegrains;
		int vvol = (int)(256.f * voices[i].vol);
		if (vvol>8) for (int g = 0; g < 4; ++g) {
			if (!(gr->outflags & (1 << (g & 1)))) {
				int pos = grainpos[i * 4 + g] & (MAX_SAMPLE_LEN - 1);
				int vol = gr->vol24 >> (24 - 4);
				if (g & 1) vol = 15 - vol;
				int graindir = (gr->dpos24 < 0) ? -1 : 1;
				int disppos = pos + graindir * 1024 * 32;
				minpos = mini(minpos, pos);
				maxpos = maxi(maxpos, pos);
				minpos = mini(minpos, disppos);
				maxpos = maxi(maxpos, disppos);
				int x = (pos / 1024) - ofs + 32;
				int y = (vol * vvol) >> 8;
				if (g & 2) y = 16 + y; else y = 16 - y;
				if (x >= 32 && x < 128 - 16 && y >= 0 && y < 32) {
					gx[numtodraw] = x;
					gy[numtodraw] = y;
					gd[numtodraw] = graindir;
					numtodraw++;
				}
			}
			if (g & 1) gr++;
		}
		
	}
	for (int i = 0; i < numtodraw; ++i) {
		int x = gx[i], y = gy[i];
		vline(x, y - 2, y + 2, 0);
		vline(x + gd[i], y - 1, y + 2, 0);
	}
	for (int i = 0; i < numtodraw; ++i) {
		int x = gx[i], y = gy[i];
		vline(x, y - 1, y + 1, 1);
		putpixel(x + gd[i], y, 1);
	}
	if (minpos>=maxpos)
		jumpable = true;
	else {
		if (jumpable)
			curofscenter = minpos-8*1024;
		else {
#define GRAIN_SCROLL_SHIFT 3
			if (minpos < curofscenter) curofscenter += (minpos - curofscenter) >> GRAIN_SCROLL_SHIFT;
			if (maxpos > curofscenter + (128-48) * 1024) curofscenter += (maxpos - curofscenter - (128-48) * 1024) >> GRAIN_SCROLL_SHIFT;
		}
		jumpable = false;
	}
}

void samplemode_ui(void) {
	SampleInfo* s = getrecsample();
	if (enable_audio == EA_PREERASE) {
		erasepos = 0;
		draw_recording_ui();
		while (spistate);
		spistate = 255;
		HAL_Delay(10);
		// mysteriously, sometimes page 0 wasn't erasing. maybe do it twice? WOOAHAHA
		spi_erase64k(0 + record_flashaddr_base, draw_recording_ui);
		for (int addr = 0; addr < MAX_SAMPLE_LEN*2; addr += 65536) {
			spi_erase64k(addr + record_flashaddr_base, draw_recording_ui);
			erasepos++;
		}
		memset(s, 0, sizeof(SampleInfo)); // since we erased the ram, we might as well nuke the sample too
		ramtime[GEN_SAMPLE] = millis();
		// done!
		spistate = 0;
		//DebugSPIPage(0);
		//DebugSPIPage(1024*1024*2-65536);
		enable_audio = EA_MONITOR_LEVEL;
	}
	if (enable_audio == EA_PLAY) {
		if (shift_down_time > 4 && (shift_down == SB_RECORD) && enable_audio == EA_PLAY) {
			clear();
			drawstr(0, 0, F_32, "record?");
			invertrectangle(0, 0, shift_down_time * 2, 32);
			oled_flip(vrambuf);
			return;
		}
		// just show the waveform of the current slice
		clear();
		if (!s->samplelen) {
			drawstr(0, 0, F_16, "<empty sample>");
			drawstr(0, 16, F_16, "hold " I_RECORD " to record");
		}
		else {
			DrawSample(s, recsliceidx);
			
			//int w = fdrawstr(0, 0, F_16, "%d", edit_sample0 + 1);
			textcol = 2;
			//fdrawstr(0, 32 - 12, F_12, "%dms", s->splitpoints[recsliceidx] / 32);
			drawstr(-128+16, 32 - 12, F_12, (s->loop & 2) ? "all" : "slc");
			drawicon(128-16, 32 - 14, ((s->loop & 1) ? I_FEEDBACK[0] : I_RIGHT[0]) - 0x80, textcol);
			if (s->pitched)
				drawstr(0, 32 - 12, F_12, notename(s->notes[recsliceidx&7])); 
			else
				drawstr(0, 32 - 12, F_12, "tape");
			textcol = 1;
		}
		flip();
		for (int x = 0; x < 8; ++x) {
			int sp0 = s->splitpoints[x];
			int sp1 = (x < 7) ? s->splitpoints[x + 1] : s->samplelen;
			for (int y = 0; y < 8; ++y) {
				int samp = sp0+(((sp1-sp0)*y)>>3);
				const static int zoom = 3;
				u16 avgpeak = getwaveform4zoom(s, samp / 1024, zoom);
				u8 h = avgpeak & 15;
				led_ram[x][y] = led_gamma(h * 32);
			}
			if (x == recsliceidx) {
				led_ram[x][0] = triangle(millis());
			}
			led_ram[8][x] = 0;
		}
		led_ram[8][SB_RECORD] = triangle(millis());
		led_ram[8][SB_PLAY] = 0;
		led_ram[8][SB_PARAMSA] = (s->pitched ? 255 : 0);
		led_ram[8][SB_PARAMSB] = ((s->loop & 1) ? 255 : 0);
		return;
	}

	if (enable_audio >= EA_RECORDING) {

		int locrecpos = recpos;
		//locrecpos=1024*1024;
		//int bufsize = locrecpos - recreadpos;
		//DebugLog("%d buf\r\n", bufsize);
		while (spistate);
		spistate = 0xff; // prevent spi reads for a while!
		while (recreadpos + 256 / 2 <= locrecpos && s->samplelen < MAX_SAMPLE_LEN) {
			s16* src = delaybuf + (recreadpos & DLMASK);
			s16* dst = (s16*)(spibigtx + 4);
			int flashaddr = (recreadpos - recstartpos) * 2;
			recreadpos += 256 / 2;
			if (!pre_erase && (flashaddr & 65535) == 0)
				spi_erase64k(flashaddr + record_flashaddr_base, /*draw_recording_ui*/ 0);
			// find peak and copy to fx buf
			int peak = 0;
			s16* delaybufend = delaybuf + DLMASK + 1;
			for (int i = 0; i < 256 / 2; ++i) {
				s16 smp = *src++;
				*dst++ = smp;
				peak = maxi(peak, abs(smp));
				if (src == delaybufend)
					src = delaybuf;
			}
			setwaveform4(s, flashaddr/2 / 1024, peak / 1024);
			if (spi_write256(flashaddr + record_flashaddr_base) != 0) {
				DebugLog("flash write fail\n");
			}
			s->samplelen = recreadpos - recstartpos;
			ramtime[GEN_SAMPLE] = millis();
			if (s->samplelen >= MAX_SAMPLE_LEN) {
				///  FINISHED RECORDING
				recording_stop();
				break;
			}
		} // spi write loop
		spistate = 0;
		if (enable_audio == EA_STOPPING4) {
			recording_stop_really();
		}
	}
	int bufsize = recpos - recreadpos;
	if (bufsize < 4096 || enable_audio != EA_RECORDING) {
		draw_recording_ui();
	}
	/////////// update leds
	

	for (int x = 0; x < 8; ++x) {
		int barpos0 = audiopeakhistory[(audiohistpos + x * 4 + 1) & 31];
		int barpos1 = audiopeakhistory[(audiohistpos + x * 4 + 2) & 31];
		int barpos2 = audiopeakhistory[(audiohistpos + x * 4 + 3) & 31];
		int barpos3 = audiopeakhistory[(audiohistpos + x * 4 + 4) & 31];
		for (int y = 0; y < 8; ++y) {
			int yy = (7 - y) * 32;
			int k = clampi(barpos0 - yy, 0, 31);
			k += clampi(barpos1 - yy, 0, 31);
			k += clampi(barpos2 - yy, 0, 31);
			k += clampi(barpos3 - yy, 0, 31);
			led_ram[x][y] = led_gamma(k * 2);
		}
		if (x == recsliceidx && enable_audio == EA_RECORDING) {
			for (int y = 0; y < 8; ++y)
				led_ram[x][y] = maxi(led_ram[x][y], (triangle(millis()) * 4) / (y + 4));
		}
		led_ram[8][x] = 0;
	}
	led_ram[8][SB_RECORD] =
	(enable_audio == EA_RECORDING) ? 255 : triangle(millis() / 2);
}


const static float life_damping = 0.91f;//  0.9f;
const static float life_force = 0.25f;// 0.25f;
const static float life_input_power = 6.f;

static int frame = 0;

void DrawLFOs(void) {
	u8* vr = getvram();
	vr += 128 - 16;
	u8 lfohp = ((lfo_history_pos >> 4) + 1) & 15;
	for (int x = 0; x < 16; ++x) {
		vr[0] &= ~(lfo_history[lfohp][0] >> 1);
		vr[128] &= ~(lfo_history[lfohp][1] >> 1);
		vr[256] &= ~(lfo_history[lfohp][2] >> 1);
		vr[384] &= ~(lfo_history[lfohp][3] >> 1);


		vr[0] |= lfo_history[lfohp][0];
		vr[128] |= lfo_history[lfohp][1];
		vr[256] |= lfo_history[lfohp][2];
		vr[384] |= lfo_history[lfohp][3];
		vr++;
		lfohp = (lfohp + 1) & 15;
	}
}

void DrawVoices(void) {
    const static u8 leftOffset = 44;
	const static u8 maxHeight = 10;
	const static u8 barWidth = 3;
	const static float moveSpeed = 5; // pixels per frame
	static float touchLineHeight[8];
	static float maxVolume[8];
	static float volLineHeight[8];
    u8 rightOffset = (rampreset.flags & FLAGS_LATCH) ? 38 : 14;
	// all voices
    for (u8 i = 0; i < 8; i++) {
		// string volume
		if (maxVolume[i] != 0) {
			volLineHeight[i] = voices[i].vol / maxVolume[i] * maxHeight;
		}
		// string pressed
        if (synthfingerdown_nogatelen & (1 << i)) {
			// move touch line
			if (touchLineHeight[i] < maxHeight)
				touchLineHeight[i] += moveSpeed;
			if (touchLineHeight[i] > maxHeight)
				touchLineHeight[i] = maxHeight;
			// volume line catches up
			volLineHeight[i] = maxf(volLineHeight[i], touchLineHeight[i]);
			// remember peak volume
			maxVolume[i] = voices[i].vol;
		}
		// string not pressed
		else {
			if (touchLineHeight[i] > 0)
				touchLineHeight[i] -= moveSpeed;
			if (touchLineHeight[i] < 0 )
				touchLineHeight[i] = 0;
			// has the sound died out?
			if (maxVolume[i] != 0 && volLineHeight[i] < 0.25) {
				// disable volume line
				maxVolume[i] = 0;
				volLineHeight[i] = 0;
			}
		}
		// draw bars
		u8 x = i * (W - leftOffset - rightOffset) / 8 + leftOffset;
		if (touchLineHeight[i] > 0) {
			for (uint8_t dx = 0; dx < barWidth; dx++)
				vline(x - barWidth / 2 + dx, H - 1 - touchLineHeight[i], H - 1, 2);
		}
		hline(x - barWidth / 2, H - 1 - volLineHeight[i], x - barWidth / 2 + barWidth, 1);
    }
}

void DrawFlags()
{
	textcol = 0;
	if ((rampreset.flags & FLAGS_ARP))
	{
		fillrectangle(128 - 32, 0, 128 - 17, 8);
		drawstr(-(128 - 17), -1, F_8, "arp");
	}
	if ((rampreset.flags & FLAGS_LATCH)) {
		fillrectangle(128 - 38, 32 - 8, 128 - 17, 32);
		drawstr(-(128 - 17), 32 - 7, F_8, "latch");
	}
	textcol = 1;
	vline(126, 32 - (maxpressure_out / 8), 32, 1);
	vline(127, 32 - (maxpressure_out / 8), 32, 1);

}

const char *getparamstr(int p, int mod, int v, char *valbuf, char *decbuf) {
	if (decbuf) *decbuf = 0;
	int valmax = param_flags[p] & FLAG_MASK;
	int vscale = valmax ? (mini(v, FULL - 1) * valmax) / FULL : v;
	int displaymax = valmax ? valmax * 10 : 1000;
	bool decimal = true;
//	const char* val = valbuf;
	if (mod==M_BASE) switch (p) {
	case P_SMP_TIME:
	case P_SMP_RATE:
		displaymax = 2000;
		break;
	case P_ARPONOFF:
		if (mod) return "";
		return ((rampreset.flags & FLAGS_ARP)) ? "On":  "Off";
	case P_LATCHONOFF:
		if (mod) return "";
		return ((rampreset.flags & FLAGS_LATCH)) ? "On" : "Off";
	case P_SAMPLE:
		if (vscale == 0) {
			return "Off";
		}
		break;
	case P_SEQDIV:
	{	if (vscale >= DIVISIONS_MAX)
			return "(Gate CV)";
		//int divisor = (clampi(vscale - 3, 0, 3 * 5) / 3);
		int n = sprintf(valbuf, "%d", divisions[vscale] /* >> divisor*/);
		if (!decbuf)
			decbuf = valbuf + n;
		sprintf(decbuf, /*divisornames[divisor]*/ "/32");
		return valbuf;
	}
	case P_ARPDIV:
		if (v < 0) {
			v = -v;
			decimal = true;
			valmax = 0;
			displaymax = 1000;
			break;
		}
		vscale = (mini(v, FULL - 1) * DIVISIONS_MAX) / FULL;
		//int divisor = (clampi(vscale-3,0,3*5) / 3);
		int n=sprintf(valbuf, "%d", divisions[vscale] /*>> divisor*/);
		if (!decbuf)
			decbuf = valbuf + n;
		sprintf(decbuf,/*divisornames[divisor]*/ "/32");
		return valbuf;
	case P_ARPOCT:
		v += (FULL * 10) / displaymax; // 1 based
		break;
	case P_MIDI_CH_IN:
	case P_MIDI_CH_OUT: {
	    int midich = clampi(vscale,0,15)+1;
	    int n=sprintf(valbuf, "%d", midich);
	    if (!decbuf)
	        decbuf = valbuf + n;
	    return valbuf;
	}
	case P_ARPMODE:
		return arpmodenames[clampi(vscale, 0, ARP_LAST - 1)];
	case P_SEQMODE:
		return seqmodenames[clampi(vscale, 0, SEQ_LAST - 1)];
	case P_CV_QUANT:
		return cvquantnames[clampi(vscale, 0, CVQ_LAST - 1)];
	case P_SCALE:
		return scalenames[clampi(vscale, 0, S_LAST - 1)];
	case P_ASHAPE: case P_BSHAPE: case P_XSHAPE: case P_YSHAPE:
		return lfonames[clampi(vscale, 0, LFO_LAST - 1)];
	case P_PITCH:
	case P_INTERVAL:
		displaymax = 120;
		break;
	case P_TEMPO:
		v += FULL;
		if (external_clock_enable)
			v = (bpm10x * FULL) / 1200;
		displaymax = 1200;
		break;
	case P_DLTIME: 
		if (v < 0) {
			if (v <= -1024) v++;
			v= (-v*13)/FULL;
			int n = sprintf(valbuf, "%d", divisions[v]);
			if (!decbuf)
				decbuf = valbuf + n;
			sprintf(decbuf, "/32 sync");
		}
		else {
			int n = sprintf(valbuf, "%d", (v*100)/FULL);
			if (!decbuf)
				decbuf = valbuf + n;
			sprintf(decbuf, "free");
		}
		return valbuf;
	default:;
	}
	v = (v * displaymax) / FULL;
	int av = abs(v);
	int n=sprintf(valbuf, "%c%d", (v < 0) ? '-' : ' ', av / 10);
	if (decimal) {
		if (!decbuf)
			decbuf = valbuf + n;
		sprintf(decbuf, ".%d", av % 10);
	}
	return valbuf;
}



void editmode_ui(void) {

	clear();



	float damping = life_damping;// / 65536.f * adcbuf[ADC_POT1];
	float force = life_force;// / 65536.f * adcbuf[ADC_POT2];
	float* prev = surf[frame & 1][0];
	frame++;
	float* next = surf[frame & 1][0];
	int i = 0;
	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x, ++i) {
			Finger* curfinger = touch_synth_getlatest(x);
			float corners = 0.f, edges = 0.f;
			if (x > 0) {
				if (y > 0)
					corners += prev[i - 9];
				edges += prev[i - 1];
				if (y < 7)
					corners += prev[i + 7];
			}
			if (y > 0)
				edges += prev[i - 8];
			if (y < 7)
				edges += prev[i + 8];
			if (x < 7) {
				if (y > 0)
					corners += prev[i - 7];
				edges += prev[i + 1];
				if (y < 7)
					corners += prev[i + 9];
			}
			float target = corners * (1.f / 12.f) + edges * (1.f * 2.f / 12.f);
			//if (x==4 && y==4) target=4.f;
			target *= damping;
			if (curfinger->pos >> 8 == y) {
				float pressure = curfinger->pressure * (1.f / 2048.f);
				if ((rampreset.flags & FLAGS_ARP) && !(arpbits & (1 << x)))
					pressure = 0.f;

				target = lerp(target, life_input_power, clampf(pressure * 2.f, 0.f, 1.f));
			}
			float pos = prev[i];
			float accel = (target - pos) * force;
			float vel = (prev[i] - next[i]) * damping + accel; // here next is really 'prev prev'
			next[i] = pos + vel;
		} // x
	} // y

	char presetname[9];
	memcpy(presetname, rampreset.name, 8);
	presetname[8] = 0;

	u8* vr = getvram();
	static u8 ui_edit_param=P_LAST;
	u8 ep = edit_param;
	if (ui_edit_param != ep) {
		ui_edit_param = ep; // as it may change in the background
/*		int page = 0;
		if ((ui_edit_param % 12) >= 6) page = 1;
		// we want to put ep into slot 0, and push whatever was there downwards until we find the new one
		u8 towrite = ep;
		for (int i = 0; i < 4; ++i) {
			u8 displaced = ui_edit_param_prev[page][i];
			ui_edit_param_prev[page][i] = towrite;
			if (displaced == ep) break;
			towrite = displaced;
		}
		*/
	}
	u8 ui_edit_mod = edit_mod;
	u8 loopstart_step = (rampreset.loopstart_step_no_offset + step_offset) & 63;

	const char* pagename = 0;
	if (shift_down == SB_RECORD) {
		if (isshortpress())
			drawstr(0, 4, F_20_BOLD, recording ? I_RECORD "record >off" : I_RECORD "record >on");
		else if (recording) {
			if (recordingknobs == 0)
				drawstr(0, 4, F_20_BOLD, "record " I_A I_B "?");
			else if (recordingknobs == 1)
				drawstr(0, 4, F_20_BOLD, "recording " I_A);
			else if (recordingknobs == 2)
				drawstr(0, 4, F_20_BOLD, "recording " I_B);
			else if (recordingknobs == 3)
				drawstr(0, 4, F_20_BOLD, "recording " I_A I_B);
		}
	}
	else if (shift_down == SB_PLAY) {
		drawstr(0, 0, F_32_BOLD, I_PLAY "play");
	}
	else if (shift_down == SB_CLEAR && editmode!=EM_PRESET) {
		drawstr(0, 0, F_32_BOLD, I_CROSS "clear");
	}
	/*
	else if (shift_down == SB_PREV && shift_down_time > 4 && enable_audio == EA_PLAY) {
		drawstr(0, 4, F_24_BOLD, rampreset.arpon ? I_NOTES "arp >off" : I_NOTES "arp >on");
		invertrectangle(0, 0, shift_down_time*4-8, 4);
	}*/
	else switch (editmode) {
	case EM_PLAY:
		if (ramsample.samplelen) {
			DrawSamplePlayback(&ramsample);
		}
		else {
			for (int x = 0; x < 128; ++x) {
				u32 m = scope[x];
				vr[0] = m;
				vr[128] = m >> 8;
				vr[256] = m >> 16;
				vr[384] = m >> 24;
				vr++;
			}
		}
		DrawLFOs();
		DrawVoices();
		textcol = 2;
		if (ui_edit_param < P_LAST)
			goto draw_parameter;

		extern int lastencodertime;

		if (last_edit_param<P_LAST && lastencodertime && lastencodertime > millis() - 2000)
			goto draw_parameter;
		DrawFlags();
		char seqicon = (rampreset.flags & FLAGS_ARP) ? I_NOTES[0] : I_SEQ[0];
		char preseticon = I_PRESET[0];
		int xtab = 0;
		if (edit_preset_pending != 255 && edit_preset_pending != sysparams.curpreset)
			xtab=fdrawstr(0, 0, F_20_BOLD, "%c%d->%d", preseticon, sysparams.curpreset + 1, edit_preset_pending + 1);
		else if (maxpressure_out > 1 && !(ramsample.samplelen && !ramsample.pitched)) {
			xtab = fdrawstr(0, 0, F_20_BOLD, "%s", notename((pitchhi_out + 1024) / 2048) );
		}
		else
			xtab = fdrawstr(0, 0, F_20_BOLD, "%c%d", preseticon, sysparams.curpreset + 1);
		drawstr(xtab+2, 0, F_8_BOLD, presetname);
		if (rampreset.category >0 && rampreset.category <CAT_LAST) drawstr(xtab + 2, 8, F_8, kpresetcats[rampreset.category]);
		if (edit_pattern_pending != 255 && edit_pattern_pending != cur_pattern)
			fdrawstr(0, 16, F_20_BOLD, "%c%d->%d", seqicon, cur_pattern + 1, edit_pattern_pending + 1);
		else
			// if (cur_sample1)
			//fdrawstr(0, 16, F_20_BOLD, "%c%d " I_WAVE "%d", seqicon, cur_pattern + 1, cur_sample1);
		//else
			fdrawstr(0, 16, F_20_BOLD, "%c%d", seqicon, cur_pattern + 1);
		//clear();
		//DrawLFOs();
		//fdrawstr(0, 0, F_8, "%5d %5d", debuga[0], debuga[1]);
		//fdrawstr(0, 8, F_8, "%5d %5d", debuga[2], debuga[3]);
		//drawstr(48, 24, F_8, "loopop edition");
		break;
	case EM_PARAMSA:
	case EM_PARAMSB:
		if (ui_edit_param >= P_LAST) {
			drawstr(0, 0, F_20_BOLD, modnames[ui_edit_mod]);
			drawstr(0, 16, F_16, "select parameter");
			break;
		}
		else {
			DrawLFOs();
			int pi;
		draw_parameter:
			pi = ui_edit_param;
			if (pi >= P_LAST) pi = last_edit_param;
			if (pi >= P_LAST) pi = 0;
			pagename = pagenames[pi / 6];
			switch (pi) {
			case P_TEMPO: pagename = I_TEMPO "Tap"; break;
#ifndef NEW_LAYOUT
			case P_ENV_LEVEL:
			case P_ENV_REPEAT:
			case P_ENV_WARP:
			case P_ENV_RATE: pagename = "env2"; break;
#endif
			case P_NOISE: pagename = I_WAVE "noise"; break;
			case P_CV_QUANT:
			case P_HEADPHONE: pagename = "system"; break;
			}
			drawstr(0, 0, F_12, (ui_edit_mod == 0) ? pagename : modnames[ui_edit_mod]);
			const char* pn = paramnames[pi];
			int pw = strwidth(F_16_BOLD, pn);
			if (pw>64) 
				drawstr(0, 20, F_12_BOLD, pn);
			else
				drawstr(0, 16, F_16_BOLD, pn);
			char valbuf[32];
			char decbuf[16];
			int w = 0;
			int v =  GetParam(pi, ui_edit_mod);
			int vbase = v;
			if (ui_edit_mod == 0) {
				v = (param_eval_int_noscale(pi, any_rnd, env16, pressure16) * FULL) >> 16;
				if (v != vbase) {
					// if there is modulation going on, show the base value below
					const char* val = getparamstr(pi, ui_edit_mod, vbase, valbuf, NULL);
					w = strwidth(F_8, val);
					drawstr(128 - 16 - w, 32 - 8, F_8, val);
				}
			}
			const char* val = getparamstr(pi, ui_edit_mod, v, valbuf, decbuf);
			int x = 128 - 15; 
			if (*decbuf) x -= strwidth(F_8, decbuf);
			int font = F_24_BOLD;
			while (1) {
				w = strwidth(font, val);
				if (w < 64 || font <= F_12_BOLD)
					break;
				font--;
			}
			drawstr(x - w, 0, font, val);
			if (*decbuf)
				drawstr(x, 0, F_8, decbuf);
		}
		break;
	case EM_START:
		fdrawstr(0, 0, F_20_BOLD, I_PREV "Start %d", loopstart_step + 1);
		fdrawstr(0, 16, F_20_BOLD, I_PLAY "Current %d", cur_step + 1);
		break;
	case EM_END:
		fdrawstr(0, 0, F_20_BOLD, I_NEXT "End %d", ((rampreset.looplen_step + loopstart_step) & 63) + 1);
		fdrawstr(0, 16, F_20_BOLD, I_INTERVAL "Length %d", rampreset.looplen_step);
		break;
	case EM_PRESET:
		if (shift_down_time > 4 && shift_down == SB_CLEAR) {
			bool done = (shift_down_time - 4) > 64;
			if (last_preset_selection_rotstep < 32)
				fdrawstr(0, 0, F_16_BOLD, done ? "cleared\n" I_PRESET "Preset %d" : "initialize\n" I_PRESET "Preset %d?", last_preset_selection_rotstep + 1);
			else if (last_preset_selection_rotstep < 64 - 8)
				fdrawstr(0, 0, F_16_BOLD, done ? "cleared\n" I_SEQ "Pattern %d." : "Clear\n" I_SEQ "Pattern %d?", last_preset_selection_rotstep - 32 + 1);
			else if (last_preset_selection_rotstep < 64 && last_preset_selection_rotstep >0)
				fdrawstr(0, 0, F_16_BOLD, done ? "cleared\n" I_WAVE "Sample %d." : "Clear\n" I_WAVE "Sample %d?", last_preset_selection_rotstep - (64 - 8) + 1);
			invertrectangle(0, 0, shift_down_time * 2 - 4, 32);
		}
		else if (longpress >= 32) {
			bool done = (longpress - 32) > 128;
			if (first_finger_rotstep >= 56)
				fdrawstr(0, 0, F_16_BOLD, done ? "ok!" : "Edit\n" I_WAVE "Sample %d?", first_finger_rotstep - 56 + 1);
			else if (first_finger_rotstep == copyfrompreset)
				fdrawstr(0, 0, F_16_BOLD, done ? "toggled\n" I_PRESET "Preset %d" : "toggle\n" I_PRESET "Preset %d?", copyfrompreset + 1);
			else if (first_finger_rotstep < 32)
				fdrawstr(0, 0, F_16_BOLD, done ? "copied to " I_PRESET "%d" : "copy over\n" I_PRESET "Preset %d?", first_finger_rotstep + 1);
			else
				fdrawstr(0, 0, F_16_BOLD, done ? "copied to " I_SEQ "%d" : "copy over\n" I_SEQ "Pat %d?", first_finger_rotstep - 32 + 1);

			invertrectangle(0, 0, longpress - 32, 32);
		}
		else {
			int xtab = fdrawstr(0, 0, F_20_BOLD, I_PRESET "%d", sysparams.curpreset + 1);
			drawstr(xtab + 2, 0, F_8_BOLD, presetname);
			if (rampreset.category > 0 && rampreset.category < CAT_LAST) 
				drawstr(xtab + 2, 8, F_8, kpresetcats[rampreset.category]);

			fdrawstr(0, 16, F_20_BOLD, I_SEQ "Pat %d ", cur_pattern + 1);
			fdrawstr(-128, 16, F_20_BOLD, cur_sample1 ? I_WAVE "%d" : I_WAVE "Off", cur_sample1);
		}
		break;
	}

	flip();
	bool playing = playmode == PLAYING;
	int clockglow = maxi(96, 255 - calcseqsubstep(0, 256 - 96));
	int flickery = triangle(millis() / 2);
	//	int flickery2 = triangle(millis() & 255);
	int flickeryfast = triangle(millis() * 8);
	int loopbright = 96;
	int phase0 = calcseqsubstep(0, 8);

	int cvpitch = (int)(adc_smooth[6].y2 * (512.f * 12.f)); // pitch cv input 
	int cvquant = param_eval_int(P_CV_QUANT, any_rnd, env16, pressure16);
	if (cvquant) {
		cvpitch = (cvpitch + 256) & (~511);
	}


	for (int fi = 0; fi < 8; ++fi) {
		//			Finger *f = touch_getlatest(fi);
				//int yy=(f->pos)/256;

		Finger* synthf = touch_synth_getlatest(fi);
		FingerRecord* fr = readpattern(fi);
		
		///////////////////////////////////// ROOT NOTE DISPLAY
		/// per finger    
		int root = param_eval_finger(P_ROTATE, fi, synthf);
		//int interval = (param_eval_finger(P_INTERVAL, fi, synthf) * 12) >> 7;
		//int totpitch = 0;
		u32 scale = param_eval_finger(P_SCALE, fi, synthf);
		if (scale >= S_LAST) scale = 0;
		if (cvquant == CVQ_SCALE) {
			// remap the 12 semitones input to the scale steps, evenly, with a slight offset so white keys map to major scale etc
			int steps = ((cvpitch / 512) * scaletab[scale][0] + 1) / 12;
			root += steps;
		}
		int stride_semitones = maxi(0, param_eval_finger(P_STRIDE, fi, synthf));
		root += stride(scale, stride_semitones, fi);

		///////////////////////////////////////////////////////
		int sp0 = ramsample.splitpoints[fi];
		int sp1 = (fi < 7) ? ramsample.splitpoints[fi + 1] : ramsample.samplelen;
			


		for (int y = 0; y < 8; ++y) {


			int step = fi + y * 8;
			int rotstep = fi * 8 + y;
			int k = 0; //f->pressure - (y^7) * 128;
			k = clampi((int)((next[step]) * 64.f) - 20, 0, 128);

			
			if (synthf->pos / 256 == y)
				k = maxi(k, synthf->pressure / 8);

			if (fr && fr->pos[phase0/2] / 32 == y)
				k = maxi(k, fr->pressure[phase0]);

			bool inloop = ((step - loopstart_step) & 63) < rampreset.looplen_step;
#ifdef NEW_LAYOUT
			int pA = (fi > 0 && fi < 7) ? (fi - 1) + (y) * 12 : P_LAST;
#else
			int p = (fi - 1) + (y - 1) * 12;
#endif

			int pAorB = pA + ((editmode == EM_PARAMSB) ? 6 : 0);

			switch (editmode) {
			case EM_PARAMSA:
			case EM_PARAMSB: {
				if (fi == 7) {
					if (y == ui_edit_mod) k = flickery; else {
						// light right side for mod sources that are non zero
						k = (y && GetParam(ui_edit_param, y)) ? 255 : 0;
					}
				}
				else if (fi == 0 && ui_edit_param < P_LAST) {
					// bar graph
				bargraph:
					k = 0;
					int v = GetParam(ui_edit_param, ui_edit_mod);
					bool issigned = param_flags[ui_edit_param] & FLAG_SIGNED;
					issigned |= (edit_mod != M_BASE);
					int kontrast = 16;

					if (issigned) {
						if (y < 4) {
							k = ((v - (3 - y) * (FULL / 4)) * (192 * 4)) / FULL;
							k = (y) * 2 * kontrast + clampi(k, 0, 191);
							if (y == 3 && v < 0) k = 255;
						}
						else {
							k = ((-v - (y - 4) * (FULL / 4)) * (192 * 4)) / FULL;
							k = (8 - y) * 2 * kontrast + clampi(k, 0, 191);
							if (y == 4 && v > 0) k = 255;
						}
					}
					else {
						k = ((v - (7 - y) * (FULL / 8)) * (192 * 8)) / FULL;
						k = (y)*kontrast + clampi(k, 0, 191);
					}

				}
				if (fi > 0 && fi < 7) {
					k = 0;
					if ((fingerediting & 128) && (fingerstable & 128) && ui_edit_mod > 0) {
						// finger down over right side! show all params with a non zero mod amount
						if (GetParam(pAorB, ui_edit_mod))
							k = 255;
					}
					if (pAorB == ui_edit_param)
						k = flickery;

						//else
						//	k = maxi(k, abs(GetParam(p, ui_edit_mod)) / 4);
				}
#ifdef NEW_LAYOUT
				if (pAorB == P_ARPONOFF)
					k = (rampreset.flags & FLAGS_ARP) ? 255 : 0;
				else if (pAorB == P_LATCHONOFF)
					k = (rampreset.flags & FLAGS_LATCH) ? 255 : 0;
#else
				if (y == 0) {
					if (fi == 1)
						k = maxi(k, (rampreset.arpon) ? 255 : 0);
					else if (fi == 6)
						k = maxi(k, (sysparams.systemflags & SYS_LATCHON) ? 255 : 0);
				}
#endif
				break; }
			case EM_PLAY:
				if (fi == 0 && ui_edit_param < P_LAST)
					goto bargraph;
				if (ui_edit_param<P_LAST && (ui_edit_param == pA || ui_edit_param == pA+6) && fi>0 && fi<7)
					k = flickery;
				{
					if (ramsample.samplelen && !ramsample.pitched) {
						int samp = sp0 + (((sp1 - sp0) * y) >> 3);
						const static int zoom = 3;
						u16 avgpeak = getwaveform4zoom(&ramsample, samp / 1024, zoom) & 15;
						k=maxi(k,avgpeak * (96/16));
					}
					else {
						int pitch = (lookupscale(scale, (7 - y) + root));
						pitch %= 12 * 512;
						if (pitch < 0) pitch += 12 * 512;
						if (pitch < 256)
							k = maxi(k, 96);
					}
				}

				loopbright = 48;
				// fall thru
			case EM_START:
			case EM_END:
				// show looping region faintly; show current playpos brightly
				if (inloop)
					k = maxi(k, loopbright);
				if (step == loopstart_step && editmode == EM_START)
					k = 255;
				if (((step + 1) & 63) == ((loopstart_step + rampreset.looplen_step) & 63) && editmode == EM_END)
					k = 255;
				// playhead
				if (step == cur_step)
					k = maxi(k, clockglow);
				if (step == pending_loopstart_step && playing)
					k = maxi(k, (clockglow * 4) & 255);
				break;
			case EM_PRESET:
				k = (fi >= 4 && fi < 7) ? 64 : 0;
				if (rotstep == edit_preset_pending)
					k = flickeryfast;
				if (rotstep == sysparams.curpreset)
					k = 255;
				if (rotstep == edit_pattern_pending + 32)
					k = flickeryfast;
				if (rotstep == cur_pattern + 32)
					k = 255;
				if (edit_sample1_pending && rotstep == (edit_sample1_pending - 1) + 32 + 24)
					k = flickeryfast;
				if (cur_sample1 && rotstep == (cur_sample1 - 1) + 32 + 24)
					k = 255;
				break;
			}

			if (editmode == EM_PLAY) {
				int delay = 1+(((7 - y) * (7 - y) + fi * fi) >> 2);
				u8 histpos = (audiohistpos + 31 - delay) & 31;
				u8 ainlvl = audiopeakhistory[histpos];
				u8 ainlvlprev = audiopeakhistory[(histpos - 1) & 31];
				ainlvl = maxi(0, ainlvl - ainlvlprev); // highpass
				ainlvl = clampi((ainlvl * (32 - delay)) >> (6), 0, 255); // fade out
				k = maxi(k, ainlvl);
			}
			led_ram[fi][y] = led_gamma(k);
		}
	}
	{
		led_ram[8][SB_PLAY] = playing ? led_gamma(clockglow) : 0;
		led_ram[8][SB_PREV] = (editmode == EM_START) ? 255 : 0;
		led_ram[8][SB_NEXT] = (editmode == EM_END) ? 255 : 0;
		led_ram[8][SB_RECORD] = recording ? 255 : 0;
		led_ram[8][SB_CLEAR] = 0;
		led_ram[8][SB_PRESET] = (editmode == EM_PRESET) ? 255 : 0;;
		led_ram[8][SB_PARAMSA] = (editmode == EM_PARAMSA) ? 255 : (editmode == EM_PLAY && ui_edit_param < P_LAST && (ui_edit_param % 12) < 6) ? flickery : 0;
		led_ram[8][SB_PARAMSB] = (editmode == EM_PARAMSB) ? 255 : (editmode == EM_PLAY && ui_edit_param < P_LAST && (ui_edit_param % 12) >= 6) ? flickery : 0;
		if (shift_down >= 0 && shift_down < 8)
			led_ram[8][shift_down] = maxi(led_ram[8][shift_down], 128);
	}
//	for (int x = 0; x < 8; ++x) for (int y = 0; y < 9; ++y) led_ram[y][x] = 255; // all leds on test
}
#ifdef DEBUG
//#define NOISETEST
#endif
#ifdef NOISETEST
extern float noisetestl;
extern float noisetestr;
extern float noisetest;
extern s16 noisegate;
#endif

void plinky_frame(void) {
	codec_setheadphonevol(sysparams.headphonevol + 45);

	PumpWebUSB(false);

#ifdef NOISETEST
	static u8 foo;
	foo++;
	if (foo>20)
		DebugLog("%d %d - %d ng %d\r\n",(int)noisetestl,(int)noisetestr,(int)noisetest, noisegate),foo=0;
#endif
	if (0)
		if ((frame & 31) == 0) {
			DebugLog("spi %d - ", spiduration);
			spiduration = 0;
			tc_log(&_tc_budget, "budget");
			tc_log(&_tc_all, "all");
			/*
			tc_log(&_tc_fx, "fx");
			tc_log(&_tc_audio, "audio");
			//		tc_log(&_tc_touch,"touch");
			//		tc_log(&_tc_led,"led");
			tc_log(&_tc_osc, "osc");
			//		tc_log(&_tc_filter,"filt");
			*/
			DebugLog("\r\n");
		}

	audiopeakhistory[audiohistpos] = clampi(audioin_peak / 64, 0, 255);

	if (g_disable_fx) {
		// web usb is up to its tricks :)
		void draw_webusb_ui(int);
		draw_webusb_ui(0);
		HAL_Delay(30);
		return;
	}

	if (editmode == EM_SAMPLE) {
		samplemode_ui();
	}
	else {
		editmode_ui();
	}
	audiohistpos = (audiohistpos + 1) & 31;
	PumpFlashWrites();

	/*
	int sensidx=34;
	int a = (1 << 23) / (finger_raw[sensidx] + 1);
	int b = (1 << 23) / (finger_raw[sensidx+1] + 1);
	a -= (1 << 23) / (finger_max[sensidx] + 1);
	b -= (1 << 23) / (finger_max[sensidx+1] + 1);
	int pressure = a + b;
	int pos = (pressure>1000) ? ((b - a)<<12) / pressure : 0;
	static int prstrata[8];
	static int postrata[8];
	int s = (pressure-1000) / 500; if (s < 0) s = 0; if (s > 7) s = 7;
	prstrata[s] = pressure;
	postrata[s]=pos;
	if (pressure>1000) {
		DebugLog("%5d %5d %5d %5d    ",finger_max[sensidx], finger_max[sensidx+1], finger_raw[sensidx], finger_raw[sensidx+1]);
		for (int i = 0; i < 8; ++i)
		//	DebugLog("%4d %5d  ", prstrata[i], postrata[i]);
		DebugLog("\r\n");
	}
//	HAL_Delay(10);
*/

}

