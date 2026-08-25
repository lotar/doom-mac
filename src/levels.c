// levels.c — the five maps, as declarative data.
// Rooms are carved in order (later rooms overwrite earlier floors).
// Wall rects decorate solid borders (force=false) or place pillars (force=true).
// Doors are punched last and need solid cells on their two flanks.
#include "doom.h"

#define R(x0, y0, x1, y1, fl, ce, li) {(x0), (y0), (x1), (y1), (fl), (ce), (li)}
#define W(x0, y0, x1, y1, wa, fo) {(x0), (y0), (x1), (y1), (wa), (fo)}
#define D(x, y, k) {(x), (y), (k)}
#define T(x, y, t) {(x), (y), (t)}

// ------------------------------------------------------------------ E1M1
static const LevelDef LVL1 = {
    .name = "Entry Hangar",
    .parSeconds = 90,
    .playerStart = {4, 4},
    .rooms = {
        R(2, 2, 8, 7, FT_TECH, CE_CONC, 200),      // start room
        R(10, 3, 18, 6, FT_TECH, CE_CONC, 150),    // north corridor
        R(20, 2, 29, 9, FT_TECH, CE_LIGHT, 210),   // computer room
        R(31, 2, 44, 12, FT_DIRT, TX_SKY, 225),    // courtyard (outdoor)
        R(2, 9, 8, 20, FT_STONE, CE_CONC, 130),    // west hall
        R(10, 18, 29, 19, FT_STONE, CE_CONC, 120), // lower corridor
        R(30, 15, 43, 25, FT_STONE, CE_CONC, 150), // south hall
        R(45, 20, 46, 23, FT_TECH, CE_LIGHT, 200), // exit closet
        R(2, 22, 5, 24, FT_TECH, CE_CONC, 170),    // secret closet
    },
    .numRooms = 9,
    .walls = {
        W(20, 0, 29, 1, WT_COMP, false),     // computer strip
        W(30, 1, 45, 13, WT_ROCK, false),    // courtyard rock ring
        W(30, 26, 43, 33, WT_BRICK, false),  // south hall brick wall
        W(44, 19, 44, 25, WT_METAL, false),  // exit closet walls
        W(45, 19, 46, 24, WT_METAL, false),
        W(1, 21, 6, 25, WT_BRICK, false),    // secret closet ring
    },
    .numWalls = 6,
    .doors = {
        D(9, 4, 0), D(19, 4, 0), D(30, 5, 0),
        D(5, 8, 0), D(9, 18, 0), D(37, 14, 0),
        D(44, 21, 4),                        // exit door (switch opens)
        D(3, 21, 5),                         // secret
    },
    .numDoors = 8,
    .switches = {{36, 26}},
    .numSwitches = 1,
    .exitPad = {46, 21},
    .hasExitPad = true,
    .hasTp = false,
    .things = {
        T(14, 4, TH_TROOPER), T(16, 5, TH_TROOPER), T(24, 4, TH_TROOPER),
        T(27, 7, TH_TROOPER), T(35, 5, TH_TROOPER), T(40, 8, TH_TROOPER),
        T(34, 10, TH_TROOPER), T(20, 18, TH_TROOPER), T(25, 18, TH_TROOPER),
        T(32, 22, TH_TROOPER), T(38, 18, TH_TROOPER), T(41, 24, TH_TROOPER),
        T(42, 4, TH_IMP), T(36, 20, TH_IMP), T(39, 22, TH_IMP),
        T(12, 5, IT_STIM), T(17, 4, IT_CLIP), T(23, 3, IT_CLIP),
        T(26, 8, IT_SHELLS), T(43, 11, IT_MEDKIT), T(44, 2, IT_POTION),
        T(36, 17, IT_CLIP), T(31, 25, IT_CLIP), T(33, 23, IT_STIM),
        T(40, 17, IT_SHELLBOX), T(3, 23, IT_ARMOR), T(4, 23, IT_GUN_SHOTGUN),
        T(33, 4, TH_BARREL), T(34, 4, TH_BARREL), T(38, 6, TH_BARREL),
        T(41, 9, TH_BARREL), T(35, 11, TH_BARREL),
        T(21, 3, TH_LAMP), T(28, 3, TH_LAMP), T(3, 2, TH_LAMP), T(7, 2, TH_LAMP),
    },
    .numThings = 35,
};

