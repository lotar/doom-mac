// map.c — level building, doors, secrets, switches, world queries
#include "doom.h"

MapData *M(void) { return &G.map; }

#define MAX_SPAWNS 256
static MapSpawn g_spawns[MAX_SPAWNS];
static int g_numSpawns;

static inline Cell *cellp(int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= MAP_W || cy >= MAP_H) return NULL;
    return &M()->cells[cy * MAP_W + cx];
}
Cell *map_cell_at(int cx, int cy) { return cellp(cx, cy); }

static bool isDoorWall(int w) {
    return w == WT_DOOR || w == WT_DOOR_RED || w == WT_DOOR_BLUE ||
           w == WT_DOOR_YELLOW || w == WT_EXITDOOR || w == WT_SECRET;
}
static Door *doorOf(int cx, int cy) {
    int i = M()->doorIndex[cy * MAP_W + cx];
    return i >= 0 ? &M()->doors[i] : NULL;
}

bool map_walkable_cell(int cx, int cy) {
    Cell *c = cellp(cx, cy);
    if (!c) return false;
    if (c->wall == 0) return true;
    if (isDoorWall(c->wall)) {
        Door *d = doorOf(cx, cy);
        return d && d->open > 0.75f;
    }
    return false;
}
bool map_solid_world(float x, float y) { return !map_walkable_cell((int)x, (int)y); }
bool map_blocks_sight(float x, float y) {
    Cell *c = cellp((int)x, (int)y);
    if (!c) return true;
    if (c->wall == 0) return false;
    if (isDoorWall(c->wall)) {
        Door *d = doorOf((int)x, (int)y);
        return !(d && d->open > 0.85f);
    }
    return true;
}

bool world_los(float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.001f) return true;
    int steps = (int)(dist / 0.1f) + 1;
    float sx = dx / steps, sy = dy / steps;
    float x = x0, y = y0;
    for (int i = 0; i < steps; i++) {
        x += sx; y += sy;
        if (map_blocks_sight(x, y)) return false;
    }
    return true;
}

float world_ray_dist(float x, float y, float dx, float dy, float maxDist) {
    float step = 0.04f;
    float t = 0;
    while (t < maxDist) {
        t += step;
        if (map_blocks_sight(x + dx * t, y + dy * t)) return t;
    }
    return maxDist;
}

