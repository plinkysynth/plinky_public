u16 finger_raw[36]; // raw value back from stm
u16 finger_min[36]; // lowest value seen (zero point)
u16 finger_max[36]; // highest value seen (zero point)

static inline int finger_cap(int sensoridx) {
	return finger_raw[sensoridx];
}
static inline int finger_mincap(int sensoridx) {
	return finger_min[sensoridx];
}
static inline int finger_maxcap(int sensoridx) {
	return finger_max[sensoridx];
}

typedef struct CalibResult {
	u16 pressure[8];
	s16 pos[8];
} CalibResult;
typedef struct CalibProgress {
	float weight[8];
	float pos[8];
	float pressure[8];
} CalibProgress;
static inline CalibProgress* GetCalibProgress(int sensoridx) {
	CalibProgress* p = (CalibProgress*)delaybuf;
	return p + sensoridx;
}
CalibResult calibresults[18];

typedef union Finger {
	s32 x;
	struct {
		s16 pressure;
		s16 pos : 15;
		s16 written : 1;
	};
} Finger;
Finger fingers_ui_time[9][8]; // 8 frames for 9 fingers
Finger fingers_synth_time[8][8]; // 8 frames for 8 fingers
Finger fingers_synth_sorted[8][8];
u8 finger_state;
u8 finger_step;
u16 finger_stepmask;
volatile u8 finger_frame_ui;
volatile u8 finger_frame_synth;
u8 finger_ui_done_this_frame;
u8 waitcounter;
u8 finalwait;
u8 fingerediting = 0; // finger is over some kind of non-musical edit control
u8 prevfingerediting = 0;

typedef struct euclid_state {
	int trigcount;
	bool did_a_retrig;
	bool supress;
	
} euclid_state;

euclid_state arp_rhythm;
euclid_state seq_rhythm;


static inline u8 touch_ui_writingframe(void) { return (finger_frame_ui) & 7; }
static inline u8 touch_ui_frame(void) { return (finger_frame_ui-1)&7; }
static inline u8 touch_ui_prevframe(void) { return (finger_frame_ui -2) & 7; }

static inline u8 touch_synth_writingframe(void) { return (finger_frame_synth) & 7; }
static inline u8 touch_synth_frame(void) { return (finger_frame_synth - 1) & 7; }
static inline u8 touch_synth_prevframe(void) { return (finger_frame_synth - 2) & 7; }


static inline Finger* touch_synth_getlatest(int finger) { return &fingers_synth_time[finger][touch_synth_frame()]; }
static inline Finger* touch_synth_getprev(int finger) { return &fingers_synth_time[finger][touch_synth_prevframe()]; }
static inline Finger* touch_synth_getwriting(int finger) { return &fingers_synth_time[finger][touch_synth_writingframe()]; }
static inline Finger* touch_ui_getwriting(int finger) { return &fingers_ui_time[finger][touch_ui_writingframe()]; }
static inline Finger* touch_ui_getlatest(int finger) { return &fingers_ui_time[finger][touch_ui_frame()]; }
static inline Finger* touch_ui_getprev(int finger) { return &fingers_ui_time[finger][touch_ui_prevframe()]; }

#define SWAP(a,b) if (a>b) { int t=a; a=b; b=t; }
void sort8(int *dst, const int *src) {
	int a0=src[0],a1=src[1],a2=src[2],a3=src[3],a4=src[4],a5=src[5],a6=src[6],a7=src[7];
	SWAP(a0,a1);SWAP(a2,a3);SWAP(a4,a5);SWAP(a6,a7);
	SWAP(a0,a2);SWAP(a1,a3);SWAP(a4,a6);SWAP(a5,a7);
	SWAP(a1,a2);SWAP(a5,a6);SWAP(a0,a4);SWAP(a3,a7);
	SWAP(a1,a5);SWAP(a2,a6);
	SWAP(a1,a4);SWAP(a3,a6);
	SWAP(a2,a4);SWAP(a3,a5);
	SWAP(a3,a4);
	dst[0]=a0; dst[1]=a1; dst[2]=a2; dst[3]=a3; dst[4]=a4; dst[5]=a5; dst[6]=a6; dst[7]=a7;
}
#undef SWAP

void touch_reset_calib(void) {
//	memset(finger_max,0,sizeof(finger_max));
	memset(finger_min,-1, sizeof(finger_min));
	memset(finger_raw,0,sizeof(finger_raw));
	memset(calibresults, 0, sizeof(calibresults));
}



void check_curstep(void) { // enforces invariants 
	if (rampreset.looplen_step <= 0 || rampreset.looplen_step > 64)
		rampreset.looplen_step = 64;
	rampreset.loopstart_step_no_offset &= 63;
	u8 loopstart_step = (rampreset.loopstart_step_no_offset + step_offset) & 63;
	cur_step= (cur_step- loopstart_step) % rampreset.looplen_step;
	if (cur_step < 0)
		cur_step += rampreset.looplen_step;
	cur_step += loopstart_step;
}

void set_cur_step(u8 newcurstep, bool triggerit) {
	//u8 old = cur_step;
	cur_step = newcurstep;
	check_curstep();
	seq_rhythm.did_a_retrig = triggerit; // make the sound play out once
	ticks_since_step = 0;
	seq_divide_counter = 0;
}

void OnLoop(void) {
	if (edit_preset_pending != 255) {
		SetPreset(edit_preset_pending, false);
		edit_preset_pending = 255;
	}
	if (edit_pattern_pending != 255) {
		EditParamQuant(P_SEQPAT, M_BASE, edit_pattern_pending);
		edit_pattern_pending = 255;
	}
	if (pending_loopstart_step !=255) {
		u8 loopstart_step = (rampreset.loopstart_step_no_offset + step_offset) & 63;
		if (loopstart_step != pending_loopstart_step) {
			rampreset.loopstart_step_no_offset = (pending_loopstart_step - step_offset)&63;
			ramtime[GEN_PRESET] = millis();
		}
		set_cur_step(loopstart_step, seq_rhythm.did_a_retrig);
		pending_loopstart_step  = 255;
		
	}
	if (edit_sample1_pending != cur_sample1 && edit_sample1_pending!=255) {
		EditParamQuant(P_SAMPLE, 0, edit_sample1_pending);
		edit_sample1_pending = 255;
	}
	check_curstep();
}


bool touched_main_area;

bool got_ui_reset = false;
int tapcount = 0;
void clearlatch(void);

