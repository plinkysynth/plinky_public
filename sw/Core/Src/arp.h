#pragma once

u8 arpbits; // the output of the arpeggiator - which fingers are down
bool arpretrig; // causes the notes to re-attack

s8 curarpfinger;
s8 arpoctave;
u8 arpdir;
s8 arp_nonpedalfinger;
u8 arpused,arpused2;
s8 arpmode;
s32 freearpclock;
#ifdef WIN32
int __builtin_popcount(int x) {
	int c = 0;
	while (x) { c++; x &= x - 1; }
	return c;
}
int __builtin_popcountll(unsigned long long x) {
	int c = 0;
	while (x) { c++; x &= x - 1; }
	return c;
}
int __builtin_ctzll(unsigned long long x) {
	if (!x) return 64;
	int c = 0;
	while (!(x&(1ull<<c))) { c++; }
	return c;
}
#endif

u8 pickrandombit(u8 mask) {
	if (!mask) return 0;
	int num = __builtin_popcount(mask);
	num = rand() % num;
	for (; num--;) mask &= mask - 1;
	return mask ^ (mask & (mask-1));
}
u8 nextup(u8 allowedfingers) {
	s8 prevarpfinger = curarpfinger;
	while (1) {
		curarpfinger = (curarpfinger + 1) & 7;
		if (allowedfingers & (1 << curarpfinger))
			break;
	}
	return (prevarpfinger >= curarpfinger);
}
u8 nextdown(u8 allowedfingers) {
	s8 prevarpfinger = curarpfinger;
	while (1) {
		curarpfinger = (curarpfinger - 1) & 7;
		if (allowedfingers & (1 << curarpfinger))
			break;
	}
	return (prevarpfinger <= curarpfinger);
}

void arp_reset_impl(bool partial) { // a 'partial' reset is used when it panics that none of the fingers that are down match the currently set arp bits.
#ifdef EMU
	if (curarpfinger >= 0) 
		EmuDebugLog("!!ARP RESET\r\n");
#endif
	arpretrig = false;
	arp_rhythm.trigcount = 0;
	arpbits = 0;
	arpused = 0;
	arpused2 = 0;
	arp_nonpedalfinger = -2;
	freearpclock = 0;
	arp_divide_counter = 0;
	ticks_since_arp = 0; 
	if (!partial) {
		curarpfinger = -1;
		arpoctave = 0;
		arpdir = 0;
	}
}

 void arp_reset(void) {
	arp_reset_impl(false);
}

static inline void arp_reset_partial(void) {
	arp_reset_impl(true);
}

bool euclidstuff(euclid_state *s, int patlen, int prob, int arpmode) {
	int aprob = clampi((abs(prob) + 256) >> 9, 0, 128);
	int apatlen = abs(patlen);
	bool click;
	if (apatlen <= 1) {
		if (arpmode == ARP_ALL)
			click = true;
		else
			click = (rand() & 127) < aprob;
	}
	else {
		float k = aprob * (1.f / 128.f); //arpeuclid / (float) (abspatlen);
		click = (floor(s->trigcount * k) != floor(s->trigcount * k - k)); // euclidian rhythms!
	}
	//  printf("%d: %d\n", arptrigcount, arpretrig);
	s->trigcount++;
	//arpretrig = true;
	bool step; // do we go to the next step?
	if (apatlen > 0) {
		s->trigcount %= apatlen;
	}
	if ((patlen < 0) ^ (prob < 0)) {
		// if patlen or prob are negative, then we clock at a regular rate but silence some steps 
		step = true;
		s->supress = !click;
	}
	else {
		// if they're both pos (or neg), then we simply clock at the erratic rate.
		step = click;
		int gatelen = param_eval_int(P_GATE_LENGTH, any_rnd, env16, pressure16) >> 8;
		bool do_you_want_silence_in_long_bits = gatelen < 256;
		s->supress = do_you_want_silence_in_long_bits ? !click : false;
	}
	s->did_a_retrig = click;
//	if (!click) {
//		int i = 1;
//	}
	return step;
}

