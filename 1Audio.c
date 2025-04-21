#include <windows.h>
#include "defs.h"

DWORD WINAPI playThread(LPVOID p) {
    uint8_t* soundBuffer = (uint8_t*)p;

    PlaySoundA((LPCSTR)soundBuffer, NULL, SND_MEMORY | SND_ASYNC);

    HeapFree(GetProcessHeap(), 0, soundBuffer);

    return 0;
}

void playSoundEffect(int type, float pitch, float duration, float volume) {
    int NUM_SAMPLES = ((int)(SAMPLE_RATE * duration));
    int bufferSize = 44 + NUM_SAMPLES;

    uint8_t* snd = (uint8_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferSize);
    if (!snd) return;

    memcpy(snd, "RIFF", 4);
    *(uint32_t*)(snd + 4) = 36 + NUM_SAMPLES;
    memcpy(snd + 8, "WAVEfmt ", 8);
    *(uint32_t*)(snd + 16) = 16;
    *(uint16_t*)(snd + 20) = 1;
    *(uint16_t*)(snd + 22) = 1;
    *(uint32_t*)(snd + 24) = SAMPLE_RATE;
    *(uint32_t*)(snd + 28) = SAMPLE_RATE;
    *(uint16_t*)(snd + 32) = 1;
    *(uint16_t*)(snd + 34) = 8;
    memcpy(snd + 36, "data", 4);
    *(uint32_t*)(snd + 40) = NUM_SAMPLES;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float s = 0;

        switch (type) {
            case SND_BEEP: {
                //float env = expf(-5.0f * t);
                s = sinf(2.0f * 3.14159f * pitch * t) * volume; //* env;
                break;
            }
            case SND_ENGINE: {
                s = (xorshift32() & 255) * volume;
                break;
            }
            case SND_GUN: {
                float decay = expf(-20.0f * t);
                s = ((xorshift32() % 2) ? 1.0f : -1.0f) * decay;
                s *= sinf(2.0f * 3.14159f * pitch * t);
                break;
            }
            /*case SND_HARSH: {
                float noise = ;
                float hum = sinf(2.0f * 3.14159f * pitch * t) * 0.6f;
                s = (noise + hum) * 0.5f;
                break;
            }*/
        }

        int v = (int)((s + 1.0f) * 127.5f);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        snd[44 + i] = (uint8_t)v;
    }

    CreateThread(NULL, 0, playThread, snd, 0, NULL);
}
