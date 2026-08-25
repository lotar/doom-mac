// textures.c — all graphics are generated procedurally at startup.
// No external assets, no copyrighted material.
#include "doom.h"

static Tex g_tex[256];
static bool g_texMade[256];

// ------------------------------------------------------------------ helpers
static Tex *tnew(int id, int w, int h) {
    Tex *t = &g_tex[id];
    t->w = w; t->h = h;
    t->px = xmalloc(sizeof(uint32_t) * w * h);
    memset(t->px, 0, sizeof(uint32_t) * w * h);
    g_texMade[id] = true;
    return t;
}

static inline void tpset(Tex *t, int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    t->px[y * t->w + x] = c;
}
static inline uint32_t tpget(Tex *t, int x, int y) {
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return 0;
    return t->px[y * t->w + x];
}
static void tfill(Tex *t, uint32_t c) {
    for (int i = 0; i < t->w * t->h; i++) t->px[i] = c;
}
static void trect(Tex *t, int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) tpset(t, i, j, c);
}
static void tdisk(Tex *t, int cx, int cy, int r, uint32_t c) {
    for (int j = cy - r; j <= cy + r; j++)
        for (int i = cx - r; i <= cx + r; i++) {
            int dx = i - cx, dy = j - cy;
            if (dx * dx + dy * dy <= r * r) tpset(t, i, j, c);
        }
}
static void tnoise(Tex *t, int amt, uint32_t seed) {
    rng_seed(seed);
    for (int j = 0; j < t->h; j++)
        for (int i = 0; i < t->w; i++) {
            uint32_t c = tpget(t, i, j);
            if (!c) continue;
            int d = (int)(rng_f() * amt * 2) - amt;
            int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            r += d; g += d; b += d;
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            tpset(t, i, j, col_rgb(r, g, b));
        }
}
static float hash01(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)(x * 374761393 + y * 668265263) ^ seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h >> 8) & 0xFFFF) / 65536.0f;
}
static float vnoise(float x, float y, uint32_t seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float xf = x - xi, yf = y - yi;
    xf = xf * xf * (3 - 2 * xf); yf = yf * yf * (3 - 2 * yf);
    float a = hash01(xi, yi, seed),     b = hash01(xi + 1, yi, seed);
    float c = hash01(xi, yi + 1, seed), d = hash01(xi + 1, yi + 1, seed);
    return a + (b - a) * xf + (c - a) * yf + (a - b - c + d) * xf * yf;
}

// ------------------------------------------------------------- wall textures
#define TS 64
static void mk_tech(void) {
    Tex *t = tnew(TX_WALL_TECH, TS, TS);
    tfill(t, col_rgb(158, 142, 112));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.15f, y * 0.15f, 11);
            uint32_t c = tpget(t, x, y);
            c = col_shade(c, 0.82f + n * 0.3f);
            tpset(t, x, y, c);
        }
    // panel seams every 21 px vertical, one horizontal
    for (int x = 0; x < TS; x += 21) {
        for (int y = 0; y < TS; y++) {
            tpset(t, x, y, col_rgb(88, 78, 62));
            if (x + 1 < TS) tpset(t, x + 1, y, col_rgb(190, 176, 146));
        }
    }
    for (int x = 0; x < TS; x++) {
        tpset(t, x, 31, col_rgb(88, 78, 62));
        tpset(t, x, 32, col_rgb(190, 176, 146));
    }
    // rivets
    for (int x = 4; x < TS; x += 21)
        for (int y = 4; y < TS; y += 27) {
            tpset(t, x, y, col_rgb(210, 200, 170));
            tpset(t, x + 1, y + 1, col_rgb(70, 62, 50));
        }
    // vents in middle panel
    for (int y = 12; y < 24; y += 3)
        trect(t, 26, y, 12, 2, col_rgb(72, 64, 52));
    tnoise(t, 7, 77);
}

static void mk_brick(void) {
    Tex *t = tnew(TX_WALL_BRICK, TS, TS);
    tfill(t, col_rgb(74, 58, 48)); // mortar
    rng_seed(42);
    for (int by = 0; by < 8; by++) {
        int off = (by & 1) ? 8 : 0;
        for (int bx = -1; bx < 5; bx++) {
            int x0 = bx * 16 + off;
            float v = hash01(bx, by, 99);
            uint32_t bc = col_rgb(118 + (int)(v * 34), 84 + (int)(v * 22), 66 + (int)(v * 18));
            trect(t, x0 + 1, by * 8 + 1, 14, 6, bc);
            trect(t, x0 + 1, by * 8 + 1, 14, 1, col_shade(bc, 1.25f));
            trect(t, x0 + 1, by * 8 + 6, 14, 1, col_shade(bc, 0.7f));
        }
    }
    tnoise(t, 9, 43);
}

static void mk_rock(void) {
    Tex *t = tnew(TX_WALL_ROCK, TS, TS);
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.09f, y * 0.09f, 7) * 0.7f + vnoise(x * 0.25f, y * 0.25f, 8) * 0.3f;
            int v = 70 + (int)(n * 80);
            tpset(t, x, y, col_rgb(v + 28, v + 6, v - 14));
        }
    // cracks
    rng_seed(5);
    for (int k = 0; k < 5; k++) {
        int x = rng_irange(4, TS - 4), y = 0;
        for (int i = 0; i < TS; i++) {
            tpset(t, x, y, col_rgb(38, 28, 20));
            if (rng_f() < 0.4f && x + 1 < TS) tpset(t, x + 1, y, col_rgb(48, 36, 26));
            y++; x += rng_irange(-1, 1);
            if (x < 1) x = 1; if (x > TS - 2) x = TS - 2;
            if (y >= TS) break;
        }
    }
}

static void mk_metal(void) {
    Tex *t = tnew(TX_WALL_METAL, TS, TS);
    tfill(t, col_rgb(108, 114, 124));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.05f, y * 0.4f, 21);
            tpset(t, x, y, col_shade(tpget(t, x, y), 0.85f + n * 0.3f));
        }
    // plates
    for (int py = 0; py < 2; py++)
        for (int px = 0; px < 2; px++) {
            int x0 = px * 32, y0 = py * 32;
            for (int i = 0; i < 32; i++) {
                tpset(t, x0 + i, y0, col_rgb(60, 64, 72));
                tpset(t, x0, y0 + i, col_rgb(60, 64, 72));
            }
            tdisk(t, x0 + 4, y0 + 4, 1, col_rgb(150, 156, 166));
            tdisk(t, x0 + 27, y0 + 4, 1, col_rgb(150, 156, 166));
            tdisk(t, x0 + 4, y0 + 27, 1, col_rgb(150, 156, 166));
            tdisk(t, x0 + 27, y0 + 27, 1, col_rgb(150, 156, 166));
        }
    tnoise(t, 6, 91);
}

static void mk_marble(void) {
    Tex *t = tnew(TX_WALL_MARBLE, TS, TS);
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float wx = vnoise(x * 0.06f, y * 0.06f, 31);
            float n = vnoise(x * 0.08f + wx * 3, y * 0.02f, 33);
            int v = (int)(n * 70);
            tpset(t, x, y, col_rgb(24 + v / 2, 44 + v, 30 + v / 2));
            if (n > 0.62f) tpset(t, x, y, col_rgb(96, 148, 104)); // vein
        }
    tnoise(t, 5, 17);
}

static void mk_comp_frame(int id, uint32_t seed) {
    Tex *t = tnew(id, TS, TS);
    tfill(t, col_rgb(26, 30, 42));
    for (int gy = 0; gy < 4; gy++)
        for (int gx = 0; gx < 4; gx++) {
            int x0 = gx * 16 + 2, y0 = gy * 16 + 2;
            trect(t, x0, y0, 12, 12, col_rgb(14, 16, 24));
            rng_seed((uint32_t)(seed + gy * 4 + gx));
            for (int r = 0; r < 4; r++) {
                uint32_t cols[3] = { col_rgb(60, 220, 90), col_rgb(230, 180, 60), col_rgb(220, 70, 60) };
                uint32_t cc = cols[rng_irange(0, 2)];
                if (rng_f() < 0.45f) continue;
                trect(t, x0 + 1, y0 + 2 + r * 2, 2 + rng_irange(2, 8), 1, cc);
            }
        }
    // status light strip bottom
    rng_seed(seed + 999);
    for (int x = 2; x < TS - 2; x += 4) {
        uint32_t c = (rng_f() < 0.5f) ? col_rgb(240, 60, 50) : col_rgb(70, 220, 90);
        trect(t, x, TS - 4, 2, 2, c);
    }
    tnoise(t, 3, seed);
}

