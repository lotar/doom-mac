// render.c — software raycaster, sprites, HUD, menus, BMP dump
#include "doom.h"

uint32_t FB[FB_W * FB_H];
static float g_zbuf[FB_W];

static inline void fb_px(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= FB_W || y >= FB_H) return;
    FB[y * FB_W + x] = c;
}
static inline uint32_t fb_get(int x, int y) { return FB[y * FB_W + x]; }
static void fb_rect(int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) fb_px(i, j, c);
}
static void fb_blend_rect(int x, int y, int w, int h, uint32_t c, float a) {
    uint32_t sr = (c >> 16) & 255, sg = (c >> 8) & 255, sb = c & 255;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) {
            if (i < 0 || j < 0 || i >= FB_W || j >= FB_H) continue;
            uint32_t d = fb_get(i, j);
            float dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
            uint32_t r = (uint32_t)(dr + (sr - dr) * a);
            uint32_t g = (uint32_t)(dg + (sg - dg) * a);
            uint32_t b = (uint32_t)(db + (sb - db) * a);
            fb_px(i, j, 0xFF000000u | (r << 16) | (g << 8) | b);
        }
}

void blit_sprite(Tex *t, int dx, int dy, int scale) {
    for (int y = 0; y < t->h; y++)
        for (int x = 0; x < t->w; x++) {
            uint32_t c = t->px[y * t->w + x];
            if (!(c & 0xFF000000u)) continue;
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    fb_px(dx + x * scale + sx, dy + y * scale + sy, c);
        }
}

// ------------------------------------------------------------------ 3D view
static Tex *wall_tex_for(Cell *c, Door *d) {
    switch (c->wall) {
    case WT_BRICK: return tex_get(TX_WALL_BRICK);
    case WT_ROCK: return tex_get(TX_WALL_ROCK);
    case WT_METAL: return tex_get(TX_WALL_METAL);
    case WT_MARBLE: return tex_get(TX_WALL_MARBLE);
    case WT_COMP: return (((int)(G.time * 1.6f)) & 1)
                        ? tex_get(TX_WALL_COMP2) : tex_get(TX_WALL_COMP);
    case WT_DOOR: return tex_get(TX_DOOR);
    case WT_DOOR_RED: return tex_get(TX_DOOR_R);
    case WT_DOOR_BLUE: return tex_get(TX_DOOR_B);
    case WT_DOOR_YELLOW: return tex_get(TX_DOOR_Y);
    case WT_EXITDOOR: return tex_get(TX_EXITDOOR);
    case WT_SWITCH: return tex_get(TX_SWITCH_OFF);
    case WT_SWITCH_ON: return tex_get(TX_SWITCH_ON);
    case WT_SECRET: return tex_get(d ? d->underlyingWall : TX_WALL_TECH);
    default: return tex_get(TX_WALL_TECH);
    }
}
static Tex *flat_tex(int id) { return tex_get_anim(id, G.time); }

