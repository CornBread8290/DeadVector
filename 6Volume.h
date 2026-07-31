#ifndef VOLUME_H
#define VOLUME_H

#include "defs.h"

enum { VOL_PLUME = 0, VOL_RCS = 1, VOL_FIRE = 2 };

typedef struct {
    Vec3  origin;
    Vec3  axis;
    Vec3  tint;
    float length;     // axial extent
    float radius;     // mouth radius
    float density;
    float intensity;  // emissive strength
    float seed;
    int   type;
} Volume;

typedef struct {
    Vec3  centre;
    Vec3  axis;       // ring plane normal
    float planet_r;
    float inner_r;
    float outer_r;
    float tau;
} Ring;

void vol_init(void);

void vol_begin(const Mat4 view, const Mat4 proj, Vec3 cam, Vec3 light_dir, float time);
void vol_draw(const Volume* v);
void vol_ring(const Mat4 view, const Mat4 proj, Vec3 cam, Vec3 light_dir, const Ring* r);
void vol_end(void);

#endif // VOLUME_H