static void mk_door(int id, uint32_t accent, uint32_t accentDark) {
    Tex *t = tnew(id, TS, TS);
    tfill(t, col_rgb(96, 102, 112));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.5f, y * 0.06f, 61); // vertical brushing
            tpset(t, x, y, col_shade(col_rgb(96, 102, 112), 0.8f + n * 0.35f));
        }
    // center seam
    for (int y = 0; y < TS; y++) { tpset(t, 31, y, col_rgb(30, 32, 38)); tpset(t, 32, y, col_rgb(30, 32, 38)); }
    // hazard chevron bands
    for (int x = 0; x < TS; x++) {
        for (int y = 0; y < 6; y++) {
            bool sw = ((x + y) / 5) & 1;
            tpset(t, x, y, sw ? col_rgb(212, 178, 40) : col_rgb(24, 24, 24));
            tpset(t, x, TS - 1 - y, sw ? col_rgb(212, 178, 40) : col_rgb(24, 24, 24));
        }
    }
    // accent band + light
    trect(t, 0, 28, TS, 8, accentDark);
    trect(t, 0, 29, TS, 2, accent);
    tdisk(t, 16, 46, 3, accent);
    tdisk(t, 47, 46, 3, accent);
    tnoise(t, 5, 63);
}

static void mk_exitdoor(void) {
    Tex *t = tnew(TX_EXITDOOR, TS, TS);
    tfill(t, col_rgb(70, 74, 84));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.4f, y * 0.08f, 71);
            tpset(t, x, y, col_shade(col_rgb(70, 74, 84), 0.8f + n * 0.3f));
        }
    trect(t, 8, 18, 48, 28, col_rgb(120, 20, 16));
    trect(t, 8, 18, 48, 2, col_rgb(180, 40, 30));
    trect(t, 8, 44, 48, 2, col_rgb(60, 10, 8));
    // EXIT letters 3x5
    static const char *E[5] = {"###", "#..", "##.", "#..", "###"};
    static const char *Xl[5] = {"#.#", "#.#", ".#.", "#.#", "#.#"};
    static const char *I[5] = {"###", ".#.", ".#.", ".#.", "###"};
    static const char *Tl[5] = {"###", ".#.", ".#.", ".#.", ".#."};
    const char **letters[4] = { E, Xl, I, Tl };
    for (int li = 0; li < 4; li++)
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 3; c++)
                if (letters[li][r][c] == '#')
                    trect(t, 12 + li * 10 + c, 23 + r, 1, 1, col_rgb(240, 230, 200));
    tnoise(t, 4, 65);
}

static void mk_switch(bool on) {
    Tex *t = tnew(on ? TX_SWITCH_ON : TX_SWITCH_OFF, TS, TS);
    tfill(t, col_rgb(84, 90, 100));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.3f, y * 0.3f, 81);
            tpset(t, x, y, col_shade(col_rgb(84, 90, 100), 0.85f + n * 0.25f));
        }
    trect(t, 14, 14, 36, 36, col_rgb(40, 42, 48));
    trect(t, 15, 15, 34, 34, col_rgb(56, 60, 68));
    uint32_t btn = on ? col_rgb(70, 220, 80) : col_rgb(210, 50, 40);
    tdisk(t, 32, 30, 9, col_rgb(24, 24, 28));
    tdisk(t, 32, 30, 7, btn);
    tdisk(t, 30, 28, 3, col_shade(btn, 1.4f));
    if (on) {
        trect(t, 20, 44, 24, 3, col_rgb(70, 220, 80));
    } else {
        trect(t, 20, 44, 24, 3, col_rgb(120, 30, 24));
    }
    tnoise(t, 4, 83);
}

static void mk_sky(void) {
    Tex *t = tnew(TX_SKY, 512, 128);
    for (int y = 0; y < 128; y++) {
        float f = y / 127.0f;
        int r = (int)(30 + f * 90);
        int g = (int)(18 + f * 40);
        int b = (int)(52 + f * 30);
        for (int x = 0; x < 512; x++) tpset(t, x, y, col_rgb(r, g, b));
    }
    // cloud bands
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 512; x++) {
            float n = vnoise(x * 0.02f, y * 0.04f, 123);
            if (n > 0.6f && y < 90) {
                uint32_t c = tpget(t, x, y);
                tpset(t, x, y, col_shade(c, 1.0f + (n - 0.6f) * 0.8f));
            }
        }
    // stars
    rng_seed(777);
    for (int k = 0; k < 90; k++) {
        int x = rng_irange(0, 511), y = rng_irange(0, 70);
        tpset(t, x, y, col_rgb(220, 220, 255));
    }
    // mountains silhouette
    for (int x = 0; x < 512; x++) {
        int mh = (int)((vnoise(x * 0.03f, 0.5f, 55) * 0.7f + vnoise(x * 0.09f, 0.5f, 56) * 0.3f) * 26) + 6;
        for (int y = 128 - mh; y < 128; y++) {
            int sh = y - (128 - mh);
            tpset(t, x, y, col_rgb(14 + sh, 8 + sh / 2, 16 + sh));
        }
    }
}

// ------------------------------------------------------------- flat textures
static void mk_flat_tech(void) {
    Tex *t = tnew(TX_FL_TECH, TS, TS);
    tfill(t, col_rgb(96, 92, 86));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.12f, y * 0.12f, 141);
            tpset(t, x, y, col_shade(col_rgb(96, 92, 86), 0.85f + n * 0.25f));
        }
    for (int i = 0; i <= TS; i += 32) {
        for (int k = 0; k < TS; k++) { tpset(t, i % TS, k, col_rgb(56, 54, 50)); tpset(t, k, i % TS, col_rgb(56, 54, 50)); }
    }
    tnoise(t, 6, 143);
}
static void mk_flat_stone(void) {
    Tex *t = tnew(TX_FL_STONE, TS, TS);
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.1f, y * 0.1f, 151) * 0.6f + vnoise(x * 0.33f, y * 0.33f, 152) * 0.4f;
            int v = 60 + (int)(n * 46);
            tpset(t, x, y, col_rgb(v, v - 6, v - 14));
        }
    tnoise(t, 7, 153);
}
static void mk_flat_dirt(void) {
    Tex *t = tnew(TX_FL_DIRT, TS, TS);
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.14f, y * 0.14f, 161);
            int v = 70 + (int)(n * 40);
            tpset(t, x, y, col_rgb(v + 16, v - 4, v - 26));
        }
    rng_seed(162);
    for (int k = 0; k < 26; k++) {
        int x = rng_irange(0, 63), y = rng_irange(0, 63);
        tpset(t, x, y, col_rgb(110, 96, 80));
    }
    tnoise(t, 6, 163);
}
static void mk_flat_slime(void) {
    for (int fr = 0; fr < 2; fr++) {
        Tex *t = tnew(fr ? TX_FL_SLIME2 : TX_FL_SLIME, TS, TS);
        for (int y = 0; y < TS; y++)
            for (int x = 0; x < TS; x++) {
                float ph = fr * 3.1f;
                float n = vnoise(x * 0.09f + ph, y * 0.09f, 171) * 0.7f + vnoise(x * 0.2f - ph, y * 0.2f + ph, 172) * 0.3f;
                int v = (int)(n * 70);
                tpset(t, x, y, col_rgb(40 + v, 120 + v, 30 + v / 2));
                if (n > 0.66f) tpset(t, x, y, col_rgb(150, 230, 110)); // highlight bubbles
            }
    }
}
static void mk_pad(int id, uint32_t glow) {
    Tex *t = tnew(id, TS, TS);
    tfill(t, col_rgb(34, 36, 44));
    for (int ring = 30; ring > 0; ring -= 6) {
        for (int a = 0; a < 360; a += 2) {
            int x = 32 + (int)(cosf(a * 0.01745f) * ring);
            int y = 32 + (int)(sinf(a * 0.01745f) * ring);
            tpset(t, x, y, glow);
            tpset(t, x, y + 1, col_shade(glow, 0.6f));
        }
    }
    tnoise(t, 4, 181);
}
static void mk_ceiling_conc(void) {
    Tex *t = tnew(CE_CONC, TS, TS);
    tfill(t, col_rgb(58, 58, 62));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.1f, y * 0.1f, 191);
            tpset(t, x, y, col_shade(col_rgb(58, 58, 62), 0.85f + n * 0.25f));
        }
    for (int i = 0; i <= TS; i += 16)
        for (int k = 0; k < TS; k++) tpset(t, i % TS, k, col_rgb(38, 38, 42));
    // pipe
    trect(t, 0, 8, TS, 4, col_rgb(84, 88, 94));
    trect(t, 0, 8, TS, 1, col_rgb(120, 126, 134));
    tnoise(t, 5, 193);
}
static void mk_ceiling_light(void) {
    Tex *t = tnew(CE_LIGHT, TS, TS);
    tfill(t, col_rgb(64, 64, 68));
    for (int y = 0; y < TS; y++)
        for (int x = 0; x < TS; x++) {
            float n = vnoise(x * 0.1f, y * 0.1f, 201);
            tpset(t, x, y, col_shade(col_rgb(64, 64, 68), 0.85f + n * 0.2f));
        }
    for (int i = 0; i <= TS; i += 16)
        for (int k = 0; k < TS; k++) tpset(t, i % TS, k, col_rgb(40, 40, 44));
    // glowing panel
    for (int y = 20; y < 44; y++)
        for (int x = 12; x < 52; x++) {
            float dx = fabsf((float)x - 32) / 20.0f, dy = fabsf((float)y - 32) / 12.0f;
            float d = dx > dy ? dx : dy;
            uint32_t c = col_rgb(255, 250, 210);
            tpset(t, x, y, col_shade(c, 1.0f - d * 0.35f));
        }
}