// ------------------------------------------------------------------ E1M2
static const LevelDef LVL2 = {
    .name = "Reactor Complex",
    .parSeconds = 120,
    .playerStart = {4, 28},
    .rooms = {
        R(2, 8, 18, 18, FT_TECH, CE_CONC, 150),    // main hall
        R(4, 20, 5, 24, FT_TECH, CE_CONC, 140),    // south corridor
        R(2, 26, 7, 30, FT_TECH, CE_CONC, 180),    // start room
        R(20, 8, 33, 18, FT_TECH, CE_CONC, 160),   // reactor pool room
        R(23, 10, 31, 16, FT_SLIME, CE_CONC, 200), // toxic pool
        R(27, 8, 28, 18, FT_TECH, CE_CONC, 160),   // walkway over pool
        R(26, 12, 29, 14, FT_TECH, CE_LIGHT, 200), // key island
        R(34, 5, 35, 5, FT_TECH, CE_CONC, 140),    // east link
        R(36, 2, 42, 8, FT_TECH, CE_LIGHT, 200),   // computer room
        R(44, 3, 46, 6, FT_TECH, CE_LIGHT, 200),   // exit closet
        R(8, 20, 11, 22, FT_TECH, CE_CONC, 170),   // secret closet
    },
    .numRooms = 11,
    .walls = {
        W(2, 7, 18, 7, WT_COMP, false),      // hall north computer wall
        W(19, 7, 19, 19, WT_METAL, false),   // hall/pool divider
        W(22, 9, 32, 17, WT_ROCK, false),    // pool rock ring
        W(34, 1, 34, 9, WT_METAL, false),
        W(35, 1, 35, 9, WT_METAL, false),
        W(36, 1, 42, 1, WT_COMP, false),     // comp room strip
        W(43, 1, 43, 8, WT_METAL, false),    // blue door wall
        W(44, 2, 46, 7, WT_METAL, false),    // exit closet ring
        W(7, 19, 12, 23, WT_BRICK, false),   // secret ring
    },
    .numWalls = 9,
    .doors = {
        D(4, 25, 0), D(4, 19, 0), D(19, 13, 0),
        D(35, 5, 0),
        D(43, 4, 2),                         // BLUE door
        D(9, 19, 5),                         // secret
    },
    .numDoors = 6,
    .numSwitches = 0,
    .exitPad = {45, 4},
    .hasExitPad = true,
    .hasTp = false,
    .things = {
        T(8, 16, TH_TROOPER), T(3, 14, TH_TROOPER), T(38, 3, TH_TROOPER),
        T(41, 6, TH_TROOPER), T(30, 9, TH_TROOPER), T(22, 17, TH_TROOPER),
        T(26, 17, TH_TROOPER), T(31, 8, TH_TROOPER),
        T(6, 10, TH_IMP), T(12, 12, TH_IMP), T(15, 9, TH_IMP),
        T(30, 12, TH_IMP), T(24, 12, TH_IMP), T(40, 7, TH_IMP),
        T(26, 9, TH_PINKY), T(22, 16, TH_PINKY), T(31, 17, TH_PINKY),
        T(3, 9, IT_GUN_CHAINGUN), T(17, 17, IT_GUN_SHOTGUN),
        T(5, 9, IT_CLIP), T(16, 10, IT_SHELLS), T(2, 17, IT_MEDKIT),
        T(32, 8, IT_STIM), T(17, 8, IT_POTION), T(10, 9, IT_CLIP),
        T(14, 15, IT_CLIP), T(21, 9, IT_SHELLBOX), T(33, 18, IT_STIM),
        T(27, 13, IT_KEY_BLUE),
        T(9, 21, IT_MEGAARMOR), T(10, 21, IT_ROCKETS),
        T(10, 10, TH_BARREL), T(11, 10, TH_BARREL), T(4, 12, TH_BARREL),
        T(2, 8, TH_LAMP), T(18, 8, TH_LAMP), T(20, 8, TH_LAMP), T(33, 8, TH_LAMP),
    },
    .numThings = 37,
};

