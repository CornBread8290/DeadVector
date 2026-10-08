// Minimal OpenGL loader covering only the entry points the game calls.
// A link error about a missing glad_gl* symbol means it needs adding here.
#include <glad/gl.h>
#include <glad/wgl.h>

PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
PFNGLATTACHSHADERPROC glad_glAttachShader;
PFNGLBINDBUFFERPROC glad_glBindBuffer;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer;
PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture;
PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer;
PFNGLBINDTEXTUREPROC glad_glBindTexture;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
PFNGLBLENDFUNCPROC glad_glBlendFunc;
PFNGLBUFFERDATAPROC glad_glBufferData;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
PFNGLCLEARPROC glad_glClear;
PFNGLCLEARCOLORPROC glad_glClearColor;
PFNGLCOMPILESHADERPROC glad_glCompileShader;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
PFNGLCREATESHADERPROC glad_glCreateShader;
PFNGLCULLFACEPROC glad_glCullFace;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
PFNGLDELETESHADERPROC glad_glDeleteShader;
PFNGLDEPTHFUNCPROC glad_glDepthFunc;
PFNGLDEPTHMASKPROC glad_glDepthMask;
PFNGLDISABLEPROC glad_glDisable;
PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute;
PFNGLDRAWARRAYSPROC glad_glDrawArrays;
PFNGLDRAWBUFFERPROC glad_glDrawBuffer;
PFNGLDRAWELEMENTSPROC glad_glDrawElements;
PFNGLENABLEPROC glad_glEnable;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D;
PFNGLFRONTFACEPROC glad_glFrontFace;
PFNGLGENBUFFERSPROC glad_glGenBuffers;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers;
PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers;
PFNGLGENTEXTURESPROC glad_glGenTextures;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
PFNGLGETINTEGERVPROC glad_glGetIntegerv;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
PFNGLGETSHADERIVPROC glad_glGetShaderiv;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
PFNGLISENABLEDPROC glad_glIsEnabled;
PFNGLLINKPROGRAMPROC glad_glLinkProgram;
PFNGLMEMORYBARRIERPROC glad_glMemoryBarrier;
PFNGLPIXELSTOREIPROC glad_glPixelStorei;
PFNGLPOINTSIZEPROC glad_glPointSize;
PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset;
PFNGLREADBUFFERPROC glad_glReadBuffer;
PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage;
PFNGLSHADERSOURCEPROC glad_glShaderSource;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D;
PFNGLUNIFORM1FPROC glad_glUniform1f;
PFNGLUNIFORM1IPROC glad_glUniform1i;
PFNGLUNIFORM2FPROC glad_glUniform2f;
PFNGLUNIFORM3FPROC glad_glUniform3f;
PFNGLUNIFORM4FPROC glad_glUniform4f;
PFNGLUNIFORM4FVPROC glad_glUniform4fv;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC glad_glUseProgram;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
PFNGLVIEWPORTPROC glad_glViewport;
PFNWGLCHOOSEPIXELFORMATARBPROC glad_wglChoosePixelFormatARB;

