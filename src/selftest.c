// selftest.c — headless verification of engine subsystems
#include "doom.h"

static int fails = 0;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL: %s (line %d)\n", msg, __LINE__);                     \
            fails++;                                                           \
        }                                                                      \
    } while (0)

// ---- flood fill reachability -----------------------------------------
static bool reach[MAP_W * MAP_H];
static void flood(int cx, int cy) {
    if (cx < 1 || cy < 1 || cx >= MAP_W - 1 || cy >= MAP_H - 1) return;
    int idx = cy * MAP_W + cx;
    if (reach[idx]) return;
    Cell *c = map_cell_at(cx, cy);
    if (!c) return;
    // floors plus all door kinds count as passable (they can be opened)
    bool passable =
        c->wall == 0 || c->wall == WT_DOOR || c->wall == WT_DOOR_RED ||
        c->wall == WT_DOOR_BLUE || c->wall == WT_DOOR_YELLOW ||
        c->wall == WT_EXITDOOR || c->wall == WT_SECRET;
    if (!passable) return;
    reach[idx] = true;
    const int nx[4] = {1, -1, 0, 0}, ny[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) flood(cx + nx[k], cy + ny[k]);
}

static void test_level(int li) {
    char tag[80];
    snprintf(tag, sizeof(tag), "[L%d %s]", li + 1, LEVELS[li].name);

    MapData *m = &G.map;
    const LevelDef *L = &LEVELS[li];
    memset(reach, 0, sizeof(reach));
    flood(L->playerStart.x, L->playerStart.y);
    Cell *pc = map_cell_at(L->playerStart.x, L->playerStart.y);
    CHECK(pc && pc->wall == 0, "player start open");
    if (!pc || pc->wall != 0) return;

    // exit pad reachable
    if (L->hasExitPad) {
        CHECK(reach[L->exitPad.y * MAP_W + L->exitPad.x], "exit pad reach");
    }
    // every spawn on reachable ground
    int n = 0;
    const MapSpawn *sp = map_spawn_list(&n);
    for (int i = 0; i < n; i++) {
        int cx = (int)sp[i].x, cy = (int)sp[i].y;
        Cell *c = map_cell_at(cx, cy);
        char msg[96];
        snprintf(msg, sizeof(msg), "%s thing (%d,%d) code %d on reachable floor",
                 tag, cx, cy, sp[i].code);
        CHECK(c && c->wall == 0 && reach[cy * MAP_W + cx], msg);
    }
    // door flanks
    for (int i = 0; i < m->numDoors; i++) {
        Door *d = &m->doors[i];
        bool ok;
        if (d->axis == 1)
            ok = !map_walkable_cell(d->cx, d->cy - 1) &&
                 !map_walkable_cell(d->cx, d->cy + 1);
        else
            ok = !map_walkable_cell(d->cx - 1, d->cy) &&
                 !map_walkable_cell(d->cx + 1, d->cy);
        char msg[96];
        snprintf(msg, sizeof(msg), "%s door %d at (%d,%d) axis %d flanked", tag,
                 i, d->cx, d->cy, d->axis);
        CHECK(ok, msg);
    }
    // switch adjacent to floor
    for (int s = 0; s < L->numSwitches; s++) {
        bool adj = false;
        const int nx[4] = {1, -1, 0, 0}, ny[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            Cell *c = map_cell_at(L->switches[s].x + nx[k], L->switches[s].y + ny[k]);
            if (c && c->wall == 0) adj = true;
        }
        CHECK(adj, "switch floor");
    }
}

static void test_simulation(void) {
    start_level(0);
    G.mode = GM_PLAY;
    Input t = {0};
    t.fwd = true;
    t.fire = true;
    float px0 = G.pl.x, py0 = G.pl.y;
    for (int i = 0; i < 600; i++) {
        t.mouseDX = (i % 120 < 60) ? 30 : -30;
        sim_step(&t, DT);
        CHECK(!map_solid_world(G.pl.x, G.pl.y), "player inside solid");
        if (G.mode != GM_PLAY) break; // may have died/finished, that's fine
    }
    CHECK(G.time > 5.0f, "time advanced");
    (void)px0; (void)py0;
}

static void test_combat(void) {
    start_level(0);
    G.mode = GM_PLAY;
    // stand player somewhere clear, spawn trooper ahead
    G.pl.x = 14.5f; G.pl.y = 4.5f; G.pl.ang = 0;
    Ent *e = ent_spawn(ET_TROOPER, 17.5f, 4.5f);
    e->alerted = true;
    e->state = MS_CHASE;
    int kills0 = G.killCount;
    for (int i = 0; i < 400 && G.killCount == kills0; i++) {
        Input t = {0};
        t.fire = true;
        sim_step(&t, DT);
    }
    CHECK(G.killCount == kills0 + 1, "trooper killed by pistol");
    bool dropped = false;
    for (int i = 0; i < MAX_ENTS; i++)
        if (G.ents[i].active && G.ents[i].type == ET_ITEM &&
            G.ents[i].itemKind == IT_CLIP && fabsf(G.ents[i].x - 17.5f) < 1)
            dropped = true;
    CHECK(dropped, "clip dropped");

    // explosion damages nearby monster
    start_level(0);
    G.mode = GM_PLAY;
    Ent *e2 = ent_spawn(ET_IMP, G.pl.x + 1.5f, G.pl.y);
    int hp0 = e2->hp;
    spawn_explosion(e2->x, e2->y, 60, 2.5f);
    CHECK(e2->hp < hp0 || e2->state >= MS_DYING, "explosion hurt imp");
}

