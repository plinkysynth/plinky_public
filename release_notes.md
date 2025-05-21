# Plinky Firmware Release Notes

this file serves as a log of changes to the firmware. with many thanks to RJ, who has been providing most of the recent fixes to the firmware, and provided the inaugural B1 notes. 

From newest to oldest:

# v0.B2 - Add Plinky+ detection & substep timing fix

When step-recording, the timing for the substep was not handled well, which lead to unexpected results. Now it's clearing last_edited_step_global every time the sequencer moves to a new step, which should make each new step-record start recording at the first substep. Also I made a small tweak to how the substep is calculated when step-recoring a long press. (A press that is longer than the step itself.)

# v0.B1 - Sequencer, Latch and Midi rewrite
## Deletions
- Latching and sequencing midi notes was never fully implemented, it is now fully removed. This fixes instances where incoming midi notes would clear out the sequencer
- Sequencing movement on the A/B knobs was partly implemented but undocumented. It is now removed but could be added again in a later update
## General improvements
- During the release phase of envelopes, pitches would jump to either the top of the string or the last pressed pitch. This was especially audible with longer release times and on sequenced and midi notes. It sounded like new, unwanted notes were being triggered, but in reality it was the pitch suddenly jumping to a different value. Pitch is now played correctly during the release phase
- In many situations, pressing any of the shift-pads (bottom row) would lead to unexpected results, such as latched or sequenced notes being muted or cut off. This is no longer the case
- The display now shows whether each of the strings is pressed (full bar), ringing out (horizontal line indicates volume level) or silent (horizontal line lies at bottom of screen.)
- The lfo scope on the right of the display now always shows contiguous lines
  - When an lfo value is too high/low to fit on the scope, it is clamped to the top/bottom pixel instead
  - Vertical jumps are now also drawn
## Voice allocation
The voice allocation was rewritten and now works as follows:
1. Touches (including latching touches)
2. Sequencer
3. Midi

Touches and sequenced notes are always linked to a string/column. A touched string takes preference over a sequenced note on that string. Midi notes do not have a string by default, they are mapped in the following way:
1. To a quiet string as close as possible to a string that could play the midi pitch
2. When no quiet strings are available, to the most quiet string which is in its release phase
3. When no strings are quiet or in their release phase, all strings are actively being played. The midi note is ignored
## CV Gate
- CV gate is processed after touches, sequencer and midi are handled. Whatever pressure is the result of those gets multiplied by the CV gate voltage: 0 - 5V equals 0 - 100%
- A low CV gate can no longer un-latch latched notes
## Latching
- Latching now only resets when all fingers are lifted off the pads. This allows for a whole new level of creativity while playing drones or arps, and is by far my favorite new feature
- Latching could complicate step-recording, as both respond to pads being pressed and released. For this reason, you can't start a new latch while in step-recording mode. An existing latch will keep sounding as step-record mode is activated, and can even be modified as long as at least one finger is touching the Plinky. But new latches will not be initiated.
## Sequencing
- Overdubbing is now fully implemented in both step and live recording modes. If a sequencer step has sequenced notes on some strings, all other strings can be sequenced without clearing existing notes.
- Substeps are now fully implemented in both step and live recording modes. For each step, the sequencer has four substeps for touch position (pitches) and eight substeps for pressure - evenly divided over the step
  - Live recording captures your playing in the described resolution
  - Step recording records your touch for as many substeps as you hold it and leaves the rest of the step empty
  - Pitch-changes will be recorded into substeps in both live and step-record mode
  - In step-record mode, holding a pad for longer than the step-length will save the last(!) step-length of touches before you lift your finger
