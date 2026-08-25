// ent.c — entities: player, monsters, projectiles, particles, combat
#include "doom.h"
#include <stdarg.h>

static inline Cell *cellAt(int cx, int cy) { return map_cell_at(cx, cy); }
bool g_levelDone = false;
int kindToWeapon(int itemKind);

// ------------------------------------------------------------------ msgs
void game_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(G.msg, sizeof(G.msg), fmt, ap);
    va_end(ap);
    G.msgTimer = 3.5f;
}

// ------------------------------------------------------------------ particles
static Particle *part_alloc(void) {
    for (int i = 0; i < MAX_PARTICLES; i++)
        if (!G.parts[i].active) return &G.parts[i];
    return NULL;
}
static void part_spawn(float x, float y, float z, float vx, float vy, float vz,
                       float life, uint32_t col, float size) {
    Particle *p = part_alloc();
    if (!p) return;
    p->active = true;
    p->x = x; p->y = y; p->z = z;
    p->vx = vx; p->vy = vy; p->vz = vz;
    p->life = p->maxLife = life;
    p->color = col; p->size = size;
}
void part_blood(float x, float y, int n) {
    for (int i = 0; i < n; i++) {
        float a = rng_f() * 6.283f, sp = 0.5f + rng_f() * 1.5f;
        part_spawn(x, y, 0.5f, cosf(a) * sp, sinf(a) * sp, 0.5f + rng_f(),
                   0.4f + rng_f() * 0.3f, col_rgb(150 + rng_irange(0, 70), 10, 10),
                   0.03f + rng_f() * 0.04f);
    }
}
void part_spark(float x, float y, int n) {
    for (int i = 0; i < n; i++) {
        float a = rng_f() * 6.283f, sp = 0.5f + rng_f() * 2.0f;
        part_spawn(x, y, 0.5f, cosf(a) * sp, sinf(a) * sp, rng_f() * 1.2f,
                   0.25f + rng_f() * 0.2f, col_rgb(255, 220 - rng_irange(0, 80), 90),
                   0.02f + rng_f() * 0.03f);
    }
}
void part_explosion_fx(float x, float y) {
    for (int i = 0; i < 26; i++) {
        float a = rng_f() * 6.283f, sp = rng_f() * 3.0f;
        uint32_t cols[3] = {col_rgb(255, 240, 140), col_rgb(255, 150, 30), col_rgb(160, 60, 20)};
        part_spawn(x, y, 0.4f + rng_f() * 0.4f, cosf(a) * sp, sinf(a) * sp,
                   rng_f() * 2.2f, 0.35f + rng_f() * 0.45f,
                   cols[rng_irange(0, 2)], 0.05f + rng_f() * 0.09f);
    }
}

// ------------------------------------------------------------------ collision
// circle vs grid; doors block unless mostly open
static bool circle_blocked(float x, float y, float r) {
    if (G.pl.noclip) return false;
    int x0 = (int)(x - r), x1 = (int)(x + r);
    int y0 = (int)(y - r), y1 = (int)(y + r);
    for (int cy = y0; cy <= y1; cy++)
        for (int cx = x0; cx <= x1; cx++) {
            if (!map_walkable_cell(cx, cy)) return true;
            // treat partially-open doors as solid
            Cell *c = cellAt(cx, cy);
            if (c && c->wall != 0) {
                int di = G.map.doorIndex[cy * MAP_W + cx];
                if (di >= 0 && G.map.doors[di].open < 0.75f) return true;
            }
        }
    return false;
}
static void move_with_slide(float *x, float *y, float dx, float dy, float r) {
    float nx = *x + dx;
    if (!circle_blocked(nx, *y, r)) *x = nx;
    float ny = *y + dy;
    if (!circle_blocked(*x, ny, r)) *y = ny;
}

// ------------------------------------------------------------------ spawning
Ent *ent_spawn(int type, float x, float y) {
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (e->active) continue;
        memset(e, 0, sizeof(*e));
        e->active = true;
        e->type = type;
        e->x = x; e->y = y;
        e->dropItem = -1;
        switch (type) {
        case ET_TROOPER: e->hp = e->maxHp = 20; e->radius = 0.3f; e->speed = 1.3f;
            e->blocking = true; e->dropItem = IT_CLIP; break;
        case ET_IMP: e->hp = e->maxHp = 60; e->radius = 0.34f; e->speed = 1.5f;
            e->blocking = true; break;
        case ET_PINKY: e->hp = e->maxHp = 150; e->radius = 0.4f; e->speed = 2.3f;
            e->blocking = true; break;
        case ET_BARON: e->hp = e->maxHp = 500; e->radius = 0.45f; e->speed = 1.7f;
            e->blocking = true; break;
        case ET_BARREL: e->hp = e->maxHp = 20; e->radius = 0.3f; e->blocking = true; break;
        case ET_LAMP: e->radius = 0.2f; e->blocking = true; break;
        default: break;
        }
        return e;
    }
    return NULL;
}
Ent *ent_spawn_item(int kind, float x, float y) {
    Ent *e = ent_spawn(ET_ITEM, x, y);
    if (e) e->itemKind = kind;
    return e;
}
void fire_projectile(int kind, float x, float y, float dxn, float dyn, float speed,
                     float dmg, bool enemy) {
    for (int i = 0; i < MAX_PROJ; i++) {
        Projectile *p = &G.projs[i];
        if (p->active) continue;
        memset(p, 0, sizeof(*p));
        p->active = true;
        p->kind = kind;
        p->x = x; p->y = y;
        p->dx = dxn; p->dy = dyn;
        p->speed = speed;
        p->dmg = dmg;
        p->enemyOwned = enemy;
        return;
    }
}

