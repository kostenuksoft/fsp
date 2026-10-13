#ifndef FSP_MIDI_H
#define FSP_MIDI_H

#include "fsp_def.h"

const i32 Midi_ProgramCount = 128;
const i32 Midi_MelodicChannel = 0;
const i32 Midi_DrumChannel = 9;
const i32 Midi_DrumBank = 128;
const i32 Midi_BankCount = Midi_DrumBank + 1;
const i32 Midi_MaxVelocity = 127;
const i32 Midi_MaxPolyphony = 128;
const i32 Midi_CentsPerKey = 100;

const u32 Midi_ControlChange = 0xB0;
const u32 Midi_ModWheel = 1;
const u32 Midi_Data1Shift = 8;
const u32 Midi_Data2Shift = 16;

#endif