- In live-record mode, latched notes will be recorded as played
- In step-record mode, latches that were started before step-record mode was activated will be recorded into the active step
- There was a bug where if a note was sequenced on the current step and step-recording was active, the sequencer would automatically take a step forward at the release of a midi note, or at the end of previewing the sequenced note. This bug is fixed
- Sequencer gate length no longer affects notes played by hand
## Midi
- Midi was quite unreliable and was rewritten to fully adhere to the midi standard. Responding to incoming midi notes and pressure now works as expected
- Midi notes now try to map to a string its pitch can be played on:
  - When a midi note comes in, the pitch of the midi note is compared to the pitches that the pads can play. The string whose center pitch is closest to the midi pitch becomes the preferred string
  - If the preferred string is not available, the closest available string is selected. Most of the time, at least the two adjacent strings can also play this midi pitch
  - see [voice allocation](#voice-allocation) for full details on voice allocation
- Midi notes light up the Plinky LEDs at the correct location
  - If the midi note can be played on its assigned string, the pad with the same pitch as the midi note lights up
  - If the midi note is an accidental of Plinky's scale, the pad of the closest lower scale pitch lights up
  - If the midi note is fully higher or lower than the pitches on its assigned string, the string's top or bottom pad respectively lights up
  
  # Earlier revisions

  These notes are taken from `config.h`

  | Version | Changes |
  |---------|---------|
  | 0.B2 | Pin detection for Plinky+ - boot the alternate display code for SSD1305 display driver |
  | 0.B1 | More RJ - lfo drawing is nicer; adc is not so smoothed? |
  | 0.B0 | MORE RJ MAGIC - https://github.com/plinkysynth/plinky_public/pull/40 - what a beast he is |
  | 0.A9 | Sequencer fixes, or more of a rewrite, from RJ-Eckie. absolutely incredible work. thankyou |
  | 0.A8 | Midi fixes from RJ-Eckie. Many thanks! |
  | 0.A7 | Bootloader work to make it work on MacOSX |
  | 0.A6 | Set Channels for MIDI in and out (unused pad) |
  | 0.A5 | Encoder fix |
  | 0.A4 | Disabled MERGE_PLAYBACK in touch.h. DISABLE_AUTOSAVE flag enables Demo Mode - changes are not written to flash. Disables ProgramPage() in params.h |
  | 0.A3 | Fixes a USB bug in which some hosts wouldn't recognize Plinky. New startup sequence is plinky_init(), then midiinit() |
  | 0.A2 | Allows for playing notes from the touch surface over a recorded sequence. Still buggy. In touch.h #define MERGE_PLAYBACK 1 |
  | 0.A1 | HW SERIAL MIDI SYNC & More CCs |
  | 0.A | USB MIDI Sync |
  | 0.9z | fix shimmer click? thanks hippo! |
  | 0.9y | fix wavetable off-by-2 - thanks Jan Matthis! |
  | 0.9x | added diminished scale |
  | 0.9w | finally fix the 'last slice gets trashed' bug, and add a saw shape :) |
  | 0.9v | flip the lights for root note, doh. SERIAL MIDI IN IS OFF |
  | 0.9u | fix note sample mode, make latch more reliable |
  | 0.9t | hilite root note |
  | 0.9s | make it so that the stupid notename doesnt overrun memory, doh |
  | 0.9r | make it so encoder click doesnt reset until release, so that long-press doesnt reset. also increase octave by 1 |
  | 0.9q | move the wavetable to the end of flash |
  | 0.9p | rescale cv in and expander cv out to 6v peak |
  | 0.9o | fix gain on expander outputs EXPANDER_GAIN |
  | 0.9n | change expander to output full mod src not just lfo |
  | 0.9m | stereo width, patch names & categories |
  | 0.9l | add accel test |
  | 0.9k | refine midi out a bit |
  | 0.9j | first version of midi out, also expander outputs lfo vals |
  | 0.9i | possible fix for midi24ppqncounter |
  | 0.9h | new wavetable header |
  | 0.9g | ?? |
  | 0.9f | midi refinements |
  | 0.9e | midi note allocation change |
  | 0.9d | bunch of small fixes! encoder click toggles arp & latch; pressure display on right of screen; note display; arp & latch show correct values, not 0.0; encoder clicks toggles between default and 0; long encoder click clears all modulation; negative strides no longer allowed; add accelerometer sensitivity parameter; LATCH IS NOW PER PRESET! not global; audio in now causes cute ripples in the LEDs from the bottom left corner; fixes for LEDs including mod A src not lighting; when choosing a parameter, all mod src's that are active light up; when choosing a mod src, all params affected by that mod src light up |
  | 0.9c | add accel sensitivity, increase ripples |
  | 0.9b | meska a/b overflow fix |
  | 0.9a | accelerometer proto special edition |
  | 0.92 | add a bit of diagnostics to calibration to try to detect shorts/ncs |
  | 0.91 | change drive to distort; add hysteresis to shift key; make touch less errory |
  | 0.9 | click encoder to zero value |
  | 0.8 | fix stereo pan when changing waveshape, and the broken wipe pattern |
  | 0.7 | flashed onto first units |
