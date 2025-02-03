#ifdef HALF_FLASH
const static int calib_sector = -1;
#else
const static int calib_sector = 255;
#endif
const static uint64_t MAGIC = 0xf00dcafe473ff02a; // bump on change
bool flash_writecalib(int which);
int flash_readcalib(void) {
#ifdef EMU
	openflash();
#endif
#if defined EMU // WASM?
#ifndef CALIB_TEST
	if (1) {
		// fake calib results
		for (int i = 0; i < 18; ++i) {
			for (int j = 0; j < 8; ++j)
				calibresults[i].pressure[j] = 8000,
				calibresults[i].pos[j] = (512 * j - 1792) * (((i % 9) == 8) ? -1 : 1);
		}
		return 3;
	}
#endif
#endif
	volatile uint64_t *flash = (volatile uint64_t*) (FLASH_ADDR_256 + calib_sector * 2048);
	int ver = 0, ok=0;
	if (flash[0] == MAGIC && flash[255] == ~MAGIC) ver = 2;
	if (ver==0) {
		DebugLog("no calibration found in flash\r\n");
		return 0;
	}
	volatile uint64_t *s = flash + 1;
	if(*s!=~(uint64_t)(0)) {
		ok|=1;
		memcpy(calibresults, (uint64_t*) s, sizeof(calibresults));
	}
	s += sizeof(calibresults) / 8;
	if (*s!=~(uint64_t)(0)) {
		ok|=2;
		memcpy(cvcalib, (int64_t*)s, sizeof(cvcalib));
	}
	s += sizeof(cvcalib) / 8;
	return ok;
}

bool flash_writecalib(int which) {
	HAL_FLASH_Unlock();
	int rv = flash_erase_page(calib_sector);
	if (rv == 0) {
		uint64_t* flash = (uint64_t*)(FLASH_ADDR_256 + calib_sector * 2048);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(size_t)flash, MAGIC);
		uint64_t* d = flash + 1;
		if (which&1)
			flash_program_block(d, calibresults, sizeof(calibresults));
		d+=(sizeof(calibresults)+7)/8;
		if (which&2)
			flash_program_block(d, cvcalib, sizeof(cvcalib));
		d+=(sizeof(cvcalib)+7)/8;
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(size_t)(flash + 255), ~MAGIC);
	}
	HAL_FLASH_Lock();
	return 0;
}

extern s8 enable_audio;

static inline float deadzone(float f, float zone) {
	if (f<zone && f>-zone) return 0.f;
	if (f > 0.f) f -= zone; else f += zone;
	return f;// *f* (1.f / 2048.f);
}

#ifdef EMU
static inline int getgatesense(void) {
	return emugatesense;
}
static inline int getpitchsense(void) {
	return emupitchsense;
}
#else
static inline int getgatesense(void) {
	return HAL_GPIO_ReadPin(SENSE1_GPIO_Port, SENSE1_Pin) == GPIO_PIN_RESET;
}
static inline int getpitchsense(void) {
	return HAL_GPIO_ReadPin(SENSE2_GPIO_Port, SENSE2_Pin) == GPIO_PIN_RESET;
}
#endif

