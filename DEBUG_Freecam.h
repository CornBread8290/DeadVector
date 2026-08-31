#ifndef DEBUG_FREECAM_H
#define DEBUG_FREECAM_H

#define DEBUG_FREECAM 1

#if DEBUG_FREECAM

#include "defs.h"
#include "3utils.h"

#define DBG_FC_TOGGLE_KEY  VK_F1
#define DBG_FC_SPEED_MIN   0.5f
#define DBG_FC_SPEED_MAX   400.0f
#define DBG_FC_LOOK_SENS   0.0028f   // matches the ship's head-look sensitivity

typedef struct {
    int   active;
    Vec3  pos;
    float yaw;
    float pitch;
    float speed;    // units per second
    int   toggle_prev;
} DbgFreecam;

static DbgFreecam dbg_freecam = { .speed = 12.0f };

// Unit vector the camera is looking along.
static Vec3 dbg_freecam_forward(void) {
    const float cp = cosf(dbg_freecam.pitch), sp = sinf(dbg_freecam.pitch);
    const float cy = cosf(dbg_freecam.yaw),   sy = sinf(dbg_freecam.yaw);
    return (Vec3){ -sy * cp, sp, -cy * cp };
}

static int dbg_freecam_mouse(int dx, int dy) {
    if (!dbg_freecam.active) return 0;
    dbg_freecam.yaw   -= dx * DBG_FC_LOOK_SENS;
    dbg_freecam.pitch -= dy * DBG_FC_LOOK_SENS;
    if (dbg_freecam.pitch >  1.55f) dbg_freecam.pitch =  1.55f;  // no gimbal flip
    if (dbg_freecam.pitch < -1.55f) dbg_freecam.pitch = -1.55f;
    return 1;
}

static void dbg_freecam_update(float dt, Vec3 seed_pos, Vec3 seed_forward) {
    const int toggle = (GetAsyncKeyState(DBG_FC_TOGGLE_KEY) & 0x8000) != 0;
    if (toggle && !dbg_freecam.toggle_prev) {
        dbg_freecam.active = !dbg_freecam.active;
        if (dbg_freecam.active) {
            Vec3 f = vec3_normalize(seed_forward);
            if (f.y >  1.0f) f.y =  1.0f;
            if (f.y < -1.0f) f.y = -1.0f;
            dbg_freecam.pos   = seed_pos;
            dbg_freecam.yaw   = atan2f(-f.x, -f.z);
            dbg_freecam.pitch = asinf(f.y);
        }
    }
    dbg_freecam.toggle_prev = toggle;
    if (!dbg_freecam.active) return;

    if (GetAsyncKeyState(VK_OEM_COMMA)  & 0x8000) dbg_freecam.speed *= powf(0.25f, dt);
    if (GetAsyncKeyState(VK_OEM_PERIOD) & 0x8000) dbg_freecam.speed *= powf(4.00f, dt);
    if (dbg_freecam.speed < DBG_FC_SPEED_MIN) dbg_freecam.speed = DBG_FC_SPEED_MIN;
    if (dbg_freecam.speed > DBG_FC_SPEED_MAX) dbg_freecam.speed = DBG_FC_SPEED_MAX;

    const Vec3 fwd   = dbg_freecam_forward();
    const Vec3 right = vec3_normalize(vec3_cross(fwd, (Vec3){0.0f, 1.0f, 0.0f}));

    Vec3 move = {0};
    if (GetAsyncKeyState('I') & 0x8000) move = vec3_add(move, fwd);
    if (GetAsyncKeyState('K') & 0x8000) move = vec3_sub(move, fwd);
    if (GetAsyncKeyState('L') & 0x8000) move = vec3_add(move, right);
    if (GetAsyncKeyState('J') & 0x8000) move = vec3_sub(move, right);
    if (GetAsyncKeyState('U') & 0x8000) move.y += 1.0f;
    if (GetAsyncKeyState('O') & 0x8000) move.y -= 1.0f;

    if (vec3_dot(move, move) > 0.0f)
        dbg_freecam.pos = vec3_add(dbg_freecam.pos,
                                   vec3_scale(vec3_normalize(move), dbg_freecam.speed * dt));
}

static void dbg_freecam_apply(Mat4 view_out, Vec3* cam_pos_out) {
    if (!dbg_freecam.active) return;
    const Vec3 eye = dbg_freecam.pos;
    mat4_lookat(view_out, eye, vec3_add(eye, dbg_freecam_forward()), (Vec3){0.0f, 1.0f, 0.0f});
    *cam_pos_out = eye;
}

static void dbg_freecam_hud(char* buf) {
    if (!dbg_freecam.active) { buf[0] = 0; return; }
    const char* p = "FREECAM x";
    int i = 0;
    while (*p) buf[i++] = *p++;

    unsigned v = (unsigned)(dbg_freecam.speed + 0.5f);
    char digits[12];
    int n = 0;
    do { digits[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) buf[i++] = digits[--n];
    buf[i] = 0;
}

#endif // DEBUG_FREECAM
#endif // DEBUG_FREECAM_H
