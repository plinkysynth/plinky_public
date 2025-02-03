
#ifdef EMU
float knobhistory[512];
int khi;
#endif

static inline bool ispow2(int x) {
	return (x & (x - 1)) == 0;
}

int time_finger_went_stable;
int longpress;

u8 fingerstable = 0; // bit set once finger is firmly down
float fingerstablepos[8];// = {}; // where the finger was when it 'went stable'
int fingerstableparamstart[8]; // what the param was at that time
knobsmoother fingerstableparamcur[8]; // what the praam is now, floating point edition
s8 first_finger_rotstep = 0;



int preset_section_from_rotstep(int rotstep) {
	if (rotstep < 0) return -1;
	if (rotstep < 32) return 0;
	if (rotstep < 32+24) return 1;
	if (rotstep < 64) return 2;
	return 3;
}

void init_slice_edit(SampleInfo *s, int fi) {
	recsliceidx = fi;
	fingerstableparamstart[fi] = /*editpitch ? s->notes[fi] * NOTE_FINGER_SCALE : */s->splitpoints[fi];
	knobsmooth_reset(&fingerstableparamcur[fi], fingerstableparamstart[fi]);
}

void on_longpress(int rotstep) {
	if (editmode != EM_PRESET)
		return;

	if (rotstep >= 64 - 8 && rotstep < 64) {
		edit_sample0 = (rotstep & 7);
		EditParamQuant(P_SAMPLE, 0, edit_sample0 + 1);
		memcpy(&ramsample, GetSavedSampleInfo(edit_sample0), sizeof(SampleInfo));
		ramsample1_idx = cur_sample1 = edit_sample0 + 1;
		edit_sample1_pending = 255;
		editmode = EM_SAMPLE;
		// the sample editor goes straight into editing slice pos...
		init_slice_edit(&ramsample, 7);
		
	}
	else {
		copyrequest = rotstep;
	}
}

#define LONGPRESS_THRESH (128 + 32)
void ResetLongPress(void) {
	if (longpress <= 0)
		return;
	/* no longer needed now we do this on 'up'
	if (longpress < LONGPRESS_THRESH && longpress>64) {
		// they cancelled a long press despite having started it
		if (editmode == EM_PRESET) {
			if (first_finger_rotstep < 32)
				SetPreset(copyfrompreset, false);
			else if (first_finger_rotstep < 64 - 8) {
				EditParamQuant(P_SEQPAT, M_BASE,copyfrompattern);
			}

		}
	}*/
	longpress = 0;
	//first_finger_rotstep = -1;
}

void togglearp(void) {
	rampreset.flags ^= FLAGS_ARP;
	ShowMessage(F_32_BOLD, ((rampreset.flags & FLAGS_ARP)) ? "arp on" : "arp off", 0);
	ramtime[GEN_SYS] = millis();
}
void clearlatch(void);
void togglelatch(void) {
	rampreset.flags^= FLAGS_LATCH;
	ShowMessage(F_32_BOLD, ((rampreset.flags & FLAGS_LATCH)) ? "latch on" : "latch off", 0);
	ramtime[GEN_SYS] = millis();
	if (!((rampreset.flags & FLAGS_LATCH)))
		clearlatch();
}
extern int lastencodertime;
extern float encaccel;
int lastencodertime = 0;
static u8 prevencbtn;
static int encbtndowntime = 0;

bool is_finger_an_edit_operation(int fi) { // return true if a given finger is an editing operation not a note
	switch (editmode) {
	case EM_SAMPLE:
		return true;
	case EM_PLAY:
		return (fi == 0 && edit_param < P_LAST);
	case EM_PARAMSA:
	case EM_PARAMSB:
#ifdef NEW_LAYOUT
		return true;
#else
		return (fi == 0 || fi == 7 || fy > 0) || (fy == 0 && fi == 1) || (fy == 0 && fi == 6);
#endif
	case EM_PRESET:
	case EM_START:
	case EM_END:
		return true;
	}
	return false;
}

