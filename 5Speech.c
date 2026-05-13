#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "5Speech.h"
#include "2Audio.h"

#define SR SND_RATE
#define SPEECH_MAX (SR * 12)

typedef enum {
    SEG_SILENCE,
    SEG_VOWEL,
    SEG_L,
    SEG_W,
    SEG_NASAL,
    SEG_H,
    SEG_FRICATIVE,
    SEG_FRICATIVE_SOFT,
    SEG_STOP
} SegmentType;

typedef struct {
    SegmentType type;
    uint16_t dur_ms;
    float f1, f2, f3;
    float q1, q2, q3;
    float pitch0, pitch1;
    float amp;
    float voiced;
    float noise;
} Segment;

typedef struct {
    float formant_scale;
    float pitch_bias;
    float pitch_scale;
    float buzz;
    float noise_scale;
    float roughness;
    float brightness;
    float vibrato;
} SpeechVoice;

typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} Biquad;

typedef struct {
    uint8_t type;
    uint8_t dur_ms;
    uint16_t f1, f2, f3;
    uint8_t q1_x10, q2_x10, q3_x10;
    uint8_t pitch0, pitch1;
    uint8_t amp_q;
    uint8_t voiced_q;
    uint8_t noise_q;
} PhonemeDef;

#define PHRASE_RECORD_SIZE 5
#define AMP_SCALE 160.0f
#define AMPQ(x) ((uint8_t)((x) * AMP_SCALE + 0.5f))
#define LVLQ(x) ((uint8_t)((x) * 255.0f + 0.5f))
#define QX10(x) ((uint8_t)((x) * 10.0f + 0.5f))