// ------------------------------------------------------------------ E1M3
static const LevelDef LVL3 = {
    .name = "Toxic Refinery",
    .parSeconds = 150,
    .playerStart = {23, 30},
    .rooms = {
        R(6, 6, 41, 28, FT_DIRT, TX_SKY, 225),     // great yard (outdoor)
        R(8, 2, 36, 4, FT_TECH, CE_CONC, 140),     // north corridor
        R(10, 5, 11, 5, FT_TECH, CE_CONC, 150),    // link west
        R(30, 5, 31, 5, FT_TECH, CE_CONC, 150),    // link east
        R(38, 6, 41, 9, FT_TECH, CE_CONC, 190),   // yellow key vault
        R(14, 6, 16, 24, FT_SLIME, TX_SKY, 200),   // slime channel N-S
        R(13, 14, 17, 16, FT_DIRT, TX_SKY, 210),   // launcher island
        R(14, 13, 16, 13, FT_DIRT, TX_SKY, 210),   // bridge n
        R(14, 17, 16, 17, FT_DIRT, TX_SKY, 210),   // bridge s
        R(28, 14, 41, 16, FT_SLIME, TX_SKY, 200),  // slime channel E-W
        R(6, 22, 8, 24, FT_DIRT, TX_SKY, 180),     // secret pocket
        R(42, 27, 42, 30, FT_TECH, CE_CONC, 150),  // exit link
        R(44, 28, 46, 31, FT_TECH, CE_LIGHT, 200), // exit closet
        R(20, 29, 27, 31, FT_TECH, CE_LIGHT, 190), // start garage
    },
    .numRooms = 14,
    .walls = {
        W(7, 1, 37, 5, WT_METAL, false),     // north corr ring
        W(37, 5, 42, 10, WT_METAL, false),   // vault ring
        W(43, 27, 46, 32, WT_METAL, false),  // exit ring
        // secret pocket enclosure (force: sits inside the open yard)
        W(5, 21, 9, 21, WT_ROCK, true),
        W(5, 25, 9, 25, WT_ROCK, true),
        W(5, 21, 5, 25, WT_ROCK, true),
        W(9, 21, 9, 25, WT_ROCK, true),
        W(19, 28, 28, 32, WT_BRICK, false),  // garage ring
        // yard rock pillars
        W(20, 10, 21, 11, WT_ROCK, true),
        W(34, 19, 35, 20, WT_ROCK, true),
        W(9, 12, 10, 12, WT_ROCK, true),
        W(25, 20, 26, 20, WT_ROCK, true),
    },
    .numWalls = 9,
    .doors = {
        D(39, 5, 0),                         // into vault
        D(43, 29, 3),                        // YELLOW door
        D(9, 23, 5),                         // secret
    },
    .numDoors = 3,
    .numSwitches = 0,
    .exitPad = {45, 29},
    .hasExitPad = true,
    .hasTp = false,
    .things = {
        T(9, 7, TH_TROOPER), T(20, 18, TH_TROOPER), T(40, 17, TH_TROOPER),
        T(26, 8, TH_TROOPER), T(35, 4, TH_TROOPER), T(12, 20, TH_TROOPER),
        T(30, 22, TH_TROOPER), T(38, 25, TH_TROOPER), T(22, 10, TH_TROOPER),
        T(24, 12, TH_IMP), T(31, 7, TH_IMP), T(38, 12, TH_IMP),
        T(10, 14, TH_IMP), T(28, 22, TH_IMP), T(18, 10, TH_IMP),
        T(36, 24, TH_IMP),
        T(12, 8, TH_PINKY), T(33, 9, TH_PINKY), T(36, 22, TH_PINKY),
        T(27, 21, TH_PINKY), T(19, 22, TH_PINKY),
        T(15, 15, IT_GUN_ROCKET), T(14, 15, IT_ROCKETS),
        T(39, 7, IT_KEY_YELLOW), T(40, 8, IT_MEDKIT),
        T(7, 7, IT_ARMOR), T(7, 23, IT_MEGAARMOR), T(8, 23, IT_CELL),
        T(11, 4, IT_STIM), T(33, 3, IT_STIM), T(24, 30, IT_STIM), T(8, 19, IT_STIM),
        T(17, 7, IT_SHELLS), T(29, 12, IT_SHELLS), T(13, 22, IT_SHELLS),
        T(36, 10, IT_SHELLS), T(21, 30, IT_SHELLS),
        T(10, 3, IT_CLIP), T(25, 4, IT_CLIP), T(19, 15, IT_CLIP),
        T(31, 25, IT_CLIP), T(40, 21, IT_CLIP),
        T(22, 12, TH_BARREL), T(23, 12, TH_BARREL), T(30, 18, TH_BARREL),
        T(31, 18, TH_BARREL), T(8, 16, TH_BARREL), T(36, 17, TH_BARREL),
        T(12, 24, TH_BARREL),
        T(21, 30, TH_LAMP), T(26, 30, TH_LAMP),
    },
    .numThings = 47,
};

