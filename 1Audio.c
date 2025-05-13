#include <windows.h>
#include "defs.h"

#define NUM_CHANNELS 2
#define BITS_PER_SAMPLE 16
#define HEADER_SIZE 44
#define SAMPLE_RATE 4000

#define MAX_CHANNELS 64


static float dir0;

typedef struct {
    HWAVEOUT hWaveOut;
    WAVEHDR waveHdr;
} SoundChannel;

static SoundChannel soundChannels[MAX_CHANNELS] = {0}; // MAX_CHANNELS defined elsewhere
int channels;

void stopSound(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;
    
    SoundChannel* sc = &soundChannels[channel];
    
    if (sc->hWaveOut) {
        waveOutReset(sc->hWaveOut); // Immediately stops playback
        waveOutUnprepareHeader(sc->hWaveOut, &sc->waveHdr, sizeof(WAVEHDR));
        waveOutClose(sc->hWaveOut);
        HeapFree(GetProcessHeap(), 0, sc->waveHdr.lpData);
        sc->hWaveOut = NULL;
    }
}

void init_channel(SoundChannel* channel) {
    channel->hWaveOut = NULL;
    memset(&channel->waveHdr, 0, sizeof(WAVEHDR));
}


void playSoundEffect(int type, float pitch, float volume, float pan, int channel) {
    // Validate channel
    if (channel < 0 || channel >= MAX_CHANNELS) return;
    
    SoundChannel* sc = &soundChannels[channel];
    
    // Clean up previous sound if done
    if (sc->hWaveOut) {
        if (sc->waveHdr.dwFlags & WHDR_DONE) {
            waveOutUnprepareHeader(sc->hWaveOut, &sc->waveHdr, sizeof(WAVEHDR));
            waveOutClose(sc->hWaveOut);
            sc->hWaveOut = NULL;
            HeapFree(GetProcessHeap(), 0, sc->waveHdr.lpData);
        } else {
            return; // Channel busy
        }
    }

    // Generate sound
    int durationMs;
    int sampleRate = 44100;
    switch (type){
        case SND_BEEP:
            durationMs = 1000;
            break;
        case SND_ENGINE:
            durationMs = 10000;
            sampleRate = 4000;
            break;
        case SND_GUN:
            durationMs = 200;
            break;
        default:
            durationMs = 2000;
    }
    
    int samplesPerChannel = (sampleRate * durationMs) / 1000;
    int numSamples = samplesPerChannel * NUM_CHANNELS;

    int16_t* sndBuffer = (int16_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, numSamples * sizeof(int16_t));
    if (!sndBuffer) return;

    float last = 0.0f;
    for (int i = 0; i < samplesPerChannel; i++) {
        float t = (float)i / sampleRate;
        float s = 0;

        switch (type) {
            case SND_BEEP:
                s = sinf(pi2 * pitch * t) * volume;
                break;
            case SND_ENGINE:
                s = (xorshift32f()) * volume*100000;
                s = last * 0.95f + s * 0.05f;
                last = s;
                break;
            case SND_GUN: {
                s=sin(t/(1+8.2f*t)*25000)*volume;
                break;
            }
        }

        int16_t lv = (int16_t)(s * (1.0f - pan) * volume * 32767.0f);
        int16_t rv = (int16_t)(s * (1.0f + pan) * volume * 32767.0f);
        lv = lv < -32768 ? -32768 : lv > 32767 ? 32767 : lv;
        rv = rv < -32768 ? -32768 : rv > 32767 ? 32767 : rv;

        sndBuffer[i * 2 + 0] = lv; // L
        sndBuffer[i * 2 + 1] = rv; // R
    }

    // Setup wave format
    WAVEFORMATEX wfx = {
        .wFormatTag = WAVE_FORMAT_PCM,
        .nChannels = NUM_CHANNELS,
        .nSamplesPerSec = sampleRate,
        .wBitsPerSample = BITS_PER_SAMPLE,
        .nBlockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8),
        .nAvgBytesPerSec = sampleRate * NUM_CHANNELS * (BITS_PER_SAMPLE / 8),
        .cbSize = 0
    };

    // Play sound
    if (waveOutOpen(&sc->hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        HeapFree(GetProcessHeap(), 0, sndBuffer);
        return;
    }

    sc->waveHdr.lpData = (LPSTR)sndBuffer;
    sc->waveHdr.dwBufferLength = numSamples* sizeof(int16_t);
    sc->waveHdr.dwFlags = 0;
    waveOutPrepareHeader(sc->hWaveOut, &sc->waveHdr, sizeof(WAVEHDR));
    waveOutWrite(sc->hWaveOut, &sc->waveHdr, sizeof(WAVEHDR));
}