static const PhonemeDef kPhonemes[] = {
    { SEG_H,     42,  900, 1600, 2500, QX10(1.0f), QX10(1.2f), QX10(1.3f), 120, 120, AMPQ(0.20f), LVLQ(0.00f), LVLQ(1.00f) },
    { SEG_VOWEL,110,  390, 2000, 2800, QX10(7.0f), QX10(9.0f), QX10(11.0f),120, 120, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.03f) },
    { SEG_VOWEL,140,  480, 1350, 1750, QX10(7.0f), QX10(9.0f), QX10(10.0f),110, 110, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_L,     70,  330, 1150, 2100, QX10(7.0f), QX10(8.0f), QX10(9.0f), 120, 120, AMPQ(0.86f), LVLQ(1.00f), LVLQ(0.01f) },
    { SEG_W,     60,  260,  600, 1600, QX10(7.0f), QX10(8.0f), QX10(9.0f), 126, 120, AMPQ(0.80f), LVLQ(1.00f), LVLQ(0.01f) },
    { SEG_NASAL, 68,  250, 1450, 2450, QX10(5.5f), QX10(7.0f), QX10(9.0f), 120, 120, AMPQ(0.78f), LVLQ(1.00f), LVLQ(0.01f) },
    { SEG_STOP,  30,  300, 1900, 2200, QX10(1.0f), QX10(1.4f), QX10(2.0f), 120, 118, AMPQ(0.66f), LVLQ(0.92f), LVLQ(0.14f) }, // G
    { SEG_VOWEL,120,  530, 1840, 2750, QX10(7.0f), QX10(9.0f), QX10(11.0f),120, 120, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.03f) },
    { SEG_VOWEL,128,  430,  950, 2450, QX10(7.0f), QX10(9.0f), QX10(11.0f),120, 120, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_VOWEL,125,  300, 2200, 3000, QX10(7.0f), QX10(9.0f), QX10(11.0f),124, 124, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_VOWEL,115,  640, 1190, 2390, QX10(7.0f), QX10(9.0f), QX10(10.0f),118, 116, AMPQ(0.98f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_VOWEL,130,  730, 1090, 2440, QX10(7.0f), QX10(9.0f), QX10(10.0f),118, 116, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_STOP,  28,  300,  900, 1600, QX10(1.0f), QX10(1.4f), QX10(1.8f),118, 116, AMPQ(0.64f), LVLQ(0.88f), LVLQ(0.20f) }, // B
    { SEG_STOP,  24,  300, 1700, 3600, QX10(1.0f), QX10(1.4f), QX10(2.0f),118, 116, AMPQ(0.62f), LVLQ(0.84f), LVLQ(0.18f) }, // D
    { SEG_STOP,  24,  300, 1750, 3900, QX10(1.0f), QX10(1.4f), QX10(2.2f),118, 116, AMPQ(0.60f), LVLQ(0.00f), LVLQ(0.30f) }, // T
    { SEG_STOP,  26,  300, 1900, 2300, QX10(1.0f), QX10(1.4f), QX10(2.2f),118, 116, AMPQ(0.60f), LVLQ(0.00f), LVLQ(0.28f) }, // K
    { SEG_STOP,  26,  300,  900, 1500, QX10(1.0f), QX10(1.4f), QX10(1.8f),118, 116, AMPQ(0.60f), LVLQ(0.00f), LVLQ(0.26f) }, // P
    { SEG_NASAL, 70,  220, 1200, 2100, QX10(5.5f), QX10(7.0f), QX10(9.0f),120, 118, AMPQ(0.78f), LVLQ(1.00f), LVLQ(0.01f) },
    { SEG_NASAL, 70,  300, 1800, 2500, QX10(5.5f), QX10(7.5f), QX10(9.0f),120, 118, AMPQ(0.78f), LVLQ(1.00f), LVLQ(0.01f) },
    { SEG_L,     75,  300, 1350, 1700, QX10(6.5f), QX10(8.0f), QX10(9.5f),120, 118, AMPQ(0.84f), LVLQ(1.00f), LVLQ(0.01f) },
    { SEG_VOWEL,125,  350,  800, 2200, QX10(7.0f), QX10(9.0f), QX10(10.0f),118, 116, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_VOWEL,110,  440, 1020, 2240, QX10(7.0f), QX10(9.0f), QX10(10.0f),118, 116, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_VOWEL,120,  660, 1720, 2410, QX10(7.0f), QX10(9.0f), QX10(10.0f),122, 118, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_VOWEL,125,  570,  840, 2410, QX10(7.0f), QX10(9.0f), QX10(10.0f),118, 114, AMPQ(1.00f), LVLQ(1.00f), LVLQ(0.02f) },
    { SEG_FRICATIVE_SOFT,64,  800, 1400, 5000, QX10(1.5f), QX10(1.6f), QX10(1.8f),118,116, AMPQ(0.20f), LVLQ(0.00f), LVLQ(0.16f) }, // F
    { SEG_FRICATIVE_SOFT,58,  800, 1400, 5000, QX10(1.5f), QX10(1.6f), QX10(1.8f),118,116, AMPQ(0.22f), LVLQ(0.90f), LVLQ(0.06f) }, // V
    { SEG_FRICATIVE,     56,  500, 4000, 6000, QX10(1.6f), QX10(2.2f), QX10(2.6f),118,116, AMPQ(0.26f), LVLQ(0.00f), LVLQ(0.26f) }, // S
    { SEG_FRICATIVE,     54,  500, 4000, 6000, QX10(1.6f), QX10(2.2f), QX10(2.6f),118,116, AMPQ(0.28f), LVLQ(0.90f), LVLQ(0.08f) }, // Z
    { SEG_FRICATIVE,     60,  500, 2000, 3300, QX10(1.6f), QX10(2.2f), QX10(2.6f),118,116, AMPQ(0.28f), LVLQ(0.00f), LVLQ(0.24f) }, // SH
    { SEG_FRICATIVE,     56,  500, 2000, 3300, QX10(1.6f), QX10(2.2f), QX10(2.6f),118,116, AMPQ(0.28f), LVLQ(0.90f), LVLQ(0.08f) }, // ZH
    { SEG_FRICATIVE_SOFT,64,  600, 1600, 6200, QX10(1.5f), QX10(1.6f), QX10(2.0f),118,116, AMPQ(0.18f), LVLQ(0.00f), LVLQ(0.14f) }, // TH
    { SEG_FRICATIVE_SOFT,60,  600, 1600, 6200, QX10(1.5f), QX10(1.6f), QX10(2.0f),118,116, AMPQ(0.20f), LVLQ(0.90f), LVLQ(0.05f) }, // DH
    { SEG_W,     56,  280, 2200, 3000, QX10(6.5f), QX10(8.5f), QX10(10.0f),124, 122, AMPQ(0.82f), LVLQ(1.00f), LVLQ(0.01f) }, // Y
    { SEG_VOWEL, 95,  500, 1500, 2500, QX10(7.0f), QX10(8.5f), QX10(10.0f),118, 116, AMPQ(0.84f), LVLQ(1.00f), LVLQ(0.02f) }, // AX
    { SEG_SILENCE,120, 500, 1500, 2500, QX10(1.0f), QX10(1.0f), QX10(1.0f),118, 118, AMPQ(0.01f), LVLQ(0.00f), LVLQ(0.00f) }  // PAU
};

