#include "6Volume.h"
#include "3utils.h"

extern GLuint fullscreen_vao;                       // empty VAO, gl_VertexID only
ShaderProgram create_shader_program_from_files(const char* vert, const char* frag);

enum {
    U_VIEW = 0, U_PROJ = 4, U_CENTRE = 8, U_CAM = 9, U_LIGHT = 10,
    U_A = 11, U_B = 12, U_C = 13, U_D = 14
};

static GLuint vol_prog, ring_prog;
static float  vol_time;

static const float* cur_view;
static const float* cur_proj;
static Vec3         cur_cam;

void vol_init(void) {
    vol_prog  = create_shader_program_from_files("shaders/volume.vert", "shaders/volume.frag").id;
    ring_prog = create_shader_program_from_files("shaders/volume.vert", "shaders/ring.frag").id;
}

static void billboard(Vec3 centre, float radius, Vec3 cam,
                      const Mat4 view, const Mat4 proj,
                      Vec3* out_centre, float* out_half)
{
    Vec3  d = vec3_sub(centre, cam);
    float dist = sqrtf(vec3_dot(d, d));

    if (dist > radius * 1.05f) {
        *out_centre = centre;
        *out_half   = radius * dist / (dist - radius);
        return;
    }

    const float z0 = 0.25f;
    Vec3 fwd = { -view[2], -view[6], -view[10] };
    *out_centre = vec3_add(cam, vec3_scale(fwd, z0));
    *out_half   = z0 / proj[0] * 1.05f;   // half-width covers the wider axis
}

static void begin_state(GLuint prog, const Mat4 view, const Mat4 proj, Vec3 cam, Vec3 light) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);   // shaders output premultiplied
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glUseProgram(prog);
    glUniformMatrix4fv(U_VIEW, 1, GL_FALSE, view);
    glUniformMatrix4fv(U_PROJ, 1, GL_FALSE, proj);
    glUniform3f(U_CAM, cam.x, cam.y, cam.z);
    glUniform3f(U_LIGHT, light.x, light.y, light.z);
    glBindVertexArray(fullscreen_vao);
}

void vol_begin(const Mat4 view, const Mat4 proj, Vec3 cam, Vec3 light, float time) {
    begin_state(vol_prog, view, proj, cam, light);
    cur_view = view;
    cur_proj = proj;
    cur_cam  = cam;
    vol_time = time;
}

void vol_draw(const Volume* v) {
    if (v->length <= 0.0f || v->density <= 0.0f) return;

    float half   = v->length * 0.5f;
    float radius = half + v->radius * 3.0f;
    Vec3  centre = vec3_add(v->origin, vec3_scale(v->axis, half));

    Vec3 qc; float qh;
    billboard(centre, radius, cur_cam, cur_view, cur_proj, &qc, &qh);

    glUniform4f(U_CENTRE, qc.x, qc.y, qc.z, qh);
    glUniform4f(U_A, v->origin.x, v->origin.y, v->origin.z, v->length);
    glUniform4f(U_B, v->axis.x, v->axis.y, v->axis.z, v->radius);
    glUniform4f(U_C, v->tint.x, v->tint.y, v->tint.z, v->density);
    glUniform4f(U_D, v->intensity, vol_time, v->seed, (float)v->type);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void vol_ring(const Mat4 view, const Mat4 proj, Vec3 cam, Vec3 light, const Ring* r) {
    begin_state(ring_prog, view, proj, cam, light);

    Vec3 qc; float qh;
    billboard(r->centre, r->outer_r, cam, view, proj, &qc, &qh);

    glUniform4f(U_CENTRE, qc.x, qc.y, qc.z, qh);
    glUniform4f(U_A, r->centre.x, r->centre.y, r->centre.z, r->planet_r);
    glUniform4f(U_B, r->axis.x, r->axis.y, r->axis.z, 0.0f);
    glUniform4f(U_C, r->inner_r, r->outer_r, r->tau, 0.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void vol_end(void) {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}