// ------------------------------------------------------------- sprite (art)
typedef struct { char ch; uint32_t c; } ArtPal;

static void art(int id, int w, int h, const char **rows, const ArtPal *pal, int palN) {
    Tex *t = tnew(id, w, h);
    for (int y = 0; y < h; y++) {
        const char *row = rows[y];
        if (!row) continue;
        for (int x = 0; row[x]; x++) {
            char ch = row[x];
            if (ch == '.') continue;
            for (int k = 0; k < palN; k++)
                if (pal[k].ch == ch) { tpset(t, x, y, pal[k].c); break; }
        }
    }
}

// ---- trooper (14x20) frames: 0 stand 1 walkA 2 walkB 3 shoot 4 pain 5-7 die
static void trooper_body(const char *out[20], int legs, int arms, int lean) {
    // legs: 0 stand 1 A 2 B ; arms: 0 hold 1 shoot ; lean shifts head back
    static char buf[8][20][16];
    static int slot = 0;
    char (*b)[16] = buf[slot++ & 7];
    for (int i = 0; i < 20; i++) { memcpy(b[i], "..............", 15); out[i] = b[i]; }
    int hx = 2 - lean;
    // helmet + face (cols hx..hx+7)
    memcpy(b[1] + hx,  "...KKKKK.", 9);
    memcpy(b[2] + hx,  "..KhhhhhK", 9);
    memcpy(b[3] + hx,  "..KsssssK", 9);
    memcpy(b[4] + hx,  "..KsesesK", 9);
    memcpy(b[5] + hx,  "..KsSSSsK", 9);
    memcpy(b[6] + hx,  "...KsssK.", 9);
    // torso
    memcpy(b[7] + 1,  ".KKggggKK.", 10);
    memcpy(b[8],      "KgggGGgggK", 10);
    memcpy(b[9],      "KggGggGggK", 10);
    memcpy(b[10],     "KggGggGggK", 10);
    memcpy(b[11],     ".KggggggK.", 10);
    // arms + rifle
    if (arms == 0) {
        memcpy(b[9] + 9,  "mMM", 3);
        memcpy(b[10] + 3, "mmmmmmmmm", 9);
        memcpy(b[11] + 9, "mM.", 3);
    } else {
        memcpy(b[8] + 9,  "w", 1);       // muzzle flash
        memcpy(b[9] + 3,  "mmmmmmmm", 8);
        memcpy(b[9] + 9,  "MM", 2);
    }
    // legs
    if (legs == 0) {
        memcpy(b[12], "..KGggGK...", 11);
        memcpy(b[13], "..KG..GK...", 11);
        memcpy(b[14], "..Kk..kK...", 11);
        memcpy(b[15], "..Kk..kK...", 11);
        memcpy(b[16], "..Kk..kK...", 11);
        memcpy(b[17], "..Kbk.kbK..", 12);
        memcpy(b[18], ".Kbbk.KbbK.", 12);
    } else if (legs == 1) {
        memcpy(b[12], "..KGggGK...", 11);
        memcpy(b[13], "..KG..GK...", 11);
        memcpy(b[14], ".Kk...kK...", 12);
        memcpy(b[15], ".Kk...kK...", 12);
        memcpy(b[16], "Kk......kK.", 12);
        memcpy(b[17], "Kbk.....kK.", 12);
        memcpy(b[18], "Kbbk...KbbK", 12);
    } else {
        memcpy(b[12], "..KGggGK...", 11);
        memcpy(b[13], "..KG..GK...", 11);
        memcpy(b[14], "..Kk...kK..", 12);
        memcpy(b[15], "..Kk...kK..", 12);
        memcpy(b[16], "..Kk....kK.", 12);
        memcpy(b[17], "..Kbk...kbK", 12);
        memcpy(b[18], "..KbbK.KbbK", 12);
    }
}
static const char *trooper_die1[20], *trooper_die2[20], *trooper_die3[20];
static char diebuf1[20][16], diebuf2[20][16], diebuf3[20][16];

static void build_trooper(void) {
    ArtPal pal[] = {
        {'K', col_rgb(24, 24, 24)}, {'H', col_rgb(58, 74, 53)}, {'h', col_rgb(85, 104, 74)},
        {'s', col_rgb(216, 163, 122)}, {'S', col_rgb(169, 123, 88)}, {'e', col_rgb(32, 16, 16)},
        {'G', col_rgb(51, 63, 40)}, {'g', col_rgb(76, 92, 58)}, {'m', col_rgb(90, 95, 102)},
        {'M', col_rgb(138, 144, 152)}, {'b', col_rgb(74, 56, 38)}, {'r', col_rgb(142, 22, 22)},
        {'w', col_rgb(255, 240, 160)},
    };
    const char *fr[20];
    for (int f = 0; f < 8; f++) {
        if (f == 3) trooper_body(fr, 0, 1, 0);
        else if (f == 1) trooper_body(fr, 1, 0, 0);
        else if (f == 2) trooper_body(fr, 2, 0, 0);
        else if (f == 4) trooper_body(fr, 0, 0, 1);
        else trooper_body(fr, 0, 0, 0);
        art(SPR_TROOPER + f, 14, 20, fr, pal, 13);
    }
    // death frames
    for (int i = 0; i < 20; i++) { memcpy(diebuf1[i], "..............", 15); trooper_die1[i] = diebuf1[i];
                                  memcpy(diebuf2[i], "..............", 15); trooper_die2[i] = diebuf2[i];
                                  memcpy(diebuf3[i], "..............", 15); trooper_die3[i] = diebuf3[i]; }
    memcpy(diebuf1[4],  "...KKKKK.r", 10);
    memcpy(diebuf1[5],  "..KhhhhhKr", 10);
    memcpy(diebuf1[6],  "..KsrsssKr", 10);
    memcpy(diebuf1[7],  "..KsSSSsK.", 10);
    memcpy(diebuf1[8],  ".KKgggrKK.", 10);
    memcpy(diebuf1[9],  "KgggGGgggK", 10);
    memcpy(diebuf1[10], "KggGggGggK", 10);
    memcpy(diebuf1[11], ".KggggggK.", 10);
    memcpy(diebuf1[12], "..KGgGGK..", 11);
    memcpy(diebuf1[13], ".KGk..kGK.", 11);
    memcpy(diebuf1[14], ".Kk....kK.", 11);
    memcpy(diebuf1[15], ".Kk....kK.", 11);
    memcpy(diebuf1[16], "KKbk..kbKK", 11);
    memcpy(diebuf2[9],  "...rrrrr..", 10);
    memcpy(diebuf2[10], ".KKKKKKKK.", 10);
    memcpy(diebuf2[11], "KhhKggKggK", 10);
    memcpy(diebuf2[12], "KsssGggggK", 10);
    memcpy(diebuf2[13], ".KKggGggK.", 11);
    memcpy(diebuf2[14], ".KGkkkkGK.", 11);
    memcpy(diebuf2[15], "KbbK..KbbK", 11);
    for (int x = 1; x < 13; x++) diebuf3[16][x] = 'r';
    memcpy(diebuf3[14], "...KKKK...", 10);
    memcpy(diebuf3[15], "..KghhgK..", 11);
    for (int x = 2; x < 12; x++) diebuf3[17][x] = 'r';
    for (int x = 3; x < 11; x++) diebuf3[18][x] = 'r';
    art(SPR_TROOPER + 5, 14, 20, trooper_die1, pal, 13);
    art(SPR_TROOPER + 6, 14, 20, trooper_die2, pal, 13);
    art(SPR_TROOPER + 7, 14, 20, trooper_die3, pal, 13);
}