static void render_view(void) {
    Player *pl = &G.pl;
    float posX = pl->x, posY = pl->y;
    float ang = pl->ang;
    float dirX = cosf(ang), dirY = sinf(ang);
    const float planeLen = 0.88f;
    float planeX = -dirY * planeLen, planeY = dirX * planeLen;
    int horizon = VIEW_H / 2;
    if (pl->shakeT > 0)
        horizon += (int)(sinf(G.time * 60.0f) * pl->shakeAmt * 12.0f);

    // ---- floor & ceiling casting (per row) ----
    for (int y = 0; y < VIEW_H; y++) {
        bool isFloor = y > horizon;
        int rowDistDen = isFloor ? (y - horizon) : (horizon - y);
        if (rowDistDen <= 0) {
            // horizon line: distant fog
            memset(&FB[y * FB_W], 0, FB_W * sizeof(uint32_t));
            continue;
        }
        float rowDist = (0.5f * VIEW_H) / rowDistDen;
        float fx = posX + rowDist * (dirX - planeX);
        float fy = posY + rowDist * (dirY - planeY);
        float stepX = rowDist * 2 * planeX / FB_W;
        float stepY = rowDist * 2 * planeY / FB_W;
        float shadeBase = 1.0f / (1.0f + rowDist * rowDist * 0.055f);
        uint32_t *row = &FB[y * FB_W];
        for (int x = 0; x < FB_W; x++) {
            int cx = (int)fx, cy = (int)fy;
            Cell *c = map_cell_at(cx, cy);
            uint32_t col = 0;
            if (c) {
                if (isFloor) {
                    Tex *t = flat_tex(c->floor == FT_SLIME ? FT_SLIME : c->floor);
                    int tx = (int)((fx - cx) * t->w) & (t->w - 1);
                    int ty = (int)((fy - cy) * t->h) & (t->h - 1);
                    col = t->px[ty * t->w + tx];
                    float li = (c->light / 255.0f) * shadeBase * 1.35f +
                               G.globalLightBoost * 0.4f;
                    col = col_shade(col, fminf(1.3f, li));
                } else {
                    if (c->ceil == TX_SKY) {
                        Tex *sky = tex_get(TX_SKY);
                        float colAng = ang + (2.0f * x / FB_W - 1.0f) * 0.72f;
                        int su = (int)((colAng / 6.28318f) * sky->w);
                        su = ((su % sky->w) + sky->w) % sky->w;
                        int sv = (y * sky->h * 2) / VIEW_H;
                        if (sv >= sky->h) sv = sky->h - 1;
                        col = sky->px[sv * sky->w + su];
                    } else {
                        Tex *t = flat_tex(c->ceil == CE_LIGHT ? CE_LIGHT : CE_CONC);
                        int tx = (int)((fx - cx) * t->w) & (t->w - 1);
                        int ty = (int)((fy - cy) * t->h) & (t->h - 1);
                        col = t->px[ty * t->w + tx];
                        float li = (c->light / 255.0f) * shadeBase * 1.1f +
                                   G.globalLightBoost * 0.3f;
                        col = col_shade(col, fminf(1.3f, li));
                    }
                }
            }
            row[x] = col;
            fx += stepX; fy += stepY;
        }
    }

    // ---- walls ----
    for (int x = 0; x < FB_W; x++) {
        float camX = 2.0f * x / FB_W - 1.0f;
        float rdx = dirX + planeX * camX;
        float rdy = dirY + planeY * camX;
        int mapX = (int)posX, mapY = (int)posY;
        float deltaX = fabsf(rdx) < 1e-6f ? 1e30f : fabsf(1.0f / rdx);
        float deltaY = fabsf(rdy) < 1e-6f ? 1e30f : fabsf(1.0f / rdy);
        float sideDistX, sideDistY;
        int stepX, stepY;
        if (rdx < 0) { stepX = -1; sideDistX = (posX - mapX) * deltaX; }
        else { stepX = 1; sideDistX = (mapX + 1 - posX) * deltaX; }
        if (rdy < 0) { stepY = -1; sideDistY = (posY - mapY) * deltaY; }
        else { stepY = 1; sideDistY = (mapY + 1 - posY) * deltaY; }

        float perpDist = 100.0f;
        float wallX = 0;
        Tex *tex = NULL;
        Cell *hitCell = NULL;
        int side = 0;
        float shade = 1;

        for (int iter = 0; iter < 96; iter++) {
            // advance
            if (sideDistX < sideDistY) {
                mapX += stepX; side = 0;
                sideDistX += deltaX;
            } else {
                mapY += stepY; side = 1;
                sideDistY += deltaY;
            }
            if (mapX < 0 || mapY < 0 || mapX >= MAP_W || mapY >= MAP_H) break;
            Cell *c = map_cell_at(mapX, mapY);
            G.map.seen[mapY * MAP_W + mapX] = true;
            hitCell = c;
            if (c->wall == 0) continue;

            Door *d = NULL;
            bool isDoorLike = false;
            if (c->wall == WT_DOOR || c->wall == WT_DOOR_RED || c->wall == WT_DOOR_BLUE ||
                c->wall == WT_DOOR_YELLOW || c->wall == WT_EXITDOOR || c->wall == WT_SECRET) {
                int di = G.map.doorIndex[mapY * MAP_W + mapX];
                if (di >= 0) { d = &G.map.doors[di]; isDoorLike = true; }
            }
            if (!isDoorLike) {
                // solid wall hit at the boundary we just crossed
                perpDist = side == 0
                    ? (sideDistX - deltaX)
                    : (sideDistY - deltaY);
                wallX = side == 0
                    ? posY + perpDist * rdy
                    : posX + perpDist * rdx;
                wallX -= floorf(wallX);
                shade = side == 1 ? 0.78f : 1.0f;
                tex = wall_tex_for(c, d);
                break;
            }
            // door: check crossing of its mid-plane
            float open = d->open;
            if (d->axis == 1) { // passage along x; plane x = mapX + 0.5
                if (fabsf(rdx) > 1e-6f) {
                    float t = (mapX + 0.5f - posX) / rdx;
                    float hy = posY + t * rdy;
                    if (hy >= mapY && hy < mapY + 1 && t > 0 &&
                        t < (sideDistY - deltaY)) {
                        float u = hy - mapY;
                        if (u >= open) { // leaf occupies [open,1]
                            perpDist = t;
                            wallX = u - open;
                            shade = 0.92f;
                            tex = wall_tex_for(c, d);
                            hitCell = c;
                            break;
                        }
                    }
                }
            } else { // passage along y; plane y = mapY + 0.5
                if (fabsf(rdy) > 1e-6f) {
                    float t = (mapY + 0.5f - posY) / rdy;
                    float hx = posX + t * rdx;
                    if (hx >= mapX && hx < mapX + 1 && t > 0 &&
                        t < (sideDistX - deltaX)) {
                        float u = hx - mapX;
                        if (u >= open) {
                            perpDist = t;
                            wallX = u - open;
                            shade = 0.92f;
                            tex = wall_tex_for(c, d);
                            hitCell = c;
                            break;
                        }
                    }
                }
            }
            // grazed past an open door: keep marching
        }

        g_zbuf[x] = perpDist;
        if (!tex || !hitCell || perpDist >= 99.0f) continue;

        int lineH = (int)(VIEW_H / perpDist);
        int y0 = horizon - lineH / 2;
        int ty0 = y0 < 0 ? -y0 : 0;
        int ty1 = lineH > VIEW_H ? VIEW_H - y0 : lineH;
        if (ty1 > lineH) ty1 = lineH;
        float light = (hitCell->light / 255.0f) *
                      (1.0f / (1.0f + perpDist * perpDist * 0.05f)) * 1.5f +
                      G.globalLightBoost * 0.5f;
        light *= shade;
        if (light > 1.25f) light = 1.25f;
        int tw = tex->w, th = tex->h;
        int texX = (int)(wallX * tw) & (tw - 1);
        uint32_t *tp = tex->px;
        for (int yy = ty0; yy < ty1; yy++) {
            int v = ((yy << 8) / lineH * th) >> 8;
            if (v < 0) v = 0;
            if (v >= th) v = th - 1;
            uint32_t col = tp[v * tw + texX];
            FB[(y0 + yy) * FB_W + x] = col_shade(col, light);
        }
    }

    // ---- sprites ----
    typedef struct { Tex *tex; float x, y, h, z0, bright; int kind; void *ref; } SprQ;
    static SprQ queue[512];
    int nq = 0;
    float invDet = 1.0f / (planeX * dirY - dirX * planeY);

#define PUSHQ(tx, ex, ey, hh, zz0, br)                                        \
    do {                                                                       \
        if (nq < 512) {                                                        \
            queue[nq].tex = (tx); queue[nq].x = (ex); queue[nq].y = (ey);      \
            queue[nq].h = (hh); queue[nq].z0 = (zz0); queue[nq].bright = (br); \
            nq++;                                                              \
        }                                                                      \
    } while (0)

    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active) continue;
        int spr = -1;
        float h = 0.5f, z0 = 0;
        bool bright = false;
        switch (e->type) {
        case ET_TROOPER: {
            spr = SPR_TROOPER;
            if (e->state == MS_DYING || e->state == MS_DEAD) spr += 5 + e->frame;
            else if (e->state == MS_PAIN) spr += 4;
            else if (e->state == MS_WINDUP) spr += 3;
            else if (e->state == MS_CHASE) spr += ((int)(e->animT * 4) & 1) ? 1 : 2;
            h = e->state >= MS_DYING ? 0.30f : 0.62f;
            break;
        }
        case ET_IMP: {
            spr = SPR_IMP;
            if (e->state == MS_DYING || e->state == MS_DEAD) spr += 4 + e->frame;
            else if (e->state == MS_PAIN) spr += 3;
            else if (e->state == MS_WINDUP) spr += 2;
            else spr += ((int)(G.time * 3 + e->x) & 1);
            h = e->state >= MS_DYING ? 0.22f : 0.58f;
            break;
        }
        case ET_PINKY: {
            spr = SPR_PINKY;
            if (e->state == MS_DYING || e->state == MS_DEAD) spr += 4 + e->frame;
            else if (e->state == MS_PAIN) spr += 3;
            else if (e->state == MS_WINDUP) spr += 2;
            else spr += ((int)(e->animT * 4) & 1);
            h = e->state >= MS_DYING ? 0.26f : 0.52f;
            break;
        }
        case ET_BARON: {
            spr = SPR_BARON;
            if (e->state == MS_DYING || e->state == MS_DEAD) spr += 4 + e->frame;
            else if (e->state == MS_PAIN) spr += 3;
            else if (e->state == MS_WINDUP) spr += 2;
            else spr += ((int)(e->animT * 4) & 1);
            h = e->state >= MS_DYING ? 0.4f : 0.86f;
            break;
        }
        case ET_BARREL: spr = SPR_BARREL; h = 0.5f; break;
        case ET_LAMP: spr = SPR_LAMP; h = 0.66f; bright = false; break;
        case ET_ITEM: {
            switch (e->itemKind) {
            case IT_STIM: spr = SPR_IT_STIM; break;
            case IT_MEDKIT: spr = SPR_IT_MEDKIT; break;
            case IT_ARMOR: spr = SPR_IT_ARMOR; break;
            case IT_MEGAARMOR: spr = SPR_IT_MEGAARMOR; break;
            case IT_POTION: spr = SPR_IT_POTION; break;
            case IT_BACKPACK: spr = SPR_IT_BACKPACK; break;
            case IT_CLIP: spr = SPR_IT_CLIP; break;
            case IT_AMMOBOX: spr = SPR_IT_AMMOBOX; break;
            case IT_SHELLS: spr = SPR_IT_SHELLS; break;
            case IT_SHELLBOX: spr = SPR_IT_SHELLBOX; break;
            case IT_ROCKETS: spr = SPR_IT_ROCKETS; break;
            case IT_CELL: spr = SPR_IT_CELL; break;
            case IT_GUN_SHOTGUN: spr = SPR_IT_GUN_SHOTGUN; break;
            case IT_GUN_CHAINGUN: spr = SPR_IT_GUN_CHAINGUN; break;
            case IT_GUN_ROCKET: spr = SPR_IT_GUN_ROCKET; break;
            case IT_GUN_PLASMA: spr = SPR_IT_GUN_PLASMA; break;
            case IT_KEY_RED: spr = SPR_KEY_RED; bright = true; break;
            case IT_KEY_BLUE: spr = SPR_KEY_BLUE; bright = true; break;
            case IT_KEY_YELLOW: spr = SPR_KEY_YELLOW; bright = true; break;
            default: spr = SPR_IT_STIM; break;
            }
            h = 0.30f;
            z0 = 0.02f + sinf(G.time * 2.2f + e->x) * 0.015f;
            break;
        }
        default: continue;
        }
        if (spr < 0) continue;
        if (getenv("DMG_SPR"))
            fprintf(stderr, "spr ent type=%d state=%d frame=%d id=%d\n",
                    e->type, e->state, e->frame, spr);
        PUSHQ(tex_get(spr), e->x, e->y, h, z0, bright ? 2.0f : 0.0f);
    }
    for (int i = 0; i < MAX_PROJ; i++) {
        Projectile *p = &G.projs[i];
        if (!p->active) continue;
        int spr = p->kind == PR_IMPBALL
                      ? (((int)(p->age * 10) & 1) ? SPR_FIREBALL2 : SPR_FIREBALL)
                  : p->kind == PR_BARONBALL ? SPR_BARONBALL
                  : p->kind == PR_PLASMA
                      ? (((int)(p->age * 12) & 1) ? SPR_PLASMA2 : SPR_PLASMABALL)
                      : SPR_ROCKETPROJ;
        PUSHQ(tex_get(spr), p->x, p->y,
              p->kind == PR_ROCKET ? 0.16f : 0.20f, 0.42f, 2.0f);
    }

    // sort far -> near
    for (int i = 1; i < nq; i++) {
        SprQ key = queue[i];
        float kd = (key.x - posX) * (key.x - posX) + (key.y - posY) * (key.y - posY);
        int j = i - 1;
        while (j >= 0) {
            float jd = (queue[j].x - posX) * (queue[j].x - posX) +
                       (queue[j].y - posY) * (queue[j].y - posY);
            if (jd >= kd) break;
            queue[j + 1] = queue[j];
            j--;
        }
        queue[i] = key; // moved below; fix by assignment
        queue[j + 1] = key;
    }

    for (int q = 0; q < nq; q++) {
        SprQ *s = &queue[q];
        float sx = s->x - posX, sy = s->y - posY;
        float tx = invDet * (dirY * sx - dirX * sy);
        float ty = invDet * (-planeY * sx + planeX * sy);
        if (ty < 0.12f) continue;
        Tex *t = s->tex;
        int screenX = (int)((FB_W / 2) * (1 + tx / ty));
        float yScale = VIEW_H / ty;
        int hPx = (int)(s->h * yScale);
        if (hPx <= 0) continue;
        int wPx = (int)(hPx * (float)t->w / t->h);
        if (wPx <= 0) continue;
        int yBottom = (int)(horizon + (0.5f - s->z0) * yScale);
        int yTop = yBottom - hPx;
        int x0 = screenX - wPx / 2;
        Cell *sc = map_cell_at((int)s->x, (int)s->y);
        float li = sc ? (sc->light / 255.0f) : 1.0f;
        li *= 1.0f / (1.0f + ty * ty * 0.03f) * 1.6f;
        if (s->bright > 0) li = 1.1f;
        li += G.globalLightBoost * 0.4f;
        if (li > 1.2f) li = 1.2f;
        for (int x = x0; x < x0 + wPx; x++) {
            if (x < 0 || x >= FB_W) continue;
            if (g_zbuf[x] <= ty) continue;
            int u = ((x - x0) * t->w) / wPx;
            if (u < 0) u = 0;
            if (u >= t->w) u = t->w - 1;
            int ya = yTop < 0 ? 0 : yTop;
            int yb = yBottom > VIEW_H ? VIEW_H : yBottom;
            for (int y = ya; y < yb; y++) {
                int v = ((y - yTop) * t->h) / hPx;
                if (v < 0) v = 0;
                if (v >= t->h) v = t->h - 1;
                uint32_t col = t->px[v * t->w + u];
                if (!(col & 0xFF000000u)) continue;
                FB[y * FB_W + x] = col_shade(col, li);
            }
        }
    }

    // particles as small billboards
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *pt = &G.parts[i];
        if (!pt->active) continue;
        float sx = pt->x - posX, sy = pt->y - posY;
        float tx = invDet * (dirY * sx - dirX * sy);
        float ty = invDet * (-planeY * sx + planeX * sy);
        if (ty < 0.15f || ty > 30) continue;
        int screenX = (int)((FB_W / 2) * (1 + tx / ty));
        float yScale = VIEW_H / ty;
        int py = (int)(horizon + (0.5f - pt->z) * yScale);
        int psz = (int)(pt->size * yScale * 2);
        if (psz < 1) psz = 1;
        float fade = pt->life / pt->maxLife;
        uint32_t col = col_shade(pt->color, 0.4f + fade * 0.6f);
        for (int dy = 0; dy < psz; dy++)
            for (int dx = 0; dx < psz; dx++) {
                int xx = screenX + dx, yy = py + dy;
                if (xx < 0 || yy < 0 || xx >= FB_W || yy >= VIEW_H) continue;
                if (g_zbuf[xx] <= ty) continue;
                FB[yy * FB_W + xx] = col;
            }
    }

    // ---- screen tints ----
    if (G.pl.damageFlash > 0)
        fb_blend_rect(0, 0, FB_W, VIEW_H, col_rgb(190, 10, 10),
                      fminf(0.55f, G.pl.damageFlash * 0.5f));
    if (G.pl.pickupFlash > 0)
        fb_blend_rect(0, 0, FB_W, VIEW_H, col_rgb(210, 190, 40),
                      fminf(0.3f, G.pl.pickupFlash * 0.3f));
}

