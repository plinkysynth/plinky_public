#pragma once

//#define CALIB_TEST

#define I2C_TIMEOUT 20

#define PEAK_INPUT_CV 5.f
#define IN_CV_SCALE (5.f/PEAK_INPUT_CV) // rescale inputs so PEAK_INPUT_CV is full scale, not 5v!
#define EXPANDER_GAIN (0.715f/IN_CV_SCALE) // thanks to discord CrazyEmperor893, measured output on expander is 
						     // higher than the CV input. so we scale by this much to make them match


#define NEW_PINOUT // betas and finals need this on
#define NEW_LAYOUT // final front panel layout. beta testers need this OFF

// THIS SHOULD NOT BE DEFINED FOR REAL BOARDS:
//#define HALF_FLASH // pcbway fitted the wrong CPU, with only half the flash ram! this disables all use of the upper 512k, to enable testing

//#define DISABLE_AUTOSAVE // enables Demo Mode - changes are not written to flash. Disables ProgramPage() in params.h

// 0.7 - flashed onto first units
// 0.8 - fix stereo pan when changing waveshape, and the broken wipe pattern
// 0.9 - click encoder to zero value
// 0.91 - change drive to distort; add hysteresis to shift key; make touch less errory
// 0.92 - add a bit of diagnostics to calibration to try to detect shorts/ncs
// 0.9a - accelerometer proto special edition
// 0.9b - meska a/b overflow fix
// 0.9c - add accel sensitivity, increase ripples
// 0.9d - bunch of small fixes! encoder click toggles arp & latch; pressure display on right of screen; note display;  arp & latch show correct values, not 0.0; encoder clicks toggles between default and 0; long encoder click clears all modulation; negative strides no longer allowed; add accelerometer sensitivity parameter; LATCH IS NOW PER PRESET! not global; audio in now causes cute ripples in the LEDs from the bottom left corner; fixes for LEDs including mod A src not lighting; when choosing a parameter, all mod src's that are active light up; when choosing a mod src, all params affected by that mod src light up;
// 0.9e - midi note allocation change
// 0.9f - midi refinements
// 0.9g - ??
// 0.9h - new wavetable header
// 0.9i - possible fix for midi24ppqncounter
// 0.9j - first version of midi out, also expander outputs lfo vals
// 0.9k - refine midi out a bit
// 0.9l - add accel test
// 0.9m - stereo width, patch names & categories
// 0.9n - change expander to output full mod src not just lfo
// 0.9o - fix gain on expander outputs EXPANDER_GAIN
// 0.9p - rescale cv in and expander cv out to 6v peak
// 0.9q - move the wavetable to the end of flash
// 0.9r - make it so encoder click doesnt reset until release, so that long-press doesnt reset. also increase octave by 1
// 0,9s - make it so that the stupid notename doesnt overrun memory, doh.
// 0.9t - hilite root note
// 0.9u - fix note sample mode, make latch more reliable
// 0.9v - flip the lights for root note, doh. SERIAL MIDI IN IS OFF
// 0.9w - finally fix the 'last slice gets trashed' bug, and add a saw shape :)
// 0.9x - added diminished scale
// 0.9y - fix wavetable off-by-2 - thanks Jan Matthis!
// 0.9z - fix shimmer click? thanks hippo!
// 0.A - USB MIDI Sync
// 0.A1 - HW SERIAL MIDI SYNC & More CCs
// 0.A2 - Allows for playing notes from the touch surface over a recorded sequence. Still buggy. In touch.h #define MERGE_PLAYBACK 1
// 0.A3 - Fixes a USB bug in which some hosts wouldn't recognize Plinky. New startup sequence is plinky_init(), then midiinit().
// 0.A4 - Disabled MERGE_PLAYBACK in touch.h. DISABLE_AUTOSAVE flag enables Demo Mode - changes are not written to flash. Disables ProgramPage() in params.h
// 0.A5 - Encoder fix
// 0.A6 - Set Channels for MIDI in and out (unused pad)
// 0.A7 - Bootloader work to make it work on MacOSX
// 0.A8 - Midi fixes from RJ-Eckie. Many thanks!
// 0.A9 - Sequencer fixes, or more of a rewrite, from RJ-Eckie. absolutely incredible work. thankyou.
// 0.B0 - MORE RJ MAGIC - https://github.com/plinkysynth/plinky_public/pull/40 - what a beast he is.
#define VERSION2			  "v0.B0"

// 0.X0 - experimental build (SSD1305)
//#define SSD1305
//#define VERSION2			  "v0.X0"

// the bootloader is manually copied to the file golden_bootloader.bin
// makebin.py uses it to make a UF2 file containing the bootloader + latest firmware together
// it also checksums the firmware and prints out the value. it should match this value

#define GOLDEN_CHECKSUM 0xb5a7228c