void ents_reset(void) {
    memset(G.ents, 0, sizeof(G.ents));
    memset(G.projs, 0, sizeof(G.projs));
    memset(G.parts, 0, sizeof(G.parts));
    G.numEnts = MAX_ENTS; // we index-scan anyway
}

void ents_spawn_level_things(void) {
    int n = 0;
    const MapSpawn *sp = map_spawn_list(&n);
    for (int i = 0; i < n; i++) {
        int code = sp[i].code;
        if (code == TH_TROOPER) ent_spawn(ET_TROOPER, sp[i].x, sp[i].y);
        else if (code == TH_IMP) ent_spawn(ET_IMP, sp[i].x, sp[i].y);
        else if (code == TH_PINKY) ent_spawn(ET_PINKY, sp[i].x, sp[i].y);
        else if (code == TH_BARON) ent_spawn(ET_BARON, sp[i].x, sp[i].y);
        else if (code == TH_BARREL) ent_spawn(ET_BARREL, sp[i].x, sp[i].y);
        else if (code == TH_LAMP) ent_spawn(ET_LAMP, sp[i].x, sp[i].y);
        else if (code >= 0) ent_spawn_item(code, sp[i].x, sp[i].y);
    }
}

// ------------------------------------------------------------------ audio helpers
static float pan_for(float x, float y) {
    float rx = cosf(G.pl.ang + 1.5708f), ry = sinf(G.pl.ang + 1.5708f);
    float dx = x - G.pl.x, dy = y - G.pl.y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 0.01f) return 0;
    float side = (dx * rx + dy * ry) / d;
    return side * 0.8f;
}
static float gain_for(float x, float y) {
    float d = hypotf(x - G.pl.x, y - G.pl.y);
    if (d > 18) return 0;
    return 1.0f - d / 19.0f;
}
void sfx_play_at(int id, float x, float y) {
    sfx_play(id, gain_for(x, y), pan_for(x, y));
}

// ------------------------------------------------------------------ combat
void ent_damage(Ent *e, int amount, bool wake);

void spawn_explosion(float x, float y, float dmg, float radius) {
    part_explosion_fx(x, y);
    sfx_play_at(SX_EXPLOSION, x, y);
    G.pl.shakeT = 0.3f;
    G.pl.shakeAmt = fminf(1.0f, 1.5f / (1 + hypotf(x - G.pl.x, y - G.pl.y)));
    // hurt player
    float pd = hypotf(x - G.pl.x, y - G.pl.y);
    if (pd < radius && !G.pl.dead) {
        int dm = (int)(dmg * (1.0f - pd / radius));
        player_damage(dm);
    }
    // hurt entities
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active || !e->blocking || e->type == ET_LAMP) continue;
        float d = hypotf(x - e->x, y - e->y);
        if (d < radius) {
            int dm = (int)(dmg * (1.0f - d / radius));
            if (dm > 0) ent_damage(e, dm, true);
        }
    }
    // chain barrels: light their fuses
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active || e->type != ET_BARREL || e->state != 0) continue;
        float d = hypotf(x - e->x, y - e->y);
        if (d < radius + 0.4f) { e->state = 1; e->stateT = 0; } // fused
    }
}

void ent_damage(Ent *e, int amount, bool wake) {
    if (!e->active || e->state == MS_DYING || e->state == MS_DEAD) return;
    if (wake) e->alerted = true;
    e->hp -= amount;
    if (e->type == ET_BARREL) {
        if (e->hp <= 0 && e->state == 0) { e->state = 1; e->stateT = 0; }
        return;
    }
    if (e->type == ET_LAMP) return;
    if (e->hp <= 0) {
        e->state = MS_DYING;
        e->stateT = 0;
        e->dyingFrame = 0;
        e->blocking = false;
        G.killCount++;
        int snd = SX_IMP_DIE;
        if (e->type == ET_TROOPER) snd = SX_TROOPER_DIE;
        else if (e->type == ET_PINKY) snd = SX_PINKY_DIE;
        else if (e->type == ET_BARON) snd = SX_BARON_DIE;
        sfx_play_at(snd, e->x, e->y);
        part_blood(e->x, e->y, 10);
        return;
    }
    // pain chance
    int chance = e->type == ET_TROOPER ? 78 : e->type == ET_IMP ? 60 :
                 e->type == ET_PINKY ? 32 : 8;
    if (e->painCool <= 0 && rng_irange(0, 99) < chance) {
        e->state = MS_PAIN;
        e->stateT = 0;
        e->painCool = 0.6f;
    }
}

