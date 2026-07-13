#ifndef PHYSICS_H
#define PHYSICS_H

#include "defs.h"

enum { SHAPE_SPHERES = 0, SHAPE_FIELD = 1 };

typedef struct { Vec3 c; float r; } Sphere;

typedef struct {
    Vec3 pos, vel, angVel;
    Quat rot;
    Vec3 force, torque;      // accumulators, cleared every step

    float inv_mass;          // 0 = immovable
    Vec3  inv_inertia;

    float restitution;
    float friction;

    int           shape;
    const Sphere* spheres;   // SHAPE_SPHERES, body space
    int           sphere_count;
    float         field_radius;  // SHAPE_FIELD
} Body;

float phys_step(Body* const* bodies, int count, float dt);

Vec3 phys_point_velocity(const Body* b, Vec3 world_point);

#endif // PHYSICS_H