// ------------------------------------------------------------------ build
bool map_load(int levelIdx) {
    if (levelIdx < 0 || levelIdx >= NUM_LEVELS) return false;
    const LevelDef *L = &LEVELS[levelIdx];
    MapData *m = M();
    memset(m, 0, sizeof(*m));
    m->numDoors = 0;
    for (int i = 0; i < MAP_W * MAP_H; i++) m->doorIndex[i] = -1;

    // base: solid tech
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            Cell *c = &m->cells[y * MAP_W + x];
            c->wall = WT_TECH;
            c->floor = FT_STONE;
            c->ceil = CE_CONC;
            c->light = 0;
        }

    // carve rooms in order
    for (int ri = 0; ri < L->numRooms; ri++) {
        const RoomDef *r = &L->rooms[ri];
        for (int y = r->y0; y <= r->y1; y++)
            for (int x = r->x0; x <= r->x1; x++) {
                if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
                Cell *c = &m->cells[y * MAP_W + x];
                c->wall = 0;
                c->floor = r->floor;
                c->ceil = r->ceil;
                c->light = r->light;
            }
    }

    // wall overrides
    for (int wi = 0; wi < L->numWalls; wi++) {
        const WallRectDef *w = &L->walls[wi];
        for (int y = w->y0; y <= w->y1; y++)
            for (int x = w->x0; x <= w->x1; x++) {
                if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
                Cell *c = &m->cells[y * MAP_W + x];
                if (!w->force && c->wall == 0) continue;
                c->wall = w->wall;
            }
    }

    // switches
    m->switchCount = L->numSwitches;
    for (int si = 0; si < L->numSwitches; si++) {
        Cell *c = cellp(L->switches[si].x, L->switches[si].y);
        if (c) c->wall = WT_SWITCH;
    }

    // doors
    for (int di = 0; di < L->numDoors; di++) {
        const DoorDef *dd = &L->doors[di];
        int cx = dd->x, cy = dd->y;
        Cell *c = cellp(cx, cy);
        if (!c) continue;
        Door *d = &m->doors[m->numDoors];
        memset(d, 0, sizeof(*d));
        d->cx = cx; d->cy = cy;
        d->open = 0; d->state = 0;
        d->locked = dd->kind;             // 1 red 2 blue 3 yellow
        d->isExit = (dd->kind == 4);
        d->isSecret = (dd->kind == 5);
        d->secretFound = false;
        // underlying texture for secrets: most common adjacent solid wall
        int counts[WT_MAX] = {0};
        const int nx[4] = {1, -1, 0, 0}, ny[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            Cell *nc = cellp(cx + nx[k], cy + ny[k]);
            if (nc && nc->wall != 0 && !isDoorWall(nc->wall)) counts[nc->wall]++;
        }
        int best = WT_TECH, bc = 0;
        for (int w = 1; w < WT_MAX; w++)
            if (counts[w] > bc) { bc = counts[w]; best = w; }
        d->underlyingWall = best;
        // axis: passage direction
        bool ew = map_walkable_cell(cx - 1, cy) && map_walkable_cell(cx + 1, cy);
        bool ns = map_walkable_cell(cx, cy - 1) && map_walkable_cell(cx, cy + 1);
        d->axis = ew ? 1 : 0;
        (void)ns;
        // wall type on the cell
        switch (dd->kind) {
        case 1: c->wall = WT_DOOR_RED; break;
        case 2: c->wall = WT_DOOR_BLUE; break;
        case 3: c->wall = WT_DOOR_YELLOW; break;
        case 4: c->wall = WT_EXITDOOR; break;
        case 5: c->wall = WT_SECRET; break;
        default: c->wall = WT_DOOR; break;
        }
        // doors inherit the brightest neighbouring light (they sit in walls
        // which are unlit by default)
        {
            const int nlx[4] = {1, -1, 0, 0}, nly[4] = {0, 0, 1, -1};
            int best = 0;
            for (int k = 0; k < 4; k++) {
                Cell *nc = cellp(cx + nlx[k], cy + nly[k]);
                if (nc && nc->wall == 0 && nc->light > best) best = nc->light;
            }
            c->light = best ? best : 170;
        }
        m->doorIndex[cy * MAP_W + cx] = m->numDoors;
        m->numDoors++;
        if (d->isSecret) m->totalSecrets++;
    }

    // light the walls: every solid cell takes the brightest neighbouring
    // floor light so faces render at room brightness
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            Cell *c = &m->cells[y * MAP_W + x];
            if (c->wall == 0 || c->light > 0) continue;
            const int nlx[4] = {1, -1, 0, 0}, nly[4] = {0, 0, 1, -1};
            int best = 0;
            for (int k = 0; k < 4; k++) {
                Cell *nc = cellp(x + nlx[k], y + nly[k]);
                if (nc && nc->wall == 0 && nc->light > best) best = nc->light;
            }
            c->light = best;
        }

    // exit pad & teleporters
    if (L->hasExitPad) {
        Cell *c = cellp(L->exitPad.x, L->exitPad.y);
        if (c) { c->wall = 0; c->floor = FT_EXITPAD; c->light = 230; }
    }
    m->hasTp = false;

    // things
    g_numSpawns = 0;
    m->totalMonsters = 0;
    m->totalItems = 0;
    for (int ti = 0; ti < L->numThings; ti++) {
        const ThingDef *t = &L->things[ti];
        if (g_numSpawns < MAX_SPAWNS) {
            g_spawns[g_numSpawns].x = t->x + 0.5f;
            g_spawns[g_numSpawns].y = t->y + 0.5f;
            g_spawns[g_numSpawns].code = t->thing;
            g_numSpawns++;
        }
        if (t->thing >= 0) m->totalItems++;
        else if (t->thing >= TH_BARON) m->totalMonsters++;
    }

    snprintf(m->name, sizeof(m->name), "%s", L->name);
    m->parSeconds = L->parSeconds;
    return true;
}