bool arpupwards(u8 allowedfingers, int minoctave, int maxoctave) {
	bool wrap = nextup(allowedfingers);
	if (wrap) {
		if (++arpoctave > maxoctave) {
			if (arpmode == ARP_UPDOWN || arpmode == ARP_PEDALUPDOWN) {
				arpdir = 1;
				arpoctave = maxoctave;
				nextdown(allowedfingers);
				nextdown(allowedfingers);
			}
			else if (arpmode == ARP_UPDOWNREP || arpmode == ARP_UPDOWN8) {
				arpdir = 1;
				arpoctave = maxoctave;
				nextdown(allowedfingers);
			}
			else
				arpoctave = minoctave;
		}
	}
	return wrap;
}

bool arpdownwards(u8 allowedfingers, int minoctave, int maxoctave) {
	bool wrap = nextdown(allowedfingers);
	if (wrap) {
		if (--arpoctave < minoctave) {
			if (arpmode == ARP_UPDOWN || arpmode == ARP_PEDALUPDOWN) {
				arpdir = 0;
				arpoctave = minoctave;
				nextup(allowedfingers);
				nextup(allowedfingers);
			}
			else if (arpmode == ARP_UPDOWNREP || arpmode == ARP_UPDOWN8) {
				arpdir = 0;
				arpoctave = minoctave;
				nextup(allowedfingers);
			}
			else
				arpoctave = maxoctave;
		}
	}
	return wrap;
}

void arprandom(u8 allowedfingers, int minoctave, int maxoctave) {
	arpoctave = minoctave + (rand() % (maxoctave + 1 - minoctave));
	u8 left = allowedfingers & ~arpused;
	if (left == 0) {
		arpused = 0;
		left = allowedfingers;
	}
	curarpfinger = -1;
	arpbits = pickrandombit(left);
	arpused |= arpbits;
	if (arpmode == ARP_RANDOM2 || arpmode == ARP_RANDOM28) {
		// pick a second random!
		u8 left = allowedfingers & ~arpused2;
		left &= ~(1 << curarpfinger);
		if (left == 0) {
			arpused2 = 0;
			left = allowedfingers & ~(1 << curarpfinger);
		}
		if (left) {
			u8 bit = pickrandombit(left);
			arpused2 |= bit;
			arpbits |= bit;
		}
	}
}

extern int audiotime;