void reverb_clear(void) {
	memset(reverbbuf, 0, (RVMASK + 1) * 2);
}
void delay_clear(void) {
	memset(delaybuf, 0, (DLMASK + 1) * 2);
}


u16 audioin_holdtime = 0;
s16 audioin_peak = 0;
s16 audioin_hold = 0;
knobsmoother recgain_smooth;
int audiorec_gain_target = 1 << 15;

int recpos = 0; // this cycles around inside the delay buffer (which we use for a recording buffer) while armed...
int recstartpos = 0; // once we start recording, we note the position in the buffer here
int recreadpos = 0; // ...and this is where we are up to in terms of reading that out and writing it to flash
u8 recsliceidx = 0;
const static bool pre_erase = true;
u32 record_flashaddr_base = 0;

static inline SampleInfo* getrecsample(void) { return &ramsample; } 
static inline u8 getwaveform4(SampleInfo* s, int x) { // x is 0-2047
	if (x < 0 || x >= 2048)
		return 0;
	return (s->waveform4_b[x >> 1] >> ((x & 1) * 4)) & 15;
}
static inline u8 getwaveform4halfres(SampleInfo* s, int x) {  // x 0-1023
	u8 b = s->waveform4_b[x & 1023];
	return maxi(b & 15, b >> 4);
}
static inline u16 getwaveform4zoom(SampleInfo* s, int x, int zoom) { // x is 0-2048. returns average and peak!
	if (zoom <= 0)
		return getwaveform4(s, x >> zoom);
	int samplepairs = 1 << (zoom - 1);
	u8* b = &s->waveform4_b[(x>>1) & 1023];
	int avg = 0, peak=0;
	u8* bend = &s->waveform4_b[1024];
	for (int i = 0; i < samplepairs && b < bend; ++i, ++b) {
		int s0 = b[0] & 15;
		int s1 = b[0] >> 4;
		avg += s0 + s1;
		peak = maxi(peak, maxi(s0, s1));
	}
	avg >>= zoom;
	return avg+peak*256;
}

static inline void setwaveform4(SampleInfo* s, int x, int v) {
	v = clampi(v, 0, 15);
	u8* b = &s->waveform4_b[(x >> 1) & 1023];
	if (x & 1) {
		v = maxi(v, (*b) >> 4);
		*b = (*b & 0x0f) | (v << 4);
	}
	else {
		v = maxi(v, (*b) & 15);
		*b = (*b & 0xf0) | v;
	}
}

void DebugSPIPage(int addr);

void recording_stop_really(void) {
	// clear out the raw audio in the delaybuf
	reverb_clear();
	delay_clear();
	ramtime[GEN_SAMPLE] = millis();	// fill in the remaining split points
	SampleInfo* s = getrecsample();
	int startsamp = s->splitpoints[recsliceidx];
	int endsamp = s->samplelen;
	int n = 8 - recsliceidx;
	for (int i = recsliceidx + 1; i < 8; ++i) {
		int samp = startsamp + ((endsamp - startsamp) * (i - recsliceidx)) / n;
		s->splitpoints[i] = samp;
		EmuDebugLog("POST RECORD EVEN SET SPLITPOINT %d to %d\n", i, s->splitpoints[i]);

	}
	recsliceidx = 0;
	ramtime[GEN_SAMPLE] = millis();
	//DebugSPIPage(0);
	//DebugSPIPage(1024*1024*2-65536);

	enable_audio = EA_PLAY;
}

void recording_stop(void) {
	if (enable_audio == EA_PLAY) {
		editmode = EM_PLAY;
	}
	else if (enable_audio == EA_RECORDING) {
		enable_audio = EA_STOPPING1;
	}
	else  if (enable_audio >= EA_STOPPING1) {
		// do nothing
	}
	else
		enable_audio = EA_PLAY;
	
}

void seq_step(int initial);

void recording_trigger(void) {
	recsliceidx = 0;
	SampleInfo* s = getrecsample();
	memset(s, 0, sizeof(SampleInfo));
#define LEADIN 1024
	int leadin = mini(recpos, LEADIN);
	recreadpos = recstartpos = recpos - leadin;
	s->samplelen = 0;
	s->splitpoints[0] = leadin;
	enable_audio = EA_RECORDING;
}


void on_shift_hold(int button) {
	
	if (editmode == EM_PRESET) {
		if (shift_down == SB_CLEAR && shift_down_time > 64+4) {
			// clear it!
			if (last_preset_selection_rotstep>=0 && last_preset_selection_rotstep<64)
				copyrequest = last_preset_selection_rotstep + 128;
		}
	}
	
	if (editmode == EM_SAMPLE) {
		if (enable_audio==EA_PLAY && (shift_down == SB_RECORD || shift_down == SB_PLAY) && shift_down_time > 64) {
			knobsmooth_reset(&recgain_smooth, audiorec_gain_target);
			record_flashaddr_base = (edit_sample0 & 7) * (2 * MAX_SAMPLE_LEN);
			recsliceidx = 0;
			recstartpos = 0;
			recreadpos = 0;
			recpos = 0;
			enable_audio = pre_erase ? EA_PREERASE : EA_MONITOR_LEVEL;
		}
	}
}

void arp_reset(void);
void ShowMessage(EFont fnt, const char* msg, const char *submsg);

