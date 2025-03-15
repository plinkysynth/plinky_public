# v0.B1 - Sequencer, Latch and Midi rewrite
## Deletions
- Latching and sequencing midi notes was never fully implemented, it is now fully removed. This fixes instances where incoming midi notes would clear out the sequencer
- Sequencing movement on the A/B knobs was partly implemented but undocumented. It is now removed but could be added again in a later update
## General improvements
- During the release phase of envelopes, pitches would jump to either the top of the string or the last pressed pitch. This was especially audible with longer release times and on sequenced and midi notes. It sounded like new, unwanted notes were being triggered, but in reality it was the pitch suddenly jumping to a different value. Pitch is now played correctly during the release phase
- In many situations, pressing any of the shift-pads (bottom row) would lead to unexpected results, such as latched or sequenced notes being muted or cut off. This is no longer the case
- The display now shows whether each of the strings is pressed (full bar), ringing out (horizontal line indicates volume level) or silent (horizontal line lies at bottom of screen.)
- The octave range is brought back to +/- 3 octaves to limit the amount of out-of-range pads at the upper and lower extremes
## Voice allocation
The voice allocation was rewritten and now works as follows:
1. Touches (including latching touches)
2. Sequencer
3. Midi

Touches and sequenced notes are always linked to a string (column,) so a touched string takes preference over a sequenced note on that string. Midi notes do not have a string by default, they are mapped in the following way:
1. A quiet string - as close as possible to a string that could play the midi pitch
2. When no quiet strings are available, the most quiet string which is in its release phase
3. When no strings are quiet or in their release phase, all strings are actively being played. The midi note is ignored
## CV Gate
- CV gate is processed after touches, sequencer and midi are handled. Whatever pressure is the result of those gets multiplied by the CV gate voltage: 0 - 5V equals 0 - 100%
- A low CV gate can no longer un-latch latched notes
## Latching
- Latching responds to touched pads
- Latching now only resets when all fingers are lifted off the pads. This allows for a whole new level of creativity while playing drones or arps, and is by far my favorite new feature
- Latching could complicate step-recording, as both respond to pads being pressed. For this reason, you can't start a new latch while in step-recording mode. (A latch will keep sounding when you enable step-recording, but any newly pressed pads will not be latched.)
## Sequencing
- Overdubbing is now fully implemented in both step and live recording modes. If a sequencer step has sequenced notes on one string, all the other strings can be sequenced without removing the existing notes.
- Substeps are now fully implemented in both step and live recording modes. Each step has eight substeps
  - Live recording captures your playing in that resolution
  - Step recording records your note for as long as you hold it:
    - Holding a pad for less than the step-length will start at the beginning of the step and only record for as long as you press down
    - Holding a pad for more than the step-length will save the last(!) step-length of pads your pressed before releasing
    - It is possible to record different pitches into substeps in step-recording simply by playing them and lifting your finger immediately after
- In live-record mode, latched notes will be recorded as played, respecting substeps
- There was a bug where if a note was sequenced on the current step and step-recording was active, the sequencer would take a step forward at the release of a midi note, or at the end of previewing the sequenced note. This bug is fixed
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
  