void arptrig(u8 fingerdown_music) {
	// try to find a higher finger
	if (fingerdown_music == 0) {
		arp_reset();
		return;
	}
	int arpoctaves = param_eval_int(P_ARPOCT, any_rnd, env16, pressure16);
	int arppatlen = param_eval_int(P_ARPLEN, any_rnd, env16, pressure16);
	int prob = param_eval_int(P_ARPPROB, any_rnd, env16, pressure16);
	bool arpstep = euclidstuff(&arp_rhythm, arppatlen, prob, arpmode);
	arpretrig = arp_rhythm.did_a_retrig;

#ifdef EMU
	static int pt = 0;
	int delta = audiotime - pt;
	if (delta > 2048 && pt) {
		int i = 1;
	}
	pt = audiotime;
	EmuDebugLog("arp %d %d %d\r\n", arpstep, arpretrig,delta);
#endif

	if (!arpstep)
		return;
	arpbits = 0;
	u8 allowedfingers = fingerdown_music;
	if (arpmode >= ARP_UP8)
		allowedfingers = 0xff;
	int maxoctave = (arpoctaves+1) / 2;
	int minoctave = maxoctave - arpoctaves;
	switch (arpmode) {
	default: return;
	case ARP_ALL:
		arpbits = allowedfingers;
		// in chord mode, with no euclid rhythm, we randomly drop chord notes
		if (abs(arppatlen) <= 1) {
			int aprob = abs(prob);
			for (int i = 0; i < 8; ++i) if (arpbits & (1 << i)) {
				bool click = (rand() & 32767) < (aprob>>1); 
				if (!click) arpbits ^= (1 << i);
			}
		}
		break;
	case ARP_PEDALDOWN: case ARP_PEDALUP: case ARP_PEDALUPDOWN:
	{
		if (arp_nonpedalfinger >= 0 && allowedfingers & (1 << arp_nonpedalfinger)) {
			//for pedal, if we have a remembered arpfinger, we just play that
			curarpfinger = arp_nonpedalfinger;
			arp_nonpedalfinger = -2;
		}
		else {
			//otherwise we are due to play the pedal, we instead remove the pedal note from the allowed fingers
			//then we do the logic as usual
			u8 allowed_no_pedal = allowedfingers & (allowedfingers - 1);
			if (allowed_no_pedal == 0) allowed_no_pedal = allowedfingers;
			if (arpmode == ARP_PEDALDOWN || (arpmode == ARP_PEDALUPDOWN && arpdir))
				arpdownwards(allowed_no_pedal, minoctave, maxoctave);
			else
				arpupwards(allowed_no_pedal, minoctave, maxoctave);
			// actually, we're gonna play the pedal! wooahahah
			arp_nonpedalfinger = curarpfinger;
			arpbits = allowedfingers ^ allowed_no_pedal;
			curarpfinger = -1;
		}
		break;
	}
	case ARP_UPDOWN: case ARP_UPDOWN8: case ARP_UPDOWNREP:
		if (arpdir)
			arpdownwards(allowedfingers, minoctave, maxoctave);
		else
			arpupwards(allowedfingers, minoctave, maxoctave);
		break;
	case ARP_UP: case ARP_UP8:
		arpupwards(allowedfingers,minoctave,maxoctave);
		break;
	case ARP_DOWN: case ARP_DOWN8:
		arpdownwards(allowedfingers, minoctave, maxoctave);
		break;
	case ARP_RANDOM: case ARP_RANDOM8: case ARP_RANDOM2: case ARP_RANDOM28:
		arprandom(allowedfingers,minoctave,maxoctave);
		break;
	}
	if (arp_rhythm.supress)
		arpbits = 0;
	else if (curarpfinger >= 0 && curarpfinger < 8) {
		arpbits |= 1 << curarpfinger;
	}
	synthfingertrigger |= arpbits;
}

void seq_reset(void ) {
	ticks_since_step = 0;
	seq_divide_counter = 0;
	seq_rhythm.trigcount = 0;
	seq_used_bits = 0;
	seq_dir = 0;
}

