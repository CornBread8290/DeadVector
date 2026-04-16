#ifndef AUDIO_H
#define AUDIO_H

#include "defs.h"

#define SND_RATE 22050

enum SoundType {
    SND_BEEP = 0,
    SND_ENGINE,
    SND_GUN
};

// Per-voice effect chain, applied before spatialization.
enum {
    SND_FX_MUFFLE = 1 << 0,
    SND_FX_RADIO  = 1 << 1,
    SND_FX_ECHO   = 1 << 2,
    SND_FX_LOOP   = 1 << 3,
};

typedef int (*SndPull)(void* user, float* out, int n);

int  snd_init(void);
void snd_shutdown(void);
void snd_listener(Vec3 pos, Quat rot); // call once per frame

int  snd_play(int type, float pitch, float gain, unsigned fx);
int  snd_play_at(int type, float pitch, float gain, unsigned fx, Vec3 pos);
int  snd_play_stream(SndPull fn, void* user, float gain, unsigned fx);
int  snd_play_stream_at(SndPull fn, void* user, float gain, unsigned fx, Vec3 pos);
void snd_move(int voice, Vec3 pos); // update a positioned voice
void snd_stop(int voice);

#endif // AUDIO_H