static Ent *monster_along_ray(float px, float py, float ang, float wallDist,
                              float *outT) {
    float dx = cosf(ang), dy = sinf(ang);
    Ent *best = NULL;
    float bt = wallDist;
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active || !e->blocking) continue;
        if (e->type == ET_BARREL || e->type == ET_LAMP) {} // barrels are shootable
        float ex = e->x - px, ey = e->y - py;
        float t = ex * dx + ey * dy;
        if (t <= 0.1f || t > bt) continue;
        float perp = fabsf(ex * dy - ey * dx);
        if (perp < e->radius + 0.12f) { bt = t; best = e; }
    }
    *outT = bt;
    return best;
}

void fire_hitscan(float ang, int dmgLo, int dmgHi, float maxDist, bool spark) {
    float dx = cosf(ang), dy = sinf(ang);
    float wallDist = world_ray_dist(G.pl.x, G.pl.y, dx, dy, maxDist);
    float t;
    Ent *e = monster_along_ray(G.pl.x, G.pl.y, ang, wallDist, &t);
    if (e) {
        bool wasAlive = e->state != MS_DYING && e->state != MS_DEAD;
        ent_damage(e, rng_irange(dmgLo, dmgHi), true);
        if (wasAlive && e->type != ET_BARREL)
            part_blood(e->x, e->y, 4);
    } else if (spark) {
        float hx = G.pl.x + dx * wallDist * 0.98f;
        float hy = G.pl.y + dy * wallDist * 0.98f;
        part_spark(hx - dx * 0.1f, hy - dy * 0.1f, 3);
    }
}

// ------------------------------------------------------------------ projectiles
static void proj_explode(Projectile *p) {
    p->active = false;
    if (p->kind == PR_ROCKET) spawn_explosion(p->x, p->y, 110, 2.1f);
    else {
        sfx_play_at(SX_FIREBALL_HIT, p->x, p->y);
        for (int k = 0; k < 8; k++) {
            float a = rng_f() * 6.283f;
            part_spawn(p->x, p->y, 0.5f, cosf(a), sinf(a), rng_f() * 1.5f, 0.3f,
                       p->kind == PR_PLASMA ? col_rgb(90, 200, 255) : col_rgb(255, 160, 40),
                       0.04f);
        }
        if (!p->enemyOwned) {
            // player plasma: direct damage handled on impact below
        }
    }
}