// ---- imp (16x18): 0,1 idle 2 throw 3 pain 4-6 die
static const char *imp_rows_idle1[18] = {
    "................",
    "....K......K....",
    "...KDK....KDK...",
    "....KKKKKKK.....",
    "...KMrrrrrMK....",
    "...KryrrryrK....",
    "...KMrrrrrMK....",
    "....KMrrrMK.....",
    "..KKMMMMMMMKK...",
    ".KMDMMMMMMDMK...",
    "KMDKMMMMMMKDMK..",
    "KDk.KMMMMK.kDK..",
    "KK..KMMMMK..KK..",
    ".....KMMMK......",
    "....KMk.kMK.....",
    "....KDK.KDK.....",
    "...KKDK.KDKK....",
    "................",
};
static const char *imp_rows_idle2[18] = {
    "................",
    "................",
    "....K......K....",
    "...KDK....KDK...",
    "....KKKKKKK.....",
    "...KMrrrrrMK....",
    "...KryrrryrK....",
    "...KMrrrrrMK....",
    "..KKMMMMMMMKK...",
    ".KMDMMMMMMDMK...",
    "KMDK.MMMM..DMK..",
    "Kdk..KMMK..kdK..",
    "KK...KMMK...KK..",
    ".....KMMMK......",
    ".....KMkMK......",
    ".....KDKDK......",
    "....KKDKDKK.....",
    "................",
};
static const char *imp_rows_throw[18] = {
    "................",
    "....K......K....",
    "...KDK....KDK...",
    "....KKKKKKK.....",
    "...KMrrrrrMK....",
    "...KryrrryrK....",
    "...KMrrrrrMK....",
    "....KMrrrMK..o..",
    "..KKMMMMMMMooO..",
    ".KMDMMMMMMKoOO..",
    "KMDKMMMMMMK.O...",
    "KDk..KMMK.......",
    "KK...KMMK.......",
    ".....KMMMK......",
    "....KMk.kMK.....",
    "....KDK.KDK.....",
    "...KKDK.KDKK....",
    "................",
};
static const char *imp_rows_pain[18] = {
    "................",
    "................",
    "................",
    "....K......K....",
    "...KDKKKKKDK....",
    "...KrrrrrrrK....",
    "..KMrywyrwyrK...",
    "...KMrrrrrMK....",
    "..KKMMMMMMMKK...",
    ".KMDMMMMMMDMK...",
    "KMDKMMMMMMKDMK..",
    "KK..KMMMMK..KK..",
    ".....KMMMK......",
    "....KMMK.KM.....",
    "...KMK...KMK....",
    "..KDK....KDK....",
    ".KKDK....KDKK...",
    "................",
};
static const char *imp_rows_die1[18] = {
    "................",
    "................",
    "................",
    "................",
    "....K......K....",
    "...KDKKKKKDK....",
    "...KrrrrrrrK....",
    "..KMrrwyrwyK....",
    "..KKMMMMMMMK....",
    ".KMDDMMMMDDMK...",
    "KMDKMMMMMMKDMK..",
    "KK..KMMMMK..KK..",
    ".....KMMMK......",
    "....KMMMMK......",
    "...KMMKKMMMK....",
    "..KMMK..KMMK....",
    ".KKKK....KKKK...",
    "................",
};
static const char *imp_rows_die2[18] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    ".....KKKKKK.....",
    "....KrrrrrrK....",
    "...KMrywyrwK....",
    "..KKMMMMMMMKK...",
    ".KMDMMMMMMMDMK..",
    "KMMKMMMMMMMKMMK.",
    "KKK.KMMMMK..KKK.",
    "....KMKKMK......",
    "...KMK..KMK.....",
    "..KKK....KKK....",
    "................",
    "................",
};
static const char *imp_rows_die3[18] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "....KKKKKKKK....",
    "..KKrrrrrrrrKK..",
    ".KMrwwrrrrwwrMK.",
    "KMMMMMMMMMMMMMMK",
    "KKKKKKKKKKKKKKKK",
    "................",
    "................",
    "................",
    "................",
};

static void build_imp(void) {
    ArtPal pal[] = {
        {'K', col_rgb(30, 18, 12)}, {'D', col_rgb(74, 42, 24)}, {'M', col_rgb(122, 70, 38)},
        {'r', col_rgb(176, 96, 48)}, {'y', col_rgb(255, 214, 64)}, {'w', col_rgb(255, 255, 255)},
        {'k', col_rgb(90, 50, 28)},
        {'o', col_rgb(255, 150, 40)}, {'O', col_rgb(255, 230, 120)},
    };
    art(SPR_IMP + 0, 16, 18, imp_rows_idle1, pal, 9);
    art(SPR_IMP + 1, 16, 18, imp_rows_idle2, pal, 9);
    art(SPR_IMP + 2, 16, 18, imp_rows_throw, pal, 9);
    art(SPR_IMP + 3, 16, 18, imp_rows_pain, pal, 9);
    art(SPR_IMP + 4, 16, 18, imp_rows_die1, pal, 9);
    art(SPR_IMP + 5, 16, 18, imp_rows_die2, pal, 9);
    art(SPR_IMP + 6, 16, 18, imp_rows_die3, pal, 9);
}

// ---- pinky (18x14): 0 walkA 1 walkB 2 bite 3 pain 4-6 die
static const char *pinky_rows_walkA[14] = {
    "..................",
    "..KKKK......KKKK..",
    ".KppppKKKKKKppppK.",
    "KpPpppppppppppPpK.",
    "KpppppppppppppppK.",
    "KpwKpppppppppwKpK.",
    "KPKKKKKKKKKKKKKPK.",
    "KpKwKwKwKwKwKwKpK.",
    ".KKpKKKKKKKKKpKK..",
    "..KpppppppppppK...",
    "..KPpppppppPpK....",
    "...KPK....KPK.....",
    "..KKPK....KPKK....",
    "..................",
};
static const char *pinky_rows_walkB[14] = {
    "..................",
    "..KKKK......KKKK..",
    ".KppppKKKKKKppppK.",
    "KpPppppppppppPppK.",
    "KpppppppppppppppK.",
    "KpwKppppppppwpKpK.",
    "KPKKKKKKKKKKKKKPK.",
    "KpKwKwKwKwKwKwKpK.",
    ".KKpKKKKKKKKKpKK..",
    "..KppppppppppK....",
    "...KPppppppPK.....",
    "....KPK..KPK......",
    "...KKPK..KPKK.....",
    "..................",
};
static const char *pinky_rows_bite[14] = {
    "..................",
    "..KKKK......KKKK..",
    ".KppppKKKKKKppppK.",
    "KpPppppppppppPppK.",
    "KpppppppppppppppK.",
    "KpwKppppppppwpKpK.",
    "KPKKKKKKKKKKKKKPK.",
    "KpKKwKwKwKwKwKKpK.",
    "KpK..........KpK..",
    "KpKK.KK..KK.KKpK..",
    ".KppKKKKKKKKppK...",
    "..KPK......KPK....",
    ".KKPK......KPKK...",
    "..................",
};
static const char *pinky_rows_pain[14] = {
    "..................",
    "..KKKK......KKKK..",
    ".KppppKKKKKKppppK.",
    "KprppppppppprpppK.",
    "KpppppppppppppPrK.",
    "KrwKppppppppwrKpK.",
    "KPKKKKKKKKKKKKKPK.",
    "KpKwKwKwKwKwKwKpK.",
    ".KKpKKKKKKKKKpKK..",
    "..KppppppppppK....",
    "..KPppppppppPK....",
    "...KPK.....KPK....",
    "..KKPK.....KPKK...",
    "..................",
};
static const char *pinky_rows_die1[14] = {
    "..................",
    "..................",
    "..KKKK......KKKK..",
    ".KprppKKKKKpprpK..",
    "KpppppppppppppppK.",
    "KrwKppppppppwrKpK.",
    "KPKKKKKKKKKKKKKPK.",
    "KpKwKwKwKwKwKwKpK.",
    ".KKpKKKKKKKKKpKK..",
    "..KpppppppppppK...",
    "..KPpppppppPPK....",
    "..KPPK.....KPPK...",
    ".KKPPK.....KPPKK..",
    "..................",
};
static const char *pinky_rows_die2[14] = {
    "..................",
    "..................",
    "..................",
    "..................",
    "...KKKKKKKKKKKK...",
    "..KpppppppppppPK..",
    ".KppwppwppwpplppK.",
    "KPKKKKKKKKKKKKKPK.",
    "KpppppppppppppppK.",
    ".KpPPpppppppPPpK..",
    "..KPPKPPPPPKPPK...",
    ".KKPKK.....KKPKK..",
    "..................",
    "..................",
};
static const char *pinky_rows_die3[14] = {
    "..................",
    "..................",
    "..................",
    "..................",
    "..................",
    "..................",
    "....KKKKKKKKK.....",
    "..KKppppppppKK....",
    ".KppwppwppwlpppK..",
    "KPPPKKKKKKKKPPPPK.",
    "KKKKK.......KKKKK.",
    "..................",
    "..................",
    "..................",
};

static void build_pinky(void) {
    ArtPal pal[] = {
        {'K', col_rgb(52, 16, 20)}, {'p', col_rgb(222, 108, 120)}, {'P', col_rgb(168, 66, 84)},
        {'w', col_rgb(245, 235, 220)}, {'r', col_rgb(200, 40, 40)}, {'l', col_rgb(120, 40, 48)},
    };
    art(SPR_PINKY + 0, 18, 14, pinky_rows_walkA, pal, 6);
    art(SPR_PINKY + 1, 18, 14, pinky_rows_walkB, pal, 6);
    art(SPR_PINKY + 2, 18, 14, pinky_rows_bite, pal, 6);
    art(SPR_PINKY + 3, 18, 14, pinky_rows_pain, pal, 6);
    art(SPR_PINKY + 4, 18, 14, pinky_rows_die1, pal, 6);
    art(SPR_PINKY + 5, 18, 14, pinky_rows_die2, pal, 6);
    art(SPR_PINKY + 6, 18, 14, pinky_rows_die3, pal, 6);
}