// ------------------------------------------------------------------ HUD face
static uint32_t mix(uint32_t a, uint32_t b, float f) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    int r = ar + (int)((br - ar) * f), g = ag + (int)((bg - ag) * f),
        bl = ab + (int)((bb - ab) * f);
    return 0xFF000000u | ((r & 255) << 16) | ((g & 255) << 8) | (bl & 255);
}
void draw_face_hud(int ox, int oy) {
    Player *p = &G.pl;
    int hpB = p->dead ? 0 : (p->hp > 80 ? 5 : p->hp > 60 ? 4 : p->hp > 40 ? 3
                                                     : p->hp > 20 ? 2 : 1);
    uint32_t skin = mix(col_rgb(222, 170, 128), col_rgb(160, 120, 110),
                        (5 - hpB) * 0.13f);
    uint32_t skinD = mix(skin, col_rgb(60, 40, 30), 0.35f);
    uint32_t hair = col_rgb(74, 50, 30);
    uint32_t outl = col_rgb(20, 16, 14);
    // head 40x44 box
    fb_rect(ox, oy, 40, 44, outl);
    fb_rect(ox + 1, oy + 1, 38, 42, skin);
    fb_rect(ox + 1, oy + 1, 38, 8, hair);           // buzz cut
    fb_rect(ox + 1, oy + 9, 4, 34, skinD);          // side shading
    // eyes
    if (p->dead) {
        fb_rect(ox + 7, oy + 16, 8, 2, outl);
        fb_rect(ox + 25, oy + 16, 8, 2, outl);
    } else if (p->faceOuchT > 0) {
        fb_rect(ox + 7, oy + 17, 8, 3, skinD);
        fb_rect(ox + 25, oy + 17, 8, 3, skinD);
    } else {
        static float lookT = 0;
        static int look = 0;
        lookT -= 1.0f / 60;
        if (lookT <= 0) { look = rng_irange(-1, 1); lookT = 0.6f + rng_f(); }
        uint32_t ew = col_rgb(240, 240, 235);
        fb_rect(ox + 6, oy + 14, 10, 6, ew);
        fb_rect(ox + 24, oy + 14, 10, 6, ew);
        uint32_t pup = col_rgb(30, 40, 60);
        fb_rect(ox + 9 + look * 2, oy + 16, 4, 4, pup);
        fb_rect(ox + 27 + look * 2, oy + 16, 4, 4, pup);
        fb_rect(ox + 6, oy + 13, 10, 1, hair);      // brows
        fb_rect(ox + 24, oy + 13, 10, 1, hair);
    }
    // nose
    fb_rect(ox + 19, oy + 21, 3, 5, skinD);
    // mouth
    if (p->dead) {
        fb_rect(ox + 12, oy + 32, 16, 2, col_rgb(120, 30, 30));
    } else if (p->faceGrinT > 0) {
        fb_rect(ox + 11, oy + 31, 18, 2, outl);
        fb_rect(ox + 12, oy + 33, 16, 2, col_rgb(240, 240, 230));
    } else if (hpB <= 1 && !p->dead) {
        fb_rect(ox + 13, oy + 32, 14, 3, col_rgb(90, 30, 30));
    } else {
        fb_rect(ox + 12, oy + 33, 16, 2, mix(skin, col_rgb(80, 40, 40), 0.6f));
    }
    // battle scars by damage tier
    if (hpB <= 3) fb_rect(ox + 4, oy + 10, 3, 6, col_rgb(140, 20, 20));
    if (hpB <= 2) fb_rect(ox + 33, oy + 22, 3, 8, col_rgb(140, 20, 20));
    if (hpB <= 1) fb_rect(ox + 14, oy + 5, 8, 3, col_rgb(140, 20, 20));
    if (p->god) fb_blend_rect(ox, oy, 40, 44, col_rgb(255, 220, 90), 0.25f);
}