static uint32_t rng_state = 1;

static float frand1(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return ((rng_state >> 8) & 0xFFFFFF) / (float)0x800000 - 1.0f;
}

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

static float clampf(float x, float a, float b) {
    return x < a ? a : x > b ? b : x;
}

static float saturatef(float x, float drive) {
    if (drive <= 0.0f) return x;
    return tanhf(x * drive) / tanhf(drive);
}

static float tail_blend(float u, float start) {
    if (u <= start) return 0.0f;
    return clampf((u - start) / (1.0f - start), 0.0f, 1.0f);
}

static float smoothstep01(float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static int seg_is_sonorant(SegmentType t) {
    return t >= SEG_VOWEL && t <= SEG_NASAL;
}

static float glottal_source(float phase) {
    float p = phase / pi2;
    if (p < 0.60f) {
        float x = p / 0.60f;
        return (0.5f - 0.5f * cosf(pi * x)) - 0.18f;
    }
    if (p < 0.82f) {
        float x = (p - 0.60f) / 0.22f;
        return 0.82f * cosf(0.5f * pi * x) - 0.18f;
    }
    {
        float x = (p - 0.82f) / 0.18f;
        return -0.12f * (1.0f - x) * (1.0f - x) - 0.04f;
    }
}

static void biquad_set_bandpass(Biquad *b, float freq, float q) {
    if (freq < 40.0f) freq = 40.0f;
    if (freq > SR * 0.45f) freq = SR * 0.45f;
    if (q < 0.3f) q = 0.3f;

    float w0 = pi2 * freq / SR;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2.0f * q);
    float a0 = 1.0f + alpha;

    b->b0 =  alpha / a0;
    b->b1 =  0.0f;
    b->b2 = -alpha / a0;
    b->a1 = -2.0f * c / a0;
    b->a2 = (1.0f - alpha) / a0;
}

static float biquad_process(Biquad *b, float x) {
    float y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}

static void biquad_reset(Biquad *b) { b->z1 = b->z2 = 0.0f; }