// ---- baron (22x26): 0 walkA 1 walkB 2 cast 3 pain 4-7 die
static void build_baron(void) {
    static const char *wa[26] = {
        "..K..............K....",
        "..KK....KKKK....KK....",
        "..KDK..KppppK..KDK....",
        "...KK..KphhpK..KK.....",
        "....KKKpppppKKK.......",
        ".....KpppppppK........",
        ".....KpeepeepK........",
        ".....KpppppppK........",
        "......KpppppK.........",
        "...KKKgggggggKKK......",
        "..KgggGGgggGGgggK.....",
        ".KggKGgggggggGkggK....",
        "KggK.KGgggggggK.KggK..",
        "KgK..KGgGGGGgGK..KgK..",
        "KgK..KGgggggggK..KgK..",
        ".K...KGggggggggK...K..",
        ".....KGggggggggK......",
        ".....KGggGGgggGK......",
        ".....KGgG..GgGgK......",
        ".....KGgK..KgGgK......",
        ".....KGgK..KgGgK......",
        "....KKGgK..KgGgKK.....",
        "....KGgK....KgGgK.....",
        "...KKggK....KgggKK....",
        "...KgggK....KgggK.....",
        "......................",
    };
    static const char *wb[26] = {
        "..K..............K....",
        "..KK....KKKK....KK....",
        "..KDK..KppppK..KDK....",
        "...KK..KphhpK..KK.....",
        "....KKKpppppKKK.......",
        ".....KpppppppK........",
        ".....KpeepeepK........",
        ".....KpppppppK........",
        "......KpppppK.........",
        "...KKKgggggggKKK......",
        "..KgggGGgggGGgggK.....",
        ".KggKGgggggggGkggK....",
        "KggK.KGgggggggK.KggK..",
        "KgK..KGgGGGGgGK..KgK..",
        "KgK..KGgggggggK..KgK..",
        ".K...KGggggggggK...K..",
        ".....KGggggggggK......",
        ".....KGggggggggK......",
        "......KGgG.GgGgK......",
        ".....KGgK...KgGgK.....",
        "....KggK....KggGK.....",
        "....KggK....KggGK.....",
        "...KggK......KggK.....",
        "..KgggK......KgggK....",
        "..KgggK......KgggK....",
        "......................",
    };
    static const char *cs[26] = {
        "..K..............K....",
        "..KK....KKKK....KK....",
        "..KDK..KppppK..KDK....",
        "...KK..KphhpK..KK.....",
        "....KKKpppppKKK.......",
        ".....KpppppppK........",
        ".....KpeepeepK........",
        ".....KpppppppK........",
        "......KpppppK.........",
        "...KKKgggggggKKK......",
        "..KgggGGgggGGgggKO....",
        ".KggKGgggggggGkggKOO..",
        "KggK.KGgggggggK.KgGO..",
        "KgK..KGgGGGGgGK..KgO..",
        "KgK..KGgggggggK..Kg...",
        ".K...KGggggggggK......",
        ".....KGggggggggK......",
        ".....KGggGGgggGK......",
        ".....KGgG..GgGgK......",
        ".....KGgK..KgGgK......",
        ".....KGgK..KgGgK......",
        "....KKGgK..KgGgKK.....",
        "....KGgK....KgGgK.....",
        "...KKggK....KgggKK....",
        "...KgggK....KgggK.....",
        "......................",
    };
    static const char *pn[26] = {
        "..K..............K....",
        "..KK....KKKK....KK....",
        "..KDK..KppppK..KDK....",
        "...KK..KprhpK..KK.....",
        "....KKKpppppKKK.......",
        ".....KpppppppK........",
        ".....KpererepK........",
        ".....KpppppppK........",
        "......KpppppK.........",
        "...KKKgggggggKKK......",
        "..KgggGGgggGGgggK.....",
        ".KggKGggrrgggGkggK....",
        "KggK.KGgggggggK.KggK..",
        "KgK..KGgGGGGgGK..KgK..",
        "KgK..KGgggggggK..KgK..",
        ".K...KGggggggggK...K..",
        ".....KGggggggggK......",
        ".....KGggGGgggGK......",
        ".....KGgG..GgGgK......",
        ".....KGgK..KgGgK......",
        ".....KGgK..KgGgK......",
        "....KKGgK..KgGgKK.....",
        "....KGgK....KgGgK.....",
        "...KKggK....KgggKK....",
        "...KgggK....KgggK.....",
        "......................",
    };
    static const char *d1[26] = {
        "......................",
        "......................",
        "..K..............K....",
        "..KK....KKKK....KK....",
        "..KDK..KppppK..KDK....",
        "...KK..KprhpK..KK.....",
        "....KKKpppppKKK.......",
        ".....KppererepK.......",
        ".....KpppppppK........",
        "...KKKgggggggKKK......",
        "..KgggGGgggGGgggK.....",
        ".KggKGgggggggGkggK....",
        "KggK.KGgggggggK.KggK..",
        "KgK..KGgGGGGgGK..KgK..",
        "KgK..KGgggggggK..KgK..",
        ".K...KGggggggggK...K..",
        ".....KGggggggggK......",
        ".....KGggggggggK......",
        "....KKGggGGggGgKK.....",
        "....KGgG....GgGgK.....",
        "...KggGK....KGgggK....",
        "...KggK......KgggK....",
        "..KKggK......KggKK....",
        "..KgggK......KgggK....",
        "..KKKKK......KKKKK....",
        "......................",
    };
    static const char *d2[26] = {
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        ".......KKKKKK.........",
        "......KppererpK.......",
        ".....KppppppppK.......",
        "....KKggggggggKK......",
        "...KggGGggggGGggK.....",
        "..KggKggggggggKggK....",
        ".KggK.KGgggggK.KggK...",
        "KggK..KGgggggK..KggK..",
        "KgK...KGgggggK...KgK..",
        ".K...KGgggggggK....K..",
        ".....KGggggggggK......",
        ".....KGggggggggK......",
        "....KKGggggggggKK.....",
        "....KGggggggggggK.....",
        "...KgggGGggGGgggK.....",
        "...KggK......KggK.....",
        "..KgggK......KgggK....",
        "..KKKKK......KKKKK....",
        "......................",
        "......................",
        "......................",
    };
    static const char *d3[26] = {
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        ".......KKKKKK.........",
        "......KpppppppK.......",
        ".....KppererppK.......",
        "....KKggggggggKK......",
        "...KgggGGggGGgggK.....",
        "..KgggGggggggGgggK....",
        ".KgggKggggggggKgggK...",
        "KgggK.KGgggggK.KgggK..",
        "KggK..KGgggggGK..KggK.",
        ".K...KGgggggggGK...KK.",
        ".....KGggggggggGK.....",
        "....KKGgggggggggKK....",
        "....KGgggggggggggK....",
        "...KggggggggggggggK...",
        "...KKKKKKKKKKKKKKKK...",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
    };
    static const char *d4[26] = {
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        ".......KKKKKK.........",
        ".....KKppppppKK.......",
        "....KppperereppK......",
        "...KggggggggggggK.....",
        "..KgggGGggggGGgggK....",
        ".KgggggggggggggggGK...",
        "KgggggggggggggggggGK..",
        "KGGGGGGGGGGGGGGGGGGK..",
        "KKKKKKKKKKKKKKKKKKKK..",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
        "......................",
    };
    ArtPal pal[] = {
        {'K', col_rgb(26, 34, 24)}, {'D', col_rgb(60, 78, 52)}, {'p', col_rgb(198, 158, 132)},
        {'h', col_rgb(232, 196, 170)}, {'e', col_rgb(255, 60, 40)}, {'g', col_rgb(96, 128, 84)},
        {'G', col_rgb(64, 88, 58)}, {'r', col_rgb(210, 50, 40)}, {'O', col_rgb(120, 255, 90)},
    };
    art(SPR_BARON + 0, 22, 26, wa, pal, 9);
    art(SPR_BARON + 1, 22, 26, wb, pal, 9);
    art(SPR_BARON + 2, 22, 26, cs, pal, 9);
    art(SPR_BARON + 3, 22, 26, pn, pal, 9);
    art(SPR_BARON + 4, 22, 26, d1, pal, 9);
    art(SPR_BARON + 5, 22, 26, d2, pal, 9);
    art(SPR_BARON + 6, 22, 26, d3, pal, 9);
    art(SPR_BARON + 7, 22, 26, d4, pal, 9);
}