static void* const gl_slots[] = {
    &glad_glActiveTexture,
    &glad_glAttachShader,
    &glad_glBindBuffer,
    &glad_glBindFramebuffer,
    &glad_glBindImageTexture,
    &glad_glBindRenderbuffer,
    &glad_glBindTexture,
    &glad_glBindVertexArray,
    &glad_glBlendFunc,
    &glad_glBufferData,
    &glad_glCheckFramebufferStatus,
    &glad_glClear,
    &glad_glClearColor,
    &glad_glCompileShader,
    &glad_glCreateProgram,
    &glad_glCreateShader,
    &glad_glCullFace,
    &glad_glDeleteProgram,
    &glad_glDeleteShader,
    &glad_glDepthFunc,
    &glad_glDepthMask,
    &glad_glDisable,
    &glad_glDispatchCompute,
    &glad_glDrawArrays,
    &glad_glDrawBuffer,
    &glad_glDrawElements,
    &glad_glEnable,
    &glad_glEnableVertexAttribArray,
    &glad_glFramebufferRenderbuffer,
    &glad_glFramebufferTexture2D,
    &glad_glFrontFace,
    &glad_glGenBuffers,
    &glad_glGenFramebuffers,
    &glad_glGenRenderbuffers,
    &glad_glGenTextures,
    &glad_glGenVertexArrays,
    &glad_glGenerateMipmap,
    &glad_glGetIntegerv,
    &glad_glGetProgramInfoLog,
    &glad_glGetProgramiv,
    &glad_glGetShaderInfoLog,
    &glad_glGetShaderiv,
    &glad_glGetUniformLocation,
    &glad_glIsEnabled,
    &glad_glLinkProgram,
    &glad_glMemoryBarrier,
    &glad_glPixelStorei,
    &glad_glPointSize,
    &glad_glPolygonOffset,
    &glad_glReadBuffer,
    &glad_glRenderbufferStorage,
    &glad_glShaderSource,
    &glad_glTexImage2D,
    &glad_glTexParameterfv,
    &glad_glTexParameteri,
    &glad_glTexSubImage2D,
    &glad_glUniform1f,
    &glad_glUniform1i,
    &glad_glUniform2f,
    &glad_glUniform3f,
    &glad_glUniform4f,
    &glad_glUniform4fv,
    &glad_glUniformMatrix4fv,
    &glad_glUseProgram,
    &glad_glVertexAttribPointer,
    &glad_glViewport,
};

static const char gl_names[] =
    "glActiveTexture\0"
    "glAttachShader\0"
    "glBindBuffer\0"
    "glBindFramebuffer\0"
    "glBindImageTexture\0"
    "glBindRenderbuffer\0"
    "glBindTexture\0"
    "glBindVertexArray\0"
    "glBlendFunc\0"
    "glBufferData\0"
    "glCheckFramebufferStatus\0"
    "glClear\0"
    "glClearColor\0"
    "glCompileShader\0"
    "glCreateProgram\0"
    "glCreateShader\0"
    "glCullFace\0"
    "glDeleteProgram\0"
    "glDeleteShader\0"
    "glDepthFunc\0"
    "glDepthMask\0"
    "glDisable\0"
    "glDispatchCompute\0"
    "glDrawArrays\0"
    "glDrawBuffer\0"
    "glDrawElements\0"
    "glEnable\0"
    "glEnableVertexAttribArray\0"
    "glFramebufferRenderbuffer\0"
    "glFramebufferTexture2D\0"
    "glFrontFace\0"
    "glGenBuffers\0"
    "glGenFramebuffers\0"
    "glGenRenderbuffers\0"
    "glGenTextures\0"
    "glGenVertexArrays\0"
    "glGenerateMipmap\0"
    "glGetIntegerv\0"
    "glGetProgramInfoLog\0"
    "glGetProgramiv\0"
    "glGetShaderInfoLog\0"
    "glGetShaderiv\0"
    "glGetUniformLocation\0"
    "glIsEnabled\0"
    "glLinkProgram\0"
    "glMemoryBarrier\0"
    "glPixelStorei\0"
    "glPointSize\0"
    "glPolygonOffset\0"
    "glReadBuffer\0"
    "glRenderbufferStorage\0"
    "glShaderSource\0"
    "glTexImage2D\0"
    "glTexParameterfv\0"
    "glTexParameteri\0"
    "glTexSubImage2D\0"
    "glUniform1f\0"
    "glUniform1i\0"
    "glUniform2f\0"
    "glUniform3f\0"
    "glUniform4f\0"
    "glUniform4fv\0"
    "glUniformMatrix4fv\0"
    "glUseProgram\0"
    "glVertexAttribPointer\0"
    "glViewport";

int gladLoadGL(GLADloadfunc load) {
    const char* n = gl_names;
    for (unsigned i = 0; i < sizeof gl_slots / sizeof gl_slots[0]; ++i) {
        *(GLADapiproc*)gl_slots[i] = load(n);
        while (*n++);
    }
    return 1;
}

int gladLoadWGL(HDC hdc, GLADloadfunc load) {
    (void)hdc;
    glad_wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)load("wglChoosePixelFormatARB");
    return 1;
}
