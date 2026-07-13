#include "7Physics.h"
#include "3utils.h"

#define MAX_CONTACTS   32
#define SOLVER_ITERS    8
#define PENETRATION_SLOP 0.010f
#define CORRECTION_RATE  0.70f

typedef struct {
    Body *a, *b;
    Vec3  point;
    Vec3  normal;
    float depth;
} Contact;

static Vec3 iinv_mul(const Body* b, Vec3 v) {
    Vec3 l = quat_rotate_vec3(quat_conjugate(b->rot), v);
    l.x *= b->inv_inertia.x;
    l.y *= b->inv_inertia.y;
    l.z *= b->inv_inertia.z;
    return quat_rotate_vec3(b->rot, l);
}

Vec3 phys_point_velocity(const Body* b, Vec3 world_point) {
    return vec3_add(b->vel, vec3_cross(b->angVel, vec3_sub(world_point, b->pos)));
}

static float body_bound(const Body* b) {
    if (b->shape == SHAPE_FIELD) {
        return b->field_radius * 3.6f;
    }
    float m = 0.0f;
    for (int i = 0; i < b->sphere_count; ++i) {
        float d = sqrtf(vec3_dot(b->spheres[i].c, b->spheres[i].c)) + b->spheres[i].r;
        if (d > m) m = d;
    }
    return m;
}

static Vec3 sphere_world(const Body* b, int i) {
    return vec3_add(b->pos, quat_rotate_vec3(b->rot, b->spheres[i].c));
}

static float field_dist(const Body* f, Vec3 local) {
    float d = sqrtf(vec3_dot(local, local));
    if (d < 1e-6f) return -f->field_radius;
    return d - f->field_radius * asteroid_shape(vec3_scale(local, 1.0f / d), 0, 0);
}

static Vec3 field_normal(const Body* f, Vec3 local) {
    const float h = 0.06f;
    static const Vec3 k[4] = {{1,-1,-1}, {-1,-1,1}, {-1,1,-1}, {1,1,1}};
    Vec3 n = {0};
    for (int i = 0; i < 4; ++i) {
        float d = field_dist(f, vec3_add(local, vec3_scale(k[i], h)));
        n = vec3_add(n, vec3_scale(k[i], d));
    }
    return vec3_normalize(n);
}

static int contacts_field_spheres(Body* f, Body* s, Contact* out, int max) {
    int n = 0;
    Quat inv = quat_conjugate(f->rot);

    for (int i = 0; i < s->sphere_count && n < max; ++i) {
        Vec3 w = sphere_world(s, i);
        Vec3 local = quat_rotate_vec3(inv, vec3_sub(w, f->pos));

        float sd = field_dist(f, local);
        float depth = s->spheres[i].r - sd;
        if (depth <= 0.0f) continue;

        Vec3 nl = field_normal(f, local);
        Vec3 nw = quat_rotate_vec3(f->rot, nl);

        out[n++] = (Contact){
            .a = f, .b = s,
            .point  = vec3_sub(w, vec3_scale(nw, sd)),
            .normal = nw,
            .depth  = depth
        };
    }
    return n;
}

static int contacts_spheres_spheres(Body* a, Body* b, Contact* out, int max) {
    int n = 0;
    for (int i = 0; i < a->sphere_count; ++i) {
        Vec3 ca = sphere_world(a, i);
        for (int j = 0; j < b->sphere_count && n < max; ++j) {
            Vec3 cb = sphere_world(b, j);
            Vec3 d = vec3_sub(cb, ca);
            float dist2 = vec3_dot(d, d);
            float rsum = a->spheres[i].r + b->spheres[j].r;
            if (dist2 >= rsum * rsum) continue;

            float dist = sqrtf(dist2);
            Vec3 nrm = dist > 1e-6f ? vec3_scale(d, 1.0f / dist) : (Vec3){0, 1, 0};

            out[n++] = (Contact){
                .a = a, .b = b,
                .point  = vec3_add(ca, vec3_scale(nrm, a->spheres[i].r)),
                .normal = nrm,
                .depth  = rsum - dist
            };
        }
    }
    return n;
}

static void apply_impulse(Body* b, Vec3 imp, Vec3 r, float sign) {
    if (b->inv_mass <= 0.0f) return;
    b->vel = vec3_add(b->vel, vec3_scale(imp, sign * b->inv_mass));
    b->angVel = vec3_add(b->angVel, iinv_mul(b, vec3_scale(vec3_cross(r, imp), sign)));
}

static float effective_mass(const Contact* c, Vec3 ra, Vec3 rb, Vec3 dir) {
    float k = c->a->inv_mass + c->b->inv_mass;
    k += vec3_dot(dir, vec3_cross(iinv_mul(c->a, vec3_cross(ra, dir)), ra));
    k += vec3_dot(dir, vec3_cross(iinv_mul(c->b, vec3_cross(rb, dir)), rb));
    return k;
}