// ---- props & fx
static void build_props(void) {
    static const char *barrel[14] = {
    "..KKKKKK..",
    ".KggggggK.",
    "KgGGggGGgK",
    "KgyyygyygK",
    "KgGGggGGgK",
    "KmmmmmmmmK",
    "KmMMMMMMmK",
    "KmmmmmmmmK",
    "KgGGggGGgK",
    "KgyyygyygK",
    "KgGGggGGgK",
    ".KmmmmmmK.",
    "..KKKKKK..",
    "..........",
    };
    static const char *lamp[18] = {
    "...KK...",
    "..KooK..",
    ".KoOOoK.",
    ".KoOOoK.",
    "..KooK..",
    "...KK...",
    "...KK...",
    "..KmmK..",
    "..KmMK..",
    "...KK...",
    "...KK...",
    "..KmMK..",
    "..KmmK..",
    "...KK...",
    "..KmmmK.",
    ".KmmmmK.",
    ".KKKKKK.",
    "........",
    };
    ArtPal pb[] = {
        {'K', col_rgb(20, 22, 20)}, {'g', col_rgb(88, 92, 88)}, {'G', col_rgb(60, 64, 60)},
        {'y', col_rgb(90, 200, 60)}, {'m', col_rgb(108, 112, 106)}, {'M', col_rgb(140, 145, 138)},
    };
    art(SPR_BARREL, 10, 14, barrel, pb, 6);
    ArtPal pl[] = {
        {'K', col_rgb(24, 24, 28)}, {'o', col_rgb(255, 160, 40)}, {'O', col_rgb(255, 230, 140)},
        {'m', col_rgb(96, 100, 106)}, {'M', col_rgb(130, 135, 142)},
    };
    art(SPR_LAMP, 8, 18, lamp, pl, 5);

    // explosion frames 24x24
    for (int f = 0; f < 3; f++) {
        Tex *t = tnew(SPR_EXPLO0 + f, 24, 24);
        int r = 6 + f * 5;
        for (int y = 0; y < 24; y++)
            for (int x = 0; x < 24; x++) {
                float dx = x - 12.0f, dy = y - 12.0f;
                float d = sqrtf(dx * dx + dy * dy);
                float n = vnoise(x * 0.4f + f * 9, y * 0.4f, 300 + f) * 4.0f;
                if (d < r + n) {
                    float inner = 1.0f - d / (r + n);
                    if (inner > 0.55f) tpset(t, x, y, col_rgb(255, 250 - f * 60, 140));
                    else if (inner > 0.25f) tpset(t, x, y, col_rgb(255, 160 - f * 50, 30));
                    else tpset(t, x, y, col_rgb(150 - f * 30, 60 - f * 15, 20));
                }
            }
    }
    // puff frames 8x8
    for (int f = 0; f < 3; f++) {
        Tex *t = tnew(SPR_PUFF0 + f, 8, 8);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                float dx = x - 3.5f, dy = y - 3.5f;
                if (dx * dx + dy * dy < 9 + f * 5)
                    tpset(t, x, y, col_rgb(150 - f * 30, 150 - f * 30, 150 - f * 30));
            }
    }
    // blood 6x6
    for (int f = 0; f < 2; f++) {
        Tex *t = tnew(SPR_BLOOD0 + f, 6, 6);
        rng_seed(400 + f);
        for (int k = 0; k < 8 + f * 4; k++) {
            int x = rng_irange(0, 5), y = rng_irange(0, 5);
            tpset(t, x, y, col_rgb(rng_irange(120, 200), 10, 10));
        }
    }
    // projectiles 8x8
    static const char *fb1[8] = {
    "..KKKK..",
    ".KooooK.",
    "KoOYYoK.",
    "KoYYYYK.",
    "KoOYYoK.",
    ".KoooK..",
    "..KKK...",
    "........",
    };
    ArtPal pf[] = {
        {'K', col_rgb(120, 40, 10)}, {'o', col_rgb(255, 120, 20)}, {'O', col_rgb(255, 190, 60)},
        {'Y', col_rgb(255, 250, 180)},
    };
    art(SPR_FIREBALL, 8, 8, fb1, pf, 4);
    static const char *fb2[8] = {
    "..KKK...",
    ".KoooK..",
    "KoOYYoK.",
    "KoYYYYK.",
    ".KoOYoK.",
    ".KooK...",
    "..KK....",
    "........",
    };
    art(SPR_FIREBALL2, 8, 8, fb2, pf, 4);
    ArtPal pg[] = {
        {'K', col_rgb(20, 90, 30)}, {'o', col_rgb(60, 220, 70)}, {'O', col_rgb(150, 255, 120)},
        {'Y', col_rgb(230, 255, 220)},
    };
    art(SPR_BARONBALL, 8, 8, fb1, pg, 4);
    ArtPal pc[] = {
        {'K', col_rgb(10, 60, 90)}, {'o', col_rgb(40, 180, 255)}, {'O', col_rgb(150, 230, 255)},
        {'Y', col_rgb(240, 252, 255)},
    };
    art(SPR_PLASMABALL, 8, 8, fb1, pc, 4);
    art(SPR_PLASMA2, 8, 8, fb1, pc, 4);
    // rocket 12x6
    static const char *rk[6] = {
    "............",
    "..Kmmmmo....",
    ".KmMMMoook..",
    "KmMMMMmoook.",
    "..Kmmmmo....",
    "............",
    };
    ArtPal pr[] = {
        {'K', col_rgb(30, 32, 36)}, {'m', col_rgb(130, 134, 140)}, {'M', col_rgb(180, 185, 192)},
        {'o', col_rgb(255, 150, 40)}, {'k', col_rgb(255, 230, 120)},
    };
    art(SPR_ROCKETPROJ, 12, 6, rk, pr, 5);
}

// ---- items
static void build_items(void) {
    ArtPal P[] = {
        {'K', col_rgb(20, 20, 24)}, {'W', col_rgb(235, 235, 235)}, {'w', col_rgb(180, 180, 185)},
        {'R', col_rgb(215, 40, 40)}, {'r', col_rgb(140, 20, 20)},
        {'G', col_rgb(60, 190, 70)}, {'g', col_rgb(30, 120, 40)},
        {'B', col_rgb(60, 110, 230)}, {'b', col_rgb(30, 60, 140)},
        {'Y', col_rgb(230, 200, 60)}, {'y', col_rgb(160, 130, 30)},
        {'C', col_rgb(80, 200, 255)}, {'c', col_rgb(30, 110, 180)},
        {'M', col_rgb(130, 136, 144)}, {'m', col_rgb(80, 84, 92)},
        {'N', col_rgb(150, 105, 60)}, {'n', col_rgb(100, 68, 38)},
        {'O', col_rgb(255, 170, 50)}, {'o', col_rgb(200, 110, 30)},
    };

    // stimpack 10x8
    static const char *stim[8] = {
    "..KKKKKK..",
    ".KWWWWWWK.",
    "KWWWRRWWWK",
    "KWWRWWRWK.",
    "KWWWRRWWWK",
    "KWWWWwwwWK",
    ".KKKKKKKK.",
    "..........",
    };
    // medkit 12x10
    static const char *med[10] = {
    "..KKKKKKKK..",
    ".KWWWWWWWWK.",
    "KWWWWWwWWWWK",
    "KWWWWRRWWWWK",
    "KWWWRRRRWWWK",
    "KWWWRRRRWWWK",
    "KWWWWRRWWWWK",
    "KWWWWWWWWWWK",
    ".KKKKKKKKKK.",
    "............",
    };
    // armor 12x10
    static const char *arm[10] = {
    ".KKK....KKK.",
    "KGGGK..KGGGK",
    "KGgGGKKGGgGK",
    "KGgggGGgggGK",
    "KGggggggggGK",
    ".KGgggggggK.",
    ".KGgggggGK..",
    "..KGggggK...",
    "...KKKKK....",
    "............",
    };
    // megaarmor (blue version)
    static const char *mega[10] = {
    ".KKK....KKK.",
    "KBBBK..KBBBK",
    "KBbBBKKBBbBK",
    "KBbbbBBbbbBK",
    "KBbbbbbbbbBK",
    ".KBbbbbbbbK.",
    ".KBbbbbbBK..",
    "..KBbbbbK...",
    "...KKKKK....",
    "............",
    };
    // potion 8x10
    static const char *pot[10] = {
    "...KK...",
    "...KK...",
    "..KCCK..",
    ".KCCCCK.",
    "KCCwcCCK",
    "KCwccCCK",
    "KCCCCCCK",
    "KcCCCCcK",
    ".KKKKKK.",
    "........",
    };
    // backpack 12x10
    static const char *bp[10] = {
    "..KKKKKK...",
    ".KNNNNNNK..",
    "KNnnnnnnNK.",
    "KNNKKKKNNK.",
    "KNnKYYKnnK.",
    "KNnKYYKnnK.",
    "KNNKKKKNNK.",
    "KNnnnnnnNK.",
    ".KKKKKKKK..",
    "...........",
    };
    // clip 8x6
    static const char *clip[6] = {
    "..KmKmK.",
    ".KMYMYMK",
    ".KMYMYMK",
    ".KmmmmK.",
    "..KKKK..",
    "........",
    };
    // ammobox 12x8
    static const char *abox[8] = {
    ".KKKKKKKKK..",
    "KMMMMMMMMMK.",
    "KMmKmKmKmMK.",
    "KMYMYMYMYMK.",
    "KMMMMMMMMMK.",
    "KmKKKKKKKmK.",
    ".KKKKKKKKK..",
    "............",
    };
    // shells 8x6
    static const char *shl[6] = {
    ".KmKmKm.",
    ".KRrRrRK",
    ".KRrRrRK",
    ".KYyYyYK",
    "..KKKK..",
    "........",
    };
    // shellbox 12x8
    static const char *sbox[8] = {
    ".KKKKKKKKK..",
    "KRRRRRRRRRK.",
    "KRmKmKmKmRK.",
    "KRrRrRrRrRK.",
    "KRYyYyYyYRK.",
    "KRRRRRRRRRK.",
    ".KKKKKKKKK..",
    "............",
    };
    // rockets 12x8
    static const char *rok[8] = {
    "..KmK..KmK..",
    "..KMK..KMK..",
    ".KmMK..KmMK.",
    "KmMMKKKKMMmK",
    "KMoOooOOoMmK",
    "KmMMKKKKMMmK",
    ".KKKK..KKKK.",
    "............",
    };
    // cell 10x10
    static const char *cel[10] = {
    "...KKKK...",
    "..KccccK..",
    ".KCCCCCCCK",
    "KCcCCCCCcK",
    "KCCwwwwCCK",
    "KCwCwwCwCK",
    "KCCwwwwCCK",
    "KCcCCCCCcK",
    ".KCCCCCCK.",
    "..KKKKKK..",
    };
    // weapons pickups 14x8 side view
    static const char *sgun[8] = {
    "..............",
    "KmmmmmmmmmmK..",
    "KmMMMMMMMMmKK.",
    ".KKnnnnnnKKmK.",
    "...KnKKnKKKK..",
    "....KnKKnK....",
    ".....KKKK.....",
    "..............",
    };
    static const char *cgun[8] = {
    "..............",
    "KmKmKmKmKmKK..",
    "KmKmKmKmKmMK..",
    "KMMMMMMMMMMK..",
    ".KnnKKKKnnK...",
    "..KnK..KnK....",
    "...KK..KK.....",
    "..............",
    };
    static const char *rlau[8] = {
    "..............",
    ".KmmmmmmmmK...",
    "KmMMMMMMMMmK..",
    "KmMoooommmmKK.",
    "KmMMMMMMMMmK..",
    ".KnnKKKKnnK...",
    "..KnK..KnK....",
    "..............",
    };
    static const char *plas[8] = {
    "..............",
    "KmmKmKmKmKK...",
    "KMCOCOCOCmMK..",
    "KmMmmmmmmmKK..",
    "KmMMKKKKMMK...",
    ".KnK....KnK...",
    "..KK....KK....",
    "..............",
    };
    // keys 7x10
    static const char *key[10] = {
    ".KKKKK.",
    "KXXXXXK",
    "KXwwwXK",
    "KXwKwXK",
    "KXXXKXK",
    ".KXXKXK",
    ".KXXKXK",
    ".KXXKXK",
    ".KXXXXK",
    ".KKKKK.",
    };
    ArtPal kr[] = {{'K', col_rgb(40, 10, 10)}, {'X', col_rgb(230, 50, 40)}, {'w', col_rgb(255, 220, 220)}};
    ArtPal kb[] = {{'K', col_rgb(10, 10, 60)}, {'X', col_rgb(70, 110, 250)}, {'w', col_rgb(210, 225, 255)}};
    ArtPal ky[] = {{'K', col_rgb(60, 50, 8)}, {'X', col_rgb(240, 210, 50)}, {'w', col_rgb(255, 245, 200)}};

    art(SPR_IT_STIM, 10, 8, stim, P, 19);
    art(SPR_IT_MEDKIT, 12, 10, med, P, 19);
    art(SPR_IT_ARMOR, 12, 10, arm, P, 19);
    art(SPR_IT_MEGAARMOR, 12, 10, mega, P, 19);
    art(SPR_IT_POTION, 8, 10, pot, P, 19);
    art(SPR_IT_BACKPACK, 12, 10, bp, P, 19);
    art(SPR_IT_CLIP, 8, 6, clip, P, 19);
    art(SPR_IT_AMMOBOX, 12, 8, abox, P, 19);
    art(SPR_IT_SHELLS, 8, 6, shl, P, 19);
    art(SPR_IT_SHELLBOX, 12, 8, sbox, P, 19);
    art(SPR_IT_ROCKETS, 12, 8, rok, P, 19);
    art(SPR_IT_CELL, 10, 10, cel, P, 19);
    art(SPR_IT_GUN_SHOTGUN, 14, 8, sgun, P, 19);
    art(SPR_IT_GUN_CHAINGUN, 14, 8, cgun, P, 19);
    art(SPR_IT_GUN_ROCKET, 14, 8, rlau, P, 19);
    art(SPR_IT_GUN_PLASMA, 14, 8, plas, P, 19);
    art(SPR_KEY_RED, 7, 10, key, kr, 3);
    art(SPR_KEY_BLUE, 7, 10, key, kb, 3);
    art(SPR_KEY_YELLOW, 7, 10, key, ky, 3);
}

