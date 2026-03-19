#ifndef MESHBC_H
#define MESHBC_H

#include "defs.h"

#define MB_FIX(x) ((int)((x) * 256.0f + ((x) < 0.0f ? -0.5f : 0.5f)))
#define S(x)      (unsigned char)(MB_FIX(x) & 0xFF), (unsigned char)((MB_FIX(x) >> 8) & 0xFF)
#define V(x,y,z)  S(x), S(y), S(z)
#define A(deg)    (unsigned char)(((int)(deg)) & 0xFF), (unsigned char)((((int)(deg)) >> 8) & 0xFF)

enum {
    MB_END = 0,

    MB_BOX,           // mn(V) mx(V)
    MB_PRISM,         // sides(u8) radius(S) height(S)
    MB_QUAD,

    MB_SEL_ALL,
    MB_SEL_NONE,
    MB_SEL_BOX,       // mn(V) mx(V)
    MB_SEL_SPHERE,    // centre(V) radius(S)
    MB_SEL_NORMAL,    // dir(V) min_dot(S)
    MB_SEL_GROW,
    MB_PUSH_SEL,
    MB_POP_SEL,

    // transforms, applied to the selection
    MB_TRANSLATE,     // delta(V)
    MB_SCALE,         // pivot(V) factor(V)
    MB_ROTATE,        // axis(V) angle(A)
    MB_MIRROR,        // normal(V) offset(S) duplicate(u8)

    // construction
    MB_EXTRUDE,       // distance(S) mode(u8) [dir(V) when MB_ALONG_DIR]
    MB_ARRAY_CIRCLE,  // count(u8) axis(V)
    MB_ARRAY_LINEAR,  // count(u8) offset(V)

    MB_MATERIAL,      // id(u8)
    MB_CALL,
    MB_COLOR          // rgb(V)
};

// MB_EXTRUDE modes
enum { MB_ALONG_NORMAL = 0, MB_ALONG_DIR = 1 };

void mesh_build(Mesh* m, const unsigned char* code, const unsigned char* const* lib);

#endif // MESHBC_H