u8 prev_editmode;
u8 last_edit_param=255;
bool param_sticky = false;
void on_shift_down(void) {
	prev_editmode = editmode;
	touched_main_area = false;
	if (editmode == EM_SAMPLE) {
		if (shift_down == SB_PREV) {
			
		} 
		else if (shift_down == SB_RECORD || shift_down==SB_RECORD || shift_down==SB_PLAY) {
			if (enable_audio == EA_PLAY) {
				/* long press */
			}
			else if (enable_audio == EA_MONITOR_LEVEL)
				enable_audio = EA_ARMED;
			else if (enable_audio == EA_ARMED) {
				recording_trigger();
			}
			else if (enable_audio == EA_RECORDING) {
				recording_stop();
			}
		}
		return;
	}

	switch (shift_down) {
	case SB_PLAY: // play/pause
		if (playmode == PLAY_WAITING_FOR_CLOCK_STOP) {
			playmode = PLAY_STOPPED;
			OnLoop();
		}
		else if (playmode == PLAY_PREVIEW) {
			//playmode = PLAYING;
			//arp_reset();
			//seq_step(true);
		} else if (playmode == PLAY_STOPPED) {
			playmode = PLAY_PREVIEW;
			seq_step(1);
		}
		else if (playmode == PLAYING) {
			playmode = PLAY_WAITING_FOR_CLOCK_STOP;
		}
		break;
	//case SB_REWIND: // reset
	//	playclock=loopstart+latency_fix;
	//	OnLoop();
	//	break;
	case SB_PREV: // prev
		editmode = EM_START;
		if (isplaying())
			got_ui_reset = true;
		break;
	case SB_NEXT: // next 
		editmode = EM_END;
		break;
	case SB_RECORD:
		knobbase[0] = adc_smooth[4].y2;
		knobbase[1] = adc_smooth[5].y2;
		recordingknobs = 0;
		break;
	case SB_CLEAR: // delete/clear. TODO: hold down in preset mode to clear a pattern/sample/preset. in play mode, supress notes.
		clearlatch();
		break; 
	case SB_PRESET:
		editmode = EM_PRESET;
		last_preset_selection_rotstep = sysparams.curpreset;
		break;
	case SB_PARAMSA:
		if (edit_param >= P_LAST && last_edit_param < P_LAST)
			edit_param = last_edit_param, param_sticky = true;
		else
			param_sticky = false;
		if (edit_param<P_LAST && (edit_param %12)>=6) edit_param -=6;
		editmode = EM_PARAMSA;
		tapcount = 0;
		break;
	case SB_PARAMSB:
		if (edit_param >= P_LAST && last_edit_param < P_LAST)
			edit_param = last_edit_param, param_sticky = true;
		else
			param_sticky = false;
		if (edit_param<P_LAST && (edit_param % 12) < 6) edit_param += 6;
		editmode = EM_PARAMSB;
		break;
	}
}

bool isshortpress(void) {
	return (ticks() - last_time_shift_was_untouched) < 250;
}

void togglearp(void);

void on_shift_up(int release_button) {
	bool shortpress = isshortpress();
	
	if (editmode == EM_SAMPLE) {
		if (shortpress) {
			if (shift_down == SB_PARAMSA) {
				ramsample.pitched = !ramsample.pitched;
				ramtime[GEN_SAMPLE] = millis();
			} else
			if (shift_down == SB_PARAMSB) {
				ramsample.loop = (ramsample.loop+1)&3;
				ramtime[GEN_SAMPLE] = millis();
			}
			else
			if (shift_down != SB_RECORD && shift_down!=SB_PLAY) {
				recording_stop();
			}
		}
		return;
	}
	switch (shift_down) {
	case SB_PARAMSA:
	case SB_PARAMSB:
		editmode = EM_PLAY;
		last_edit_param = edit_param;
		if (!touched_main_area && !param_sticky) {
			edit_param = P_LAST;
			edit_mod = 0;
		}
		// these arent real params, so dont stick on them.
		if (edit_param == P_ARPONOFF || edit_param == P_LATCHONOFF) {
			edit_param = P_LAST;
			edit_mod = 0;
		}
		break;
	case SB_PREV: // prev
		//if (!touched_main_area && shift_down_time > 128 / 4 && prev_editmode == EM_PLAY) {
		//	togglearp();
		//	editmode = EM_PLAY;
		//}
		//else 
		if (!touched_main_area && shortpress) {
			if (isplaying()) {
			//	got_ui_reset = true; did it on note down already
			}
			else
				set_cur_step(cur_step - 1, !isplaying());
			editmode = EM_PLAY;
		}
		if (touched_main_area || prev_editmode==editmode)
			editmode = EM_PLAY;
		break;
	case SB_NEXT: // next
		if (!touched_main_area && shortpress) {
			set_cur_step(cur_step + 1, !isplaying());
			editmode = EM_PLAY;
		}
		if (touched_main_area || prev_editmode == editmode)
			editmode = EM_PLAY;
		break;
	case SB_PRESET: // preset
		if (touched_main_area || prev_editmode == editmode)
			editmode = EM_PLAY;
		break;
	case SB_PLAY:
		if (playmode == PLAY_PREVIEW )
			playmode = shortpress ? PLAYING: PLAY_STOPPED;
	
		break;
	case SB_RECORD:
		if (shortpress && recordingknobs==0)
			recording = !recording;
		recordingknobs = 0;
		break;
	case SB_CLEAR:
		if (!isplaying() && recording && editmode == EM_PLAY) {
			bool dirty = false;
			int q = (cur_step >> 4) & 3;
			FingerRecord* fr = &rampattern[q].steps[cur_step & 15][0];
			for (int fi = 0; fi < 8; ++fi, ++fr) {
				for (int k = 0; k < 8; ++k) {
					if (fr->pressure[k] > 0)
						dirty = true;
					fr->pressure[k] = 0;
					if (fi < 2) {
						s8* d = &rampattern[q].autoknob[(cur_step & 15) * 8 + k][fi];
						if (*d) {
							*d = 0; dirty = true;
						}
					}
				}

			}
			if (dirty)
				ramtime[GEN_PAT0 + ((cur_step >> 4) & 3)] = millis();
			set_cur_step(cur_step + 1, false);
		}
		
		break;
	}
	check_curstep();
}


int prev_prev_total_ui_pressure = 0;
int prev_total_ui_pressure = 0;
int total_ui_pressure = 0;

typedef struct FingerStorage {
	u8 minpos, maxpos;
	u8 avgvel;
} FingerStorage;
FingerStorage latch[8];
void clearlatch(void) {
	memset(latch, 0, sizeof(latch));
}

	
static inline int randrange(int mn, int mx) {
	return mn + (((rand() & 255) * (mx - mn)) >> 8);
}



int param_eval_finger(u8 paramidx, int fingeridx, Finger* f);
u8 synthfingerdown_nogatelen_internal;
u8 physical_touch_finger = 0;
bool read_from_seq = false;


FingerRecord* readpattern(int fi) {
	if (rampattern_idx != cur_pattern || shift_down == SB_CLEAR)
		return 0;
	FingerRecord* fr = &rampattern[(cur_step >> 4) & 3].steps[cur_step & 15][fi];
    // does any substep hold data?
	int record_pressure = 0;
	for (u8 i = 0; i < 8; i++)
		record_pressure += fr->pressure[i];
	return record_pressure == 0 ? 0 : fr;
}

/////////////////// XXXXX MIDI HERE?
	// bitmask of 'midi pitch override' and 'midi pressure override'
	// midipitch
	// here: if real pressure, clear both
	// here: if pressure override, set it as midi channel aftertouch + midi note pressure
	// in plinky.c midi note down: inc next voice index; set both override bits; set voice midi pitch and channel and velocity
	// in plinky.c midi note up: clear pressure override bit
	// in the synth - override pitch if bit is set