// ------------------------------------------------------------------- font
// 5x7 glyphs. '.' empty, 'X' filled.
typedef struct { char ch; const char *rows[7]; } Glyph;

static const Glyph GLYPHS[] = {
{'A',{"XXXX.","X..XX","X..XX","XXXX.","X..XX","X..XX","X..XX"}},
{'B',{"XXXX.","X..XX","X..XX","XXXX.","X..XX","X..XX","XXXX."}},
{'C',{"XXXX.","X....","X....","X....","X....","X....","XXXX."}},
{'D',{"XXXX.","X..XX","X..XX","X..XX","X..XX","X..XX","XXXX."}},
{'E',{"XXXX.","X....","X....","XXXX.","X....","X....","XXXX."}},
{'F',{"XXXX.","X....","X....","XXXX.","X....","X....","X...."}},
{'G',{"XXXX.","X....","X....","X.XXX","X..XX","X..XX","XXXX."}},
{'H',{"X..XX","X..XX","X..XX","XXXX.","X..XX","X..XX","X..XX"}},
{'I',{"XXXX.","..X..","..X..","..X..","..X..","..X..","XXXX."}},
{'J',{"..XXX","...X.","...X.","...X.","...X.","X..X.",".XX.."}},
{'K',{"X..XX","X.XX.","XXX..","XX...","XXX..","X.XX.","X..XX"}},
{'L',{"X....","X....","X....","X....","X....","X....","XXXX."}},
{'M',{"X...X","XX.XX","X.X.X","X.X.X","X...X","X...X","X...X"}},
{'N',{"X..XX","X..XX","XX.XX","X.X.X","X.XX.","X..XX","X..XX"}},
{'O',{".XXX.","X...X","X...X","X...X","X...X","X...X",".XXX."}},
{'P',{"XXXX.","X..XX","X..XX","XXXX.","X....","X....","X...."}},
{'Q',{".XXX.","X...X","X...X","X...X","X.X.X","X..X.",".XX.X"}},
{'R',{"XXXX.","X..XX","X..XX","XXXX.","XXX..","X.XX.","X..XX"}},
{'S',{".XXXX","X....","X....",".XXX.","...XX","...XX","XXXX."}},
{'T',{"XXXX.","..X..","..X..","..X..","..X..","..X..","..X.."}},
{'U',{"X..XX","X..XX","X..XX","X..XX","X..XX","X..XX",".XXXX"}},
{'V',{"X...X","X...X","X...X","X...X",".X.X.",".X.X.","..X.."}},
{'W',{"X...X","X...X","X...X","X.X.X","X.X.X","XX.XX","X...X"}},
{'X',{"X...X","X...X",".X.X.","..X..",".X.X.","X...X","X...X"}},
{'Y',{"X...X","X...X",".X.X.","..X..","..X..","..X..","..X.."}},
{'Z',{"XXXX.","...XX","..XX.","..X..",".XX..","XX...","XXXX."}},
{'0',{".XXX.","X...X","X..XX","X.X.X","XX..X","X...X",".XXX."}},
{'1',{"..X..",".XX..","..X..","..X..","..X..","..X..",".XXX."}},
{'2',{".XXX.","X...X","....X","..XX.",".X...","X....","XXXXX"}},
{'3',{"XXXX.","....X","...X.","..XX.","....X","X...X",".XXX."}},
{'4',{"...XX","..XXX",".X.XX","X..XX","XXXXX","...XX","...XX"}},
{'5',{"XXXXX","X....","XXXX.","....X","....X","X...X",".XXX."}},
{'6',{".XXX.","X....","X....","XXXX.","X...X","X...X",".XXX."}},
{'7',{"XXXXX","....X","...X.","..X..",".X...",".X...",".X..."}},
{'8',{".XXX.","X...X","X...X",".XXX.","X...X","X...X",".XXX."}},
{'9',{".XXX.","X...X","X...X",".XXXX","....X","....X",".XXX."}},
{'.',{".....",".....",".....",".....",".....",".XX..",".XX.."}},
{',',{".....",".....",".....",".....",".XX..",".XX..","XX..."}},
{'!',{"..X..","..X..","..X..","..X..","..X..",".....","..X.."}},
{'?',{".XXX.","X...X","....X","..XX.","..X..",".....","..X.."}},
{':',{".XX..",".XX..",".....",".....",".....",".XX..",".XX.."}},
{';',{".XX..",".XX..",".....",".....",".XX..",".XX..","XX..."}},
{'-',{".....",".....",".....","XXXX.",".....",".....","....."}},
{'/',{"....X","...XX","...X.","..XX.",".XX..","XX...","X...."}},
{'\'',{".XX..",".XX..","..X..",".....",".....",".....","....."}},
{'(',{"...XX","..XX.",".XX..",".XX..",".XX..","..XX.","...XX"}},
{')',{"XX...",".XX..","..XX.","..XX.","..XX.",".XX..","XX..."}},
{'+',{".....","..X..","..X..","XXXX.","..X..","..X..","....."}},
{'%',{"XX..X","XX.X.","...X.","..X..",".X...","X.XX.","X..XX"}},
{'=',{".....",".....","XXXX.",".....","XXXX.",".....","....."}},
{'<',{"...XX","..XX.",".XX..","XX...",".XX..","..XX.","...XX"}},
{'>',{".XX..","..XX.","...XX","....X","...XX","..XX.",".XX.."}},
{'*',{".....","X.X.X",".XXX.","XXXXX",".XXX.","X.X.X","....."}},
{'[',{".XXX.",".XX..",".XX..",".XX..",".XX..",".XX..",".XXX."}},
{']',{".XXX.","..XX.","..XX.","..XX.","..XX.","..XX.",".XXX."}},
{'#',{".X.X.","XXXXX","XXXXX",".X.X.","XXXXX","XXXXX",".X.X."}},
};

