#ifndef DEFS_H
#define DEFS_H

#include <math.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Quat;

#define pi 3.14159265358979323846f
#define pi2 (pi * 2.0f)

#define NUM_STARS 64

#define SAMPLE_RATE 4000

enum SoundType {
    SND_BEEP = 0,
    SND_ENGINE,
    SND_GUN,
    SND_HARSH
};
uint32_t xorshift32(void);


#endif // DEFS_H