void finger_editing(int fi, int frame) {

	int ev = encval >> 2;
	if (fi == 0) {
		if (encbtn) encbtndowntime++;
		if (!encbtn) {
			if (encbtndowntime > 500) {
				HAL_Delay(500);
#ifndef EMU
				HAL_NVIC_SystemReset();
#endif
			}
		}
		if (encbtndowntime > 500) {
			ShowMessage(F_20_BOLD, "REBOOT!!", "");
		}
		else if (encbtndowntime > 250) {
			ShowMessage(F_20_BOLD, "REBOOT?", "");
		}
		if ((ev || encbtn || prevencbtn)) {
			// ENCODER EDITING
			lastencodertime = millis();
			encval -= ev << 2;
			int pi = edit_param;
			if (pi >= P_LAST) pi = last_edit_param;
			if ((pi < P_LAST) && (editmode == EM_PLAY || editmode == EM_PARAMSA || editmode == EM_PARAMSB)) {
				int cur = GetParam(pi, edit_mod);
				int prev = cur;
				int f = param_flags[pi];
				bool issigned = f & FLAG_SIGNED;
				issigned |= (edit_mod != M_BASE);
				if ((f & FLAG_MASK) && edit_mod == 0) {
					int maxi = f & FLAG_MASK;
					cur += ev * (FULL / maxi);
				}
				else {
					int ev_sens = 1;
					if (pi == P_HEADPHONE) ev_sens = 4;
					cur += (int)floorf(0.5f + ev * ev_sens * maxf(1.f, encaccel * encaccel));
#ifdef DEBUG
					DebugLog("%d\r\n", (int)(maxf(1.f, encaccel * encaccel) * 100));
#endif
				}
				cur = clampi(cur, issigned ? -FULL : 0, FULL);
				if (encbtndowntime > 10) {
					if (encbtndowntime >= 50) {
						ShowMessage(F_20_BOLD, I_CROSS "Mod Cleared", "");
						if (encbtndowntime == 50) {
							for (int mod=1;mod<M_LAST;++mod)
								EditParamNoQuant(pi, mod, (s16)0);
						}
					}
					else {
						ShowMessage(F_20_BOLD, I_CROSS "Clear Mod?", "");
					}
				}
				if (!encbtn && prevencbtn && encbtndowntime<=50) {
					int deflt = (edit_mod) ? 0 : init_params.params[pi][0];
					if (deflt != 0) {
						if (cur != deflt) cur = deflt; else cur = 0;
					}
					else {
						if (cur != 0) cur = 0; else cur = FULL;
					}
				}
				if (encbtn && !prevencbtn) {
					if (pi == P_ARPONOFF) {
						togglearp();
					}
					else if (pi == P_LATCHONOFF) {
						togglelatch();
					}
					else {
						// used to do clear-param here, but now we do it on RELEASE!
					}
				}
				if (prev != cur) {
					EditParamNoQuant(pi, edit_mod, (s16)cur);
				}
			}
			else if (editmode == EM_SAMPLE && recsliceidx >= 0 && recsliceidx < 8) {
				SampleInfo* s = getrecsample();
				if (s->pitched) {
					int newnote = clampi(s->notes[recsliceidx] + ev, 0, 96);
					if (newnote != s->notes[recsliceidx])
					{
						s->notes[recsliceidx] = newnote;
						ramtime[GEN_SAMPLE] = millis();
					}
				}
				else {
					float smin = recsliceidx ? s->splitpoints[recsliceidx - 1] + 1024.f : 0.f;
					float smax = (recsliceidx < 7) ? s->splitpoints[recsliceidx + 1] - 1024.f : s->samplelen;

					float sp = clampf(s->splitpoints[recsliceidx] + ev * 512, smin, smax);
					if (sp != s->splitpoints[recsliceidx]) {
						s->splitpoints[recsliceidx] = sp;
						ramtime[GEN_SAMPLE] = millis();
					}
				}
			} // sample mode
		} // en/encbtn
		if (!encbtn)
			encbtndowntime = 0;
		prevencbtn = encbtn;
	}
	Finger* uif = &fingers_ui_time[fi][frame];
	Finger* uif_prev = &fingers_ui_time[fi][(frame+5)&7];
	///////////////////////////////////////// FINGER BASED EDITING
	bool trig = false;
	int bit = 1 << fi;
	bool pressurestable = abs(uif_prev->pressure-uif->pressure)<200;
	bool posstable = abs(uif_prev->pos - uif->pos) < 32;
	if (uif->pressure > 100) {
		//int fy = uif->pos >> 8;
		//if (fi == 7) EmuDebugLog("pressure delta %d, pos delta %d, y=%d\n", abs(uif_prev->pressure - uif->pressure), abs(uif_prev->pos - uif->pos), fy);
		// finger down
		bool isediting = is_finger_an_edit_operation(fi);
		
		if (isediting)
			fingerediting |= bit;
		if (posstable && pressurestable) {
			int fyi = uif->pos >> 8;
			if (fingerstable == 0) {
				first_finger_rotstep = fyi + fi * 8;
				time_finger_went_stable = millis();
			}
			if ((fingerstable & bit) == 0) {
				trig = true;
				fingerstable |= bit;
				fingerstablepos[fi] = uif->pos;
			//	if (fi == 7) EmuDebugLog("finger stable y = %d %d\n", uif->pos >> 8, uif->pos);
			}
			
		}
		//bool increasing_pressure = latestf->pressure > previousf->pressure;
	}
	else {
		// finger up!
		if (uif->pressure<1 && ( fingerstable & bit)) {
			//onfingerstableup((fingerediting&bit),fi, latestf, previousf);
			fingerstable &= ~bit;
			if (editmode == EM_PRESET) {
				int firstsection = preset_section_from_rotstep(first_finger_rotstep);
				
				switch (firstsection) {
				case 0:
					if (edit_preset_pending == prev_preset_pending || !isplaying()) {
						if (edit_preset_pending != 255) SetPreset(edit_preset_pending, false);
						edit_preset_pending = 255;
					}
					break;
				case 1:
					if (edit_pattern_pending == prev_pattern_pending || !isplaying()) {
						if (edit_pattern_pending != 255) EditParamQuant(P_SEQPAT, M_BASE, edit_pattern_pending);
						edit_pattern_pending = 255;
					}
					break;
				case 2:
					if (edit_sample1_pending == prev_sample1_pending || !isplaying()) {
						//EmuDebugLog("set edit_sample1_pending to %d\n", edit_sample1_pending);
						if (edit_sample1_pending != 255) EditParamQuant(P_SAMPLE, M_BASE, edit_sample1_pending);
						edit_sample1_pending = 255;
					}
					break;
				}

			}
		}
		fingerediting &= ~bit;
	}
	//		fingerpos_smoothed[fi] += (latestf->pos - fingerpos_smoothed[fi]) * fingerpos_k[fi];

	if ((fingerediting & bit) && (fingerstable & bit)) {
		// finger is down over some editing operation!
		int fyi = uif->pos >> 8;
		if (fyi >= 0 && fyi < 8)
			touched_main_area = true;
		int step = fyi * 8 + fi;
		int rotstep = fyi + fi * 8;
		u8 loopstart_step = (rampreset.loopstart_step_no_offset + step_offset) & 63;
		bool inloop = ((step - loopstart_step) & 63) < rampreset.looplen_step;
		int old = edit_param;


		switch (editmode) {
		case EM_SAMPLE: {
			SampleInfo* s = getrecsample();
			if (shift_down < 0 && trig) {
				if (enable_audio == EA_MONITOR_LEVEL) {
					enable_audio = EA_ARMED;
				}
				else if (enable_audio == EA_ARMED) {
					recording_trigger();
				}
				else if (enable_audio == EA_RECORDING && s->samplelen >= BLOCK_SAMPLES) {
					if (recsliceidx < 7) {
						recsliceidx++;
						s->splitpoints[recsliceidx] = s->samplelen - BLOCK_SAMPLES;
						EmuDebugLog("SET SPLITPOINT %d to %d\n", recsliceidx, s->splitpoints[recsliceidx]);
					}
					else {
						recording_stop();
					}
				}
			}
			if (enable_audio == EA_RECORDING && s->samplelen >= BLOCK_SAMPLES && longpress > 32) {
				// long press to stop recording?
				if (recsliceidx > 0) recsliceidx--;
				recording_stop();
			}
			if (enable_audio == EA_PLAY) {
				if (trig) {
					init_slice_edit(s, fi);
				}
				if (fingerstable & bit) {
					float finger_offset = (uif->pos - fingerstablepos[fi]);
					finger_offset = deadzone(finger_offset, 32.f);
					//finger_offset = fabsf(finger_offset) * finger_offset * (1.f / 32.f);
					float target = fingerstableparamstart[fi] - finger_offset * (32000.f / 2048.f);
					float cur = knobsmooth_update_knob(&fingerstableparamcur[fi], target, 32000.f);
					/*bool editpitch = false; //  fingerstableparamidx[fi];
					if (editpitch) {
						cur = clampf(cur, 0.f, 96 * NOTE_FINGER_SCALE);
						int newnote = (int)(cur * (1.f / NOTE_FINGER_SCALE) + 0.5f);
						if (s->notes[fi] != newnote) {
							s->notes[fi] = newnote;
							EmuDebugLog("FINGER SET NOTE %d to %d\n", fi, newnote);
							ramtime[GEN_SAMPLE] = millis();
						}
					}
					else */
					{
						float smin = fi ? s->splitpoints[fi - 1]+1024.f : 0.f;
						float smax = (fi < 7) ? s->splitpoints[fi + 1]-1024.f : s->samplelen;
						if (smin < 0.f) smin = 0.f;
						if (smax > s->samplelen) smax = s->samplelen;
						cur = clampf(cur, smin, smax);
						if (s->splitpoints[fi] != cur) {
							s->splitpoints[fi] = cur;
							//EmuDebugLog("FINGER SET SPLITPOINT %d to %d\n", fi, s->splitpoints[fi]);
							ramtime[GEN_SAMPLE] = millis();
						}
					}
				}
			}
			break;
		} // sample
		case EM_PARAMSA:
		case EM_PARAMSB:
		case EM_PLAY:
			if (fi > 0 && fi < 7 && fyi >= 0 && fyi < 8) {
				// Reset mod source when user selects an edit param. Otherwise, the
				// previous mod source stays selected, and the user would then have
				// to select the base mod source pad before editing the newly selected
				// param.
				edit_mod = M_BASE;

#ifdef NEW_LAYOUT
				edit_param = (fyi) * 12 + (fi - 1);
#else
				edit_param = (fyi - 1) * 12 + (fi - 1);
#endif
				if (editmode == EM_PARAMSB) edit_param += 6;
				if (trig) {
#ifndef NEW_LAYOUT
					if (fi == 1 && fyi == 0) {
						togglearp();
					}
					else if (fi == 6 && fyi == 0) {
						togglelatch();
					}
#else
					if (edit_param == P_ARPONOFF) {
						togglearp();
					}
					else if (edit_param == P_LATCHONOFF) {
						togglelatch();
					}
#endif
				} // trig
				if (edit_param<0 || edit_param>P_LAST)
					edit_param = P_LAST;
				if (edit_param != old && edit_param < P_LAST) {
					ShowMessage(F_20_BOLD, paramnames[edit_param], pagenames[edit_param / 6]);
				}
				static int firsttaptime;
				static int lasttaptime;

				if (trig) {
					if (edit_param == P_TEMPO) {
						if (ticks() - lasttaptime > 1000)
							tapcount = 0;
						lasttaptime = ticks();
						if (!tapcount)
							firsttaptime = ticks();
						tapcount++;

						if (tapcount > 1) { // tap tempo!
							float taps_per_minute = (32000.f * (tapcount - 1) * 60.f) / ((ticks() - firsttaptime) * BLOCK_SAMPLES);
							//DebugLog("%d - %0.1f\n", tapcount, taps_per_minute);
							bpm10x = clampi((int)(taps_per_minute * 10.f + 0.5f), 300, 2400);
							EditParamNoQuant(P_TEMPO, 0, ((bpm10x - 1200) * FULL) / 1200);
						}
					}
					else
						tapcount = 0;
				}

			}
			if (edit_param < P_LAST && (edit_param != old || trig)) {
				fingerstableparamstart[fi] = GetParam(edit_param, edit_mod);
				knobsmooth_reset(&fingerstableparamcur[fi], fingerstableparamstart[fi]);
			}
			if (fi == 7 && fyi >= 0 && fyi < 8 && fyi != edit_mod) {
				edit_mod = fyi;
				
			}
			if (fi == 0 && edit_param < P_LAST && pressurestable) {
				int pi = edit_param;
				bool issigned = param_flags[pi] & FLAG_SIGNED;
				issigned |= (edit_mod != M_BASE);
				float target = clampf((2048 - 256 - uif->pos) * (FULL / (2048.f - 512.f)), 0.f, FULL);
				if (issigned)
					target = target * 2 - FULL;
				float cur = knobsmooth_update_knob(&fingerstableparamcur[fi], target, FULL);
				cur = clampf(cur, (issigned) ? -FULL - 0.1f : 0.f, FULL + 0.1f);
				if (cur < 0.f && fingerstableparamstart[fi]>0)
					cur = 0.f;
				if (cur > 0.f && fingerstableparamstart[fi] < 0)
					cur = 0.f;
				bool notchat50 = (pi == P_SMP_RATE || pi == P_SMP_TIME);
				if (notchat50) {
					if (cur < HALF && fingerstableparamstart[fi]>HALF)
						cur = HALF;
					if (cur > HALF && fingerstableparamstart[fi] < HALF)
						cur = HALF;
					if (cur < -HALF && fingerstableparamstart[fi]>-HALF)
						cur = -HALF;
					if (cur > -HALF && fingerstableparamstart[fi] < -HALF)
						cur = -HALF;

				}
#ifdef EMU
				knobhistory[khi] = cur * (1.f / FULL);
				khi = (khi + 1) & 511;
#endif
				EditParamNoQuant(pi, edit_mod, (s16)cur);
			}
			break;
		case EM_START:
			// if you click inside the loop, just set pos
			if (inloop) {
				if (trig) { // need the trig otherwise when we move the loop, this code fires
					set_cur_step(step, false);
				}
			}
			else {
				if ((trig && pending_loopstart_step == step) || !isplaying()) {
					if (loopstart_step != step) {
						int newstep = cur_step - loopstart_step + step; // move curstep into new loop
						loopstart_step = step;
						rampreset.loopstart_step_no_offset = (step - step_offset) & 63;
						ramtime[GEN_PRESET] = millis();
						set_cur_step(newstep, false); // reset our cur step, based on the new loop
					}
					pending_loopstart_step = 255;
				}
				else
					pending_loopstart_step = step;

			}
			break;
		case EM_END:
			// set the end of the loop
		{
			u8 old = rampreset.looplen_step;
			rampreset.looplen_step = (step - loopstart_step) + 1;
			if (rampreset.looplen_step <= 0)
				rampreset.looplen_step += 64;
			if (rampreset.looplen_step != old)
				ramtime[GEN_PRESET] = millis();
			set_cur_step(cur_step, false); // reset our cur step, based on the new loop
		} break;
		case EM_PRESET: {
			int section = preset_section_from_rotstep(rotstep);
			int firstsection = preset_section_from_rotstep(first_finger_rotstep);
			last_preset_selection_rotstep = first_finger_rotstep; // remember what they chose
			if (section == firstsection) switch (section) {
			case 0:
				//if (trig) 
				{
					copyfrompreset = sysparams.curpreset;
					prev_preset_pending = edit_preset_pending;
				}
				edit_preset_pending = rotstep;
				break;
			case 1: {
				//if (trig) 
				{
					copyfrompattern = cur_pattern;
					prev_pattern_pending = edit_pattern_pending;
				}
				edit_pattern_pending = rotstep - 32;
				break; }
			case 2:
			{
				int ns = rotstep - 56 + 1;
				if (trig) {
					EmuDebugLog("sample trig on %d\n", ns);
					prev_sample1_pending = edit_sample1_pending;
					copyfromsample = cur_sample1;
					if (ns == copyfromsample)
						ns = 0;
					edit_sample1_pending = ns;
				}
				break;
			}
			} // section switch
			break;
		} // preset
		} // mode
	}
	
	if (fi == 7) {
		if (fingerstable && ispow2(fingerstable)) {
			// exactly one finger down
			int fi = 0;
			for (; fi < 8; ++fi) if (fingerstable & (1 << fi)) break;
			int fyi = (touch_ui_getlatest(fi)->pos >> 8);
			int rotstep = fi * 8 + fyi;
			if (rotstep == first_finger_rotstep) {
				longpress+=2;
				if (longpress == LONGPRESS_THRESH) {
					on_longpress(rotstep);
				}
			} 
			else
				ResetLongPress();
		}
		else {
			// not exactly one finger down
			ResetLongPress();
		}

	}

}




