// audio.c — synthesized SFX + pre-rendered music loops (pure C, no assets)
#include "doom.h"
#include <SDL2/SDL.h>

#define SR 48000

static bool ready = false;
static bool muted = false;
static SDL_AudioDeviceID dev = 0;

// ------------------------------------------------------------------ buffers
typedef struct { float *d; int n; } Buf;

static Buf mkbuf(float secs) {
    Buf b;
    b.n = (int)(secs * SR);
    b.d = calloc((size_t)b.n, sizeof(float));
    if (!b.d) fatal("audio oom");
    return b;
}
typedef enum { W_SINE, W_SQUARE, W_SAW, W_TRI, W_NOISE } Wave;

static float wavef(Wave w, float ph) {
    ph -= floorf(ph);
    switch (w) {
    case W_SINE: return sinf(ph * 6.2831853f);
    case W_SQUARE: return ph < 0.5f ? 0.7f : -0.7f;
    case W_SAW: return ph * 2 - 1;
    case W_TRI: return ph < 0.5f ? ph * 4 - 1 : 3 - ph * 4;
    case W_NOISE: return rng_f() * 2 - 1;
    }
    return 0;
}
// frequency sweep tone with exp decay, added into buf
static void add_tone(Buf b, Wave w, float t0, float dur, float f0, float f1,
                     float amp, float decay, float vibHz, float vibAmt) {
    int s0 = (int)(t0 * SR), n = (int)(dur * SR);
    float ph = 0, lp = 0;
    for (int i = 0; i < n && s0 + i < b.n; i++) {
        float t = (float)i / SR;
        float f = f0 + (f1 - f0) * (t / dur);
        if (vibHz > 0) f *= 1.0f + sinf(t * 6.2831f * vibHz) * vibAmt;
        ph += f / SR;
        float v = wavef(w, ph);
        if (w == W_NOISE) {
            lp += (v - lp) * 0.35f; // soften noise slightly
            v = lp * 2.2f;
        }
        float e = expf(-t * decay);
        if (t < 0.002f) e *= t / 0.002f;
        b.d[s0 + i] += v * amp * e;
    }
}
// filtered noise burst with sweeping lowpass
static void add_noise(Buf b, float t0, float dur, float amp, float lpStart,
                      float lpEnd, float decay) {
    int s0 = (int)(t0 * SR), n = (int)(dur * SR);
    float lp = 0;
    for (int i = 0; i < n && s0 + i < b.n; i++) {
        float t = (float)i / SR;
        float k = lpStart + (lpEnd - lpStart) * (t / dur);
        float v = rng_f() * 2 - 1;
        lp += (v - lp) * k;
        float e = expf(-t * decay);
        if (t < 0.001f) e *= t / 0.001f;
        b.d[s0 + i] += lp * amp * e * 3.0f;
    }
}
// percussive drum hits appended to music buffers
static void drum(Buf b, int pos, int kind, float amp) {
    // kind 0 kick 1 snare 2 hat
    int n = kind == 0 ? (int)(0.16f * SR) : kind == 1 ? (int)(0.14f * SR) : (int)(0.05f * SR);
    float ph = 0, lp = 0;
    for (int i = 0; i < n && pos + i < b.n; i++) {
        float t = (float)i / SR;
        float v = 0;
        if (kind == 0) {
            float f = 110 - 70 * (t / 0.16f);
            ph += f / SR;
            v = sinf(ph * 6.2831f) * 2.2f;
        } else {
            float wn = rng_f() * 2 - 1;
            lp += (wn - lp) * (kind == 1 ? 0.5f : 0.9f);
            v = lp * (kind == 1 ? 2.0f : 1.2f);
        }
        b.d[pos + i] += v * amp * expf(-t * (kind == 0 ? 24 : kind == 1 ? 28 : 70));
    }
}
static void normalize(Buf b, float peak) {
    float m = 0;
    for (int i = 0; i < b.n; i++)
        if (fabsf(b.d[i]) > m) m = fabsf(b.d[i]);
    if (m > 0)
        for (int i = 0; i < b.n; i++) b.d[i] = b.d[i] / m * peak;
}