// ------------------------------------------------------------------ E1M4
static const LevelDef LVL4 = {
    .name = "Command Bunker",
    .parSeconds = 180,
    .playerStart = {3, 17},
    .rooms = {
        R(2, 15, 5, 18, FT_TECH, CE_CONC, 160),    // start
        R(8, 4, 20, 28, FT_STONE, CE_CONC, 130),   // central hall
        R(2, 4, 6, 10, FT_TECH, CE_CONC, 140),    // west wing
        R(6, 16, 7, 16, FT_TECH, CE_CONC, 140),    // start link
        R(10, 1, 17, 2, FT_STONE, CE_LIGHT, 200), // red key vault
        R(21, 7, 22, 7, FT_TECH, CE_CONC, 140),    // east link
        R(23, 4, 30, 12, FT_TECH, CE_LIGHT, 200),  // east wing
        R(21, 24, 22, 24, FT_TECH, CE_CONC, 140),  // south link
        R(23, 20, 34, 28, FT_TECH, CE_CONC, 150),  // south lab
        R(36, 23, 36, 24, FT_TECH, CE_CONC, 140),  // red door link
        R(38, 20, 42, 28, FT_STONE, CE_CONC, 170),// baron wing
        R(44, 19, 45, 23, FT_TECH, CE_LIGHT, 200), // exit closet
        R(2, 12, 4, 13, FT_TECH, CE_CONC, 170),    // secret 1
        R(36, 26, 37, 27, FT_TECH, CE_CONC, 170),  // secret 2
    },
    .numRooms = 14,
    .walls = {
        W(7, 3, 21, 29, WT_MARBLE, false),   // hall ring
        W(13, 14, 15, 18, WT_MARBLE, true),  // hall pillar block
        W(1, 3, 7, 11, WT_METAL, false),     // west wing ring
        W(9, 0, 18, 3, WT_MARBLE, false),    // vault ring
        W(22, 3, 31, 13, WT_COMP, false),    // east wing ring
        W(22, 19, 35, 29, WT_METAL, false),  // lab ring
        W(37, 19, 43, 29, WT_MARBLE, false), // wing ring
        W(43, 18, 45, 24, WT_METAL, false),  // exit closet ring
        W(1, 11, 5, 14, WT_BRICK, false),    // secret1 ring
        W(35, 25, 38, 28, WT_BRICK, false),  // secret2 ring
    },
    .numWalls = 10,
    .doors = {
        D(7, 7, 0),                          // west wing
        D(13, 3, 0),                         // vault
        D(22, 7, 0),                         // east wing
        D(22, 24, 0),                        // south lab
        D(37, 23, 1),                        // RED door
        D(43, 21, 4),                        // exit door (switch)
        D(3, 11, 5), D(35, 27, 5),           // secrets
    },
    .numDoors = 8,
    .switches = {{30, 29}},
    .numSwitches = 1,
    .exitPad = {44, 21},
    .hasExitPad = true,
    .hasTp = false,
    .things = {
        T(13, 6, TH_BARON),
        T(10, 20, TH_PINKY), T(17, 25, TH_PINKY), T(9, 10, TH_PINKY),
        T(25, 9, TH_PINKY), T(28, 10, TH_PINKY), T(11, 22, TH_PINKY),
        T(12, 8, TH_IMP), T(18, 12, TH_IMP), T(15, 22, TH_IMP),
        T(26, 6, TH_IMP), T(29, 11, TH_IMP), T(25, 26, TH_IMP),
        T(31, 21, TH_IMP), T(40, 22, TH_IMP), T(40, 26, TH_IMP),
        T(9, 5, TH_TROOPER), T(17, 9, TH_TROOPER), T(19, 20, TH_TROOPER),
        T(24, 4, TH_TROOPER), T(33, 27, TH_TROOPER), T(27, 21, TH_TROOPER),
        T(30, 25, TH_TROOPER), T(39, 27, TH_TROOPER), T(41, 20, TH_TROOPER),
        T(29, 4, IT_CELL), T(4, 5, IT_CELL),
        T(10, 7, IT_SHELLS), T(19, 15, IT_SHELLS), T(24, 11, IT_SHELLS),
        T(41, 24, IT_SHELLBOX),
        T(33, 26, IT_ROCKETS), T(37, 27, IT_ROCKETS),
        T(13, 1, IT_KEY_RED), T(16, 1, IT_MEGAARMOR), T(11, 1, IT_CELL),
        T(9, 27, IT_STIM), T(19, 5, IT_STIM), T(8, 28, IT_POTION),
        T(19, 27, IT_ARMOR),
        T(28, 5, IT_GUN_PLASMA), T(30, 4, IT_CLIP),
        T(33, 20, IT_SHELLBOX), T(24, 27, IT_MEDKIT),
        T(3, 13, IT_BACKPACK), T(2, 13, IT_CELL),
        T(36, 26, IT_MEDKIT),
        T(9, 6, TH_BARREL), T(10, 6, TH_BARREL), T(16, 26, TH_BARREL),
        T(17, 26, TH_BARREL), T(30, 8, TH_BARREL), T(31, 8, TH_BARREL),
        T(26, 22, TH_BARREL),
        T(8, 4, TH_LAMP), T(20, 4, TH_LAMP), T(8, 28, TH_LAMP), T(20, 28, TH_LAMP),
    },
    .numThings = 55,
};

