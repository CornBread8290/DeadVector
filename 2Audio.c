#include <windows.h>
#include "3utils.h"
#include "2Audio.h"

#define BLOCK_FRAMES 512
#define NUM_BLOCKS   3
#define MAX_VOICES   16
#define ITD_MAX      14
#define DL_LEN       32
#define ECHO_LEN     6144        // shared echo bus, ~0.28 s

typedef struct {
    volatile int active;
    int spatial, type;
    unsigned fx;
    SndPull pull; void* user;
    float pitch, gain;
    Vec3 pos;
    float t, last;
    float muffle, bq1, bq2;      // effect chain state
    float lpL, lpR;              // per-ear head-shadow state
    float dl[DL_LEN]; int dw;    // mono delay line for ITD
} SndVoice;

// Heap-allocated in snd_init so the zeroed buffers don't end up in the exe
typedef struct {
    int16_t pcm[NUM_BLOCKS][BLOCK_FRAMES * 2];
    float echoL[ECHO_LEN], echoR[ECHO_LEN];
    float busL[BLOCK_FRAMES], busR[BLOCK_FRAMES];
    float mono[BLOCK_FRAMES], dryE[BLOCK_FRAMES];
} SndBuffers;

static SndVoice voices[MAX_VOICES];
static HWAVEOUT dev;
static WAVEHDR hdrs[NUM_BLOCKS];
static SndBuffers* buf;
static HANDLE ev, thread;
static volatile int running;
static Vec3 lis_pos;
static Quat lis_rot = {0, 0, 0, 1};
static int echo_w;
static float rb0, rb1, rb2, ra1, ra2; // radio bandpass coefficients

static float mono_sample(SndVoice* v) {
    float t = v->t, s = 0;
    switch (v->type) {
        case SND_BEEP:
            s = sinf(pi2 * v->pitch * t) * expf(-3.0f * t);
            if (t > 1.0f) v->active = v->fx & SND_FX_LOOP ? (v->t = 0, 1) : 0;
            break;
        case SND_ENGINE:
            s = xorshift32f() * 2.0f - 1.0f;
            v->last += 0.05f * v->pitch * (s - v->last);
            s = v->last * 4.0f;
            break;
        case SND_GUN:
            s = sinf(t / (1.0f + 8.2f * t) * 25000.0f * v->pitch);
            if (t > 0.25f) v->active = v->fx & SND_FX_LOOP ? (v->t = 0, 1) : 0;
            break;
    }
    v->t = t + 1.0f / SND_RATE;
    return s;
}

static void mix_block(int16_t* out) {
    float *busL = buf->busL, *busR = buf->busR, *mono = buf->mono, *dryE = buf->dryE;
    float *echoL = buf->echoL, *echoR = buf->echoR;
    memset(busL, 0, sizeof buf->busL);
    memset(busR, 0, sizeof buf->busR);
    memset(dryE, 0, sizeof buf->dryE);

    for (int vi = 0; vi < MAX_VOICES; ++vi) {
        SndVoice* v = &voices[vi];
        if (!v->active) continue;

        // Source
        int n = BLOCK_FRAMES;
        if (v->pull) {
            n = v->pull(v->user, mono, BLOCK_FRAMES);
            for (int i = n; i < BLOCK_FRAMES; ++i) mono[i] = 0;
            if (n < BLOCK_FRAMES) v->active = 0;
        } else {
            int i = 0;
            for (; i < BLOCK_FRAMES && v->active; ++i) mono[i] = mono_sample(v);
            for (; i < BLOCK_FRAMES; ++i) mono[i] = 0;
        }

        // Effect chain
        if (v->fx & SND_FX_MUFFLE)
            for (int i = 0; i < BLOCK_FRAMES; ++i)
                mono[i] = v->muffle += 0.08f * (mono[i] - v->muffle);
        if (v->fx & SND_FX_RADIO)
            for (int i = 0; i < BLOCK_FRAMES; ++i) {
                float x = mono[i] * 3.0f;
                float y = rb0 * x + v->bq1;
                v->bq1 = rb1 * x - ra1 * y + v->bq2;
                v->bq2 = rb2 * x - ra2 * y;
                mono[i] = y / (1.0f + fabsf(y));
            }

        // Spatial parameters (per block is plenty)
        float att = 1.0f, side = 0.0f, shadow = 1.0f;
        if (v->spatial) {
            Vec3 rel = quat_rotate_vec3(quat_conjugate(lis_rot), vec3_sub(v->pos, lis_pos));
            float d = sqrtf(vec3_dot(rel, rel)) + 1e-4f;
            att = 1.0f / (1.0f + 0.02f * d * d);
            side = rel.x / d;                       // -1 left .. +1 right
            shadow = 1.0f - 0.75f * fabsf(side);    // far-ear lowpass amount
            if (rel.z > 0.0f) att *= 1.0f - 0.3f * rel.z / d; // behind the head
        }
        float g = v->gain * att;
        float gl = g * sqrtf(0.5f * (1.0f - 0.85f * side));
        float gr = g * sqrtf(0.5f * (1.0f + 0.85f * side));
        int dLeft = side > 0 ? (int)(side * ITD_MAX) : 0;   // far ear lags
        int dRight = side < 0 ? (int)(-side * ITD_MAX) : 0;

        for (int i = 0; i < BLOCK_FRAMES; ++i) {
            v->dl[v->dw] = mono[i];
            float sl = v->dl[(v->dw - dLeft) & (DL_LEN - 1)];
            float sr = v->dl[(v->dw - dRight) & (DL_LEN - 1)];
            v->dw = (v->dw + 1) & (DL_LEN - 1);
            // head shadow: filter whichever ear is delayed
            if (dLeft)  sl = v->lpL += shadow * (sl - v->lpL);
            if (dRight) sr = v->lpR += shadow * (sr - v->lpR);
            busL[i] += sl * gl;
            busR[i] += sr * gr;
            if (v->fx & SND_FX_ECHO) dryE[i] += mono[i] * g * 0.4f;
        }
    }

    for (int i = 0; i < BLOCK_FRAMES; ++i) {
        float oldL = echoL[echo_w], oldR = echoR[echo_w];
        busL[i] += oldL;
        busR[i] += oldR;
        echoL[echo_w] = dryE[i] + oldR * 0.42f;
        echoR[echo_w] = dryE[i] + oldL * 0.42f;
        echo_w = (echo_w + 1) % ECHO_LEN;
    }

    for (int i = 0; i < BLOCK_FRAMES; ++i) {
        float l = busL[i], r = busR[i];
        l = l < -1 ? -1 : l > 1 ? 1 : l;
        r = r < -1 ? -1 : r > 1 ? 1 : r;
        out[i * 2]     = (int16_t)(l * 32767.0f);
        out[i * 2 + 1] = (int16_t)(r * 32767.0f);
    }
}

