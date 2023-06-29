#pragma once

#define WAVETABLE_SIZE (1022+9) // 9 octaves, top octave is 512 samples

enum EWavetables {
	WT_SAW,
	WT_SQUARE,
	WT_SIN,
	WT_SIN_2,
	WT_FM,
	WT_SIN_FOLD1,
	WT_SIN_FOLD2,
	WT_SIN_FOLD3,
	WT_NOISE_FOLD1,
	WT_NOISE_FOLD2,
	WT_NOISE_FOLD3,
	WT_WHITE_NOISE,
	WT_UNUSED1,
	WT_UNUSED2,
	WT_UNUSED3,
	WT_UNUSED4,
	WT_UMUSED5,
	WT_LAST,
};
const char* const wavetablenames[WT_LAST] = {
	"Saw",
	"Square",
	"Sin",
	"Sin2",
	"FM",
	"SinFold1",
	"SinFold2",
	"SinFold3",
	"ZZZ",
	"ZZZFold1",
	"ZZZFold2",
	"Noise",
	"TODO1",
	"TODO2",
	"TODO3",
	"TODO4",
	"TODO5",
};