// ------------------------------------------------------------------ HUD bar
#define WDEF_AMMO(w) ((w) == WP_SHOTGUN ? AM_SHELL : (w) == WP_CHAINGUN ? AM_BULLET \
                       : (w) == WP_ROCKET ? AM_ROCKET : (w) == WP_PLASMA ? AM_CELL : AM_BULLET)
static void hud_number(int xRight, int y, int val, int scale) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    int w = font_measure(buf, scale);
    font_draw_str(xRight - w, y, buf, col_rgb(228, 60, 40), scale);
}
static void hud_label(int xc, int y, const char *s) {
    int w = font_measure(s, 1);
    font_draw_str(xc - w / 2, y, s, col_rgb(140, 140, 140), 1);
}
static void render_hud(void) {
    Player *p = &G.pl;
    int y0 = VIEW_H;
    // metal backdrop
    for (int y = 0; y < HUD_H; y++) {
        float f = y / (float)HUD_H;
        uint32_t c = mix(col_rgb(74, 74, 82), col_rgb(40, 40, 46), f);
        for (int x = 0; x < FB_W; x++) FB[(y0 + y) * FB_W + x] = c;
    }
    fb_rect(0, y0, FB_W, 2, col_rgb(16, 16, 18));
    // section dividers
    for (int i = 0; i < 6; i++) fb_rect(106 + i * 89, y0 + 4, 2, HUD_H - 8, col_rgb(28, 28, 32));

    // ammo (current weapon)
    const char *ammoNames[] = {"BULL", "SHELL", "RKT", "CELL"};
    int at = WDEF_AMMO(p->weapon);
    hud_number(98, y0 + 14, p->ammo[at], 3);
    hud_label(54, y0 + 48, "AMMO");
    font_draw_str(30, y0 + 62, ammoNames[at], col_rgb(110, 110, 118), 1);

    // health
    hud_number(196, y0 + 14, p->hp, 3);
    hud_label(152, y0 + 48, "HEALTH");

    // arms 2..6
    hud_label(241, y0 + 8, "ARMS");
    for (int k = 2; k <= 6; k++) {
        int ax = 214 + ((k - 2) % 3) * 26;
        int ay = y0 + 22 + ((k - 2) / 3) * 26;
        bool own = p->hasWeapon[k - 1];
        char num[4] = {(char)('0' + k), 0};
        font_draw_str(ax, ay, num, own ? col_rgb(228, 180, 60) : col_rgb(90, 90, 96), 2);
    }

    // face
    draw_face_hud(FB_W / 2 - 20, y0 + 18);

    // armor
    hud_number(470, y0 + 14, p->armor, 3);
    hud_label(420, y0 + 48, "ARMOR");

    // keys column
    int kx = 500;
    if (p->keys & KEY_RED) blit_sprite(tex_get(SPR_KEY_RED), kx, y0 + 8, 2);
    if (p->keys & KEY_BLUE) blit_sprite(tex_get(SPR_KEY_BLUE), kx, y0 + 30, 2);
    if (p->keys & KEY_YELLOW) blit_sprite(tex_get(SPR_KEY_YELLOW), kx, y0 + 52, 2);

    // weapon slots quick display
    for (int w = 0; w < WP_COUNT; w++) {
        if (!p->hasWeapon[w]) continue;
        char lbl[4] = {(char)('1' + w), 0};
        uint32_t c = (w == p->weapon) ? col_rgb(255, 220, 90) : col_rgb(120, 120, 126);
        font_draw_str(560 + (w % 3) * 24, y0 + 10 + (w / 3) * 16, lbl, c, 2);
    }

    // message line overlays bottom of view
    if (G.msgTimer > 0) {
        int w = font_measure(G.msg, 1);
        fb_blend_rect(4, VIEW_H - 16, w + 8, 14, col_rgb(0, 0, 0), 0.5f);
        font_draw_str(8, VIEW_H - 14, G.msg, col_rgb(235, 235, 235), 1);
    }
}
// ------------------------------------------------------------------ automap
static void render_automap(void) {
    fb_blend_rect(0, 0, FB_W, VIEW_H, col_rgb(6, 6, 10), 0.72f);
    const float sc = 9.0f;
    float ox = FB_W / 2 - G.pl.x * sc;
    float oy = VIEW_H / 2 - G.pl.y * sc;
    for (int cy = 0; cy < MAP_H; cy++)
        for (int cx = 0; cx < MAP_W; cx++) {
            Cell *c = map_cell_at(cx, cy);
            if (!c->wall) continue;
            // only draw walls adjacent to seen floor
            bool near = false;
            const int nx[4] = {1, -1, 0, 0}, ny[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                Cell *nc = map_cell_at(cx + nx[k], cy + ny[k]);
                if (nc && nc->wall == 0 && G.map.seen[(cy + ny[k]) * MAP_W + cx + nx[k]])
                    near = true;
            }
            if (!near) continue;
            uint32_t col = col_rgb(150, 60, 40);
            if (c->wall == WT_DOOR) col = col_rgb(210, 180, 60);
            if (c->wall == WT_DOOR_RED) col = col_rgb(230, 60, 50);
            if (c->wall == WT_DOOR_BLUE) col = col_rgb(70, 110, 250);
            if (c->wall == WT_DOOR_YELLOW) col = col_rgb(240, 210, 50);
            if (c->wall == WT_EXITDOOR || c->wall == WT_SWITCH ||
                c->wall == WT_SWITCH_ON)
                col = col_rgb(255, 90, 60);
            int px = (int)(ox + cx * sc), py = (int)(oy + cy * sc);
            fb_rect(px, py, (int)sc - 1, (int)sc - 1, col_shade(col, 0.8f));
        }
    // player arrow
    int px = (int)(FB_W / 2), py = (int)(VIEW_H / 2);
    for (int i = -2; i <= 6; i++)
        for (int j = -(6 - i) / 3; j <= (6 - i) / 3; j++) {
            int wx = (int)(i * cosf(G.pl.ang) - j * sinf(G.pl.ang));
            int wy = (int)(i * sinf(G.pl.ang) + j * cosf(G.pl.ang));
            fb_px(px + wx, py + wy, col_rgb(240, 240, 240));
        }
    char lvl[80];
    snprintf(lvl, sizeof(lvl), "%s", G.map.name);
    font_draw_str(10, 10, lvl, col_rgb(220, 220, 220), 1);
}