u8 midi_pressure_override = 0; // true if midi note is pressed
u8 midi_pitch_override = 0; // true if midi note is sounded out, includes release phase
u8 midi_suppress = 0; // true if midi is suppressed by touch / latch / sequencer note
int memory_position[8];
u8 midi_notes[8];
int midi_positions[8];
u8 midi_velocities[8];
u8 midi_aftertouch[8];
u8 midi_channels[8] = { 255,255,255,255,255,255,255,255 };
u8 midi_chan_aftertouch[16];
s16 midi_chan_pitchbend[16];
u8 midi_next_finger;
u8 midi_lsb[32];
u8 find_midi_note(u8 chan, u8 note) {
	for (int fi = 0; fi < 8; ++fi) if ((midi_pitch_override & (1 << fi)) && midi_notes[fi] == note && midi_channels[fi] == chan)
		return fi;
	return 255;
}

int stride(u32 scale, int stride_semitones, int fingeridx);

int string_pitch_at_pad(u8 fi, u8 pad) {
	Finger* synthf = touch_synth_getlatest(fi);
	u32 scale = param_eval_finger(P_SCALE, fi, synthf);
	// pitch calculation:
	return 
		// calculate pitch offset, based on
		lookupscale(
			// the scale
			scale, 
			// the step-offset set by "degree"
			param_eval_finger(P_ROTATE, fi, synthf) + 
			// the step-offset of this string based on "column"
			stride(scale, maxi(0, param_eval_finger(P_STRIDE, fi, synthf)), fi) + 
			// the step-offset caused by the pad on the string
			pad
		) + 
		// add this to the pitch of the bottom-left pad
		12 * (
			// octave offset
			(param_eval_finger(P_OCT, fi, synthf) << 9) + 
			// pitch offset
			(param_eval_finger(P_PITCH, fi, synthf) >> 7)
		);
}

int string_center_pitch(u8 fi) {
	return (string_pitch_at_pad(fi, 0) + string_pitch_at_pad(fi, 7)) / 2;
}

// find the string whose center pitch is the closest to midi_pitch
u8 find_string_for_midi_pitch (int midi_pitch) {
	// pitch falls below bottom string center
	if (midi_pitch < string_center_pitch(0))
		return 0;
	// pitch falls above top string center
	if (midi_pitch >= string_center_pitch(7))
		return 7;
	// find the string with the closest center pitch
	u8 desired_string = 0;
	int min_dist = 2147483647; // int max
	for (u8 i = 0; i < 8; i++) {
		int pitch_dist = abs(string_center_pitch(i) - midi_pitch);
		if (pitch_dist < min_dist) {
			min_dist = pitch_dist;
			desired_string = i;
		}
	}
	return desired_string;
}

int find_string_position_for_midi_pitch(u8 fi, int midi_pitch) {
	// note that string positions are ordered top-to-bottom
	static const u16 pad_spacing = 256;
	// return the position of the highest pad the midi pitch is higher than - or equal to
	for (u8 pad = 7; pad > 0; pad--) {
		if (midi_pitch >= string_pitch_at_pad(fi, pad)) {
			return (7 - pad) * pad_spacing;
		}
	}
	// if the pitch was lower than the pitch of pad 1, we return the bottom pad
	return 7 * pad_spacing;
}

u8 find_free_midi_string(u8 midi_note_number, int *midi_note_position) {
	Finger* synthf = touch_synth_getlatest(0);
	int midi_pitch = 
		// base pitch
		12 * ((param_eval_finger(P_OCT, 0, synthf) << 9) + (param_eval_finger(P_PITCH, 0, synthf) >> 7)) +
		// pitch
		((midi_note_number - 24) << 9);

	// find the best string for this midi note
	u8 desired_string = find_string_for_midi_pitch(midi_pitch);

	// try to find:
	// 1. the non-sounding string closest to our desired string
	// 2. the sounding string that is the quietest
	int string_option[8];
	u8 num_string_options = 0;
	u8 min_string_dist = 255;
	float min_vol = __FLT_MAX__;
	u8 min_string_id = 255;

	// collect non-sounding strings
	for (u8 string_id = 0; string_id < 8; string_id++) {
		if (voices[string_id].vol < 0.001f) {
			string_option[num_string_options] = string_id;
			num_string_options++;
		}
	}
	// find closest
	for (uint8_t option_id = 0; option_id < num_string_options; option_id++) {
		if (abs(string_option[option_id] - desired_string) < min_string_dist) {
			min_string_dist = abs(string_option[option_id] - desired_string);
			min_string_id = string_option[option_id];
		}
	}
	// return closest, if found
	if (min_string_dist != 255) {
		// collect the position on the string before returning
		*midi_note_position = find_string_position_for_midi_pitch(min_string_id, midi_pitch);
		return min_string_id;
	}
	// collect non-pressed strings
	num_string_options = 0;
	for (u8 string_id = 0; string_id < 8; string_id++) {
		if (!(synthfingerdown_nogatelen_internal & (1 << string_id))) {
			string_option[num_string_options] = string_id;
			num_string_options++;
		}
	}
	// find quietest
	for (uint8_t option_id = 0; option_id < num_string_options; option_id++) {
		if (voices[string_option[option_id]].vol < min_vol) {
			min_vol = voices[string_option[option_id]].vol;
			min_string_id = string_option[option_id];
		}
	}
	// collect the position on the string before returning
	if (min_string_id != 255) {
		*midi_note_position = find_string_position_for_midi_pitch(min_string_id, midi_pitch);
	}
	// return quietest - this returns 255 if nothing was found
	return min_string_id;
}

bool is_finger_an_edit_operation(int fi);

u8 pres_compress(int pressure) {
	return clampi((pressure + 12) / 24, 0, 255);
}
int pres_decompress(int pressure) {
	return maxi(randrange(24 * pressure - 12, 24 * pressure + 12), 0);
}

u8 pos_compress(int position) {
	return clampi((position + 4) / 8, 0, 255);
}
int pos_decompress(int position) {
	return maxi(randrange(8 * position - 4, 8 * position + 4), 0);
}