const MapSpawn *map_spawn_list(int *count) {
    *count = g_numSpawns;
    return g_spawns;
}

// ------------------------------------------------------------------ doors
static void door_open_req(Door *d) {
    if (d->state == 0 || d->state == 3) {
        d->state = 1;
        sfx_play_at(d->isSecret ? SX_SECRET : SX_DOOR_OPEN,
                    d->cx + 0.5f, d->cy + 0.5f);
    }
    d->timer = 0;
}

void map_monster_open(int cx, int cy) {
    Door *d = doorOf(cx, cy);
    if (!d || d->locked || d->isSecret) return;
    door_open_req(d);
    d->busy = true;
}

int map_use_target(float px, float py, float ang) {
    float dx = cosf(ang), dy = sinf(ang);
    for (int probe = 0; probe < 2; probe++) {
        float dist = probe == 0 ? 0.9f : 1.5f;
        int cx = (int)(px + dx * dist), cy = (int)(py + dy * dist);
        Cell *c = cellp(cx, cy);
        if (!c) continue;
        if (c->wall == WT_SWITCH) {
            c->wall = WT_SWITCH_ON;
            // unlock & open all exit doors permanently
            for (int i = 0; i < M()->numDoors; i++) {
                Door *d = &M()->doors[i];
                if (d->isExit) {
                    d->locked = 0;
                    door_open_req(d);
                    d->state = 1;
                    d->timer = -9999; // never closes
                }
            }
            sfx_play(SX_SWITCH, 1.0f, 0);
            return USE_SWITCH;
        }
        if (isDoorWall(c->wall)) {
            Door *d = doorOf(cx, cy);
            if (!d) return USE_NONE;
            if (d->isSecret) {
                if (!d->secretFound) {
                    d->secretFound = true;
                    G.secretCount++;
                    door_open_req(d);
                    d->state = 1;
                    d->timer = -9999;
                    return USE_SECRET_FOUND;
                }
                return USE_NONE;
            }
            if (d->state == 1 || d->state == 2) return USE_NONE;
            if (d->isExit && d->locked) {
                // sealed until the level switch is flipped
                sfx_play(SX_DOOR_LOCKED, 1.0f, 0);
                return USE_LOCKED + 4;
            }
            if (d->locked) {
                int keyBit =
                    d->locked == 1 ? KEY_RED : d->locked == 2 ? KEY_BLUE : KEY_YELLOW;
                if (!(G.pl.keys & keyBit)) {
                    sfx_play(SX_DOOR_LOCKED, 1.0f, 0);
                    return USE_LOCKED + d->locked; // encode which
                }
            }
            door_open_req(d);
            return USE_DOOR;
        }
    }
    return USE_NONE;
}

void doors_update(float dt) {
    MapData *m = M();
    for (int i = 0; i < m->numDoors; i++) {
        Door *d = &m->doors[i];
        if (d->state == 1) { // opening
            d->open += dt * 1.4f;
            if (d->open >= 1) { d->open = 1; d->state = 2; d->timer = 0; }
        } else if (d->state == 2) { // open, waiting
            d->timer += dt;
            if (d->timer > 4.0f && d->timer > 0) {
                // don't close if something stands inside
                bool occupied = false;
                for (int e = 0; e < G.numEnts; e++) {
                    Ent *en = &G.ents[e];
                    if (en->active && en->blocking &&
                        (int)en->x == d->cx && (int)en->y == d->cy) occupied = true;
                }
                if ((int)G.pl.x == d->cx && (int)G.pl.y == d->cy) occupied = true;
                if (!occupied) {
                    d->state = 3;
                    sfx_play_at(SX_DOOR_CLOSE, d->cx + 0.5f, d->cy + 0.5f);
                } else {
                    d->timer = 0;
                }
            }
        } else if (d->state == 3) { // closing
            d->open -= dt * 1.4f;
            if (d->open <= 0) { d->open = 0; d->state = 0; }
        }
    }
}
