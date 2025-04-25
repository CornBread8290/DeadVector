#include <windows.h>
#include "defs.h"

#define NUM_CHANNELS 2
#define BITS_PER_SAMPLE 8
#define HEADER_SIZE 44
#define SAMPLE_RATE 4000

static HWAVEOUT hWaveOut = NULL;
static WAVEHDR waveHdr = {0};
static uint8_t* sndBuffer = NULL;

void stopSound() {
    if (hWaveOut) {
        waveOutReset(hWaveOut);
        if (waveHdr.lpData) waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
        if (sndBuffer) HeapFree(GetProcessHeap(), 0, sndBuffer);
        waveOutClose(hWaveOut);
        hWaveOut = NULL;
        sndBuffer = NULL;
    }
}

void playSoundEffect(int type, float pitch, float volume, float pan) {
    if (hWaveOut) return; // Already playing
    float leftGain  = (1.0f - pan) * 0.5f;
    float rightGain = (1.0f + pan) * 0.5f;  

    int duration = 20;
    int numSamples = (int)(SAMPLE_RATE * duration * NUM_CHANNELS);
    int bufferSize = HEADER_SIZE + numSamples;

    sndBuffer = (uint8_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferSize);
    //if (!sndBuffer) return;

    // WAV HEADER
    memcpy(sndBuffer, "RIFF", 4);
    *(uint32_t*)(sndBuffer + 4) = 36 + numSamples;
    memcpy(sndBuffer + 8, "WAVEfmt ", 8);
    *(uint32_t*)(sndBuffer + 16) = 16;
    *(uint16_t*)(sndBuffer + 20) = 1;
    *(uint16_t*)(sndBuffer + 22) = NUM_CHANNELS;
    *(uint32_t*)(sndBuffer + 24) = SAMPLE_RATE;
    *(uint32_t*)(sndBuffer + 28) = SAMPLE_RATE * NUM_CHANNELS;
    *(uint16_t*)(sndBuffer + 32) = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    *(uint16_t*)(sndBuffer + 34) = BITS_PER_SAMPLE;
    memcpy(sndBuffer + 36, "data", 4);
    *(uint32_t*)(sndBuffer + 40) = numSamples;

    float last = 0.0f;
    for (int i = 0; i < numSamples / NUM_CHANNELS; i++) {
        float t = (float)i / SAMPLE_RATE;
        float s = 0;

        switch (type) {
            case SND_BEEP:
                s = sinf(pi2 * pitch * t) * volume;
                break;
            case SND_ENGINE:
                s = ((xorshift32() & 255) * volume);
                float alpha = 0.05f;
                s = last * (1.0f - alpha) + s * alpha;
                last = s;         
                break;
            case SND_GUN: {
                float decay = expf(-20.0f * t);
                s = ((xorshift32() % 2) ? 1.0f : -1.0f) * decay;
                s *= sinf(pi2 * pitch * t);
                break;
            }
        }

        int lv = (int)((s * leftGain  + 1.0f) * 127.5f);
        int rv = (int)((s * rightGain + 1.0f) * 127.5f);
        
        lv = lv < 0 ? 0 : lv > 255 ? 255 : lv;
        rv = rv < 0 ? 0 : rv > 255 ? 255 : rv;
        
        sndBuffer[HEADER_SIZE + i * 2 + 0] = (uint8_t)lv; // L
        sndBuffer[HEADER_SIZE + i * 2 + 1] = (uint8_t)rv; // R
            }

    WAVEFORMATEX wfx = {
        .wFormatTag = WAVE_FORMAT_PCM,
        .nChannels = NUM_CHANNELS,
        .nSamplesPerSec = SAMPLE_RATE,
        .wBitsPerSample = BITS_PER_SAMPLE,
        .nBlockAlign = NUM_CHANNELS * BITS_PER_SAMPLE / 8,
        .nAvgBytesPerSec = SAMPLE_RATE * NUM_CHANNELS,
        .cbSize = 0
    };

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        HeapFree(GetProcessHeap(), 0, sndBuffer);
        return;
    }

    waveHdr.lpData = (LPSTR)(sndBuffer + HEADER_SIZE);
    waveHdr.dwBufferLength = numSamples;
    waveOutPrepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));
}