// ------------------------------------------------------------------ fire fx
static uint8_t fireBuf[FB_W / 4 * 40];
static bool fireInit = false;
void render_title_fire(float dt) {
    (void)dt;
    int fw = FB_W / 4, fh = 40;
    if (!fireInit) {
        memset(fireBuf, 0, sizeof(fireBuf));
        for (int x = 0; x < fw; x++) fireBuf[(fh - 1) * fw + x] = 36;
        fireInit = true;
    }
    static float acc = 0;
    acc += dt;
    while (acc > 1.0f / 30) {
        acc -= 1.0f / 30;
        for (int x = 0; x < fw; x++)
            for (int y = 1; y < fh; y++) {
                int src = y * fw + x;
                int dstX = x + rng_irange(-1, 1);
                if (dstX < 0 || dstX >= fw) continue;
                int dst = (y - 1) * fw + dstX;
                int v = fireBuf[src] - (rng_f() < 0.5f ? 1 : 0);
                if (v < 0) v = 0;
                fireBuf[dst] = (uint8_t)v;
            }
    }
    static const uint32_t fpal[37] = {
        col_rgb(7, 7, 7), col_rgb(31, 7, 7), col_rgb(47, 15, 7), col_rgb(71, 15, 7),
        col_rgb(87, 23, 7), col_rgb(103, 31, 7), col_rgb(119, 31, 7), col_rgb(143, 39, 7),
        col_rgb(159, 47, 7), col_rgb(175, 63, 7), col_rgb(191, 71, 7), col_rgb(199, 71, 7),
        col_rgb(223, 79, 7), col_rgb(223, 87, 7), col_rgb(223, 87, 7), col_rgb(215, 95, 7),
        col_rgb(215, 95, 7), col_rgb(215, 103, 15), col_rgb(207, 111, 15), col_rgb(207, 119, 15),
        col_rgb(207, 127, 15), col_rgb(207, 135, 23), col_rgb(199, 135, 23), col_rgb(199, 143, 23),
        col_rgb(199, 151, 31), col_rgb(191, 159, 31), col_rgb(191, 159, 31), col_rgb(191, 167, 39),
        col_rgb(191, 167, 39), col_rgb(191, 177, 47), col_rgb(183, 177, 47), col_rgb(183, 183, 47),
        col_rgb(183, 183, 55), col_rgb(207, 207, 111), col_rgb(223, 223, 159), col_rgb(239, 239, 199),
        col_rgb(255, 255, 255),
    };
    for (int y = 0; y < fh; y++)
        for (int x = 0; x < fw; x++) {
            int v = fireBuf[y * fw + x];
            if (v >= 37) v = 36;
            uint32_t c = fpal[v];
            // only show fire in lower band of the screen
            int sy = VIEW_H - fh * 4 + y * 4 - 40;
            for (int dy = 0; dy < 4; dy++)
                for (int dx = 0; dx < 4; dx++)
                    fb_px(x * 4 + dx, sy + y * 4 + dy, c);
        }
}