static float solve(Contact* c) {
    Vec3 ra = vec3_sub(c->point, c->a->pos);
    Vec3 rb = vec3_sub(c->point, c->b->pos);

    Vec3 vrel = vec3_sub(phys_point_velocity(c->b, c->point),
                         phys_point_velocity(c->a, c->point));
    float vn = vec3_dot(vrel, c->normal);
    if (vn > 0.0f) return 0.0f;            // already separating

    float kn = effective_mass(c, ra, rb, c->normal);
    if (kn <= 1e-9f) return 0.0f;

    float e = c->a->restitution < c->b->restitution ? c->a->restitution : c->b->restitution;
    if (-vn < 0.6f) e = 0.0f;

    float jn = -(1.0f + e) * vn / kn;
    Vec3 imp = vec3_scale(c->normal, jn);
    apply_impulse(c->a, imp, ra, -1.0f);
    apply_impulse(c->b, imp, rb, +1.0f);

    vrel = vec3_sub(phys_point_velocity(c->b, c->point),
                    phys_point_velocity(c->a, c->point));
    Vec3 vt = vec3_sub(vrel, vec3_scale(c->normal, vec3_dot(vrel, c->normal)));
    float vt_len = sqrtf(vec3_dot(vt, vt));
    if (vt_len > 1e-4f) {
        Vec3 tdir = vec3_scale(vt, 1.0f / vt_len);
        float kt = effective_mass(c, ra, rb, tdir);
        if (kt > 1e-9f) {
            float mu = (c->a->friction + c->b->friction) * 0.5f;
            float jt = -vt_len / kt;
            float lim = mu * jn;
            if (jt < -lim) jt = -lim;
            if (jt >  lim) jt =  lim;
            Vec3 fimp = vec3_scale(tdir, jt);
            apply_impulse(c->a, fimp, ra, -1.0f);
            apply_impulse(c->b, fimp, rb, +1.0f);
        }
    }
    return jn;
}

static void integrate(Body* b, float dt) {
    if (b->inv_mass > 0.0f) {
        b->vel = vec3_add(b->vel, vec3_scale(b->force, b->inv_mass * dt));
        b->angVel = vec3_add(b->angVel, vec3_scale(iinv_mul(b, b->torque), dt));
    }
    b->pos = vec3_add(b->pos, vec3_scale(b->vel, dt));

    // q' = 0.5 * w_world (x) q
    Quat w = { b->angVel.x, b->angVel.y, b->angVel.z, 0.0f };
    Quat d = quat_mul(w, b->rot);
    b->rot.x += d.x * 0.5f * dt;
    b->rot.y += d.y * 0.5f * dt;
    b->rot.z += d.z * 0.5f * dt;
    b->rot.w += d.w * 0.5f * dt;
    b->rot = quat_normalize(b->rot);
}

float phys_step(Body* const* bodies, int count, float dt) {
    if (dt <= 0.0f) return 0.0f;

    float fastest = 0.0f;
    for (int i = 0; i < count; ++i) {
        float v = sqrtf(vec3_dot(bodies[i]->vel, bodies[i]->vel));
        if (v > fastest) fastest = v;
    }
    int steps = (int)(fastest * dt / 0.30f) + 1;
    if (steps > 8) steps = 8;
    float h = dt / (float)steps;

    float peak = 0.0f;

    while (steps--) {
        for (int i = 0; i < count; ++i) integrate(bodies[i], h);

        Contact contacts[MAX_CONTACTS];
        int nc = 0;

        for (int i = 0; i < count && nc < MAX_CONTACTS; ++i) {
            for (int j = i + 1; j < count && nc < MAX_CONTACTS; ++j) {
                Body* a = bodies[i];
                Body* b = bodies[j];
                if (a->inv_mass <= 0.0f && b->inv_mass <= 0.0f) continue;

                Vec3 d = vec3_sub(b->pos, a->pos);
                float reach = body_bound(a) + body_bound(b);
                if (vec3_dot(d, d) > reach * reach) continue;

                if (a->shape == SHAPE_FIELD && b->shape == SHAPE_SPHERES)
                    nc += contacts_field_spheres(a, b, contacts + nc, MAX_CONTACTS - nc);
                else if (b->shape == SHAPE_FIELD && a->shape == SHAPE_SPHERES)
                    nc += contacts_field_spheres(b, a, contacts + nc, MAX_CONTACTS - nc);
                else if (a->shape == SHAPE_SPHERES && b->shape == SHAPE_SPHERES)
                    nc += contacts_spheres_spheres(a, b, contacts + nc, MAX_CONTACTS - nc);
            }
        }

        for (int it = 0; it < SOLVER_ITERS; ++it) {
            for (int i = 0; i < nc; ++i) {
                float j = solve(&contacts[i]);
                if (it == 0 && j > peak) peak = j;
            }
        }

        for (int i = 0; i < nc; ++i) {
            Contact* c = &contacts[i];
            float excess = c->depth - PENETRATION_SLOP;
            if (excess <= 0.0f) continue;

            float inv_sum = c->a->inv_mass + c->b->inv_mass;
            if (inv_sum <= 0.0f) continue;

            Vec3 push = vec3_scale(c->normal, CORRECTION_RATE * excess / inv_sum);
            c->a->pos = vec3_sub(c->a->pos, vec3_scale(push, c->a->inv_mass));
            c->b->pos = vec3_add(c->b->pos, vec3_scale(push, c->b->inv_mass));
        }
    }

    for (int i = 0; i < count; ++i) bodies[i]->force = bodies[i]->torque = (Vec3){0};

    return peak;
}