static const Glyph *glyph_for(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    for (size_t i = 0; i < sizeof(GLYPHS) / sizeof(GLYPHS[0]); i++)
        if (GLYPHS[i].ch == c) return &GLYPHS[i];
    return NULL;
}
void font_draw_str(int x, int y, const char *s, uint32_t color, int scale) {
    int ox = x;
    for (; *s; s++) {
        if (*s == '\n') { x = ox; y += 8 * scale; continue; }
        if (*s == ' ') { x += 6 * scale; continue; }
        const Glyph *g = glyph_for(*s);
        if (!g) { x += 6 * scale; continue; }
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g->rows[r][c] == 'X')
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++) {
                            int px = x + c * scale + sx, py = y + r * scale + sy;
                            if (px >= 0 && py >= 0 && px < FB_W && py < FB_H)
                                FB[py * FB_W + px] = color;
                        }
        x += 6 * scale;
    }
}
int font_measure(const char *s, int scale) {
    return (int)strlen(s) * 6 * scale;
}

// ---------------------------------------------------------------- weapons
// painted straight into the framebuffer. frameState: 0 idle, 1 firing, 2 recover
#define WCANVAS_W 128
#define WCANVAS_H 96
static uint32_t wcanvas[WCANVAS_W * WCANVAS_H];
static void wrect(int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && j >= 0 && i < WCANVAS_W && j < WCANVAS_H)
                wcanvas[j * WCANVAS_W + i] = c;
}
static void wflash(int x, int y, int r) {
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            int d = abs(i) + abs(j);
            if (d <= r) {
                uint32_t c = d < r / 2 ? col_rgb(255, 255, 220) : col_rgb(255, 220, 80);
                wrect(x + i, y + j, 1, 1, c);
            }
        }
}
static void whands(int x, int y) {
    uint32_t skin = col_rgb(216, 163, 122), dk = col_rgb(150, 100, 70),
             out = col_rgb(30, 24, 20);
    wrect(x - 1, y - 1, 26, 20, out);
    wrect(x, y, 24, 18, skin);
    wrect(x, y + 12, 24, 6, dk);
    for (int i = 0; i < 4; i++) {
        wrect(x + 2 + i * 6, y, 4, 8, skin);
        wrect(x + 2 + i * 6, y + 7, 4, 1, dk);
    }
}
void paint_weapon(int weapon, int fs, float bobX, float bobY, float flash) {
    memset(wcanvas, 0, sizeof(wcanvas));
    int cx = WCANVAS_W / 2;
    int by = WCANVAS_H - 2;
    uint32_t met = col_rgb(70, 74, 82), metL = col_rgb(110, 116, 126),
             metD = col_rgb(40, 42, 48);
    uint32_t wood = col_rgb(110, 74, 40), woodD = col_rgb(74, 48, 26);
    uint32_t out = col_rgb(16, 16, 18);
    switch (weapon) {
    case WP_PISTOL: {
        int x = cx - 8;
        wrect(x, by - 26, 16, 10, out);
        wrect(x + 1, by - 25, 14, 6, fs ? metL : met);
        wrect(x + 2, by - 16, 12, 14, out);
        wrect(x + 3, by - 15, 10, 12, woodD);
        whands(cx - 34, by - 14);
        if (fs == 1) wflash(x + 8, by - 30, 7 + (int)(flash * 3));
        break;
    }
    case WP_SHOTGUN: {
        int x = cx - 12;
        wrect(x, by - 34, 24, 12, out);
        wrect(x + 1, by - 33, 22, 8, met);
        wrect(x + 4, by - 33, 3, 8, metL);
        wrect(x + 15, by - 33, 3, 8, metL);
        wrect(x - 2, by - 24, 28, 10, wood);
        wrect(x - 2, by - 16, 28, 3, woodD);
        wrect(x + 4, by - 14, 16, 14, out);
        wrect(x + 5, by - 13, 14, 12, wood);
        whands(cx - 44, by - 12);
        if (fs == 1) wflash(cx, by - 40, 10 + (int)(flash * 4));
        break;
    }
    case WP_CHAINGUN: {
        int x = cx - 20;
        wrect(x, by - 30, 40, 16, out);
        wrect(x + 1, by - 29, 38, 12, met);
        for (int i = 0; i < 4; i++) {
            int bx = x + 4 + i * 9;
            wrect(bx, by - 40, 6, 12, out);
            wrect(bx + 1, by - 39, 4, 10, i & 1 ? met : metL);
            wrect(bx + 1, by - 41, 4, 2, metD);
        }
        wrect(x + 6, by - 14, 28, 14, out);
        wrect(x + 7, by - 13, 26, 12, metD);
        whands(cx - 48, by - 12);
        if (fs == 1) { wflash(x + 7, by - 44, 7); wflash(x + 34, by - 44, 7); }
        break;
    }
    case WP_ROCKET: {
        int x = cx - 16;
        wrect(x, by - 38, 32, 20, out);
        wrect(x + 1, by - 37, 30, 16, met);
        wrect(x + 1, by - 37, 30, 4, metL);
        wrect(x + 10, by - 44, 12, 6, out);
        wrect(x + 12, by - 43, 3, 3, col_rgb(220, 60, 40));
        wrect(x + 4, by - 18, 24, 16, out);
        wrect(x + 5, by - 17, 22, 14, metD);
        whands(cx - 44, by - 12);
        if (fs == 1) wflash(cx, by - 44, 12 + (int)(flash * 5));
        break;
    }
    case WP_PLASMA: {
        int x = cx - 18;
        int pulse = (int)(flash * 60);
        wrect(x, by - 34, 36, 18, out);
        wrect(x + 1, by - 33, 34, 14, metD);
        wrect(x + 4, by - 42, 28, 10, out);
        wrect(x + 5, by - 41, 26, 8, met);
        for (int i = 0; i < 3; i++)
            wrect(x + 6 + i * 9, by - 40, 5, 6,
                  col_rgb(40, (uint8_t)(160 + pulse), (uint8_t)(220 + pulse / 2)));
        wrect(x + 8, by - 16, 20, 14, out);
        wrect(x + 9, by - 15, 18, 12, metD);
        whands(cx - 46, by - 12);
        if (fs == 1) wflash(cx, by - 46, 9 + (int)(flash * 3));
        break;
    }
    default: break;
    }
    // blit canvas at 2x, anchored to bottom-center of the view
    int dx = FB_W / 2 - WCANVAS_W + (int)bobX;
    int dy = VIEW_H - WCANVAS_H * 2 + 6 + (int)bobY;
    for (int y = 0; y < WCANVAS_H * 2; y++)
        for (int x = 0; x < WCANVAS_W * 2; x++) {
            uint32_t c = wcanvas[(y / 2) * WCANVAS_W + (x / 2)];
            if (!c) continue;
            int px = dx + x, py = dy + y;
            if (px < 0 || py < 0 || px >= FB_W || py >= VIEW_H) continue;
            FB[py * FB_W + px] = c;
        }
}

// ---------------------------------------------------------------- init
void tex_init(void) {
    mk_tech(); mk_brick(); mk_rock(); mk_metal(); mk_marble();
    mk_comp_frame(TX_WALL_COMP, 1000);
    mk_comp_frame(TX_WALL_COMP2, 2000);
    mk_door(TX_DOOR, col_rgb(200, 205, 215), col_rgb(60, 64, 72));
    mk_door(TX_DOOR_R, col_rgb(230, 60, 50), col_rgb(90, 20, 16));
    mk_door(TX_DOOR_B, col_rgb(70, 110, 250), col_rgb(20, 30, 90));
    mk_door(TX_DOOR_Y, col_rgb(240, 210, 50), col_rgb(90, 75, 15));
    mk_exitdoor();
    mk_switch(false); mk_switch(true);
    mk_sky();
    mk_flat_tech(); mk_flat_stone(); mk_flat_dirt(); mk_flat_slime();
    mk_pad(TX_FL_PAD, col_rgb(70, 220, 255));
    mk_pad(TX_FL_EXITPAD, col_rgb(80, 255, 120));
    mk_ceiling_conc(); mk_ceiling_light();
    build_trooper(); build_imp(); build_pinky(); build_baron();
    build_props(); build_items();
}

Tex *tex_get(int id) {
    if (!g_texMade[id]) fatal("tex %d not built", id);
    return &g_tex[id];
}
Tex *tex_get_anim(int id, float time) {
    switch (id) {
    case FT_SLIME: case FT_SLIME2:
        return &g_tex[(((int)(time * 2.0f)) & 1) ? TX_FL_SLIME2 : TX_FL_SLIME];
    default: {
        int t = id;
        if (t == CE_CONC) t = CE_CONC;
        return &g_tex[t];
    }
    }
}