// ------------------------------------------------------------------ screens
static void draw_logo(int cx, int y) {
    const char *txt = "DOOM";
    int scale = 13;
    int w = font_measure(txt, scale);
    int x0 = cx - w / 2;
    font_draw_str(x0 + 6, y + 6, txt, col_rgb(50, 8, 8), scale);   // shadow
    font_draw_str(x0 + 3, y + 3, txt, col_rgb(120, 16, 12), scale);
    font_draw_str(x0, y, txt, col_rgb(206, 34, 22), scale);
}
static void render_title(float dt) {
    render_title_fire(dt);
    draw_logo(FB_W / 2, 30);
    int w = font_measure("AN ORIGINAL C ENGINE FOR MACOS", 1);
    font_draw_str(FB_W / 2 - w / 2, 130, "AN ORIGINAL C ENGINE FOR MACOS",
                  col_rgb(180, 150, 90), 1);
    const char *items[] = {"NEW GAME", "INSTRUCTIONS", "QUIT"};
    for (int i = 0; i < 3; i++) {
        int iw = font_measure(items[i], 2);
        int ix = FB_W / 2 - iw / 2;
        int iy = 175 + i * 34;
        uint32_t c = G.titleSel == i ? col_rgb(255, 220, 90) : col_rgb(200, 200, 200);
        font_draw_str(ix, iy, items[i], c, 2);
        if (G.titleSel == i) {
            float blink = sinf(G.time * 6) > -0.3f;
            if (blink) font_draw_str(ix - 26, iy, ">", col_rgb(255, 60, 40), 2);
        }
    }
    char df[64];
    const char *dn[] = {"I'M TOO YOUNG TO DIE", "HURT ME PLENTY", "ULTRA-VIOLENCE"};
    snprintf(df, sizeof(df), "DIFFICULTY: %s", dn[G.difficulty]);
    int dw = font_measure(df, 1);
    font_draw_str(FB_W / 2 - dw / 2, 290, df, col_rgb(160, 160, 170), 1);
    font_draw_str(8, FB_H - 12, "VERSION " VERSION_STR, col_rgb(120, 120, 120), 1);
    int cw = font_measure("ORIGINAL CODE - NO ID SOFTWARE ASSETS", 1);
    font_draw_str(FB_W - cw - 8, FB_H - 12, "ORIGINAL CODE - NO ID SOFTWARE ASSETS",
                  col_rgb(120, 120, 120), 1);
    font_draw_str(8, 8, "ARROWS/WASD SELECT - ENTER CONFIRM - M MUTE",
                  col_rgb(140, 140, 140), 1);
}
static void render_diffsel(float dt) {
    (void)dt;
    fb_blend_rect(0, 0, FB_W, FB_H, col_rgb(10, 4, 4), 1.0f);
    render_title_fire(0.0f);
    int w = font_measure("CHOOSE YOUR SKILL LEVEL", 3);
    font_draw_str(FB_W / 2 - w / 2, 60, "CHOOSE YOUR SKILL LEVEL",
                  col_rgb(220, 60, 40), 3);
    const char *dn[] = {"I'M TOO YOUNG TO DIE", "HURT ME PLENTY", "ULTRA-VIOLENCE"};
    for (int i = 0; i < 3; i++) {
        int iw = font_measure(dn[i], 2);
        font_draw_str(FB_W / 2 - iw / 2, 150 + i * 40, dn[i],
                      G.diffSel == i ? col_rgb(255, 220, 90) : col_rgb(190, 190, 190), 2);
        if (G.diffSel == i && sinf(G.time * 6) > -0.3f)
            font_draw_str(FB_W / 2 - iw / 2 - 26, 150 + i * 40, ">",
                          col_rgb(255, 60, 40), 2);
    }
}
static void render_instructions(void) {
    fb_blend_rect(0, 0, FB_W, FB_H, col_rgb(8, 8, 12), 1.0f);
    int w = font_measure("HOW TO PLAY", 3);
    font_draw_str(FB_W / 2 - w / 2, 20, "HOW TO PLAY", col_rgb(220, 60, 40), 3);
    const char *lines[] = {
        "WASD / ARROWS ....... MOVE",
        "MOUSE ............... TURN",
        "LEFT CLICK / CTRL ... FIRE",
        "SPACE / E ........... OPEN DOORS, USE SWITCHES",
        "1-5 ................. SWITCH WEAPON",
        "TAB ................. AUTOMAP",
        "ESC ................. PAUSE",
        "M ................... MUTE",
        "",
        "FIND THE EXIT OF EACH MAP.",
        "RED/BLUE/YELLOW DOORS NEED KEYCARDS.",
        "SOME WALLS LOOK DIFFERENT... PUSH THEM.",
        "SHOOT BARRELS. WATCH THE SLIME.",
        "",
        "TYPE 'IDDQD' OR 'IDKFA' IF DESPERATE.",
    };
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
        font_draw_str(60, 80 + i * 20, lines[i], col_rgb(200, 200, 200), 1);
    font_draw_str(60, FB_H - 30, "PRESS ESC TO RETURN", col_rgb(255, 220, 90), 1);
}
static void render_intermission(void) {
    fb_blend_rect(0, 0, FB_W, FB_H, col_rgb(12, 10, 8), 1.0f);
    char buf[128];
    int w;
    w = font_measure(G.map.name, 3);
    font_draw_str(FB_W / 2 - w / 2, 30, G.map.name, col_rgb(220, 60, 40), 3);
    w = font_measure("FINISHED", 2);
    font_draw_str(FB_W / 2 - w / 2, 76, "FINISHED", col_rgb(200, 200, 200), 2);

    int total = G.map.totalMonsters > 0 ? G.map.totalMonsters : 1;
    int itemTot = G.map.totalItems > 0 ? G.map.totalItems : 1;
    int secTot = G.map.totalSecrets > 0 ? G.map.totalSecrets : 1;
    struct { const char *label; int val; int max; } rows[3] = {
        {"KILLS", G.killCount * 100 / total, 100},
        {"ITEMS", G.itemCount * 100 / itemTot, 100},
        {"SECRETS", G.secretCount * 100 / secTot, 100},
    };
    for (int i = 0; i < 3; i++) {
        int shown = (int)G.interTally[i];
        if (shown > rows[i].val) shown = rows[i].val;
        snprintf(buf, sizeof(buf), "%s: %d%%", rows[i].label, shown);
        font_draw_str(140, 140 + i * 44, buf, col_rgb(235, 220, 120), 2);
    }
    int par = G.map.parSeconds;
    int t = (int)G.time;
    snprintf(buf, sizeof(buf), "TIME: %d:%02d  PAR: %d:%02d", t / 60, t % 60,
             par / 60, par % 60);
    w = font_measure(buf, 2);
    font_draw_str(FB_W / 2 - w / 2, 280, buf, col_rgb(200, 200, 200), 2);
    if (sinf(G.time * 4) > -0.2f) {
        w = font_measure("PRESS FIRE TO CONTINUE", 1);
        font_draw_str(FB_W / 2 - w / 2, 340, "PRESS FIRE TO CONTINUE",
                      col_rgb(255, 220, 90), 1);
    }
}
static void render_victory(void) {
    fb_blend_rect(0, 0, FB_W, FB_H, col_rgb(6, 4, 4), 1.0f);
    // starfield
    rng_seed(1234);
    for (int i = 0; i < 120; i++) {
        int x = rng_irange(0, FB_W - 1), y = rng_irange(0, FB_H - 1);
        fb_px(x, y, col_rgb(90, 90, 110));
    }
    const char *story[] = {
        "THE GATE IS BROKEN.",
        "",
        "WITH THE LAST BARON DESTROYED, THE RIFT",
        "CLOSES WITH A THUNDERCLAP THAT SHAKES",
        "THE BONES OF THE MOUNTAIN.",
        "",
        "YOU WALK OUT INTO COLD, CLEAN STARLIGHT.",
        "THE HORDE IS SILENT. THE HANGARS ARE QUIET.",
        "",
        "YOU HAVE EARNED YOUR REST.",
        "...FOR NOW.",
    };
    float scroll = G.victoryScroll;
    for (int i = 0; i < (int)(sizeof(story) / sizeof(story[0])); i++) {
        int y = (int)(300 + i * 26 - scroll);
        if (y < -20 || y > FB_H) continue;
        int w2 = font_measure(story[i], 2);
        font_draw_str(FB_W / 2 - w2 / 2, y, story[i],
                      i == 0 ? col_rgb(255, 200, 80) : col_rgb(210, 205, 195), 2);
    }
    int w = font_measure("THE END", 4);
    font_draw_str(FB_W / 2 - w / 2, 24, "THE END", col_rgb(220, 60, 40), 4);
    if (scroll > 380 && sinf(G.time * 4) > -0.2f) {
        w = font_measure("PRESS FIRE FOR TITLE", 1);
        font_draw_str(FB_W / 2 - w / 2, 360, "PRESS FIRE FOR TITLE",
                      col_rgb(255, 220, 90), 1);
    }
}