static void test_doors(void) {
    start_level(0);
    G.mode = GM_PLAY;
    // find a normal unlocked door
    Door *target = NULL;
    for (int i = 0; i < G.map.numDoors; i++) {
        Door *d = &G.map.doors[i];
        if (d->locked == 0 && !d->isSecret && !d->isExit) { target = d; break; }
    }
    CHECK(target != NULL, "found plain door");
    if (target) {
        // place player next to it facing it
        int cx = target->cx, cy = target->cy;
        if (target->axis == 1) { G.pl.x = cx - 0.7f; G.pl.y = cy + 0.5f; G.pl.ang = 0; }
        else { G.pl.x = cx + 0.5f; G.pl.y = cy - 0.7f; G.pl.ang = 1.5708f; }
        bool opened = false;
        for (int i = 0; i < 200; i++) {
            Input t = {0};
            t.use = true;
            sim_step(&t, DT);
            if (target->open > 0.5f) { opened = true; break; }
        }
        CHECK(opened, "door opens on use");
    }
    // locked door without key refuses
    start_level(1);
    G.mode = GM_PLAY;
    Door *locked = NULL;
    for (int i = 0; i < G.map.numDoors; i++) {
        Door *d = &G.map.doors[i];
        if (d->locked) { locked = d; break; }
    }
    CHECK(locked != NULL, "level 2 has a locked door");
    if (locked) {
        G.pl.keys = 0;
        G.pl.x = locked->cx - 0.7f;
        G.pl.y = locked->cy + 0.5f;
        G.pl.ang = 0;
        float o0 = locked->open;
        for (int i = 0; i < 10; i++) {
            Input t = {0};
            t.use = true;
            sim_step(&t, DT);
        }
        CHECK(locked->open <= o0 + 0.01f, "locked door stays shut");
        // now grant the right key
        G.pl.keys = locked->locked == 1 ? KEY_RED : locked->locked == 2 ? KEY_BLUE : KEY_YELLOW;
        bool opened = false;
        for (int i = 0; i < 200; i++) {
            Input t = {0};
            t.use = true;
            sim_step(&t, DT);
            if (locked->open > 0.5f) { opened = true; break; }
        }
        CHECK(opened, "locked door opens with key");
    }
}

static void test_pickups(void) {
    start_level(0);
    G.mode = GM_PLAY;
    G.pl.hp = 50;
    ent_spawn_item(IT_MEDKIT, G.pl.x + 0.2f, G.pl.y);
    int items0 = G.itemCount;
    Input t = {0};
    sim_step(&t, DT);
    CHECK(G.pl.hp == 75, "medikit heals 25");
    CHECK(G.itemCount == items0 + 1, "item counted");

    // ammo cap respected
    G.pl.ammo[AM_BULLET] = G.pl.maxAmmo[AM_BULLET];
    ent_spawn_item(IT_CLIP, G.pl.x + 0.2f, G.pl.y);
    sim_step(&t, DT);
    CHECK(G.pl.ammo[AM_BULLET] == G.pl.maxAmmo[AM_BULLET], "ammo capped");

    // cheat
    player_cheat("idkfa");
    CHECK(G.pl.hasWeapon[WP_PLASMA] && G.pl.keys == 7, "idkfa grants arsenal");
}

static void test_render(void) {
    memset(FB, 0, sizeof(FB));
    render_frame(); // title
    GameMode modes[] = {GM_TITLE, GM_DIFFSEL, GM_INSTRUCTIONS, GM_PLAY,
                        GM_INTERMISSION, GM_VICTORY};
    for (int i = 0; i < 6; i++) {
        G.mode = modes[i];
        if (modes[i] == GM_PLAY) start_level(2);
        if (modes[i] == GM_INTERMISSION) map_load(1);
        render_frame();
        bool any = false;
        for (int k = 0; k < FB_W * FB_H; k += 997)
            if (FB[k]) { any = true; break; }
        CHECK(any, "render produced pixels");
    }
    write_bmp("/tmp/doom_selftest.bmp", FB, FB_W, FB_H);
    FILE *f = fopen("/tmp/doom_selftest.bmp", "rb");
    CHECK(f != NULL, "bmp written");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        CHECK(sz > 54 * 10, "bmp has content");
    }
    G.mode = GM_TITLE;
}

int selftest_run(void) {
    printf("=== doom-mac selftest ===\n");
    rng_seed(12345);

    for (int i = 0; i < NUM_LEVELS; i++) test_level(i);
    printf("levels: done\n");

    audio_init(); // dummy driver in tests; must not crash either way
    test_simulation();
    printf("simulation: done\n");
    test_combat();
    printf("combat: done\n");
    test_doors();
    printf("doors: done\n");
    test_pickups();
    printf("pickups: done\n");
    test_render();
    printf("render: done\n");

    if (fails == 0) printf("ALL TESTS PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
