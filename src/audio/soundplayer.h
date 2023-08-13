#ifndef _SOUND_PLAYER_H
#define _SOUND_PLAYER_H

#include <ultra64.h>
#include "math/vector3.h"
#include "math/quaternion.h"

#define MAX_SOUNDS 4

#define SOUND_SAMPLE_RATE 44100

#define SOUND_ID_NONE -1

extern char _soundsSegmentRomStart[];
extern char _soundsSegmentRomEnd[];
extern char _soundsTblSegmentRomStart[];
extern char _soundsTblSegmentRomEnd[];

void soundPlayerInit();
void soundPlayerUpdate();
ALSndId soundPlayerPlay(int soundClipId, float volume, float pitch, int panning);
float soundClipDuration(int soundClipId, float pitch);
void soundPlayerStop(ALSndId soundId);
void soundPlayerStopAll();

void soundPlayerPause();
void soundPlayerResume();

void soundPlayerAdjustVolume(ALSndId soundId, float newVolume);

int soundPlayerIsPlaying(ALSndId soundId);
float soundPlayerTimeLeft(ALSndId soundId);

#endif