void seq_step(int initial) { // initial means - this is the initial clock pulse when switching into play mode
	int seqpatlen = param_eval_int(P_SEQLEN, any_rnd, env16, pressure16);
	int prob = param_eval_int(P_SEQPROB, any_rnd, env16, pressure16);
	bool controlled_by_gatecv = seqdiv < 0;
	if (initial>0) {
		arp_reset();
		seq_divide_counter = 0;
		ticks_since_step = 0;
		seq_used_bits |= ((uint64_t)1) << (cur_step & 63);
		if (!controlled_by_gatecv) {
			bool seqretrig = euclidstuff(&seq_rhythm, seqpatlen, prob, -1);
			(void)seqretrig;
		}
		return;
	}
	seq_divide_counter++;

	if (initial>=0)	if (seq_divide_counter <= seqdiv || controlled_by_gatecv) // if initial is negative, we FORCE a step
		return;
	seq_divide_counter = 0;
	
//	EmuDebugLog("last_step_period %d\n", last_step_period);
	last_step_period = ticks_since_step;
	ticks_since_step = 0;
	seq_divide_counter = 0;
	int prevstep = cur_step;
	if (!isplaying()) {
		seq_rhythm.did_a_retrig = false;
	} else { // playing!
		// actually advance!
		bool seqretrig = euclidstuff(&seq_rhythm, seqpatlen, prob, -1);
		if (!seqretrig)
			return;
		u8 loopstart_step = (rampreset.loopstart_step_no_offset + step_offset) & 63;
		int seqmode = param_eval_int(P_SEQMODE, any_rnd, env16, pressure16);
		switch (seqmode) {
		default:
		case SEQ_FWD:
			set_cur_step(cur_step + 1, true);
			if (cur_step <= prevstep)
				OnLoop();
			break;
		case SEQ_BACK:
			set_cur_step(cur_step - 1, true);
			if (cur_step >= prevstep)
				OnLoop();
			break;
		case SEQ_PAUSE:
			break;
		case SEQ_PINGPONG: {
			bool looped = false;
			int end = rampreset.looplen_step + loopstart_step - 1;
			if (seq_dir == 0 && cur_step >= end) {
				seq_dir = 1;
				looped = true;
			}
			else if (seq_dir == 1 && cur_step <= loopstart_step) {
				seq_dir = 0;
				looped = true;
			}
			set_cur_step(cur_step + (seq_dir ? -1 : 1), true);
			if (looped)
				OnLoop();
			break; }
		case SEQ_PINGPONGREP: {
			int end = rampreset.looplen_step + loopstart_step - 1;
			if (seq_dir == 0 && cur_step >= end) {
				seq_dir = 1;
				set_cur_step(end, true);
				OnLoop();
			}
			else if (seq_dir == 1 && cur_step <= loopstart_step) {
				seq_dir = 0;
				set_cur_step(loopstart_step, true);
				OnLoop();
			}
			else
				set_cur_step(cur_step + (seq_dir ? -1 : 1), true);
			break;
		}
		case SEQ_RANDOM: {
			int len = rampreset.looplen_step & 63;
			uint64_t mask = len ? (((uint64_t)1) << len) - 1 : ~0ull;
			uint64_t bits = mask & ~(seq_used_bits >> loopstart_step); // bitmask of which steps we are allowed to choose from
			bool looped = false;
			if (!bits) {
				seq_used_bits = 0;
				bits = mask; // reset!
				looped = true;
			}
			int n = bits ? rand() % __builtin_popcountll(bits) : 1; // pick a random bit number
			while (n-- > 0) bits &= bits - 1; // peel off the bits
			int step = bits ? __builtin_ctzll(bits) : 0;
			set_cur_step(loopstart_step + step, true);
			if (looped)
				OnLoop();
			break;
		}
		}
		seq_used_bits |= ((uint64_t)1) << (cur_step & 63);
		if (playmode == PLAY_WAITING_FOR_CLOCK_STOP) {
			playmode = PLAY_STOPPED;
			seq_rhythm.did_a_retrig = false;
		}
	}
}


void update_arp(bool clock) {
	arpretrig = false;
//	bool called_trig = false;
	arpmode = ((rampreset.flags & FLAGS_ARP)) ? param_eval_int(P_ARPMODE, any_rnd, env16, pressure16) : -1;
	if (arpmode>=0 && !isgrainpreview()) {
		int div = param_eval_int(P_ARPDIV, any_rnd, env16, pressure16);
		if (div < 0) {
			u32 dclock = (u32)(table_interp(pitches, 32768 + (-div >> 2)) * (1 << 24));
			freearpclock -= dclock;
			clock = freearpclock < 0;
			if (clock) {
				freearpclock += 1<<31;
				arp_divide_counter = 0;
			}
			div = 0;
		}
		else {
			div = divisions[(clampi(div, 0,65535) * DIVISIONS_MAX) >>16]-1;
		}
		if (arp_divide_counter > div)
			arp_divide_counter = 0;
		if (clock) {
			if (arp_divide_counter <= 0) {
//				called_trig = true;
				last_arp_period = ticks_since_arp;
				/*
				if (last_arp_period < 31 || last_arp_period>32) {
					EmuDebugLog("arp %d\r\n", last_arp_period);
					int i = 1;
				}
				*/
				ticks_since_arp = 0;
				arptrig(synthfingerdown_nogatelen);
				arp_divide_counter = 0;
			}
			arp_divide_counter++;
			
		}
		
	}

}

void OnGotReset(void) {
	u8 loopstart_step = (rampreset.loopstart_step_no_offset + step_offset) & 63;
	set_cur_step(loopstart_step, false);
	bpm_clock_phase = 0;
	seq_reset();
	arp_reset();
	seq_step(1);
	OnLoop();
}