static DWORD WINAPI mixer_thread(void* arg) {
    while (running) {
        WaitForSingleObject(ev, 100);
        for (int b = 0; b < NUM_BLOCKS; ++b)
            if (hdrs[b].dwFlags & WHDR_DONE) {
                mix_block(buf->pcm[b]);
                hdrs[b].dwFlags &= ~WHDR_DONE;
                waveOutWrite(dev, &hdrs[b], sizeof(WAVEHDR));
            }
    }
    return 0;
}

int snd_init(void) {
    WAVEFORMATEX wfx = {
        .wFormatTag = WAVE_FORMAT_PCM, .nChannels = 2,
        .nSamplesPerSec = SND_RATE, .wBitsPerSample = 16,
        .nBlockAlign = 4, .nAvgBytesPerSec = SND_RATE * 4
    };
    buf = (SndBuffers*)calloc(1, sizeof *buf);
    if (!buf) return 0;
    ev = CreateEvent(0, FALSE, FALSE, 0);
    if (waveOutOpen(&dev, WAVE_MAPPER, &wfx, (DWORD_PTR)ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
        return 0;

    {
        float w = pi2 * 1100.0f / SND_RATE, a = sinf(w) / 1.6f, inv = 1.0f / (1.0f + a);
        rb0 = a * inv; rb1 = 0; rb2 = -a * inv;
        ra1 = -2.0f * cosf(w) * inv; ra2 = (1.0f - a) * inv;
    }

    running = 1;
    for (int b = 0; b < NUM_BLOCKS; ++b) {
        hdrs[b].lpData = (LPSTR)buf->pcm[b];
        hdrs[b].dwBufferLength = sizeof buf->pcm[b];
        waveOutPrepareHeader(dev, &hdrs[b], sizeof(WAVEHDR));
        mix_block(buf->pcm[b]);
        waveOutWrite(dev, &hdrs[b], sizeof(WAVEHDR));
    }
    thread = CreateThread(0, 0, mixer_thread, 0, 0, 0);
    return 1;
}

void snd_shutdown(void) {
    running = 0;
    if (thread) { WaitForSingleObject(thread, 500); CloseHandle(thread); }
    if (dev) { waveOutReset(dev); waveOutClose(dev); dev = 0; }
    if (ev) CloseHandle(ev);
    free(buf); buf = 0;
}

void snd_listener(Vec3 pos, Quat rot) { lis_pos = pos; lis_rot = rot; }

static int start_voice(int type, SndPull fn, void* user, float pitch,
                       float gain, unsigned fx, const Vec3* pos) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        SndVoice* v = &voices[i];
        if (v->active) continue;
        memset(v, 0, sizeof *v);
        v->type = type; v->pull = fn; v->user = user;
        v->pitch = pitch; v->gain = gain; v->fx = fx;
        if (pos) { v->spatial = 1; v->pos = *pos; }
        v->active = 1;
        return i;
    }
    return -1;
}

int snd_play(int type, float pitch, float gain, unsigned fx) {
    return start_voice(type, 0, 0, pitch, gain, fx, 0);
}
int snd_play_at(int type, float pitch, float gain, unsigned fx, Vec3 pos) {
    return start_voice(type, 0, 0, pitch, gain, fx, &pos);
}
int snd_play_stream(SndPull fn, void* user, float gain, unsigned fx) {
    return start_voice(0, fn, user, 1, gain, fx, 0);
}
int snd_play_stream_at(SndPull fn, void* user, float gain, unsigned fx, Vec3 pos) {
    return start_voice(0, fn, user, 1, gain, fx, &pos);
}
void snd_move(int voice, Vec3 pos) {
    if (voice >= 0 && voice < MAX_VOICES) voices[voice].pos = pos;
}
void snd_stop(int voice) {
    if (voice >= 0 && voice < MAX_VOICES) voices[voice].active = 0;
}