static Buf sfx[SX_COUNT];
static Buf musicLoop[3]; // 0 none, 1 game, 2 title

static void build_sfx(void) {
    rng_seed(777);

    sfx[SX_PISTOL] = mkbuf(0.16f);
    add_noise(sfx[SX_PISTOL], 0, 0.10f, 1.0f, 0.55f, 0.12f, 26);
    add_tone(sfx[SX_PISTOL], W_SINE, 0, 0.06f, 190, 90, 0.7f, 50, 0, 0);
    normalize(sfx[SX_PISTOL], 0.9f);

    sfx[SX_SHOTGUN] = mkbuf(0.42f);
    add_noise(sfx[SX_SHOTGUN], 0, 0.34f, 1.3f, 0.42f, 0.06f, 11);
    add_tone(sfx[SX_SHOTGUN], W_SINE, 0, 0.18f, 95, 42, 1.1f, 17, 0, 0);
    add_noise(sfx[SX_SHOTGUN], 0.02f, 0.08f, 0.5f, 0.7f, 0.3f, 40);
    normalize(sfx[SX_SHOTGUN], 0.95f);

    sfx[SX_CHAINGUN] = mkbuf(0.10f);
    add_noise(sfx[SX_CHAINGUN], 0, 0.07f, 1.0f, 0.65f, 0.18f, 38);
    add_tone(sfx[SX_CHAINGUN], W_SINE, 0, 0.04f, 210, 110, 0.5f, 70, 0, 0);
    normalize(sfx[SX_CHAINGUN], 0.8f);

    sfx[SX_ROCKET] = mkbuf(0.55f);
    add_noise(sfx[SX_ROCKET], 0, 0.45f, 0.9f, 0.12f, 0.5f, 6);
    add_tone(sfx[SX_ROCKET], W_SAW, 0, 0.3f, 130, 55, 0.5f, 8, 3, 0.1f);
    normalize(sfx[SX_ROCKET], 0.85f);

    sfx[SX_EXPLOSION] = mkbuf(1.0f);
    add_noise(sfx[SX_EXPLOSION], 0, 0.85f, 1.6f, 0.28f, 0.03f, 4.5f);
    add_tone(sfx[SX_EXPLOSION], W_SINE, 0, 0.6f, 66, 30, 1.2f, 5, 0, 0);
    normalize(sfx[SX_EXPLOSION], 1.0f);

    sfx[SX_BARREL_EXP] = mkbuf(0.8f);
    add_noise(sfx[SX_BARREL_EXP], 0, 0.6f, 1.4f, 0.4f, 0.04f, 6);
    add_tone(sfx[SX_BARREL_EXP], W_SINE, 0, 0.4f, 90, 36, 1.0f, 7, 0, 0);
    normalize(sfx[SX_BARREL_EXP], 1.0f);

    sfx[SX_PLASMA] = mkbuf(0.15f);
    add_tone(sfx[SX_PLASMA], W_SQUARE, 0, 0.11f, 1500, 320, 0.6f, 22, 40, 0.06f);
    add_tone(sfx[SX_PLASMA], W_SQUARE, 0, 0.09f, 1520, 340, 0.35f, 26, 53, 0.05f);
    normalize(sfx[SX_PLASMA], 0.7f);

    sfx[SX_DOOR_OPEN] = mkbuf(0.75f);
    add_tone(sfx[SX_DOOR_OPEN], W_SQUARE, 0, 0.55f, 82, 96, 0.35f, 3.5f, 13, 0.12f);
    add_noise(sfx[SX_DOOR_OPEN], 0.52f, 0.12f, 0.5f, 0.3f, 0.1f, 26);
    normalize(sfx[SX_DOOR_OPEN], 0.7f);

    sfx[SX_DOOR_CLOSE] = mkbuf(0.75f);
    add_noise(sfx[SX_DOOR_CLOSE], 0, 0.1f, 0.5f, 0.3f, 0.1f, 26);
    add_tone(sfx[SX_DOOR_CLOSE], W_SQUARE, 0.12f, 0.5f, 96, 78, 0.33f, 3.5f, 11, 0.12f);
    normalize(sfx[SX_DOOR_CLOSE], 0.7f);

    sfx[SX_DOOR_LOCKED] = mkbuf(0.25f);
    add_tone(sfx[SX_DOOR_LOCKED], W_SQUARE, 0, 0.2f, 112, 108, 0.5f, 9, 27, 0.0f);
    normalize(sfx[SX_DOOR_LOCKED], 0.6f);

    sfx[SX_PICKUP] = mkbuf(0.14f);
    add_tone(sfx[SX_PICKUP], W_SQUARE, 0, 0.05f, 660, 660, 0.4f, 18, 0, 0);
    add_tone(sfx[SX_PICKUP], W_SQUARE, 0.06f, 0.07f, 880, 880, 0.4f, 16, 0, 0);
    normalize(sfx[SX_PICKUP], 0.55f);

    sfx[SX_PICKUP_WEAPON] = mkbuf(0.30f);
    add_tone(sfx[SX_PICKUP_WEAPON], W_SQUARE, 0, 0.07f, 440, 442, 0.4f, 12, 0, 0);
    add_tone(sfx[SX_PICKUP_WEAPON], W_SQUARE, 0.09f, 0.07f, 587, 589, 0.4f, 12, 0, 0);
    add_tone(sfx[SX_PICKUP_WEAPON], W_SQUARE, 0.18f, 0.11f, 880, 884, 0.45f, 9, 0, 0);
    normalize(sfx[SX_PICKUP_WEAPON], 0.6f);

    sfx[SX_PICKUP_KEY] = mkbuf(0.4f);
    add_tone(sfx[SX_PICKUP_KEY], W_TRI, 0, 0.35f, 880, 880, 0.5f, 7, 0, 0);
    add_tone(sfx[SX_PICKUP_KEY], W_TRI, 0.02f, 0.33f, 1318, 1318, 0.35f, 7, 0, 0);
    normalize(sfx[SX_PICKUP_KEY], 0.6f);

    sfx[SX_SECRET] = mkbuf(0.9f);
    add_tone(sfx[SX_SECRET], W_SAW, 0, 0.8f, 220, 222, 0.3f, 2.5f, 5, 0.01f);
    add_tone(sfx[SX_SECRET], W_SAW, 0.1f, 0.7f, 277, 279, 0.3f, 2.5f, 6, 0.01f);
    add_tone(sfx[SX_SECRET], W_SAW, 0.2f, 0.6f, 330, 333, 0.3f, 2.5f, 7, 0.01f);
    normalize(sfx[SX_SECRET], 0.65f);

    sfx[SX_SWITCH] = mkbuf(0.3f);
    add_noise(sfx[SX_SWITCH], 0, 0.06f, 0.8f, 0.4f, 0.15f, 40);
    add_tone(sfx[SX_SWITCH], W_SQUARE, 0.08f, 0.12f, 240, 180, 0.4f, 20, 0, 0);
    normalize(sfx[SX_SWITCH], 0.75f);

    sfx[SX_PLAYER_PAIN] = mkbuf(0.3f);
    add_tone(sfx[SX_PLAYER_PAIN], W_SAW, 0, 0.24f, 165, 92, 0.8f, 11, 9, 0.05f);
    add_noise(sfx[SX_PLAYER_PAIN], 0.01f, 0.15f, 0.3f, 0.25f, 0.2f, 16);
    normalize(sfx[SX_PLAYER_PAIN], 0.8f);

    sfx[SX_PLAYER_DIE] = mkbuf(1.0f);
    add_tone(sfx[SX_PLAYER_DIE], W_SAW, 0, 0.85f, 210, 48, 0.9f, 3.2f, 7, 0.08f);
    add_noise(sfx[SX_PLAYER_DIE], 0.15f, 0.6f, 0.35f, 0.2f, 0.05f, 5);
    normalize(sfx[SX_PLAYER_DIE], 0.9f);

    sfx[SX_IMP_SIGHT] = mkbuf(0.5f);
    add_tone(sfx[SX_IMP_SIGHT], W_SAW, 0, 0.42f, 118, 88, 0.9f, 5, 9, 0.18f);
    normalize(sfx[SX_IMP_SIGHT], 0.75f);
    sfx[SX_TROOPER_SIGHT] = mkbuf(0.3f);
    add_tone(sfx[SX_TROOPER_SIGHT], W_SAW, 0, 0.2f, 135, 105, 0.8f, 10, 14, 0.1f);
    normalize(sfx[SX_TROOPER_SIGHT], 0.7f);
    sfx[SX_PINKY_SIGHT] = mkbuf(0.6f);
    add_tone(sfx[SX_PINKY_SIGHT], W_SAW, 0, 0.5f, 84, 62, 1.0f, 4, 8, 0.15f);
    add_tone(sfx[SX_PINKY_SIGHT], W_SINE, 0, 0.5f, 42, 31, 0.7f, 4, 0, 0);
    normalize(sfx[SX_PINKY_SIGHT], 0.8f);
    sfx[SX_BARON_SIGHT] = mkbuf(0.9f);
    add_tone(sfx[SX_BARON_SIGHT], W_SAW, 0, 0.8f, 58, 40, 1.0f, 2.8f, 6, 0.14f);
    add_tone(sfx[SX_BARON_SIGHT], W_SINE, 0, 0.8f, 29, 20, 0.9f, 2.8f, 0, 0);
    normalize(sfx[SX_BARON_SIGHT], 0.9f);

    sfx[SX_IMP_THROW] = mkbuf(0.3f);
    add_noise(sfx[SX_IMP_THROW], 0, 0.22f, 0.7f, 0.1f, 0.45f, 9);
    add_tone(sfx[SX_IMP_THROW], W_SINE, 0, 0.2f, 320, 620, 0.3f, 9, 0, 0);
    normalize(sfx[SX_IMP_THROW], 0.7f);

    sfx[SX_IMP_DIE] = mkbuf(0.6f);
    add_tone(sfx[SX_IMP_DIE], W_SAW, 0, 0.5f, 150, 55, 0.9f, 5, 10, 0.1f);
    add_noise(sfx[SX_IMP_DIE], 0.1f, 0.35f, 0.3f, 0.2f, 0.08f, 8);
    normalize(sfx[SX_IMP_DIE], 0.85f);
    sfx[SX_TROOPER_DIE] = mkbuf(0.5f);
    add_tone(sfx[SX_TROOPER_DIE], W_SAW, 0, 0.4f, 380, 160, 0.8f, 6, 16, 0.12f);
    normalize(sfx[SX_TROOPER_DIE], 0.8f);
    sfx[SX_PINKY_DIE] = mkbuf(0.7f);
    add_tone(sfx[SX_PINKY_DIE], W_SAW, 0, 0.6f, 100, 38, 1.0f, 4, 8, 0.1f);
    normalize(sfx[SX_PINKY_DIE], 0.85f);
    sfx[SX_BARON_DIE] = mkbuf(1.4f);
    add_tone(sfx[SX_BARON_DIE], W_SAW, 0, 1.2f, 70, 24, 1.0f, 2.2f, 6, 0.1f);
    add_tone(sfx[SX_BARON_DIE], W_SINE, 0, 1.2f, 35, 14, 0.9f, 2.2f, 0, 0);
    add_noise(sfx[SX_BARON_DIE], 0.2f, 0.9f, 0.4f, 0.15f, 0.03f, 3.5f);
    normalize(sfx[SX_BARON_DIE], 0.95f);

    sfx[SX_FIREBALL_HIT] = mkbuf(0.35f);
    add_noise(sfx[SX_FIREBALL_HIT], 0, 0.25f, 0.8f, 0.5f, 0.1f, 14);
    add_tone(sfx[SX_FIREBALL_HIT], W_SINE, 0, 0.2f, 140, 60, 0.6f, 12, 0, 0);
    normalize(sfx[SX_FIREBALL_HIT], 0.8f);

    sfx[SX_TELEPORT] = mkbuf(0.6f);
    add_noise(sfx[SX_TELEPORT], 0, 0.5f, 0.6f, 0.06f, 0.6f, 5);
    add_tone(sfx[SX_TELEPORT], W_SINE, 0, 0.45f, 180, 900, 0.4f, 4, 0, 0);
    normalize(sfx[SX_TELEPORT], 0.8f);

    sfx[SX_MENU_MOVE] = mkbuf(0.07f);
    add_tone(sfx[SX_MENU_MOVE], W_SQUARE, 0, 0.045f, 440, 430, 0.4f, 30, 0, 0);
    normalize(sfx[SX_MENU_MOVE], 0.45f);
    sfx[SX_MENU_SELECT] = mkbuf(0.14f);
    add_tone(sfx[SX_MENU_SELECT], W_SQUARE, 0, 0.06f, 520, 520, 0.4f, 20, 0, 0);
    add_tone(sfx[SX_MENU_SELECT], W_SQUARE, 0.05f, 0.08f, 780, 790, 0.4f, 16, 0, 0);
    normalize(sfx[SX_MENU_SELECT], 0.5f);

    sfx[SX_EMPTY] = mkbuf(0.06f);
    add_noise(sfx[SX_EMPTY], 0, 0.03f, 0.5f, 0.8f, 0.5f, 90);
    normalize(sfx[SX_EMPTY], 0.4f);

    sfx[SX_STIM_NO] = mkbuf(0.16f);
    add_tone(sfx[SX_STIM_NO], W_SQUARE, 0, 0.13f, 148, 142, 0.4f, 10, 0, 0);
    normalize(sfx[SX_STIM_NO], 0.5f);
}