void cv_calib(void) {
#ifdef WASM
	return;
#endif
	enable_audio = EA_OFF;
	clear();
	int topscroll = 128;
	const char* topline = "unplug all inputs"
#ifndef NEW_LAYOUT
		" except plug gate out to gate in"
#endif
		". use left 4 columns to adjust pitch cv outputs. plug pitch lo output to pitch input when done.";
	int toplinew = strwidth(F_16, topline);
	const char* const botlines[5] = { "plug gate out->in", "Pitch lo=0v/C0", "Pitch lo=2v/C2", "Pitch hi=0v/C0", "Pitch hi=2v/C2" };
	u8 ff = finger_frame_ui;
	float adcavgs[6][2];
	for (int i = 0; i < 6; ++i)
		adcavgs[i][0] = -1.f;
	int curx = -1;
	float downpos[4]={}, downval[4]={};
	float cvout[4] = {
		cvcalib[8].bias, cvcalib[8].bias + cvcalib[8].scale * 2048.f * 24.f,
		cvcalib[9].bias, cvcalib[9].bias + cvcalib[9].scale * 2048.f * 24.f,
	};
	SetOutputCVPitchLo((int)cvout[0], false);
	SetOutputCVPitchHi((int)cvout[2], false);
	u8 curlo = 0;
	u8 curhi = 2;
	bool prevprevpitchsense=true;
	bool prevpitchsense=true;
	while (1) {
		clear();
		drawstr_noright(topscroll, 0, F_16, topline);
		bool gateok = getgatesense();
#ifdef NEW_LAYOUT
		gateok = !gateok; // in new layout, theres a bleed resistor so no need for gate - in fact, we dont want it
#endif
		drawstr(0, 18, F_12_BOLD, (curx<0 && gateok) ? "pitch out>in when done" : botlines[curx+1]);
		if (curx >= 0)
			fdrawstr(-128, 24, F_8, "(%d)", (int)cvout[curx]);
		oled_flip(vrambuf);
		topscroll-=2;
		if (topscroll < -toplinew)
			topscroll = 128;
		while (finger_frame_ui == ff); // wait for new touch data
		ff = finger_frame_ui;
		int pitchsense = getpitchsense();
		if (pitchsense && !prevprevpitchsense && gateok)
			break;
		prevprevpitchsense=prevpitchsense;
		prevpitchsense=pitchsense;

		// calibrate the 0 point for the inputs
		for (int i = 0; i < 6; ++i) {
			int tot = 0;
			for (int j = 0; j < ADC_SAMPLES; ++j)
				tot += adcbuf[j * ADC_CHANS + i];
			tot /= ADC_SAMPLES;
			if (adcavgs[i][0] < 0)
				adcavgs[i][0] = adcavgs[i][1] = tot;
			adcavgs[i][0] += (tot - adcavgs[i][0]) * 0.05f;
			adcavgs[i][1] += (adcavgs[i][0] - adcavgs[i][1]) * 0.05f;
		}
		for (int x = 0; x < 4; ++x) {
			Finger* f = touch_ui_getlatest(x);
			Finger* pf = touch_ui_getprev(x);
			if (f->pressure >= 500) {
				if (pf->pressure < 500) {
					downpos[x] = f->pos;
					downval[x] = cvout[x];
					curx = x;
				}
				float delta = deadzone(f->pos - downpos[x], 64.f);
//				delta=(delta*delta)>>12;
				cvout[x] += (clampf(downval[x] + delta*0.25f, 0.f, 65535.f) - cvout[x])*0.1f;
				if (x < 2)
					curlo = x;
				else
					curhi = x;
			}
		}
		SetOutputCVPitchLo((int)cvout[curlo], false);
		SetOutputCVPitchHi((int)cvout[curhi], false);
		AdvanceCVOut();

		// update the leds, innit
		for (int fi = 0; fi < 9; ++fi) {
			for (int y = 0; y < 8; ++y) {
				int k = (fi<4)?(triangle(y*64-(int)cvout[fi]/4)/4):128;
				led_ram[fi][y] = led_gamma(((fi==curx)?255:128)-k);
			}
		}

	}
	// zero is now nicely set
	for (int i = 0; i < 6; ++i) {
		cvcalib[i].bias = adcavgs[i][1];
		cvcalib[i].scale = 0.2f / -6548.1f;
		DebugLog("adc zero point %d - %d\r\n", i, (int)adcavgs[i][1]);
	}
	cvcalib[6].bias=32000.f; // hw seems to skew towards 0 slightly...
	cvcalib[6].scale = 1.05f / -32768.f;
	cvcalib[7].bias=32000.f;
	cvcalib[7].scale= 1.05f / -32768.f;

	// output calib is now nicely set
	cvcalib[8].bias = cvout[0];
	cvcalib[8].scale = (cvout[1] - cvout[0]) * 1.f / (2048.f*24.f);
	cvcalib[9].bias = cvout[2];
	cvcalib[9].scale = (cvout[3] - cvout[2]) * 1.f / (2048.f * 24.f);
	for (int i = 0; i < 4; ++i)
		DebugLog("selected dac value %d - %d\r\n", i, (int)cvout[i]);
	DebugLog("dac pitch lo zero point %d, step*1000 %d\r\n", (int)cvcalib[8].bias, (int)(cvcalib[8].scale*1000.f));
	DebugLog("dac pitch hi zero point %d, step*1000 %d\r\n", (int)cvcalib[9].bias, (int)(cvcalib[9].scale * 1000.f));

	// use it to calibrate 

	clear();
	drawstr(0, 4, F_12, "waiting for pitch\nloopback cable");
	oled_flip(vrambuf);
	HAL_Delay(1000);
	// wait for them to plug the other end in
	while (1) {
		int tots[2] = { 0 };
		for (int hilo = 0; hilo < 2; ++hilo) {
			SetOutputCVPitchLo((int)cvout[hilo], false);
			SetOutputCVPitchHi((int)cvout[hilo + 2], false);
			HAL_Delay(50);
			int tot = 0;
			for (int j = 0; j < ADC_SAMPLES; ++j)
				tot += adcbuf[j * ADC_CHANS + ADC_PITCH];
			tot /= ADC_SAMPLES;
			tots[hilo] = tot;
		}
		if (abs(tots[0]- tots[1]) > 5000)
			break;
	}
	clear();
	drawstr(0,4,F_24_BOLD, "just a mo...");
	oled_flip(vrambuf);
	HAL_Delay(1000);
	for (int hilo = 0; hilo < 2; ++hilo) {
		SetOutputCVPitchLo((int)cvout[hilo], false);
		SetOutputCVPitchHi((int)cvout[hilo+2], false);
		HAL_Delay(50);
		int tot = 0;
		for (int iter = 0; iter < 256; ++iter) {
			HAL_Delay(2);
			for (int j = 0; j < ADC_SAMPLES; ++j)
				tot += adcbuf[j * ADC_CHANS + ADC_PITCH];
		}
		tot /= ADC_SAMPLES * 256;
		DebugLog("pitch adc for hilo=%d is %d\r\n", hilo, tot);
		if (hilo == 0)
			cvcalib[ADC_PITCH].bias = tot;
		else
			cvcalib[ADC_PITCH].scale = 2.f / (minf(-0.00001f,tot - cvcalib[ADC_PITCH].bias));
	}
	clear();
	drawstr(0, 0, F_16_BOLD, "Done!");
	drawstr(0, 16, F_12_BOLD, "Unplug pitch cable!");
	oled_flip(vrambuf);
	while (getpitchsense()) {
		HAL_Delay(1);
	}
}