static void proj_update(Projectile *p, float dt) {
    float steps = (int)(p->speed * dt / 0.08f) + 1;
    float sd = dt / steps;
    for (int s = 0; s < steps && p->active; s++) {
        p->x += p->dx * p->speed * sd;
        p->y += p->dy * p->speed * sd;
        p->age += sd;
        if (map_blocks_sight(p->x, p->y)) { proj_explode(p); return; }
        if (p->age > 6) { p->active = false; return; }
        if (p->enemyOwned) {
            if (!G.pl.dead) {
                float d = hypotf(p->x - G.pl.x, p->y - G.pl.y);
                if (d < 0.42f) {
                    player_damage((int)p->dmg);
                    proj_explode(p);
                    return;
                }
            }
        } else {
            for (int i = 0; i < MAX_ENTS; i++) {
                Ent *e = &G.ents[i];
                if (!e->active || !e->blocking) continue;
                float d = hypotf(p->x - e->x, p->y - e->y);
                if (d < e->radius + 0.15f) {
                    if (e->type == ET_BARREL || e->type >= ET_TROOPER) {
                        if (p->kind == PR_ROCKET) { proj_explode(p); return; }
                        ent_damage(e, (int)p->dmg, true);
                        part_blood(p->x, p->y, 3);
                        p->active = false;
                        return;
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------ monster AI
static void monster_fire(Ent *e) {
    float dx = G.pl.x - e->x, dy = G.pl.y - e->y;
    float d = hypotf(dx, dy);
    if (d < 0.01f) return;
    dx /= d; dy /= d;
    switch (e->type) {
    case ET_TROOPER: {
        sfx_play_at(SX_PISTOL, e->x, e->y);
        // trooper hitscan with inaccuracy
        float miss = (rng_f() - 0.5f) * 0.22f;
        float ca = atan2f(dy, dx) + miss;
        float wd = world_ray_dist(e->x, e->y, cosf(ca), sinf(ca), 20);
        float pd = d;
        if (wd > pd) {
            int dmg = rng_irange(3, 12);
            if (G.difficulty == 0) dmg = dmg * 2 / 3;
            player_damage(dmg);
        }
        break;
    }
    case ET_IMP:
        sfx_play_at(SX_IMP_THROW, e->x, e->y);
        fire_projectile(PR_IMPBALL, e->x + dx * 0.4f, e->y + dy * 0.4f,
                        dx, dy, 6.5f + G.difficulty, (float)rng_irange(8, 22), true);
        break;
    case ET_PINKY:
        if (d < 1.25f) {
            sfx_play_at(SX_PINKY_BITE, e->x, e->y);
            int dmg = rng_irange(4, 30);
            if (G.difficulty == 0) dmg = dmg * 2 / 3;
            player_damage(dmg);
        }
        break;
    case ET_BARON:
        sfx_play_at(SX_IMP_THROW, e->x, e->y);
        fire_projectile(PR_BARONBALL, e->x + dx * 0.5f, e->y + dy * 0.5f,
                        dx, dy, 8.0f + G.difficulty, (float)rng_irange(18, 48), true);
        break;
    }
}

static void monster_ai(Ent *e, float dt) {
    float pdx = G.pl.x - e->x, pdy = G.pl.y - e->y;
    float pdist = hypotf(pdx, pdy);
    e->painCool -= dt;

    switch (e->state) {
    case MS_SLEEP:
        if (pdist < 11 && !G.pl.dead &&
            world_los(e->x, e->y, G.pl.x, G.pl.y)) {
            e->alerted = true;
        }
        if (e->alerted) {
            e->state = MS_CHASE;
            e->thinkT = 0;
            int snd = SX_IMP_SIGHT;
            if (e->type == ET_TROOPER) snd = SX_TROOPER_SIGHT;
            else if (e->type == ET_PINKY) snd = SX_PINKY_SIGHT;
            else if (e->type == ET_BARON) snd = SX_BARON_SIGHT;
            sfx_play_at(snd, e->x, e->y);
        }
        break;

    case MS_CHASE: {
        if (G.pl.dead) break;
        e->animT += dt;
        // face & move toward player with wobble
        float wob = sinf(G.time * 2.1f + (e->x + e->y) * 7.0f) * 0.5f;
        float ax = pdx / (pdist + 0.001f), ay = pdy / (pdist + 0.001f);
        float wx = ax - ay * wob * 0.4f, wy = ay + ax * wob * 0.4f;
        float spdMul = G.difficulty == 0 ? 0.85f : G.difficulty == 2 ? 1.1f : 1.0f;
        float oldx = e->x, oldy = e->y;
        move_with_slide(&e->x, &e->y, wx * e->speed * spdMul * dt,
                        wy * e->speed * spdMul * dt, e->radius);
        // separation from other blockers
        for (int i = 0; i < MAX_ENTS; i++) {
            Ent *o = &G.ents[i];
            if (o == e || !o->active || !o->blocking) continue;
            float ddx = e->x - o->x, ddy = e->y - o->y;
            float dd = hypotf(ddx, ddy);
            float minD = e->radius + o->radius;
            if (dd > 0.001f && dd < minD) {
                move_with_slide(&e->x, &e->y, ddx / dd * dt * 0.8f,
                                ddy / dd * dt * 0.8f, e->radius);
            }
        }
        bool moved = fabsf(e->x - oldx) + fabsf(e->y - oldy) > 0.0005f;
        if (!moved) {
            // bumping a door? ask it to open
            int fx = (int)(e->x + wx * 0.6f), fy = (int)(e->y + wy * 0.6f);
            map_monster_open(fx, fy);
        }
        // attack decision
        float range = e->type == ET_PINKY ? 1.3f : e->type == ET_TROOPER ? 9.0f : 11.0f;
        e->thinkT -= dt;
        if (e->thinkT <= 0 && pdist < range &&
            (e->type == ET_PINKY ? pdist < 1.25f : world_los(e->x, e->y, G.pl.x, G.pl.y))) {
            e->state = MS_WINDUP;
            e->stateT = 0;
        }
        break;
    }

    case MS_WINDUP: {
        e->stateT += dt;
        float wind = e->type == ET_PINKY ? 0.28f : 0.42f;
        wind *= (G.difficulty == 2) ? 0.75f : 1.0f;
        if (e->stateT >= wind) {
            monster_fire(e);
            e->state = MS_CHASE;
            float cool = G.difficulty == 2 ? 1.0f : G.difficulty == 0 ? 2.0f : 1.5f;
            e->thinkT = cool + rng_f() * 0.7f;
        }
        break;
    }

    case MS_PAIN:
        e->stateT += dt;
        if (e->stateT > 0.32f) e->state = MS_CHASE;
        break;

    default:
        break;
    }
}

// ------------------------------------------------------------------ alerts
void ent_alert_near(float x, float y, float radius) {
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active || e->state != MS_SLEEP) continue;
        if (e->type < ET_TROOPER || e->type > ET_BARON) continue;
        if (hypotf(e->x - x, e->y - y) < radius) e->alerted = true;
    }
}

// ------------------------------------------------------------------ items
static bool pickup_apply(Ent *e) {
    Player *p = &G.pl;
    int kind = e->itemKind;
    bool took = false;
    switch (kind) {
    case IT_STIM:
        if (p->hp < 100) { p->hp = (p->hp + 10 > 100) ? 100 : p->hp + 10;
            game_msg("Picked up a stimpack."); took = true; }
        break;
    case IT_MEDKIT:
        if (p->hp < 100) { p->hp = (p->hp + 25 > 100) ? 100 : p->hp + 25;
            game_msg("Picked up a medikit."); took = true; }
        break;
    case IT_POTION:
        if (p->hp < 200) { p->hp++; game_msg("Picked up a health potion."); took = true; }
        break;
    case IT_ARMOR:
        if (p->armor < 100) { p->armor = 100; p->armorAbsorb = 1.0f / 3.0f;
            game_msg("Picked up the armor."); took = true; }
        break;
    case IT_MEGAARMOR:
        if (p->armor < 200) { p->armor = 200; p->armorAbsorb = 0.5f;
            game_msg("Picked up the MEGAARMOR!"); took = true; }
        break;
    case IT_BACKPACK:
        p->maxAmmo[AM_BULLET] = 400; p->maxAmmo[AM_SHELL] = 100;
        p->maxAmmo[AM_ROCKET] = 100; p->maxAmmo[AM_CELL] = 600;
        p->ammo[AM_BULLET] += 10; p->ammo[AM_SHELL] += 4;
        p->ammo[AM_ROCKET] += 2; p->ammo[AM_CELL] += 20;
        for (int i = 0; i < AM_MAX; i++)
            if (p->ammo[i] > p->maxAmmo[i]) p->ammo[i] = p->maxAmmo[i];
        game_msg("Picked up a backpack full of ammo!");
        took = true;
        break;
    case IT_CLIP:
        if (p->ammo[AM_BULLET] < p->maxAmmo[AM_BULLET]) {
            p->ammo[AM_BULLET] += 10; game_msg("Picked up a clip."); took = true; }
        break;
    case IT_AMMOBOX:
        if (p->ammo[AM_BULLET] < p->maxAmmo[AM_BULLET]) {
            p->ammo[AM_BULLET] += 50; game_msg("Picked up a box of bullets."); took = true; }
        break;
    case IT_SHELLS:
        if (p->ammo[AM_SHELL] < p->maxAmmo[AM_SHELL]) {
            p->ammo[AM_SHELL] += 8; game_msg("Picked up shotgun shells."); took = true; }
        break;
    case IT_SHELLBOX:
        if (p->ammo[AM_SHELL] < p->maxAmmo[AM_SHELL]) {
            p->ammo[AM_SHELL] += 20; game_msg("Picked up a box of shells."); took = true; }
        break;
    case IT_ROCKETS:
        if (p->ammo[AM_ROCKET] < p->maxAmmo[AM_ROCKET]) {
            p->ammo[AM_ROCKET] += 10; game_msg("Picked up a crate of rockets."); took = true; }
        break;
    case IT_CELL:
        if (p->ammo[AM_CELL] < p->maxAmmo[AM_CELL]) {
            p->ammo[AM_CELL] += 40; game_msg("Picked up an energy cell."); took = true; }
        break;
    case IT_GUN_SHOTGUN:
        p->hasWeapon[WP_SHOTGUN] = true;
        p->ammo[AM_SHELL] += 8;
        goto gunTaken;
    case IT_GUN_CHAINGUN:
        p->hasWeapon[WP_CHAINGUN] = true;
        p->ammo[AM_BULLET] += 20;
        goto gunTaken;
    case IT_GUN_ROCKET:
        p->hasWeapon[WP_ROCKET] = true;
        p->ammo[AM_ROCKET] += 10;
        goto gunTaken;
    case IT_GUN_PLASMA:
        p->hasWeapon[WP_PLASMA] = true;
        p->ammo[AM_CELL] += 40;
        goto gunTaken;
    gunTaken:
        for (int i = 0; i < AM_MAX; i++)
            if (p->ammo[i] > p->maxAmmo[i]) p->ammo[i] = p->maxAmmo[i];
        p->faceGrinT = 1.6f;
        game_msg("You got a new weapon!");
        sfx_play(SX_PICKUP_WEAPON, 1.0f, 0);
        // auto-switch if current is weak
        if (p->weapon < kindToWeapon(kind)) p->nextWeapon = kindToWeapon(kind);
        took = true;
        break;
    case IT_KEY_RED:
        p->keys |= KEY_RED; game_msg("Picked up a RED keycard.");
        sfx_play(SX_PICKUP_KEY, 1.0f, 0);
        p->faceGrinT = 1.2f;
        took = true;
        break;
    case IT_KEY_BLUE:
        p->keys |= KEY_BLUE; game_msg("Picked up a BLUE keycard.");
        sfx_play(SX_PICKUP_KEY, 1.0f, 0);
        p->faceGrinT = 1.2f;
        took = true;
        break;
    case IT_KEY_YELLOW:
        p->keys |= KEY_YELLOW; game_msg("Picked up a YELLOW keycard.");
        sfx_play(SX_PICKUP_KEY, 1.0f, 0);
        p->faceGrinT = 1.2f;
        took = true;
        break;
    }
    if (kind != IT_GUN_SHOTGUN && kind != IT_GUN_CHAINGUN && kind != IT_GUN_ROCKET &&
        kind != IT_GUN_PLASMA && kind < IT_KEY_RED) {
        if (took) sfx_play(SX_PICKUP, 0.9f, 0);
    }
    return took;
}
// helper used above (declared late is fine in C? No—need prototype). See top fix below.

void player_damage(int amount) {
    Player *p = &G.pl;
    if (p->dead || amount <= 0) return;
    if (G.difficulty == 0) amount = amount * 2 / 3;
    if (G.difficulty == 2) amount = amount * 4 / 3;
    if (p->god) amount = 0;
    if (amount > 0 && p->armor > 0) {
        int abs = (int)(amount * p->armorAbsorb);
        if (abs > p->armor) abs = p->armor;
        p->armor -= abs;
        amount -= abs;
    }
    p->hp -= amount;
    p->damageFlash = fminf(1.0f, p->damageFlash + amount / 45.0f + 0.15f);
    p->faceOuchT = 0.8f;
    if (p->hp <= 0) {
        p->hp = 0;
        p->dead = true;
        p->deadT = 0;
        sfx_play(SX_PLAYER_DIE, 1.0f, 0);
        game_msg("You died. Press SPACE to try again.");
    } else if (amount > 0) {
        sfx_play(SX_PLAYER_PAIN, 0.9f, 0);
    }
}

bool player_use_action(void) {
    int r = map_use_target(G.pl.x, G.pl.y, G.pl.ang);
    if (r == USE_LOCKED + 1) { game_msg("You need a RED keycard!"); return true; }
    if (r == USE_LOCKED + 2) { game_msg("You need a BLUE keycard!"); return true; }
    if (r == USE_LOCKED + 3) { game_msg("You need a YELLOW keycard!"); return true; }
    if (r == USE_LOCKED + 4) { game_msg("The exit door is sealed - find the switch."); return true; }
    switch (r) {
    case USE_NONE: break;
    case USE_DOOR: break;
    case USE_SECRET_FOUND:
        game_msg("*** A SECRET IS REVEALED! ***");
        sfx_play(SX_SECRET, 1.0f, 0);
        break;
    case USE_SWITCH:
        game_msg("The exit door grinds open...");
        break;
    }
    return r != USE_NONE;
}

// ------------------------------------------------------------------ weapons
static const struct WeaponDef {
    float cool;
    int ammoType;
    int ammoUse;
    int dmgLo, dmgHi;
    int pellets;
} WDEF[WP_COUNT] = {
    [WP_PISTOL]   = {0.40f, AM_BULLET, 1, 5, 15, 1},
    [WP_SHOTGUN]  = {0.88f, AM_SHELL, 1, 3, 10, 7},
    [WP_CHAINGUN] = {0.11f, AM_BULLET, 1, 5, 13, 1},
    [WP_ROCKET]   = {0.85f, AM_ROCKET, 1, 0, 0, 0},
    [WP_PLASMA]   = {0.12f, AM_CELL, 1, 15, 28, 1},
};

int kindToWeapon(int itemKind) {
    switch (itemKind) {
    case IT_GUN_SHOTGUN: return WP_SHOTGUN;
    case IT_GUN_CHAINGUN: return WP_CHAINGUN;
    case IT_GUN_ROCKET: return WP_ROCKET;
    case IT_GUN_PLASMA: return WP_PLASMA;
    }
    return 0;
}

static void player_try_fire(void) {
    Player *p = &G.pl;
    if (p->fireCool > 0 || p->raiseLower > 0.3f || p->dead) return;
    const struct WeaponDef *w = &WDEF[p->weapon];
    if (p->ammo[w->ammoType] < w->ammoUse) {
        sfx_play(SX_EMPTY, 0.7f, 0);
        p->fireCool = 0.3f;
        // auto switch down
        for (int wp = WP_COUNT - 1; wp >= 0; wp--) {
            if (p->hasWeapon[wp] && p->ammo[WDEF[wp].ammoType] >= WDEF[wp].ammoUse) {
                p->nextWeapon = wp;
                break;
            }
        }
        return;
    }
    p->ammo[w->ammoType] -= w->ammoUse;
    p->fireCool = w->cool;
    p->weaponFiring = 2;
    p->muzzleFlash = 1.0f;
    G.globalLightBoost = 0.55f;
    ent_alert_near(p->x, p->y, 13);
    switch (p->weapon) {
    case WP_PISTOL: sfx_play(SX_PISTOL, 1, 0);
        fire_hitscan(p->ang + (rng_f() - 0.5f) * 0.03f, w->dmgLo, w->dmgHi, 30, true);
        break;
    case WP_SHOTGUN: sfx_play(SX_SHOTGUN, 1, 0);
        for (int i = 0; i < w->pellets; i++)
            fire_hitscan(p->ang + (rng_f() - 0.5f) * 0.14f, w->dmgLo, w->dmgHi, 26, i == 0);
        break;
    case WP_CHAINGUN: sfx_play(SX_CHAINGUN, 1, 0);
        fire_hitscan(p->ang + (rng_f() - 0.5f) * 0.06f, w->dmgLo, w->dmgHi, 30, true);
        break;
    case WP_ROCKET: {
        sfx_play(SX_ROCKET, 1, 0);
        fire_projectile(PR_ROCKET, p->x + cosf(p->ang) * 0.5f,
                        p->y + sinf(p->ang) * 0.5f, cosf(p->ang), sinf(p->ang),
                        8.5f, 0, false);
        break;
    }
    case WP_PLASMA: sfx_play(SX_PLASMA, 1, 0);
        fire_projectile(PR_PLASMA, p->x + cosf(p->ang) * 0.5f,
                        p->y + sinf(p->ang) * 0.5f, cosf(p->ang), sinf(p->ang),
                        13.0f, (float)rng_irange(w->dmgLo, w->dmgHi), false);
        break;
    }
}

// ------------------------------------------------------------------ player
void player_reset_for_level(void) {
    Player *p = &G.pl;
    const LevelDef *L = &LEVELS[G.level];
    memset(p, 0, sizeof(*p));
    p->x = L->playerStart.x + 0.5f;
    p->y = L->playerStart.y + 0.5f;
    p->ang = 0;
    p->hp = 100;
    p->nextWeapon = -1;
    p->maxAmmo[AM_BULLET] = 200; p->maxAmmo[AM_SHELL] = 50;
    p->maxAmmo[AM_ROCKET] = 50; p->maxAmmo[AM_CELL] = 300;
    player_give_default_kit(false);
}

void player_give_default_kit(bool keepGear) {
    Player *p = &G.pl;
    if (!keepGear) {
        memset(p->hasWeapon, 0, sizeof(p->hasWeapon));
        memset(p->ammo, 0, sizeof(p->ammo));
        p->armor = 0;
        p->keys = 0;
    }
    p->hasWeapon[WP_PISTOL] = true;
    if (p->ammo[AM_BULLET] < 50) p->ammo[AM_BULLET] = 50;
    p->weapon = WP_PISTOL;
    p->nextWeapon = -1;
}

void player_switch_weapon(int w) {
    Player *p = &G.pl;
    if (w >= 0 && w < WP_COUNT && p->hasWeapon[w] && w != p->weapon)
        p->nextWeapon = w;
}

static void player_move(Input *in, float dt) {
    Player *p = &G.pl;
    float spd = 4.2f;
    float ca = cosf(p->ang), sa = sinf(p->ang);
    float mx = 0, my = 0;
    if (in->fwd) { mx += ca; my += sa; }
    if (in->back) { mx -= ca; my -= sa; }
    if (in->strafeLeft) { mx += sa; my -= ca; }
    if (in->strafeRight) { mx -= sa; my += ca; }
    float ml = hypotf(mx, my);
    if (ml > 0.01f) {
        mx /= ml; my /= ml;
        float ox = p->x, oy = p->y;
        move_with_slide(&p->x, &p->y, mx * spd * dt, my * spd * dt, 0.28f);
        // pushing against a closed door? open it (modern QoL, Space still works)
        if (hypotf(p->x - ox, p->y - oy) < spd * dt * 0.25f) {
            static float bumpMsgCool = 0;
            int fx = (int)(p->x + mx * 0.7f), fy = (int)(p->y + my * 0.7f);
            Cell *c = map_cell_at(fx, fy);
            if (c && c->wall != 0) {
                int di = G.map.doorIndex[fy * MAP_W + fx];
                if (di >= 0) {
                    Door *d = &G.map.doors[di];
                    if (!d->locked && !d->isSecret && (d->state == 0 || d->state == 3)) {
                        map_monster_open(fx, fy);
                    } else if (d->locked && bumpMsgCool <= 0 && d->open < 0.1f) {
                        bumpMsgCool = 2.5f;
                        sfx_play(SX_DOOR_LOCKED, 0.8f, 0);
                        if (d->isExit)
                            game_msg("The exit door is sealed - find the switch.");
                        else if (d->locked == 1) game_msg("You need a RED keycard!");
                        else if (d->locked == 2) game_msg("You need a BLUE keycard!");
                        else if (d->locked == 3) game_msg("You need a YELLOW keycard!");
                    }
                }
            }
            bumpMsgCool -= dt;
        }
        p->bobPhase += dt * 9.0f;
        p->bobAmt = fminf(1, p->bobAmt + dt * 6);
    } else {
        p->bobAmt = fmaxf(0, p->bobAmt - dt * 6);
    }
}

void player_update(float dt) {
    Player *p = &G.pl;
    extern Input IN;

    if (p->dead) {
        p->deadT += dt;
        p->damageFlash = fminf(0.8f, 0.4f + p->deadT * 0.2f);
        return;
    }

    // turning handled in main (mouse + keys modify ang there or here via IN)
    if (IN.turnLeft) p->ang -= 2.6f * dt;
    if (IN.turnRight) p->ang += 2.6f * dt;
    p->ang += IN.mouseDX * 0.0023f;
    IN.mouseDX = 0;

    player_move(&IN, dt);

    // weapon raise/lower + switching
    if (p->nextWeapon >= 0 && p->nextWeapon < WP_COUNT) {
        p->raiseLower += dt * 4.5f;
        if (p->raiseLower >= 1.0f) {
            p->weapon = p->nextWeapon;
            p->nextWeapon = -1;
        }
    } else if (p->raiseLower > 0) {
        p->raiseLower -= dt * 4.5f;
        if (p->raiseLower < 0) p->raiseLower = 0;
    }

    if (IN.weaponNext) {
        for (int k = 1; k <= WP_COUNT; k++) {
            int w = (p->weapon + k) % WP_COUNT;
            if (p->hasWeapon[w]) { player_switch_weapon(w); break; }
        }
        IN.weaponNext = false;
    }
    if (IN.weaponPrev) {
        for (int k = WP_COUNT - 1; k >= 0; k--) {
            int w = (p->weapon + k) % WP_COUNT;
            if (p->hasWeapon[w]) { player_switch_weapon(w); break; }
        }
        IN.weaponPrev = false;
    }
    if (IN.selectWeapon >= 0) {
        int slot = IN.selectWeapon; // 1..5 -> 0..4
        if (slot >= 1 && slot <= WP_COUNT) player_switch_weapon(slot - 1);
        IN.selectWeapon = -1;
    }

    if (IN.fire) player_try_fire();
    if (IN.use) { IN.use = false; player_use_action(); }

    // cooldowns / fx decay
    p->fireCool -= dt;
    if (p->weaponFiring > 0) p->weaponFiring--;
    p->muzzleFlash = fmaxf(0, p->muzzleFlash - dt * 6);
    p->damageFlash = fmaxf(0, p->damageFlash - dt * 1.2f);
    p->pickupFlash = fmaxf(0, p->pickupFlash - dt * 2.5f);
    p->teleportFlash = fmaxf(0, p->teleportFlash - dt * 2.0f);
    p->faceGrinT = fmaxf(0, p->faceGrinT - dt);
    p->faceOuchT = fmaxf(0, p->faceOuchT - dt);
    p->shakeT = fmaxf(0, p->shakeT - dt);
    G.globalLightBoost = fmaxf(0, G.globalLightBoost - dt * 4);

    // pickups
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active || e->type != ET_ITEM) continue;
        if (hypotf(e->x - p->x, e->y - p->y) < 0.55f) {
            if (pickup_apply(e)) {
                e->active = false;
                G.itemCount++;
                p->pickupFlash = fminf(1, p->pickupFlash + 0.4f);
            }
        }
    }

    // exit pad?
    Cell *c = cellAt((int)p->x, (int)p->y);
    if (c && c->floor == FT_EXITPAD && !p->dead) {
        extern bool g_levelDone;
        g_levelDone = true;
    }
}

// ------------------------------------------------------------------ ents tick
void ents_update(float dt) {
    // barrels fuse
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active) continue;
        if (e->type == ET_BARREL && e->state == 1) {
            e->stateT += dt;
            if (e->stateT > 0.18f) {
                e->active = false;
                spawn_explosion(e->x, e->y, 120, 2.2f);
            }
        }
    }
    // monsters
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active) continue;
        if (e->type < ET_TROOPER || e->type > ET_BARON) continue;
        if (e->state == MS_DYING) {
            e->stateT += dt;
            float frames = e->type == ET_BARON ? 4 : 3;
            float fdur = 0.16f;
            e->frame = (int)(e->stateT / fdur);
            if (e->frame >= frames) { e->frame = frames - 1; e->state = MS_DEAD; }
        } else if (e->state != MS_DEAD) {
            monster_ai(e, dt);
        }
    }
    // projectiles
    for (int i = 0; i < MAX_PROJ; i++) {
        Projectile *p = &G.projs[i];
        if (p->active) proj_update(p, dt);
    }
    // particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *pt = &G.parts[i];
        if (!pt->active) continue;
        pt->life -= dt;
        if (pt->life <= 0) { pt->active = false; continue; }
        pt->x += pt->vx * dt; pt->y += pt->vy * dt; pt->z += pt->vz * dt;
        pt->vz -= dt * 2.2f; // gravity
        if (pt->z < 0.02f) { pt->z = 0.02f; pt->vz = 0; pt->vx *= 0.6f; pt->vy *= 0.6f; }
    }
    doors_update(dt);

    // slime damage
    if (!G.pl.dead) {
        Cell *c = cellAt((int)G.pl.x, (int)G.pl.y);
        if (c && (c->floor == FT_SLIME || c->floor == FT_SLIME2)) {
            static float slimeAcc = 0;
            slimeAcc += dt;
            if (slimeAcc > 0.6f) {
                slimeAcc = 0;
                player_damage(G.difficulty == 0 ? 3 : 5);
            }
        }
    }
}

// ------------------------------------------------------------------ cheats
bool player_cheat(const char *code) {
    Player *p = &G.pl;
    if (!strcmp(code, "iddqd")) {
        p->god = !p->god;
        game_msg(p->god ? "Degreelessness mode ON" : "Degreelessness mode OFF");
        return true;
    }
    if (!strcmp(code, "idkfa")) {
        for (int w = 0; w < WP_COUNT; w++) p->hasWeapon[w] = true;
        p->ammo[AM_BULLET] = p->maxAmmo[AM_BULLET];
        p->ammo[AM_SHELL] = p->maxAmmo[AM_SHELL];
        p->ammo[AM_ROCKET] = p->maxAmmo[AM_ROCKET];
        p->ammo[AM_CELL] = p->maxAmmo[AM_CELL];
        p->armor = 200; p->armorAbsorb = 0.5f;
        p->keys = KEY_RED | KEY_BLUE | KEY_YELLOW;
        game_msg("Very happy ammo added");
        return true;
    }
    if (!strcmp(code, "idclip")) {
        p->noclip = !p->noclip;
        game_msg(p->noclip ? "No clipping mode ON" : "No clipping mode OFF");
        return true;
    }
    return false;
}
