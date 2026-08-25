// doom.h — shared types, constants and prototypes for DOOM-Mac
// An original Doom-style FPS engine written in C. No id Software code or assets.
#ifndef DOOM_H
#define DOOM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define VERSION_STR "1.0.0"

// ---------------------------------------------------------------- screen
#define FB_W   640          // internal framebuffer width
#define FB_H   400          // internal framebuffer height (view 320 + hud 80)
#define VIEW_H 320
#define HUD_H  80
#define TICK_RATE 60
#define DT (1.0f / TICK_RATE)

// ---------------------------------------------------------------- util
void  fatal(const char *fmt, ...);
void *xmalloc(size_t n);

// rng (xorshift32, deterministic when seeded)
void   rng_seed(uint32_t s);
uint32_t rng_u32(void);
float  rng_f(void);            // 0..1
int    rng_irange(int lo, int hi); // inclusive

#define col_rgb(r, g, b) \
    (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
static inline uint32_t col_shade(uint32_t c, float f) {
    if (f > 1.0f) f = 1.0f;
    if (f < 0.0f) f = 0.0f;
    uint32_t r = (uint32_t)(((c >> 16) & 0xFF) * f);
    uint32_t g = (uint32_t)(((c >> 8) & 0xFF) * f);
    uint32_t b = (uint32_t)((c & 0xFF) * f);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// ---------------------------------------------------------------- game modes
typedef enum {
    GM_TITLE = 0,
    GM_DIFFSEL,
    GM_INSTRUCTIONS,
    GM_PLAY,
    GM_INTERMISSION,
    GM_VICTORY,
} GameMode;

// ---------------------------------------------------------------- map
#define MAP_W 64
#define MAP_H 64

enum WallTypes {
    WT_NONE = 0,
    WT_TECH,     // '#'
    WT_BRICK,    // 'B'
    WT_ROCK,     // 'N'
    WT_METAL,    // 'M'
    WT_MARBLE,   // 'G'
    WT_COMP,     // 'C' animated computer panel
    WT_DOOR,     // 'D'
    WT_DOOR_RED,    // 'R'
    WT_DOOR_BLUE,   // 'U'
    WT_DOOR_YELLOW, // 'Y'
    WT_EXITDOOR,    // 'X'
    WT_SECRET,      // 'S'
    WT_SWITCH,      // 'V'
    WT_SWITCH_ON,   // (runtime state)
    WT_MAX
};

// flats (floors / ceilings)
enum FlatTex {
    FT_TECH = 0, FT_STONE, FT_DIRT, FT_SLIME, FT_SLIME2, FT_PAD, FT_EXITPAD,
    CE_CONC, CE_LIGHT,
    TX_SKY = 100   // ceiling marker: open sky
};

typedef struct Cell {
    uint8_t wall;    // enum WallTypes, 0 = open
    uint8_t floor;   // enum FlatTex
    uint8_t ceil;    // enum FlatTex or TX_SKY
    uint8_t light;   // 0..255
} Cell;

typedef struct Door {
    int cx, cy;
    float open;        // 0 closed .. 1 fully open
    int state;         // 0 closed, 1 opening, 2 open(timer), 3 closing
    float timer;
    int axis;          // 0: passage along y (door spans x), 1: passage along x
    int locked;        // 0 none, 1 red, 2 blue, 3 yellow
    bool isExit;       // opened by its key or switch
    bool isSecret;     // secret pushwall (opens once, counts a secret)
    bool secretFound;
    int underlyingWall;// texture shown when secret (matches neighbours)
    bool busy;         // monster holds it open
} Door;

typedef struct Teleport { float sx, sy, dx, dy; bool used; } Teleport;

typedef struct { float x, y; int code; } MapSpawn;

typedef struct MapData {
    Cell cells[MAP_W * MAP_H];
    bool seen[MAP_W * MAP_H];      // automap explored
    Door doors[192];
    int  numDoors;
    int  doorIndex[MAP_W * MAP_H]; // -1 none
    Teleport tp;
    bool hasTp;
    int  switchCount;
    char name[64];
    int  parSeconds;
    int  totalMonsters, totalItems, totalSecrets;
} MapData;

// levels.c — declarative map format
#define MAX_ROOMS 24
#define MAX_WALLRECTS 28
#define MAX_DOORS 12
#define MAX_THINGS 110
typedef struct { int x0, y0, x1, y1; uint8_t floor, ceil, light; } RoomDef;
typedef struct { int x0, y0, x1, y1; uint8_t wall; bool force; } WallRectDef;
// force=false: only re-type cells that are already solid walls (ring decoration)
// force=true:  place walls even over floor (pillars, blocks)
typedef struct { int x, y, kind; } DoorDef;   // kind 0 norm 1 red 2 blue 3 yellow 4 exit 5 secret
typedef struct { int x, y; } CoordDef;
enum ThingCodes { // negative = monsters/props, >=0 = ItemKind
    TH_TROOPER = -1, TH_IMP = -2, TH_PINKY = -3, TH_BARON = -4,
    TH_BARREL = -5, TH_LAMP = -6,
};
typedef struct { int x, y, thing; } ThingDef;
typedef struct LevelDef {
    const char *name;
    int parSeconds;
    CoordDef playerStart;
    RoomDef rooms[MAX_ROOMS];        int numRooms;
    WallRectDef walls[MAX_WALLRECTS]; int numWalls;
    DoorDef doors[MAX_DOORS];        int numDoors;
    CoordDef switches[4];            int numSwitches;
    CoordDef exitPad;                bool hasExitPad;
    CoordDef tpSrc, tpDst;           bool hasTp;
    ThingDef things[MAX_THINGS];     int numThings;
} LevelDef;
extern const LevelDef LEVELS[];
extern const int NUM_LEVELS;

// map.c
bool map_load(int levelIdx);
Cell *map_cell_at(int cx, int cy);
const MapSpawn *map_spawn_list(int *count);
bool map_walkable_cell(int cx, int cy);
bool map_solid_world(float x, float y);           // for movement (doors count)
bool map_blocks_sight(float x, float y);          // for LOS (closed doors block)
bool world_los(float x0, float y0, float x1, float y1);
float world_ray_dist(float x, float y, float dx, float dy, float maxDist);
void doors_update(float dt);
void map_monster_open(int cx, int cy);
enum { USE_NONE = 0, USE_DOOR, USE_LOCKED, USE_SECRET_FOUND, USE_SWITCH };
int  map_use_target(float px, float py, float ang); // hitscan vs walls/doors

// ---------------------------------------------------------------- entities
enum EType {
    ET_NONE = 0,
    ET_TROOPER, ET_IMP, ET_PINKY, ET_BARON,
    ET_BARREL, ET_LAMP,
    ET_ITEM,
    ET_PROJ,
};

enum ItemKind {
    IT_STIM, IT_MEDKIT, IT_ARMOR, IT_MEGAARMOR, IT_POTION, IT_BACKPACK,
    IT_CLIP, IT_AMMOBOX, IT_SHELLS, IT_SHELLBOX, IT_ROCKETS, IT_CELL,
    IT_GUN_SHOTGUN, IT_GUN_CHAINGUN, IT_GUN_ROCKET, IT_GUN_PLASMA,
    IT_KEY_RED, IT_KEY_BLUE, IT_KEY_YELLOW,
    IT_MAX
};

enum ProjKind { PR_IMPBALL, PR_BARONBALL, PR_PLASMA, PR_ROCKET };

enum MonsterState {
    MS_SLEEP = 0, MS_CHASE, MS_WINDUP, MS_ATTACK, MS_PAIN, MS_DYING, MS_DEAD
};

#define MAX_ENTS 384
#define MAX_PROJ 96
#define MAX_PARTICLES 2048

typedef struct Ent {
    bool active;
    int  type;            // enum EType
    float x, y;
    float radius;
    float vx, vy;
    int   hp, maxHp;
    int   state;          // enum MonsterState
    float stateT;         // time in state
    int   frame;          // anim frame selector
    float animT;
    float thinkT;
    bool  alerted;
    float painCool;
    int   itemKind;       // ET_ITEM
    int   projKind;       // ET_PROJ
    float dmg;
    float speed;
    float age;
    int   dyingFrame;
    bool  blocking;
    int   dropItem;       // item dropped on death (-1 none)
} Ent;

typedef struct Projectile {
    bool active;
    int  kind;
    float x, y, dx, dy, speed, dmg;
    float age;
    bool  enemyOwned;
} Projectile;

typedef struct Particle {
    bool active;
    float x, y, z, vx, vy, vz;
    float life, maxLife;
    uint32_t color;
    float size;
} Particle;

// ---------------------------------------------------------------- player & weapons
enum Weapons { WP_PISTOL = 0, WP_SHOTGUN, WP_CHAINGUN, WP_ROCKET, WP_PLASMA, WP_COUNT };
enum Ammo { AM_BULLET = 0, AM_SHELL, AM_ROCKET, AM_CELL, AM_MAX };

typedef struct Player {
    float x, y;
    float ang;             // radians
    float vx, vy;
    int   hp;
    int   armor;
    float armorAbsorb;     // 0..1
    bool  hasWeapon[WP_COUNT];
    int   weapon;          // current
    int   nextWeapon;      // requested switch (-1 none)
    float raiseLower;      // 0 up .. 1 lowered
    int   ammo[AM_MAX];
    int   maxAmmo[AM_MAX];
    int   keys;            // bit 1 red, 2 blue, 4 yellow
    float fireCool;
    float bobPhase, bobAmt;
    float refire;
    float damageFlash, pickupFlash, teleportFlash;
    float faceGrinT, faceOuchT;
    int   faceHealth;      // cached bracket for redraw
    float shakeT, shakeAmt;
    bool  god;
    bool  noclip;
    bool  dead;
    float deadT;
    int   weaponFiring;    // frames remaining of fire animation
    float muzzleFlash;
} Player;

#define KEY_RED 1
#define KEY_BLUE 2
#define KEY_YELLOW 4

// ---------------------------------------------------------------- game
typedef struct Game {
    GameMode mode;
    int   level;           // current level index
    int   difficulty;      // 0 easy 1 medium 2 hard
    MapData map;
    Player pl;
    Ent   ents[MAX_ENTS];
    Projectile projs[MAX_PROJ];
    Particle parts[MAX_PARTICLES];
    int   numEnts;
    float time;            // level time
    int   killCount, itemCount, secretCount;
    float msgTimer;
    char  msg[128];
    char  cheatBuf[16];
    bool  automap;
    int   titleSel;
    int   diffSel;
    float interTally[3];   // animated percentages
    float victoryScroll;
    bool  paused;
    float titleFire[FB_W]; // fire intensity seeds
    float globalLightBoost;// muzzle flash lights world briefly
    bool  quitRequested;
    uint32_t frameSeed;
} Game;

extern Game G;

// ---------------------------------------------------------------- ent.c
void ents_reset(void);
void ents_spawn_level_things(void);
void ents_update(float dt);
void player_reset_for_level(void);
void player_update(float dt);
void player_give_default_kit(bool keepGear);
void player_damage(int amount);
bool player_use_action(void);                 // space/E: doors, switches, secrets
int  kindToWeapon(int itemKind);
bool player_cheat(const char *code);
extern bool g_levelDone;
void start_level(int idx);
int  selftest_run(void);
void fire_hitscan(float ang, int dmgLo, int dmgHi, float maxDist, bool spark);
void fire_projectile(int kind, float x, float y, float dx, float dy, float speed, float dmg, bool enemy);
void spawn_explosion(float x, float y, float dmg, float radius);
void part_blood(float x, float y, int n);
void part_spark(float x, float y, int n);
void part_explosion_fx(float x, float y);
void game_msg(const char *fmt, ...);
Ent *ent_spawn(int type, float x, float y);
Ent *ent_spawn_item(int kind, float x, float y);

// ---------------------------------------------------------------- render.c
extern uint32_t FB[FB_W * FB_H];
void render_init(void);
void render_frame(void);
void render_title_fire(float dt);
void draw_face_hud(int x, int y);
void write_bmp(const char *path, const uint32_t *pix, int w, int h);

// ---------------------------------------------------------------- textures.c
typedef struct { int w, h; uint32_t *px; } Tex;
enum TexIds {
    TX_WALL_TECH = 0, TX_WALL_BRICK, TX_WALL_ROCK, TX_WALL_METAL, TX_WALL_MARBLE,
    TX_WALL_COMP, TX_WALL_COMP2,
    TX_DOOR, TX_DOOR_R, TX_DOOR_B, TX_DOOR_Y, TX_EXITDOOR,
    TX_SWITCH_OFF, TX_SWITCH_ON,
    TX_FL_TECH, TX_FL_STONE, TX_FL_DIRT, TX_FL_SLIME, TX_FL_SLIME2,
    TX_FL_PAD, TX_FL_EXITPAD, TX_CE_CONC, TX_CE_LIGHT,
    TX_SPR_FIRST = 32
};
// sprite ids (offset from TX_SPR_FIRST)
enum SpriteIds {
    SPR_TROOPER = 32,   // +0..7  stand,walkA,walkB,shoot,pain,die1..3
    SPR_IMP = 40,       // +0..6  idleA,idleB,throw,pain,die1..3
    SPR_PINKY = 48,     // +0..6  walkA,walkB,bite,pain,die1..3
    SPR_BARON = 56,     // +0..7  walkA,walkB,cast,pain,die1..4
    SPR_BARREL = 64,
    SPR_LAMP = 65,
    SPR_EXPLO0 = 66, SPR_EXPLO1 = 67, SPR_EXPLO2 = 68,
    SPR_PUFF0 = 69, SPR_PUFF1 = 70, SPR_PUFF2 = 71,
    SPR_BLOOD0 = 72, SPR_BLOOD1 = 73,
    SPR_FIREBALL = 74, SPR_FIREBALL2 = 75, SPR_BARONBALL = 76,
    SPR_PLASMABALL = 77, SPR_PLASMA2 = 78, SPR_ROCKETPROJ = 79,
    SPR_IT_STIM = 80, SPR_IT_MEDKIT, SPR_IT_ARMOR, SPR_IT_MEGAARMOR,
    SPR_IT_POTION, SPR_IT_BACKPACK, SPR_IT_CLIP, SPR_IT_AMMOBOX,
    SPR_IT_SHELLS, SPR_IT_SHELLBOX, SPR_IT_ROCKETS, SPR_IT_CELL,
    SPR_IT_GUN_SHOTGUN, SPR_IT_GUN_CHAINGUN, SPR_IT_GUN_ROCKET, SPR_IT_GUN_PLASMA,
    SPR_KEY_RED, SPR_KEY_BLUE, SPR_KEY_YELLOW,
    SPR_MAX
};

void tex_init(void);
Tex *tex_get(int id);
Tex *tex_get_anim(int id, float time);   // animated textures (slime/computer)

// screen-space sprite blit (nearest neighbour), returns width used
void blit_sprite(Tex *t, int x, int y, int scale);
// font
void font_draw_str(int x, int y, const char *s, uint32_t color, int scale);
int  font_measure(const char *s, int scale);
// weapon painter: draws weapon sprite into fb at bottom-center with offset & flash
void paint_weapon(int weapon, int frameState, float bobX, float bobY, float flash);

// ---------------------------------------------------------------- audio.c
enum SfxIds {
    SX_PISTOL = 0, SX_SHOTGUN, SX_CHAINGUN, SX_ROCKET, SX_EXPLOSION, SX_PLASMA,
    SX_DOOR_OPEN, SX_DOOR_CLOSE, SX_DOOR_LOCKED,
    SX_PICKUP, SX_PICKUP_WEAPON, SX_PICKUP_KEY, SX_SECRET,
    SX_SWITCH, SX_PLAYER_PAIN, SX_PLAYER_DIE, SX_IMP_SIGHT, SX_IMP_THROW,
    SX_IMP_DIE, SX_TROOPER_SIGHT, SX_TROOPER_DIE, SX_PINKY_SIGHT, SX_PINKY_BITE,
    SX_PINKY_DIE, SX_BARON_SIGHT, SX_BARON_DIE, SX_FIREBALL_HIT, SX_BARREL_EXP,
    SX_TELEPORT, SX_MENU_MOVE, SX_MENU_SELECT, SX_EMPTY, SX_STIM_NO,
    SX_COUNT
};
bool audio_init(void);
void audio_quit(void);
void sfx_play(int id, float gain, float pan);       // pan -1..1
void sfx_play_at(int id, float x, float y);         // positional vs player
void music_play(int track);                          // 0 none, 1 game, 2 title
void audio_toggle_mute(void);

// ---------------------------------------------------------------- main.c / app
typedef struct Input {
    bool fwd, back, turnLeft, turnRight, strafeLeft, strafeRight;
    bool fire, use, run;
    bool weaponNext, weaponPrev;
    int  selectWeapon;   // -1 none else slot#
    float mouseDX;
    bool automapToggle;
    bool pauseToggle;
    bool confirm;        // menu confirm (enter/fire)
    bool menuUp, menuDown, menuLeft, menuRight;
    bool quit;
    bool muteToggle;
    char keyChar;          // printable char captured this frame (cheats/menu)
} Input;
extern Input IN;
void sim_step(Input *in, float dt);

int  app_main(int argc, char **argv);   // real-time loop
void sim_step(Input *in, float dt);     // one deterministic simulation tick (also used by tests)
void take_screenshot(const char *path); // dumps FB to BMP

#endif // DOOM_H