void finger_synth_update(int fi) {
	static u8 last_edited_step_global = 255;
	static u8 last_edited_substep_global = 255;
	static u8 last_edited_step[8] = {255, 255, 255, 255, 255, 255, 255, 255};
	static int record_to_substep;
	static bool suppress_latch = false;

	int bit = 1 << fi;
	int ui_frame = (finger_frame_ui - (finger_ui_done_this_frame & bit ? 0 : 1)) & 7;
	Finger* ui_finger = &fingers_ui_time[fi][ui_frame];
	Finger* synth_finger = &fingers_synth_time[fi][finger_frame_synth];
	int previous_pressure = fingers_ui_time[fi][(ui_frame - 2) & 7].pressure;
	int substep = calcseqsubstep(0, 8);
	bool latchon = (rampreset.flags & FLAGS_LATCH);
	int pressure = 0;
	int position = 0;
	bool pressure_increasing;
	bool position_updated = false;

	// === TOUCH INPUT === //

	if (!is_finger_an_edit_operation(fi)) {
		// read touch values from ui
		pressure = ui_finger->pressure;
		position = ui_finger->pos;
		pressure_increasing = (pressure > previous_pressure);
		if (pressure > 0) {
			physical_touch_finger |= bit;
			position_updated = true;
		}
		else physical_touch_finger &= ~bit;

		if (latchon) {

			// === LATCH WRITE === // 

			// finger touching and pressure increasing
			if (pressure > 0 && pressure_increasing) {
				// is this a new touch after no fingers where touching?
				if (previous_pressure <= 0 && physical_touch_finger == bit) {
					// start a new latch, clear all previous latch values
					for (uint8_t i = 0; i < 8; i++) {
						latch[i].avgvel = 0;
						latch[i].minpos = 0;
						latch[i].maxpos = 0;
					}
					// in step record mode, trying to start a new latch temporarily turns off latching
					// trying to start a new latch outside of step record mode turns it on again
					suppress_latch = recording && !isplaying();
				}
				// save latch values
				if (!suppress_latch) {
					latch[fi].avgvel = pres_compress(pressure);
					latch[fi].minpos = pos_compress(position);
				}
				// RJ: I could not work out a way to work with average values that wasn't
				// sluggish or gave undesired intermediate values - slides and in-between notes
				// Current solution is just saving one value and randomizing when reading it out
				// Result feels great, but good to reconsider when the exact contents of
				// fingers_ui_time and fingers_synth_time are more clear

				// Averaging code for reference:
				//
				// u8 maxpos = 0, minpos = 255, maxpressure = 0;
				// Finger* f = fingers_synth_time[fi];
				// for (int j = 0; j < 8; ++j, ++f) {
				// 	u8 p = clampi((f->pos + 4) / 8, 0, 255);
				// 	minpos = mini(p, minpos);
				// 	maxpos = maxi(p, maxpos);
				// 	u8 pr = clampi(f->pressure / 12, 0, 255);
				// 	maxpressure = maxi(maxpressure, pr);
				// }
				// latch[fi].avgvel = maxpressure;
				// latch[fi].minpos = minpos;
				// latch[fi].maxpos = maxpos;
			}

			// === LATCH RECALL === // 

			// latch pressure larger than touch pressure
			if (latch[fi].avgvel > 0 && latch[fi].avgvel * 24 > pressure) {
				//recall latch values
				pressure = pres_decompress(latch[fi].avgvel);;
				position = pos_decompress(latch[fi].minpos);
				position_updated = true;

				// Averaging code for reference:
				//
				// int minpos = latch[fi].minpos * 8 + 2;
				// int maxpos = latch[fi].maxpos * 8 + 6;
				// int avgpos = (minpos + maxpos) / 2;
				// int range = (maxpos - minpos) / 4;
				// pressure = latchpres ? randrange(latchpres - 12, latchpres) : -1024;
				// position = randrange(avgpos-range,avgpos+range);
			}
		}

		// === SEQ RECORDING === //

		// RJ: because of the way data is stored in the sequencer, it is currently not possible to record
		// midi data into it without imposing some serious restrictions on the range of midi notes that 
		// can be played. So we're only doing this for touches for now

		int quarter = (cur_step >> 4) & 3;
		FingerRecord* seq_record = &rampattern[quarter].steps[cur_step & 15][fi];
		int data_saved = false;	
		// We're recording into the loaded pattern
		if (recording && rampattern_idx == cur_pattern) {
			// holding clear sets the pressure to zero, which will effectively clear the sequencer at this point
			int seq_pressure = shift_down == SB_CLEAR ? 0 : pres_compress(pressure);
			int seq_position = shift_down == SB_CLEAR ? 0 : pos_compress(position);

			// holding a note or clearing during playback
			if ((seq_pressure > 0 && pressure_increasing) || shift_down == SB_CLEAR) {
				// live recording
				if (isplaying()) {
					record_to_substep = substep;
				}				
				// step recording
				else {
					// editing a new step, and waited for substep to reset to zero
					if (cur_step != last_edited_step_global && substep == 0) {
						// we have not edited any substep
						last_edited_substep_global = 255;
						// we have not edited this step for any finger
						memset(last_edited_step, 255, sizeof(last_edited_step));
						// start at substep 0
						record_to_substep = 0;
						// this skips the first increment of record_to_substep, right below
						last_edited_substep_global = 0;
						// we're now editing the current step
						last_edited_step_global = cur_step;
					}
					// editing a new substep
					if (substep != last_edited_substep_global) {
						// are we in the step?
						if (record_to_substep < 8) {
							// move one substep forward
							record_to_substep++;
						}
						// are we at the end of the step?
						else {
							// push all data one substep backward
							for (u8 i = 0; i < 7; i++) {
								seq_record->pressure[i] = seq_record->pressure[i + 1];
								if (!(substep & 1) && !(i & 1))
									seq_record->pos[i / 2] = seq_record->pos[i / 2 + 1];
							}
						}
						last_edited_substep_global = substep;
					}
					// first finger edit on this step
					if (cur_step != last_edited_step[fi]) {
						// clear the step for this finger
						memset(seq_record->pressure, 0, sizeof(seq_record->pressure));
						memset(seq_record->pos, 0, sizeof(seq_record->pos));
						// we're now editing this step with this finger
						last_edited_step[fi] = cur_step;
					}		
				}
				// record!
				seq_record->pressure[mini(record_to_substep, 7)] = seq_pressure;
				seq_record->pos[mini(record_to_substep, 7) / 2] = seq_position;		
				data_saved = true;
			}
			if (data_saved)
				ramtime[GEN_PAT0 + quarter] = millis();
		}
		// not recording
		else {
			// clear this for next recording
			last_edited_step_global = 255;
		}
	}

	// === SEQ PLAYING === //

	// no pressure generated by touch or latch and sequencer wants to play
	if (pressure <= 0 && (isplaying() || seq_rhythm.did_a_retrig)) {
		FingerRecord *seq_record = readpattern(fi);
		if (seq_record) {
			int gatelen = param_eval_finger(P_GATE_LENGTH, fi, synth_finger) >> 8;
			int small_substep = calcseqsubstep(0, 256);
			if (seq_record->pressure[substep] && !seq_rhythm.supress && small_substep <= gatelen && shift_down != SB_CLEAR) {
				read_from_seq = true;
				pressure = pres_decompress(seq_record->pressure[substep]);
				position = pos_decompress(seq_record->pos[substep / 2]);
				position_updated = true;
			}
		}
	}

	// === MIDI INPUT === //

	// midi gets suppressed if touch, latch or sequencer generate pressure
	if (pressure > 0) midi_suppress |= bit;
	else midi_suppress &= ~bit;
	// a midi note is playing this string
	if ((midi_pressure_override & bit) && !(midi_suppress & bit)) {
		// take pressure and position from midi data
		pressure = 1+(midi_velocities[fi] + maxi(midi_aftertouch[fi], midi_chan_aftertouch[midi_channels[fi]]))*16;		
		// for midi, position only defines where the leds light up on the string        
		position = midi_positions[fi];
	}

	// manually save position for release phase
	if (position_updated) {
		memory_position[fi] = position;
	}

	// === CV GATE === //

	// scale pressure with cv gate input
	pressure = (int)((pressure + 256) * adc_smooth[7].y2) - 256;

	// save input results to global variables to be used by other code
	synth_finger->pressure = pressure;
	synth_finger->pos = position;
	total_ui_pressure += maxi(0, synth_finger->pressure);
	if (synth_finger->pressure > 0) {
		synthfingerdown_nogatelen_internal |= bit;
	}
	else {
		synthfingerdown_nogatelen_internal &= ~bit;
	}

	// new finger touch, slightly randomize touch-values in history to avoid static touch values
	static s16 prevpressure[8];
	if (prevpressure[fi]<= 0 && synth_finger->pressure > 0) {
		Finger* of = fingers_synth_time[fi];
		int newp = synth_finger->pos;
		for (int h = 0; h < 8; ++h, of++) if (h != finger_frame_synth) {
			if (of->pressure <= 0) {
				of->pos = (of->pos & 3) ^ newp;
			}
		}
	}
	prevpressure[fi] = synth_finger->pressure;

	// sort fingers by pitch
	sort8((int*)fingers_synth_sorted[fi], (int*)fingers_synth_time[fi]);
}