void reflash(void);
extern volatile u8 gotclkin;

void led_test(void) {
	enable_audio = EA_PASSTHRU;
	for (int y = 0; y < 9; ++y) for (int x = 0; x < 8; ++x)
		led_ram[y][x] = 255;
	u16 tri = 128;
	int encoder_down_count = -1;
	while (1) {
		clear();
		if (encbtn) {
			if (encoder_down_count >= 0)
				encoder_down_count++;
		}
		else {
			if (encoder_down_count > 2) {
				HAL_Delay(20);
				return;
			}
			encoder_down_count = 0;
		}
		if (encoder_down_count > 100)
			reflash();
		fdrawstr(0, 2, F_12, "TEST %d %d %d %d %02x", adcbuf[0] / 256, adcbuf[1] / 256, adcbuf[2] / 256, adcbuf[3] / 256, gotclkin);
		fdrawstr(0, 18, F_12, "%d %d %d %d %d %d", adcbuf[4] / 256, adcbuf[5] / 256, adcbuf[6] / 256, adcbuf[7] / 256, encval >> 2, encbtn);
		oled_flip(vrambuf);
		HAL_Delay(20);
		for (int srcidx = 0; srcidx < 9; ++srcidx) {
			int a = finger_cap(srcidx * 2);
			int b = finger_cap(srcidx * 2 + 1);
			int amin = finger_mincap(srcidx * 2);
			int bmin = finger_mincap(srcidx * 2 + 1);
			int rawpressure = finger_rawpressure(a - amin, b - bmin);
			int rawpos = finger_rawpos(a, b);
			if (rawpressure > 300) {
				DebugLog("f %d - a=%4d b=%4d amin=%4d bmin=%4d pos=%4d pr=%4d   \r\n", srcidx, a, b, amin, bmin, rawpos, rawpressure);
			}
		}
		tri += 256;
		SetOutputCVTrigger((tri < 16384) ? 65535 : 0);
		SetOutputCVClk((tri < (16384 + 32768)) ? 65535 : 0);
		SetOutputCVPressure(tri);
		SetOutputCVGate((tri * 2) & 65535);
		SetOutputCVPitchLo(tri, false);
		SetOutputCVPitchHi((tri * 2) & 65535, false);
	}
}