static int b64_value(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

static size_t decode_base64url(const char *src, uint8_t *dst, size_t cap) {
    unsigned accum = 0;
    int bits = 0;
    size_t out = 0;

    for (size_t i = 0; src[i]; i++) {
        int v = b64_value((unsigned char)src[i]);
        if (v < 0) continue;
        accum = (accum << 6) | (unsigned)v;
        bits += 6;
        while (bits >= 8) {
            bits -= 8;
            if (out < cap) dst[out++] = (uint8_t)((accum >> bits) & 0xFFu);
        }
    }
    return out;
}

/* Bright broadband noise for fricatives/aspiration: leaky differentiator
   for high-frequency tilt, one-pole low-pass to tame the top octave. */
static float fric_noise(void) {
    static float x1, lp;
    float w = frand1();
    float y = 0.45f * (w - x1) + 0.55f * w;
    x1 = w;
    lp += 0.6f * (y - lp);
    return lp;
}

static int decode_phrase_segments(const char *encoded, Segment **out_seg, int *out_count) {
    size_t enc_len = strlen(encoded);
    size_t raw_cap = enc_len * 3 / 4 + 4;
    uint8_t *raw = malloc(raw_cap);
    if (!raw) return 0;

    size_t raw_len = decode_base64url(encoded, raw, raw_cap);
    if (raw_len == 0 || raw_len % PHRASE_RECORD_SIZE != 0) { free(raw); return 0; }

    int count = (int)(raw_len / PHRASE_RECORD_SIZE);
    Segment *seg = calloc((size_t)count, sizeof(*seg));
    if (!seg) { free(raw); return 0; }

    for (int i = 0; i < count; i++) {
        size_t off = (size_t)i * PHRASE_RECORD_SIZE;
        uint8_t phoneme_id = raw[off + 0];
        uint8_t dur_ms = raw[off + 1];
        uint8_t pitch0 = raw[off + 2];
        uint8_t pitch1 = raw[off + 3];
        uint8_t amp_q = raw[off + 4];

        if (phoneme_id >= sizeof(kPhonemes) / sizeof(kPhonemes[0])) {
            free(seg); free(raw);
            return 0;
        }
        const PhonemeDef *def = &kPhonemes[phoneme_id];
        seg[i].type = (SegmentType)def->type;
        seg[i].dur_ms = dur_ms ? dur_ms : def->dur_ms;
        seg[i].f1 = (float)def->f1;
        seg[i].f2 = (float)def->f2;
        seg[i].f3 = (float)def->f3;
        seg[i].q1 = def->q1_x10 / 10.0f;
        seg[i].q2 = def->q2_x10 / 10.0f;
        seg[i].q3 = def->q3_x10 / 10.0f;
        seg[i].pitch0 = pitch0 ? pitch0 : def->pitch0;
        seg[i].pitch1 = pitch1 ? pitch1 : def->pitch1;
        seg[i].amp = (amp_q ? amp_q : def->amp_q) / AMP_SCALE;
        seg[i].voiced = def->voiced_q / 255.0f;
        seg[i].noise = def->noise_q / 255.0f;
    }

    free(raw);
    *out_seg = seg;
    *out_count = count;
    return 1;
}

static int synth_segments(float *out, int max_samples, const Segment *seg, int count, const SpeechVoice *v) {
    int pos = 0;
    float phase = 0.0f;
    float jitter = 0.0f;
    float shimmer = 0.0f;
    float tract_f1 = seg[0].f1 * v->formant_scale;
    float tract_f2 = seg[0].f2 * v->formant_scale;
    float tract_f3 = seg[0].f3 * v->formant_scale;
    float tract_q1 = seg[0].q1;
    float tract_q2 = seg[0].q2;
    float tract_q3 = seg[0].q3;
    float rad_x1 = 0.0f;
    float rad_y1 = 0.0f;

    Biquad f1, f2, f3;
    biquad_reset(&f1);
    biquad_reset(&f2);
    biquad_reset(&f3);

    for (int i = 0; i < count; i++) {
        Segment cur = seg[i];
        Segment next = (i + 1 < count) ? seg[i + 1] : seg[i];
        Segment prev = (i > 0) ? seg[i - 1] : seg[i];
        int n = (int)((cur.dur_ms * SR) / 1000);

        int cur_sonorant  = seg_is_sonorant(cur.type);
        int prev_sonorant = (i > 0) && seg_is_sonorant(prev.type);
        int next_sonorant = (i + 1 < count) && seg_is_sonorant(next.type);

        int onset_glide = cur_sonorant && (i > 0) && !prev_sonorant;
        float loc_f1 = 280.0f * v->formant_scale;
        float loc_f2 = clampf((float)prev.f2, 700.0f, 2600.0f) * v->formant_scale;
        float loc_f3 = clampf((float)prev.f3, 1800.0f, 3400.0f) * v->formant_scale;
        float onset_frac = 0.0f;
        if (onset_glide) {
            onset_frac = clampf(42.0f / (float)(cur.dur_ms ? cur.dur_ms : 1), 0.15f, 0.60f);
        }

        if (cur.type == SEG_STOP) {
            biquad_reset(&f1);
            biquad_reset(&f2);
            biquad_reset(&f3);
        }

        for (int j = 0; j < n && pos < max_samples; j++, pos++) {
            float u = (float)j / (float)(n ? n : 1);

            float blend = 0.0f;
            if (cur_sonorant) {
                float raw_blend = (next.type >= SEG_H) ? tail_blend(u, 0.72f) : u;
                blend = smoothstep01(raw_blend);
            }

            float f1hz = lerpf(cur.f1, next.f1, blend) * v->formant_scale;
            float f2hz = lerpf(cur.f2, next.f2, blend) * v->formant_scale;
            float f3hz = lerpf(cur.f3, next.f3, blend) * v->formant_scale;

            if (onset_glide && u < onset_frac) {
                float on = smoothstep01(u / onset_frac);
                f1hz = lerpf(loc_f1, f1hz, on);
                f2hz = lerpf(loc_f2, f2hz, on);
                f3hz = lerpf(loc_f3, f3hz, on);
            }
            float q1 = lerpf(cur.q1, next.q1, blend);
            float q2 = lerpf(cur.q2, next.q2, blend);
            float q3 = lerpf(cur.q3, next.q3, blend);
            float local_pitch = lerpf(cur.pitch0, cur.pitch1, u);
            float next_pitch  = lerpf(next.pitch0, next.pitch1, blend);
            float pitch = (lerpf(local_pitch, next_pitch, blend * 0.35f) + v->pitch_bias) * v->pitch_scale;
            float amp    = lerpf(cur.amp, next.amp, blend);
            float voiced = lerpf(cur.voiced, next.voiced, blend);
            float noise  = lerpf(cur.noise, next.noise, blend);

            jitter = 0.985f * jitter + 0.015f * frand1();
            shimmer = 0.992f * shimmer + 0.008f * frand1();

            pitch *= 1.0f + (0.004f + 0.004f * v->roughness) * jitter;
            pitch *= 1.0f + v->vibrato * sinf((float)pos * (pi2 * 5.3f / SR));
            amp *= 1.0f + (0.006f + 0.010f * v->roughness) * shimmer;
            amp = clampf(amp, 0.0f, 1.35f);

            if (pitch < 30.0f) pitch = 30.0f;

            phase += pi2 * pitch / SR;
            if (phase >= pi2) phase -= pi2;

            {
                float tract_alpha = 0.14f;
                if (cur.type == SEG_STOP) tract_alpha = 0.40f;
                else if (cur.type >= SEG_H) tract_alpha = 0.24f;

                tract_f1 += (f1hz - tract_f1) * tract_alpha;
                tract_f2 += (f2hz - tract_f2) * tract_alpha;
                tract_f3 += (f3hz - tract_f3) * tract_alpha;
                tract_q1 += (q1 - tract_q1) * tract_alpha;
                tract_q2 += (q2 - tract_q2) * tract_alpha;
                tract_q3 += (q3 - tract_q3) * tract_alpha;
            }

            float voiced_src = glottal_source(phase);
            voiced_src += frand1() * (0.006f + 0.012f * v->roughness) * (phase < pi ? 1.0f : 0.35f);

            float src = 0.0f;
            float env = 1.0f;
            float y = 0.0f;

            if (cur_sonorant) {
                float fade = (cur.type == SEG_W) ? 0.16f
                           : (cur.type == SEG_VOWEL) ? 0.08f : 0.10f;
                int prev_vfric = (i > 0) && prev.type >= SEG_H && prev.type != SEG_STOP && prev.voiced > 0.5f;
                int next_vfric = (i + 1 < count) && next.type >= SEG_H && next.type != SEG_STOP && next.voiced > 0.5f;
                float lead  = prev_sonorant ? 0.0f : prev_vfric ? fade * 0.4f : fade;
                float trail = next_sonorant ? 0.0f : next_vfric ? fade * 0.4f : fade;
                if (lead > 0.0f && u < lead) env = u / lead;
                else if (trail > 0.0f && u > 1.0f - trail) env = (1.0f - u) / trail;
                env = clampf(env, 0.0f, 1.0f);
            }

            if (cur.type == SEG_SILENCE) {
                y = 0.0f;
            } else if (cur.type <= SEG_NASAL) {
                static const float G[4][5] = {
                    /* src    f1     f2     f3     out */
                    { 1.00f, 1.25f, 0.92f, 0.42f, 0.55f }, /* vowel      */
                    { 0.95f, 1.40f, 0.58f, 0.16f, 0.48f }, /* l/r liquid */
                    { 0.82f, 1.35f, 0.48f, 0.09f, 0.44f }, /* w/y glide  */
                    { 0.78f, 1.55f, 0.28f, 0.08f, 0.40f }, /* nasal      */
                };
                const float *g = G[cur.type - SEG_VOWEL];

                src = voiced_src * voiced * g[0] * v->buzz
                    + frand1() * (noise * v->noise_scale + 0.008f * voiced);

                if (onset_glide && u < onset_frac && prev.voiced < 0.5f)
                    src += fric_noise() * 0.35f * (1.0f - u / onset_frac) * v->noise_scale;

                biquad_set_bandpass(&f1, tract_f1, tract_q1);
                biquad_set_bandpass(&f2, tract_f2, tract_q2);
                biquad_set_bandpass(&f3, tract_f3, tract_q3);

                y = g[1] * biquad_process(&f1, src) +
                    g[2] * v->brightness * biquad_process(&f2, src) +
                    g[3] * v->brightness * biquad_process(&f3, src);
                y *= amp * env * g[4];
            } else if (cur.type != SEG_STOP) {
                static const float N[3][7] = {
                    { 0.10f, 0.00f, 0.95f, 0.35f, 0.08f, 0.0f,  0.24f }, /* H        */
                    { 0.10f, 1.50f, 0.00f, 0.35f, 1.00f, 1.4f,  0.15f }, /* sibilant */
                    { 0.15f, 1.20f, 0.30f, 0.45f, 0.55f, 1.4f,  0.19f }, /* soft     */
                };
                const float *nw = N[cur.type - SEG_H];
                float voiced_fric = clampf(voiced, 0.0f, 1.0f);
                float gmod = clampf(0.35f + 0.75f * voiced_src, 0.0f, 1.0f);
                float hiss = 1.0f - voiced_fric * (0.85f - 0.55f * gmod);
                float nz = fric_noise() * (nw[0] + nw[1] * noise) * hiss * v->noise_scale;
                float bar = voiced_src * voiced_fric * v->buzz;

                float rise = (prev_sonorant && voiced_fric > 0.5f) ? 0.05f : 0.16f;
                float fall = (next_sonorant && voiced_fric > 0.5f) ? 0.94f : 0.85f;
                if (u < rise) env = u / rise;
                else if (u > fall) env = (1.0f - u) / (1.0f - fall);
                else env = 1.0f;
                env = clampf(env, 0.0f, 1.0f);

                biquad_set_bandpass(&f1,
                    lerpf(tract_f1, 240.0f * v->formant_scale, voiced_fric),
                    lerpf(tract_q1, 0.8f, voiced_fric));
                biquad_set_bandpass(&f2, tract_f2, tract_q2 * 0.65f);
                biquad_set_bandpass(&f3, tract_f3, tract_q3 * 0.55f);

                y = biquad_process(&f1, nw[2] * nz + nw[5] * bar) +
                    v->brightness * biquad_process(&f2, nw[3] * nz + 0.35f * voiced_fric * bar) +
                    biquad_process(&f3, nw[4] * nz);

                y *= amp * env * nw[6];
                y = saturatef(y, 0.6f);
            } else {
                float closure = clampf(0.62f - noise * 0.10f, 0.45f, 0.68f);

                if (u < closure) {
                    float cu = u / (closure > 1e-6f ? closure : 1.0f);
                    float closure_voiced = (i > 0 && prev.type != SEG_STOP) ? voiced : 0.0f;
                    float close_out = clampf((1.0f - cu) / 0.30f, 0.0f, 1.0f);
                    y = sinf(phase) * amp * closure_voiced * close_out * 0.028f;
                } else {
                    float voiced_release = clampf(voiced, 0.0f, 1.0f);
                    float release_u = (u - closure) / (1.0f - closure + 1e-6f);
                    float attack = smoothstep01(release_u / 0.15f);
                    float decay = expf(-release_u * (8.0f + 3.0f * noise));
                    float burst_env = attack * decay;

                    src =
                        fric_noise() * (0.6f + 0.7f * noise) * (1.0f - 0.35f * voiced_release) * v->noise_scale +
                        voiced_src * voiced_release * v->buzz * 0.5f;

                    biquad_set_bandpass(&f1, cur.f1 * v->formant_scale, cur.q1);
                    biquad_set_bandpass(&f2, cur.f2 * v->formant_scale, cur.q2);
                    biquad_set_bandpass(&f3, cur.f3 * v->formant_scale, cur.q3);

                    y =
                        0.30f * biquad_process(&f1, src) +
                        0.60f * biquad_process(&f2, src) +
                        1.00f * biquad_process(&f3, src);

                    y *= amp * burst_env * (0.55f + 0.35f * noise);
                    y = saturatef(y, 0.5f);
                }
            }

            {
                float radiated = y - rad_x1 + 0.992f * rad_y1;
                rad_x1 = y;
                rad_y1 = radiated;
                out[pos] += radiated;
            }
        }
        if (cur.type == SEG_STOP) {
            biquad_reset(&f1);
            biquad_reset(&f2);
            biquad_reset(&f3);
        }
    }

    return pos;
}

static float speech_buf[SPEECH_MAX];
static volatile int speech_len;
static int speech_cursor;
static int speech_voice = -1;

static int speech_pull(void* user, float* out, int n) {
    (void)user;
    int left = speech_len - speech_cursor;
    if (left > n) left = n;
    for (int i = 0; i < left; i++) out[i] = speech_buf[speech_cursor + i];
    speech_cursor += left;
    return left;
}

int speak_at(const char* encoded, Vec3 pos) {
    static const SpeechVoice robot = {
        1.00f, 0.0f, 1.00f, 1.02f, 0.38f, 0.06f, 0.98f, 0.000f
    };
    Segment* seg;
    int count;

    if (speech_voice >= 0) snd_stop(speech_voice);
    speech_len = 0;
    speech_cursor = 0;

    if (!decode_phrase_segments(encoded, &seg, &count)) return -1;
    memset(speech_buf, 0, sizeof speech_buf);
    int n = synth_segments(speech_buf, SPEECH_MAX, seg, count, &robot);
    free(seg);

    float peak = 1e-6f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(speech_buf[i]);
        if (a > peak) peak = a;
    }
    float gain = 0.8f / peak;
    for (int i = 0; i < n; i++) speech_buf[i] *= gain;

    speech_len = n;
    speech_voice = snd_play_stream_at(speech_pull, 0, 1.0f, 0, pos);
    return speech_voice;
}