void finger_editing(int fi, int frame);


#ifdef EMU
int htsc;
typedef struct TSC_IOConfigTypeDef {
	u32 ChannelIOs;
	u32 SamplingIOs;
}TSC_IOConfigTypeDef;
typedef int TSC_GroupStatusTypeDef;
#define TSC_GROUP1_IO1 (1<<0)
#define TSC_GROUP1_IO2 (1<<1)
#define TSC_GROUP1_IO3 (1<<2)
#define TSC_GROUP1_IO4 (1<<3)
#define TSC_GROUP2_IO1 (1<<4)
#define TSC_GROUP2_IO2 (1<<5)
#define TSC_GROUP2_IO3 (1<<6)
#define TSC_GROUP2_IO4 (1<<7)
#define TSC_GROUP3_IO1 (1<<8)
#define TSC_GROUP3_IO2 (1<<9)
#define TSC_GROUP3_IO3 (1<<10)
#define TSC_GROUP3_IO4 (1<<11)
#define TSC_GROUP4_IO1 (1<<12)
#define TSC_GROUP4_IO2 (1<<13)
#define TSC_GROUP4_IO3 (1<<14)
#define TSC_GROUP4_IO4 (1<<15)
#define TSC_GROUP5_IO1 (1<<16)
#define TSC_GROUP5_IO2 (1<<17)
#define TSC_GROUP5_IO3 (1<<18)
#define TSC_GROUP5_IO4 (1<<19)
#define TSC_GROUP6_IO1 (1<<20)
#define TSC_GROUP6_IO2 (1<<21)
#define TSC_GROUP6_IO3 (1<<22)
#define TSC_GROUP6_IO4 (1<<23)
#define TSC_GROUP7_IO1 (1<<24)
#define TSC_GROUP7_IO2 (1<<25)
#define TSC_GROUP7_IO3 (1<<26)
#define TSC_GROUP7_IO4 (1<<27)

#define ENABLE 1
#define TSC_GROUP_COMPLETED 1
u32 _chanios;
void HAL_TSC_IOConfig(int* htsc, TSC_IOConfigTypeDef* config) { _chanios = config->ChannelIOs; }
void HAL_TSC_IODischarge(int *htsc, int enable) {}
void HAL_TSC_Start(int* htsc) {}
void HAL_TSC_Stop(int* htsc) {}
TSC_GroupStatusTypeDef HAL_TSC_GroupGetStatus(int* htsc, int groupidx) {
	return TSC_GROUP_COMPLETED;
}
short HAL_TSC_GroupGetValue(int* htsc, int groupidx) {
	// hacked so groupidx is actually 0-35 sensor idx
	groupidx %= 18;
	int fingeridx = groupidx / 2;
	extern int emutouch[9][2];
	int pos = emutouch[fingeridx][1];
	int pressure = emutouch[fingeridx][0];
	int a = pressure * (2048 - pos);
	int b = pressure * (pos);
	if (fingeridx == 8) {
		int t = a; a = b; b = t; // oops I swapped the pins
	}
	if (groupidx&1) a = b;
	a >>= 10;
	a += 2048;
//	if (groupidx == 0)
//		printf("hello finger 0 %d %d = %d\n", pos, pressure, (2048 * 2048) / a);
	a += rand() & 31;
	return (2048 * 2048) / a;
}
#endif