// ------------------------------------------------------------------ E1M5
static const LevelDef LVL5 = {
    .name = "Hell Gate",
    .parSeconds = 240,
    .playerStart = {23, 29},
    .rooms = {
        R(18, 28, 29, 31, FT_TECH, CE_CONC, 170),  // start
        R(6, 16, 41, 26, FT_STONE, CE_CONC, 110),  // grand hall
        R(16, 19, 31, 23, FT_SLIME, CE_CONC, 190), // slime moat
        R(16, 21, 17, 21, FT_STONE, CE_CONC, 120), // west bridge
        R(30, 21, 31, 21, FT_STONE, CE_CONC, 120), // east bridge
        R(14, 8, 33, 12, FT_STONE, CE_CONC, 100), // north gate
        R(6, 8, 12, 12, FT_STONE, CE_CONC, 90),   // west crypt
        R(35, 8, 41, 12, FT_STONE, CE_CONC, 90),  // east crypt
        R(14, 1, 33, 5, FT_STONE, CE_CONC, 120),  // hell arena
        R(2, 18, 4, 20, FT_TECH, CE_CONC, 170),    // secret pocket
    },
    .numRooms = 10,
    .walls = {
        W(5, 15, 42, 27, WT_MARBLE, false),  // grand hall ring
        W(13, 7, 34, 13, WT_MARBLE, false),  // gate ring
        W(5, 7, 13, 13, WT_MARBLE, false),   // west crypt ring
        W(34, 7, 42, 13, WT_MARBLE, false),  // east crypt ring
        W(13, 0, 34, 6, WT_ROCK, false),     // arena ring
        W(17, 27, 30, 32, WT_BRICK, false),  // start ring
    },
    .numWalls = 6,
    .doors = {
        D(23, 27, 0),                        // start -> grand hall
        D(18, 15, 0), D(29, 15, 0),          // hall -> gate
        D(23, 7, 0),                         // gate -> arena
        D(13, 9, 0), D(34, 9, 0),            // crypts
        D(5, 19, 5),                         // secret
    },
    .numDoors = 7,
    .numSwitches = 0,
    .exitPad = {23, 2},
    .hasExitPad = true,
    .hasTp = false,
    .things = {
        T(17, 3, TH_BARON), T(30, 3, TH_BARON), T(23, 21, TH_BARON),
        T(10, 18, TH_PINKY), T(37, 18, TH_PINKY), T(20, 10, TH_PINKY),
        T(27, 10, TH_PINKY), T(8, 9, TH_PINKY), T(39, 9, TH_PINKY),
        T(15, 25, TH_PINKY), T(32, 25, TH_PINKY),
        T(14, 17, TH_IMP), T(33, 17, TH_IMP), T(16, 24, TH_IMP),
        T(31, 24, TH_IMP), T(23, 17, TH_IMP), T(24, 12, TH_IMP),
        T(21, 12, TH_IMP), T(8, 10, TH_IMP), T(39, 10, TH_IMP),
        T(20, 4, TH_IMP), T(27, 4, TH_IMP), T(12, 22, TH_IMP), T(35, 22, TH_IMP),
        T(12, 25, TH_TROOPER), T(35, 25, TH_TROOPER), T(21, 30, TH_TROOPER),
        T(26, 30, TH_TROOPER), T(17, 9, TH_TROOPER), T(30, 9, TH_TROOPER),
        T(3, 19, IT_MEGAARMOR), T(3, 18, IT_CELL), T(3, 20, IT_CELL),
        T(7, 25, IT_SHELLS), T(40, 25, IT_SHELLS), T(15, 10, IT_SHELLS),
        T(32, 10, IT_SHELLS),
        T(40, 8, IT_ROCKETS), T(40, 11, IT_MEDKIT), T(41, 8, IT_BACKPACK),
        T(2, 25, IT_MEDKIT), T(7, 8, IT_ARMOR), T(7, 11, IT_CELL),
        T(23, 25, IT_STIM), T(24, 29, IT_STIM), T(38, 24, IT_ARMOR),
        T(6, 16, IT_POTION), T(41, 16, IT_POTION),
        T(11, 11, IT_GUN_PLASMA), T(36, 11, IT_CELL),
        T(19, 18, TH_BARREL), T(20, 18, TH_BARREL), T(27, 18, TH_BARREL),
        T(28, 18, TH_BARREL), T(9, 24, TH_BARREL), T(38, 24, TH_BARREL),
        T(22, 10, TH_BARREL), T(25, 10, TH_BARREL),
        T(7, 17, TH_LAMP), T(40, 17, TH_LAMP), T(15, 8, TH_LAMP),
        T(32, 8, TH_LAMP), T(20, 2, TH_LAMP), T(27, 2, TH_LAMP),
    },
    .numThings = 63,
};

const LevelDef LEVELS[] = { LVL1, LVL2, LVL3, LVL4, LVL5 };
const int NUM_LEVELS = sizeof(LEVELS) / sizeof(LEVELS[0]);