// note name -> frequency (midi)
static float midif(int m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }

static void build_music(void) {
    // ---- track 1: driving chug (E minor), 64 x 16ths @ 138bpm ----
    const float spb = 60.0f / 138.0f / 4.0f; // seconds per 16th
    int total = (int)(64 * spb * SR);
    musicLoop[1].n = total;
    musicLoop[1].d = calloc((size_t)total, sizeof(float));
    Buf M = musicLoop[1];
    static const int bass[64] = {
        40, 40, 0, 40, 40, 43, 0, 45, 40, 40, 0, 40, 47, 45, 43, 41,
        40, 40, 0, 40, 40, 43, 0, 45, 40, 40, 0, 40, 43, 41, 40, 38,
        40, 40, 0, 40, 40, 43, 0, 45, 40, 40, 0, 40, 47, 45, 43, 41,
        43, 43, 0, 43, 45, 45, 0, 45, 47, 47, 46, 46, 45, 43, 41, 38,
    };
    static const int lead[64] = {
        0, 0, 0, 0, 0, 0, 0, 0, 76, 0, 75, 0, 76, 0, 79, 0,
        0, 0, 0, 0, 83, 81, 79, 76, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 76, 0, 79, 0, 81, 0, 83, 0,
        86, 0, 83, 0, 81, 0, 79, 0, 81, 79, 76, 74, 76, 0, 0, 0,
    };
    for (int st = 0; st < 64; st++) {
        int pos = (int)(st * spb * SR);
        int bd = bass[st];
        if (bd)
            add_tone(M, W_SQUARE, st * spb, spb * 0.95f, midif(bd), midif(bd),
                     0.30f, 5.5f, 0, 0);
        int ld = lead[st];
        if (ld)
            add_tone(M, W_SAW, st * spb, spb * 1.8f, midif(ld), midif(ld),
                     0.16f, 3.0f, 5.5f, 0.008f);
        if (st % 8 == 0) drum(M, pos, 0, 0.85f);
        if (st % 8 == 4) drum(M, pos, 1, 0.55f);
        if (st % 2 == 0) drum(M, pos, 2, 0.22f);
    }
    normalize(musicLoop[1], 0.55f);

    // ---- track 2: ominous title drone ----
    musicLoop[2] = mkbuf(8.0f);
    Buf T = musicLoop[2];
    add_tone(T, W_SAW, 0, 8.0f, 41.2f, 41.2f, 0.30f, 0.12f, 0.35f, 0.01f); // E1
    add_tone(T, W_SAW, 0, 8.0f, 61.7f, 61.7f, 0.18f, 0.12f, 0.23f, 0.012f);
    add_tone(T, W_SINE, 0, 8.0f, 82.4f, 82.4f, 0.22f, 0.1f, 0.17f, 0.008f);
    // slow heartbeat
    for (int k = 0; k < 8; k++) {
        drum(T, (int)((k * 2 + 0.2f) * SR), 0, 0.5f);
        drum(T, (int)((k * 2 + 0.55f) * SR), 0, 0.3f);
    }
    normalize(musicLoop[2], 0.5f);
    musicLoop[0].n = 0;
    musicLoop[0].d = NULL;
}