// ------------------------------------------------------------------ frame
void render_frame(void) {
    switch (G.mode) {
    case GM_TITLE: render_title(1.0f / 60); break;
    case GM_DIFFSEL: render_diffsel(1.0f / 60); break;
    case GM_INSTRUCTIONS: render_instructions(); break;
    case GM_PLAY:
        render_view();
        // weapon
        if (!G.pl.dead) {
            float bx = sinf(G.pl.bobPhase) * 7 * G.pl.bobAmt;
            float by = fabsf(cosf(G.pl.bobPhase)) * 5 * G.pl.bobAmt +
                       G.pl.raiseLower * 110;
            int fs = G.pl.weaponFiring > 0 ? (G.pl.muzzleFlash > 0.5f ? 1 : 2) : 0;
            paint_weapon(G.pl.weapon, fs, bx, by, G.pl.muzzleFlash);
        }
        if (G.automap) render_automap();
        render_hud();
        break;
    case GM_INTERMISSION: render_intermission(); break;
    case GM_VICTORY: render_victory(); break;
    default: break;
    }
}

// ------------------------------------------------------------------ bmp
void write_bmp(const char *path, const uint32_t *pix, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int rowBytes = w * 3;
    int pad = (4 - (rowBytes % 4)) % 4;
    int dataSize = (rowBytes + pad) * h;
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    uint32_t fileSize = 54 + dataSize;
    memcpy(hdr + 2, &fileSize, 4);
    uint32_t dataOff = 54;
    memcpy(hdr + 10, &dataOff, 4);
    uint32_t infoSize = 40;
    memcpy(hdr + 14, &infoSize, 4);
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    uint16_t planes = 1, bpp = 24;
    memcpy(hdr + 26, &planes, 2);
    memcpy(hdr + 28, &bpp, 2);
    memcpy(hdr + 34, &dataSize, 4);
    fwrite(hdr, 1, 54, f);
    uint8_t *row = calloc(1, rowBytes + pad);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint32_t c = pix[y * w + x];
            row[x * 3 + 0] = c & 255;
            row[x * 3 + 1] = (c >> 8) & 255;
            row[x * 3 + 2] = (c >> 16) & 255;
        }
        fwrite(row, 1, rowBytes + pad, f);
    }
    free(row);
    fclose(f);
}

void render_init(void) {}