#define FF0 TSC_GROUP1_IO2+TSC_GROUP4_IO2
#define FF1 TSC_GROUP2_IO2+TSC_GROUP5_IO2
#define FF2 TSC_GROUP3_IO3+TSC_GROUP6_IO2
#define FF3 TSC_GROUP1_IO3+TSC_GROUP4_IO3
#define FF4 TSC_GROUP2_IO3+TSC_GROUP5_IO3
#define FF5 TSC_GROUP3_IO4+TSC_GROUP6_IO3
#define FF6 TSC_GROUP1_IO4+TSC_GROUP4_IO4
#define FF7 TSC_GROUP2_IO4+TSC_GROUP5_IO4
#define FF8a TSC_GROUP7_IO2
#define FF8b TSC_GROUP7_IO3
#define SS0 TSC_GROUP1_IO1+TSC_GROUP4_IO1
#define SS1 TSC_GROUP2_IO1+TSC_GROUP5_IO1
#define SS2 TSC_GROUP3_IO2+TSC_GROUP6_IO1
#define SS3 TSC_GROUP1_IO1+TSC_GROUP4_IO1
#define SS4 TSC_GROUP2_IO1+TSC_GROUP5_IO1
#define SS5 TSC_GROUP3_IO2+TSC_GROUP6_IO1
#define SS6 TSC_GROUP1_IO1+TSC_GROUP4_IO1
#define SS7 TSC_GROUP2_IO1+TSC_GROUP5_IO1
#define SS8a TSC_GROUP7_IO1
#define SS8b TSC_GROUP7_IO1

int raw_isdown(int srcidx) {
	int a = finger_cap(srcidx * 2) - finger_mincap(srcidx*2);
	int b = finger_cap(srcidx * 2 + 1) - finger_mincap(srcidx * 2 + 1);
	return (a + b > 1000) ? 1 : 0;
}

static inline int finger_rawpos(int a, int b) {
	return clampi(((b - a) << 12) / (a+b+ 1), -32767, 32767);
}

static inline int finger_rawpressure(int a, int b) {
	return clampi(a + b, 0, 32767);
}

void update_finger(int srcidx) {
	int dstidx = srcidx;
	if (dstidx >= 9) dstidx -= 9;
	int a = finger_cap(srcidx*2);
	int b = finger_cap(srcidx*2+1);
	int amin = finger_mincap(srcidx * 2);
	int bmin = finger_mincap(srcidx * 2 + 1);
	int rawpressure = finger_rawpressure(a-amin,b-bmin);
	int rawpos = finger_rawpos(a, b);
	int pos = rawpos, pressure = rawpressure;

//	if (pressure>500) DebugLog("%d %d\r\n", pressure, pos);

	// scale pos and pressure by calibration
	const CalibResult* c = &calibresults[srcidx];
	if (c->pressure[0] != 0) { // if we dont have any calibration data, we just pass thru the raw values
		int prev = c->pos[0] - (c->pos[1] - c->pos[0]); // extrapolate below 0
		int reversed = c->pos[7] < c->pos[0];
		int maxp;
		if ((rawpos < prev) ^ reversed) {
			pos = -128;
			maxp = c->pressure[0];
		}
		else {
			pos = 8 * 256 + 128;
			maxp = c->pressure[7];
			for (int x = 0; x <= 8; ++x) {
				int next;
				if (x == 8) next = c->pos[7] + (c->pos[7] - c->pos[6]); // extrapolate
				else next = c->pos[x];
				if ((rawpos < next) ^ reversed) {
					int diff = next-prev;

					int t = diff ? ((rawpos - prev) * 256) / diff : 0;
					pos = x * 256 - 128 + t;
					int prevp = c->pressure[maxi(0, x - 1)];
					int nextp = c->pressure[mini(7, x)];
					maxp = prevp + (((nextp - prevp) * t) >> 8);
					break;
				}
				prev = next;
			}
		}
		if (maxp < 1000) maxp = 1000;
		pressure = (rawpressure * 4096) / maxp - 2048;
	//	if (pressure>100) DebugLog("%d %d\r\n", pressure, pos);
	}
	
	int frame = finger_frame_ui;
	ASSERT(frame >= 0 && frame < 8);
	int prevframe = (frame - 1) & 7;
	int fi = dstidx;
	Finger* uif = &fingers_ui_time[fi][frame];
	Finger* prev_uif = &fingers_ui_time[fi][prevframe];
	uif->pressure = pressure;
	//bool written = 0;
	if (pressure > 0 && pressure > prev_uif->pressure - 128) { // not significantly lifting finger off
		uif->pos = clampi(pos, 0, 2047);
		//written = 1;
	}
	else
		uif->pos = prev_uif->pos;
	uif->written = 1;//written;
	// else dont even write pos! let it be old random values



	if (enable_audio <= EA_OFF) {
		return;
	}
	
	if (fi == 8) {
		Finger* oldf = &fingers_ui_time[fi][(frame-2)&7];
		int button = uif->pos>>8;
		if (shift_down != SB_NOT_PRESSED) {
			int oldpos = shift_down * 256 + 128;
			if (abs(oldpos - uif->pos) < 192) // a bit of hysteresis
				button = shift_down;
		}
		int midbutton = prev_uif->pos>>8;
		int oldbutton = oldf->pos>>8;
		bool valid=oldf->pressure>700 && uif->pressure>700 && abs(uif->pos-oldf->pos)<60;
		if (shift_down == SB_NOT_PRESSED)
			valid&= oldbutton==midbutton && button==oldbutton;

		if (valid) {
			if (shift_down != button) {
				// check for ghosted presses
				Finger* f0 = touch_ui_getwriting(button);
				Finger* fl = touch_ui_getwriting(maxi(0, button - 1));
				Finger* fr = touch_ui_getwriting(mini(7, button + 1));
				if (
					(f0->pressure > 256 && f0->pos >= 6 * 256) ||
					(fl->pressure > 256 && fl->pos >= 7 * 256) ||
					(fr->pressure > 256 && fr->pos >= 7 * 256))
					shift_down = SB_GHOSTED; // ghosted press! they have a finger on a nearby strip, maybe it was an accident.
				else {
					shift_down = button;
					shift_down_time = 0;
					on_shift_down();
				}
			}
		} else if (uif->pressure<=0) {
			if (shift_down >= 0)
				on_shift_up(button);
			shift_down = SB_NOT_PRESSED;
		}
		if (shift_down >= 0) {
			shift_down_time++;
			on_shift_hold(button);
		}
		if (uif->pressure <= 0)
			last_time_shift_was_untouched = ticks();

		//DebugLog("%d %d %02x\r\n", maxi(0,pressure), valid ? button*256 : -256, finger_stepmask);
		
	} 
	else {
		finger_editing(fi, finger_frame_ui);
		finger_ui_done_this_frame |= 1 << fi;
	}
}