// ------------------------------------------------------------------ mixer
#define MAX_VOICES 40
typedef struct {
    bool active;
    const Buf *buf;
    int pos;
    float gainL, gainR;
} Voice;
static Voice voices[MAX_VOICES];

static void mixSFX(float *out, int frames) {
    for (int v = 0; v < MAX_VOICES; v++) {
        Voice *vc = &voices[v];
        if (!vc->active) continue;
        const Buf *b = vc->buf;
        int remaining = frames;
        float *o = out;
        int p = vc->pos;
        while (remaining > 0) {
            if (p >= b->n) { vc->active = false; break; }
            float s = b->d[p++];
            o[0] += s * vc->gainL;
            o[1] += s * vc->gainR;
            o += 2;
            remaining--;
        }
        vc->pos = p;
    }
}
static int musicPos = 0;
static int curTrack = 0;
static void mixMusic(float *out, int frames) {
    const Buf *m = &musicLoop[curTrack];
    if (!m->d || m->n == 0) return;
    for (int i = 0; i < frames; i++) {
        float s = m->d[musicPos++] * 0.9f;
        if (musicPos >= m->n) musicPos = 0;
        out[i * 2] += s;
        out[i * 2 + 1] += s;
    }
}

static void audio_cb(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int frames = len / (int)(sizeof(float) * 2);
    float tmp[1024 * 2];
    float *dst = (float *)stream;
    memset(stream, 0, (size_t)len);
    if (muted) return;
    while (frames > 0) {
        int chunk = frames > 1024 ? 1024 : frames;
        memset(tmp, 0, sizeof(float) * 2 * chunk);
        mixMusic(tmp, chunk);
        mixSFX(tmp, chunk);
        for (int i = 0; i < chunk * 2; i++) dst[i] = tmp[i];
        dst += chunk * 2;
        frames -= chunk;
    }
}

bool audio_init(void) {
    if (ready) return true;
    build_sfx();
    build_music();
    SDL_AudioSpec want = {0}, have;
    want.freq = SR;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_cb;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        fprintf(stderr, "audio unavailable: %s\n", SDL_GetError());
        return false; // game still works silent
    }
    SDL_PauseAudioDevice(dev, 0);
    ready = true;
    return true;
}
void audio_quit(void) {
    if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
    ready = false;
}
void sfx_play(int id, float gain, float pan) {
    if (!ready || muted || id < 0 || id >= SX_COUNT || gain <= 0.01f) return;
    if (!sfx[id].d) return;
    float ang = (pan + 1) * 0.785398f;
    float gl = gain * cosf(ang), gr = gain * sinf(ang);
    for (int v = 0; v < MAX_VOICES; v++) {
        if (voices[v].active) continue;
        voices[v].active = true;
        voices[v].buf = &sfx[id];
        voices[v].pos = 0;
        voices[v].gainL = gl;
        voices[v].gainR = gr;
        return;
    }
}
void music_play(int track) {
    curTrack = track;
    musicPos = 0;
}
void audio_toggle_mute(void) { muted = !muted; }
