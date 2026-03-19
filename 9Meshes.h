#ifndef MESHES_H
#define MESHES_H

#include "8Mesh.h"

static const unsigned char MESH_KEYCAP[] = {
    MB_BOX, V(-0.030f,-0.022f,-0.014f), V(0.030f,0.022f,0.014f),
    MB_MATERIAL, 10,
    MB_SEL_NORMAL, V(0,0,1), S(0.9f),
    MB_EXTRUDE, S(0.010f), MB_ALONG_NORMAL,
    MB_MATERIAL, 11,
    MB_END
};

static const unsigned char MESH_CONSOLE[] = {
    // main slab
    MB_BOX, V(-0.70f,-0.26f,-0.04f), V(0.70f,0.26f,0.04f),
    MB_MATERIAL, 10,

    MB_BOX, V(-0.44f,-0.02f,0.04f), V(0.44f,0.20f,0.052f),
    MB_MATERIAL, 10,
    MB_SEL_NORMAL, V(0,0,1), S(0.9f),
    MB_EXTRUDE, S(0.008f), MB_ALONG_NORMAL,
    MB_MATERIAL, 11,

    MB_CALL, 0,
    MB_TRANSLATE, V(-0.52f,-0.16f,0.052f),
    MB_ARRAY_LINEAR, 8, V(0.149f,0,0),

    MB_BOX, V(0.50f,-0.20f,0.04f), V(0.64f,-0.08f,0.075f),
    MB_MATERIAL, 10,
    MB_MIRROR, V(1,0,0), S(0), 1,
    MB_END
};

static const unsigned char* const MESH_LIB[] = { MESH_KEYCAP };

#endif // MESHES_H