extern volatile u8 gotclkin;
volatile u8 gotclkin=0;



int update_clock(void) { // returns 1 for clock, 2 for half clock, 0 for neither
	tick++;
	bool gotclock = false;
	//////////////////////////////////////////// eurorack clock input
//	int gate_input_max = 65536;
	static u8 prevgotclk=0;
	u8 newgotclk=gotclkin;
	if(newgotclk!=prevgotclk) {
		prevgotclk=newgotclk;
		gotclock=true;
		external_clock_enable=true;
	}

	/*
	for (int i = 0; i < ADC_SAMPLES * ADC_CHANS; i += ADC_CHANS) {
		if (adcbuf[i + ADC_CLK] <= 0)
			clk_in_high = true;
		if (adcbuf[i + ADC_RESET] <= 0)
			reset_in_high = true;
		gate_input_max = mini(gate_input_max, adcbuf[i + ADC_GATE]);
	}
	*/


	if (/*(reset_in_high && !reset_in_high_prev) || */got_ui_reset) { // TODO - if audio in level is turned down, look for pulses?
		OnGotReset();
	} else 	if (playmode == PLAYING && seqdiv < 0 && getgatesense()) {
		// gate cv controls step
		static bool curgate_digital = true;
		float curgate = GetADCSmoothed(ADC_GATE);
		float thresh = curgate_digital ? 0.01f : 0.02f;
		bool newgate_digital = curgate > thresh;
		if (newgate_digital && !curgate_digital) {
			seq_step(-1); // force a step
		}
		curgate_digital = newgate_digital;
	}
	got_ui_reset = false;
//	clk_in_high_prev = clk_in_high;
//	reset_in_high_prev = reset_in_high;
#define ACCURATE_FS 31250

	// revert to internal clock after not getting any external clock signal for a second
	if (external_clock_enable && ticks_since_clock >= 500) // a tick is 2ms
		external_clock_enable = false;

	//////////////////////////////////////////// internal clock
	if (!external_clock_enable) {
		bpm10x = ((param_eval_int(P_TEMPO, any_rnd, env16, pressure16) * 1200) >> 16) + 1200;

		u32 dclock = ((1 << 18) * bpm10x) / ((ACCURATE_FS * 600 / 32) / BLOCK_SAMPLES);
		bpm_clock_phase += dclock;
		if (bpm_clock_phase > 1 << 21) {
			bpm_clock_phase &= (1 << 21) - 1;
			gotclock = true;
		}
		//if (playmode == PLAY_WAITING_FOR_CLOCK) {
		//	bpm_clock_phase = 0;
		//	gotclock = true;
		//}
	}
	////////////////////////////////////////////////////// clock advance 
	ticks_since_clock++;
	ticks_since_step++;
	ticks_since_arp++;
	// if (playmode == PLAY_PREVIEW && shift_down==SB_PLAY) -- maybe eat a single clock if the finger is still down?

	if (!gotclock) {
		SetOutputCVClk(0); // trigger style clock
		if (ticks_since_clock == last_clock_period / 2) {
			SetOutputCVClk(0);
			seq_step(0);
			return 2;
		}
		return 0;
	}
	SetOutputCVClk(65535);
	if (external_clock_enable) {
		// figure out bpm from the external clock's last 2 periods
		float avgclockperiod_per_sec = (ticks_since_clock + last_clock_period) * (0.5f * BLOCK_SAMPLES / ACCURATE_FS);
		float guessed_bpm = (1200.f / 8.f) / maxf(1.f / 16.f, avgclockperiod_per_sec);
		guessed_bpm+=(bpm10x-guessed_bpm)*(1.f-0.2f); // smooth it a bit
		bpm10x = (int)(guessed_bpm+0.5f);
	}		

	last_clock_period = ticks_since_clock;
	ticks_since_clock = 0;
	int initial = 0;
	if (playmode == PLAY_WAITING_FOR_CLOCK_START) {
		playmode = PLAYING;
		initial = 1;
		// also sync the arp!
		arp_reset();
	}
	seq_step(initial);
	return true;
}