u32 fingererror=0;

void touch_update(void) {
again:
	waitcounter++;
#define MAX_STEPS 13 // 13 steps that generate 36 sensor readings.
	static u8 const sensororder[MAX_STEPS][8] = {
			{ 0, 2, 4, 1, 3, 5, 16}, // first step does a group of first three touch + half of strip
			{ 6, 8,10, 7, 9,11, 17}, // second step does next 3 touch in a group + other half of step
			{12,14,    13,15},			// third step does last 2 touch in a group
			{18,19},	// remaining steps do single fingers at a time
			{20,21},
			{22,23},
			{24,25},
			{26,27},
			{28,29},
			{30,31},
			{32,33},
			{34},{35} }; // this is actually two steps, doh pin wiring
	// the bitmask version of the above table:
	static u32 const chansio[MAX_STEPS] = {FF0 + FF1 + FF2 + FF8a,FF3 + FF4 + FF5 + FF8b, FF6 + FF7,	FF0,FF1,FF2,FF3,FF4,FF5,FF6,FF7,FF8a,FF8b};
	static u32 const sampio[MAX_STEPS] =  {SS0 + SS1 + SS2 + SS8a,SS3 + SS4 + SS5 + SS8b, SS6 + SS7,	SS0,SS1,SS2,SS3,SS4,SS5,SS6,SS7,SS8a,SS8b };
	if (finger_state == 0) {
		TSC_IOConfigTypeDef config={0};
		config.ChannelIOs= chansio[finger_step];
		config.SamplingIOs=sampio[finger_step];
		HAL_TSC_IOConfig(&htsc, &config);
		HAL_TSC_IODischarge(&htsc, ENABLE);
		finger_state++;
		return;
	} 
	if (finger_state == 1) {
		finger_state++;
		//return; // more discharge time !
	}
	if (finger_state == 2) {
		HAL_TSC_Start(&htsc);
		finger_state++;
		return;
	}
	u32 chans = chansio[finger_step];
	int subchan = 0;
	bool errorstate=false;
#ifndef EMU
	if (HAL_TSC_GetState(&htsc)==HAL_TSC_STATE_ERROR) {
		errorstate=true;
	}
#endif
	for (int grp = 0; grp < 7; ++grp) {
		if (!(chans & (0xf << (grp * 4))))
			continue;
		TSC_GroupStatusTypeDef status = HAL_TSC_GroupGetStatus(&htsc, grp);
		int oidx=sensororder[finger_step][subchan++];
		if (status != TSC_GROUP_COMPLETED) {
			if (errorstate) {
				fingererror|=(1<<oidx);
			}
			else
				return;// not done yet
		}
		else {
			fingererror&=~(1<<oidx);
		}
		short v = finger_raw[oidx] = (1<<23)/maxi(129,
#ifdef EMU
			HAL_TSC_GroupGetValue(&htsc, oidx));
#else
			HAL_TSC_GroupGetValue(&htsc, grp));
#endif
//		if (oidx == 3 * 2 + 1) v = finger_min[1]; FAKE AN ERROR
//		if (oidx == 4) v = finger_min[3]; // FAKE AN ERROR
		if (v > finger_max[oidx])
			finger_max[oidx] = v;
		if (v < finger_min[oidx])
			finger_min[oidx] = v;
		//if (oidx == 34 || oidx == 35 || oidx == 16 || oidx == 17) {
		//	DebugLog("%d = %d\n", oidx, v);
		//}
	}
	prevfingerediting = fingerediting;
	HAL_TSC_Stop(&htsc);
	// update fingers and decide next step
	bool calib = (enable_audio == EA_OFF);
	switch (finger_step) {
	case 0: { // group of fingers 0,1,2 plus half of strip
		finger_stepmask = 7; // always do the first 3 steps, doh
		bool d0 = raw_isdown(0), d1 = raw_isdown(1), d2 = raw_isdown(2), d8 = raw_isdown(8);
		int numdown = d0 + d1 + d2 + d8*2;
		if (numdown <= 1 || calib) {
			update_finger(0);
			update_finger(1);
			update_finger(2);
			// do the shift finger in the next one
		}
		else {
			if (d0) finger_stepmask |= 1 << (3 + 0); else update_finger(0);
			if (d1) finger_stepmask |= 1 << (3 + 1); else update_finger(1);
			if (d2) finger_stepmask |= 1 << (3 + 2); else update_finger(2);
		}
	}
	break;
	case 1: { // group of fingers 3,4,5 plus half of strip
		bool d3 = raw_isdown(3), d4 = raw_isdown(4), d5 = raw_isdown(5), d8 = raw_isdown(8);
		int numdown = d3 + d4 + d5 + d8 * 2;
		if (numdown <= 1 || calib) {
			update_finger(3);
			update_finger(4);
			update_finger(5);
			update_finger(8);
		}
		else {
			if (d3) finger_stepmask |= 1 << (3 + 3); else update_finger(3);
			if (d4) finger_stepmask |= 1 << (3 + 4); else update_finger(4);
			if (d5) finger_stepmask |= 1 << (3 + 5); else update_finger(5);
			if (d8) finger_stepmask |= (1 << (3 + 8)) + (1 << (3 + 9)); else update_finger(8); // strip is two steps alas
		}
	}
	break;
	case 2: { // group of fingers 6,7
		bool d6 = raw_isdown(6), d7 = raw_isdown(7);
		int numdown = d6 + d7;
		if (numdown <= 1 || calib) {
			update_finger(6);
			update_finger(7);
		}
		else {
			if (d6) finger_stepmask |= 1 << (3 + 6); else update_finger(6);
			if (d7) finger_stepmask |= 1 << (3 + 7); else update_finger(7);
		}
	}
	break;
	default: // 3...10: individual fingers
		update_finger(9 + finger_step - 3);
		break;
	case 11: // half of strip. do nothing :(
		break;
	case 12: // second half of strip! yay!
		update_finger(9 + 8);
		break;
	}
	finger_state = 0;
	// find next step that is active
	while (finger_step < MAX_STEPS) {
		finger_step++;
		if (calib)
			break;
		if (finger_stepmask & (1 << finger_step))
			break;
	}

	// move on to next step!
	if (finger_step == MAX_STEPS) {
		finalwait = waitcounter;
		waitcounter = 0;
		finger_step = 0;
		finger_frame_ui = (finger_frame_ui + 1) & 7;
		finger_ui_done_this_frame=0;
	}
	goto again;
}