void calib(void) {
again:
#ifdef WASM
	return;
#endif



	enable_audio=EA_OFF;
	touch_reset_calib();
	
	HAL_Delay(20);
	CalibProgress* state = GetCalibProgress(0);
	memset(state, 0, sizeof(CalibProgress) * 18);
	int prevrawpressure[18] = { 0 };
	s8 curstep[9] = { 7,7,7,7,7,7,7,7,7 };
	int done = false;
	u8 ff = finger_frame_ui;
	u8 refreshscreen = 0;
	char helptext[64] = "slowly/evenly press lit pads.\ntake care, be accurate!";
	bool blink = false;
	int errors = 0;
	while (!done) {

		if (!refreshscreen) {
			refreshscreen = 16;
			blink = !blink;
			clear();
			fdrawstr(0, 0, F_16, "Calibration%c",blink?'!':' ');
			drawstr(0, 16, F_8, helptext);
			if (errors)
				invertrectangle(0, 0, 128, 32);
			oled_flip(vrambuf);
		}
		else {
			refreshscreen--;
		}

		if (encbtn) {
			led_test();
			goto again;
		}


		while (finger_frame_ui == ff); // wait for new touch data
		ff = finger_frame_ui;
		// update the 18 calibration entries for the current step
		done = 0;
		int readymask=0;
		for (int si = 0; si < 18; ++si) {
			int a = finger_cap(si*2);
			int b = finger_cap(si*2 + 1);
			int amin = finger_mincap(si * 2);
			int bmin = finger_mincap(si * 2 + 1);
			int amax = finger_maxcap(si * 2);
			int bmax = finger_maxcap(si * 2 + 1);
			int rawpressure = finger_rawpressure(a-amin,b-bmin);
			int prevrawp = prevrawpressure[si];
			int rawpos = finger_rawpos(a,b);
			int step = curstep[si % 9];

			int pressureband = rawpressure / 20;
			if (step >=0 && step<8 && rawpressure > 1200 && rawpressure > prevrawp - pressureband/2 && rawpressure < prevrawp + pressureband) {
				// pressure is quite stable
				float w = (rawpressure - 1200.f) / 1000.f;
				float change = abs(prevrawp - rawpressure)*(1.f/250.f);
				w *= maxf(0.f,1.f - change);
				if(w>1.f) w=1.f;
				w *= w;
				const static float LEAK = 0.90f;
				state[si].weight[step] *= LEAK;
				state[si].pos[step] *= LEAK;
				state[si].pressure[step] *= LEAK;
				state[si].weight[step] += w;
				state[si].pos[step] += rawpos * w;
				state[si].pressure[step] += rawpressure * w;

				if (0) if (si<9) DebugLog("finger %d step %d pos %4d %4d pressure %5d %5d weight %3d %d      \r\n", si, step,
								(int)(state[si].pos[step] / state[si].weight[step]),
								(int)(state[si + 9].pos[step] / state[si + 9].weight[step]),
								(int)(state[si].pressure[step] / state[si].weight[step]),
								(int)(state[si + 9].pressure[step] / state[si + 9].weight[step]),
										(int)state[si].weight[step], (int)state[si+9].weight[step]
							);
			}
			int ti = si + 9;
			bool ready=si < 9 && step<8 && step>=0 && state[si].weight[step]>4.f && state[ti].weight[step]>4.f;
			if (ready) {
				if (rawpressure < 900) {
					// move on!
					calibresults[si].pressure[step] = state[si].pressure[step] / state[si].weight[step];
					calibresults[si].pos[step] = state[si].pos[step] / state[si].weight[step];
					calibresults[ti].pressure[step] = state[ti].pressure[step] / state[ti].weight[step];
					calibresults[ti].pos[step] = state[ti].pos[step] / state[ti].weight[step];
					if (step <= 4) {
						errors &= ~(1 << si);
						if (amax - amin < 1000) {
							snprintf(helptext, sizeof(helptext), "!pad %d upper not conn\ncheck soldering", si + 1);
							errors |= (1 << si);
						}
						else if (bmax - bmin < 1000) {
							snprintf(helptext, sizeof(helptext), "!pad %d lower not conn\ncheck soldering", si + 1);
							errors |= (1 << si);
						}
						else if (abs(calibresults[si].pos[step] - calibresults[si].pos[7]) < 300) {
							snprintf(helptext, sizeof(helptext), "!pad %d shorted?\ncheck soldering", si + 1);
							errors |= (1 << si);
						}
					}
					DebugLog("\n");
					curstep[si]--;
				}
				else
					readymask |= 1 << si; // flash the next finger if we want them to move on
			}
			if (step < 0)
				done++;
			prevrawpressure[si] = rawpressure;
		}
		if (done < 18)
			done = 0;
		int flash=triangle(millis());
		for (int fi = 0; fi < 9; ++fi) {
			int ready = readymask & (1 << fi);
			bool err = (errors & (1 << fi));
			for (int x = 0; x < 8; ++x) {
				int k = 0;
				if (x == curstep[fi])
					k = ready ? flash : 255 - state[fi].weight[x] * 12.f;
				if (err) 
					k = maxi(k, flash / 2);
				led_ram[fi][x] = led_gamma(k);
			}
		}
	} // calibration loop
}
