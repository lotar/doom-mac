// main.c — game state machine, SDL platform layer, headless modes
#include "doom.h"
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

Game G;
Input IN;
bool g_levelDone; // defined in ent.c

// ------------------------------------------------------------------ core util
void fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "FATAL: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) fatal("out of memory (%zu)", n);
    return p;
}

static uint32_t rngState = 0x12345678u;
void rng_seed(uint32_t s) { rngState = s ? s : 0xA5A5A5A5u; }
uint32_t rng_u32(void) {
    uint32_t x = rngState;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rngState = x;
    return x;
}
float rng_f(void) { return (rng_u32() >> 8) / (float)(1 << 24); }
int rng_irange(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rng_u32() % (uint32_t)(hi - lo + 1));
}

// ------------------------------------------------------------------ flow
void start_level(int idx) {
    G.level = idx;
    if (!map_load(idx)) fatal("cannot load level %d", idx);
    ents_reset();
    player_reset_for_level();
    ents_spawn_level_things();
    G.killCount = 0; G.itemCount = 0; G.secretCount = 0;
    G.time = 0;
    G.msg[0] = 0; G.msgTimer = 0;
    G.paused = false;
    g_levelDone = false;
    G.automap = false;
    rng_seed(0xBEEF0000u + (uint32_t)idx);
    music_play(1);
}

static void cheat_check(char c) {
    if (!isalpha((unsigned char)c)) return;
    int len = (int)strlen(G.cheatBuf);
    if (len >= 15) { memmove(G.cheatBuf, G.cheatBuf + 1, 14); G.cheatBuf[14] = 0; len = 14; }
    G.cheatBuf[len] = (char)(c | 32);
    G.cheatBuf[len + 1] = 0;
    const char *codes[] = {"iddqd", "idkfa", "idclip"};
    for (int i = 0; i < 3; i++) {
        int cl = (int)strlen(codes[i]);
        int bl = (int)strlen(G.cheatBuf);
        if (bl >= cl && !strcmp(G.cheatBuf + bl - cl, codes[i])) {
            player_cheat(codes[i]);
            G.cheatBuf[0] = 0;
            break;
        }
    }
}

void sim_step(Input *input, float dt) {
    Input *in = &IN;
    if (input != &IN) *in = *input; // headless/test path feeds a template
    G.time += dt;
    if (G.msgTimer > 0) G.msgTimer -= dt;

    // consume automap toggle anywhere
    if (in->automapToggle) { G.automap = !G.automap; in->automapToggle = false; }
    if (in->muteToggle) { audio_toggle_mute(); in->muteToggle = false; }

    switch (G.mode) {
    case GM_TITLE:
        if (in->menuUp) { G.titleSel = (G.titleSel + 2) % 3; sfx_play(SX_MENU_MOVE, .7f, 0); }
        if (in->menuDown) { G.titleSel = (G.titleSel + 1) % 3; sfx_play(SX_MENU_MOVE, .7f, 0); }
        if (in->menuLeft || in->menuRight) {
            G.difficulty = (G.difficulty + 1) % 3;
            sfx_play(SX_MENU_MOVE, .7f, 0);
        }
        if (in->confirm) {
            sfx_play(SX_MENU_SELECT, .9f, 0);
            if (G.titleSel == 0) { G.diffSel = G.difficulty; G.mode = GM_DIFFSEL; }
            else if (G.titleSel == 1) G.mode = GM_INSTRUCTIONS;
            else G.quitRequested = true;
        }
        break;

    case GM_DIFFSEL:
        if (in->menuUp || in->menuDown) {
            G.diffSel = (G.diffSel + 1) % 3;
            sfx_play(SX_MENU_MOVE, .7f, 0);
        }
        if (in->pauseToggle) { G.mode = GM_TITLE; }
        if (in->confirm) {
            sfx_play(SX_MENU_SELECT, 1, 0);
            G.difficulty = G.diffSel;
            start_level(0);
            G.mode = GM_PLAY;
        }
        break;

    case GM_INSTRUCTIONS:
        if (in->confirm || in->pauseToggle) G.mode = GM_TITLE;
        break;

    case GM_PLAY:
        if (in->pauseToggle) { G.paused = !G.paused; sfx_play(SX_MENU_MOVE, .6f, 0); }
        if (!G.paused) {
            if (in->keyChar) { cheat_check(in->keyChar); }
            if (G.pl.dead) {
                G.pl.deadT += 0; // advanced in player_update
                if (in->confirm || in->fire) start_level(G.level);
            } else {
                player_update(dt);
                ents_update(dt);
            }
            if (g_levelDone) {
                G.mode = GM_INTERMISSION;
                G.interTally[0] = G.interTally[1] = G.interTally[2] = 0;
                music_play(0);
                sfx_play(SX_SECRET, 1, 0);
            }
        }
        break;

    case GM_INTERMISSION:
        for (int i = 0; i < 3; i++) G.interTally[i] += dt * 60.0f;
        if (in->confirm) {
            if (G.level + 1 >= NUM_LEVELS) {
                G.mode = GM_VICTORY;
                G.victoryScroll = 0;
                music_play(2);
            } else {
                start_level(G.level + 1);
                G.mode = GM_PLAY;
            }
        }
        break;

    case GM_VICTORY:
        G.victoryScroll += dt * 26.0f;
        if (in->confirm && G.victoryScroll > 380) {
            G.mode = GM_TITLE;
            music_play(2);
        }
        break;
    }

    // clear one-shot flags; held movement/fire state persists
    in->confirm = in->menuUp = in->menuDown = in->menuLeft = in->menuRight = false;
    in->use = in->automapToggle = in->pauseToggle = false;
    in->selectWeapon = -1;
    in->weaponNext = in->weaponPrev = false;
    in->keyChar = 0;
    in->muteToggle = false;
}

// ------------------------------------------------------------------ headless
static void run_headless_frames(int n, Input tmpl) {
    for (int i = 0; i < n; i++) {
        Input f = tmpl;
        sim_step(&f, DT);
        render_frame();
    }
}
static void save_shot(const char *dir, const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    write_bmp(path, FB, FB_W, FB_H);
}

static void make_icon(const char *path) {
    static uint32_t buf[512 * 512];
    for (int y = 0; y < 512; y++)
        for (int x = 0; x < 512; x++) {
            uint32_t c = col_rgb(16, 12, 12);
            int d = x - 256, e = y - 256;
            if (d * d + e * e > 240 * 240) c = 0;
            else if (d * d + e * e > 226 * 226) c = col_rgb(90, 90, 96);
            buf[y * 512 + x] = c;
        }
    // fire band
    rng_seed(99);
    for (int y = 340; y < 500; y++)
        for (int x = 40; x < 472; x++) {
            float f = (y - 340) / 160.0f;
            float n = rng_f();
            if ((1 - f) * (0.5f + n * 0.7f) > 0.45f)
                buf[y * 512 + x] =
                    col_shade(col_rgb(255, (int)(120 + n * 110), 30), f * 0.9f + 0.25f);
        }
    // draw logo into temp fb-sized? font draws into FB only — borrow FB
    memset(FB, 0, sizeof(FB));
    char txt[] = "DOOM";
    int scale = 18;
    int w = font_measure(txt, scale);
    font_draw_str(FB_W / 2 - w / 2, 100, txt, col_rgb(206, 34, 22), scale);
    // blit region of FB into icon buffer center
    int fx0 = FB_W / 2 - w / 2 - 10, fy0 = 80;
    for (int y = 0; y < 130; y++)
        for (int x = 0; x < w + 20; x++) {
            int fx = fx0 + x, fy = fy0 + y;
            if (fx < 0 || fy < 0 || fx >= FB_W || fy >= FB_H) continue;
            uint32_t c = FB[fy * FB_W + fx];
            if (!(c & 0xFF000000u)) continue;
            int ix = 256 - (w + 20) / 2 + x, iy = 150 + y;
            if (ix >= 0 && iy >= 0 && ix < 512 && iy < 512 &&
                buf[iy * 512 + ix])
                buf[iy * 512 + ix] = c;
        }
    write_bmp(path, buf, 512, 512);
    printf("icon written to %s\n", path);
}

static void shots_tour(const char *dir) {
    mkdir(dir, 0755);
    tex_init();
    rng_seed(20240101);
    Input none = {0};

    G.mode = GM_TITLE;
    run_headless_frames(50, none);
    save_shot(dir, "title.bmp");

    G.mode = GM_DIFFSEL;
    run_headless_frames(3, none);
    save_shot(dir, "difficulty.bmp");

    G.mode = GM_INSTRUCTIONS;
    render_frame();
    save_shot(dir, "instructions.bmp");

    static const struct { int dx, dy; } turns[] = {{1, 0}, {-1, 0}};
    (void)turns;
    for (int lv = 0; lv < NUM_LEVELS; lv++) {
        start_level(lv);
        G.mode = GM_PLAY;
        // settle + look around + shoot
        Input t = {0};
        t.fwd = true;
        run_headless_frames(34, t);
        Input turn = {0};
        turn.mouseDX = 260 + lv * 47;
        run_headless_frames(16, turn);
        Input strafe = {0};
        strafe.strafeRight = true;
        run_headless_frames(12, strafe);
        Input still = {0};
        run_headless_frames(6, still);
        char nm[64];
        snprintf(nm, sizeof(nm), "level%d_view.bmp", lv + 1);
        save_shot(dir, nm);

        Input fire = {0};
        fire.fire = true;
        run_headless_frames(14, fire);
        snprintf(nm, sizeof(nm), "level%d_combat.bmp", lv + 1);
        save_shot(dir, nm);

        Input am = {0};
        am.automapToggle = true;
        run_headless_frames(1, am);
        snprintf(nm, sizeof(nm), "level%d_automap.bmp", lv + 1);
        save_shot(dir, nm);
        Input amoff2 = {0};
        amoff2.automapToggle = true;
        run_headless_frames(1, amoff2);

        // monster close-up: teleport in front of the nearest live monster
        {
            Ent *best = NULL;
            float bd = 1e9f;
            for (int i = 0; i < MAX_ENTS; i++) {
                Ent *e = &G.ents[i];
                if (!e->active || e->state == MS_DEAD || e->state == MS_DYING)
                    continue;
                if (e->type < ET_TROOPER || e->type > ET_BARON) continue;
                float d = hypotf(e->x - G.pl.x, e->y - G.pl.y);
                if (d < bd) { bd = d; best = e; }
            }
            if (best) {
                best->alerted = true;
                best->state = MS_CHASE;
                float dx = G.pl.x - best->x, dy = G.pl.y - best->y;
                float dl = hypotf(dx, dy);
                if (dl < 0.5f) { dx = 1; dy = 0; dl = 1; }
                G.pl.x = best->x + dx / dl * 2.0f;
                G.pl.y = best->y + dy / dl * 2.0f;
                G.pl.ang = atan2f(best->y - G.pl.y, best->x - G.pl.x);
                Input st = {0};
                st.fwd = true;
                run_headless_frames(6, st); // approach; it wakes & animates
                snprintf(nm, sizeof(nm), "level%d_monster.bmp", lv + 1);
                save_shot(dir, nm);
            }
        }
        Input amoff = {0};
        amoff.automapToggle = true;
        run_headless_frames(1, amoff);
    }
    // intermission + victory
    G.mode = GM_INTERMISSION;
    G.level = 0;
    map_load(0);
    G.killCount = 5; G.itemCount = 3; G.secretCount = 1;
    G.time = 83;
    run_headless_frames(40, none);
    save_shot(dir, "intermission.bmp");
    G.mode = GM_VICTORY;
    G.victoryScroll = 300;
    run_headless_frames(3, none);
    save_shot(dir, "victory.bmp");
    printf("shots written to %s\n", dir);
}

// ------------------------------------------------------------------ GUI
typedef struct {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
} Gui;

static void gui_present(Gui *g) {
    void *pixels;
    int pitch;
    if (SDL_LockTexture(g->tex, NULL, &pixels, &pitch) == 0) {
        // copy row by row: texture pitch may exceed FB_W * 4
        uint8_t *dst = (uint8_t *)pixels;
        const uint32_t *src = FB;
        size_t rowBytes = (size_t)FB_W * 4;
        for (int y = 0; y < FB_H; y++) {
            memcpy(dst, src, rowBytes);
            dst += pitch;
            src += FB_W;
        }
        SDL_UnlockTexture(g->tex);
    }
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    // integer-preserving aspect fit
    int ww, wh;
    SDL_GetWindowSize(g->win, &ww, &wh);
    float scaleW = ww / (float)FB_W, scaleH = wh / (float)FB_H;
    float s = scaleW < scaleH ? scaleW : scaleH;
    SDL_Rect dst = {(ww - (int)(FB_W * s)) / 2, (wh - (int)(FB_H * s)) / 2,
                    (int)(FB_W * s), (int)(FB_H * s)};
    SDL_RenderCopy(g->ren, g->tex, NULL, &dst);
    // DOOM_CAPTURE=/path.bmp: verify the full presentation chain (texture ->
    // renderer -> backbuffer) by reading back what would appear on screen
    const char *capPath = getenv("DOOM_CAPTURE");
    if (capPath && !getenv("DOOM_CAPTURE_DONE")) {
        static uint32_t cap[1280 * 800];
        int pw = ww, ph = wh;
        if (SDL_RenderReadPixels(g->ren, NULL, SDL_PIXELFORMAT_ARGB8888,
                                 cap, pw * 4) == 0) {
            write_bmp(capPath, cap, pw, ph);
            setenv("DOOM_CAPTURE_DONE", "1", 1);
            fprintf(stderr, "capture written: %s (%dx%d)\n", capPath, pw, ph);
        } else {
            fprintf(stderr, "RenderReadPixels failed: %s\n", SDL_GetError());
            setenv("DOOM_CAPTURE_DONE", "1", 1);
        }
    }
    SDL_RenderPresent(g->ren);
}

static void poll_events(Gui *g, bool *running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT: *running = false; break;
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {}
            break;
        case SDL_MOUSEMOTION:
            if (SDL_GetRelativeMouseMode()) IN.mouseDX += ev.motion.xrel;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) IN.fire = true;
            break;
        case SDL_MOUSEWHEEL:
            if (ev.wheel.y > 0) IN.weaponNext = true;
            if (ev.wheel.y < 0) IN.weaponPrev = true;
            break;
        case SDL_KEYDOWN: {
            SDL_Keycode k = ev.key.keysym.sym;
            bool repeat = ev.key.repeat;
            if (!repeat) {
                switch (k) {
                case SDLK_w: case SDLK_UP: IN.fwd = true; break;
                case SDLK_s: case SDLK_DOWN: IN.back = true; break;
                case SDLK_a: IN.strafeLeft = true; break;
                case SDLK_d: IN.strafeRight = true; break;
                case SDLK_LEFT: IN.turnLeft = true; break;
                case SDLK_RIGHT: IN.turnRight = true; break;
                case SDLK_SPACE: IN.use = true; break;
                case SDLK_e: IN.use = true; break;
                case SDLK_TAB: IN.automapToggle = true; break;
                case SDLK_ESCAPE: IN.pauseToggle = true; break;
                case SDLK_RETURN: case SDLK_KP_ENTER: IN.confirm = true; break;
                case SDLK_m: IN.muteToggle = true; break;
                case SDLK_LEFTBRACKET: IN.weaponPrev = true; break;
                case SDLK_RIGHTBRACKET: IN.weaponNext = true; break;
                case SDLK_F11: {
                    Uint32 f = SDL_GetWindowFlags(g->win);
                    SDL_SetWindowFullscreen(
                        g->win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0
                                                                    : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    break;
                }
                default: break;
                }
                // menu nav shares arrows: also set menu flags on non-play modes
                if (G.mode != GM_PLAY || G.paused) {
                    if (k == SDLK_UP) IN.menuUp = true;
                    if (k == SDLK_DOWN) IN.menuDown = true;
                    if (k == SDLK_LEFT) IN.menuLeft = true;
                    if (k == SDLK_RIGHT) IN.menuRight = true;
                }
                if (k >= '1' && k <= '5') IN.selectWeapon = k - '0';
                if (k == SDLK_LCTRL || k == SDLK_RCTRL) IN.fire = true;
            }
            break;
        }
        case SDL_KEYUP: {
            SDL_Keycode k = ev.key.keysym.sym;
            switch (k) {
            case SDLK_w: case SDLK_UP: IN.fwd = false; break;
            case SDLK_s: case SDLK_DOWN: IN.back = false; break;
            case SDLK_a: IN.strafeLeft = false; break;
            case SDLK_d: IN.strafeRight = false; break;
            case SDLK_LEFT: IN.turnLeft = false; break;
            case SDLK_RIGHT: IN.turnRight = false; break;
            case SDLK_LCTRL: case SDLK_RCTRL: IN.fire = false; break;
            default: break;
            }
            break;
        }
        case SDL_TEXTINPUT:
            if (ev.text.text[0]) IN.keyChar = ev.text.text[0];
            break;
        }
    }
    // mouse-up for fire
    if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) &&
        !((SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) ||
          (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_RCTRL])))
        IN.fire = false;
}

int app_main(int argc, char **argv) {
    Gui gui = {0};
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        fatal("SDL_Init: %s", SDL_GetError());
    tex_init();

    gui.win = SDL_CreateWindow("DOOM — original C engine", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, 1280, 800,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!gui.win) fatal("window: %s", SDL_GetError());
    gui.ren = SDL_CreateRenderer(gui.win, -1, SDL_RENDERER_ACCELERATED);
    if (!gui.ren) gui.ren = SDL_CreateRenderer(gui.win, -1, 0);
    if (!gui.ren) fatal("renderer: %s", SDL_GetError());
    gui.tex = SDL_CreateTexture(gui.ren, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, FB_W, FB_H);
    if (!gui.tex) fatal("texture: %s", SDL_GetError());
    audio_init();
    music_play(2);

    G.mode = GM_TITLE;
    rng_seed((uint32_t)time(NULL));

    bool running = true;
    uint64_t pcFreq = SDL_GetPerformanceFrequency();
    uint64_t prev = SDL_GetPerformanceCounter();
    float acc = 0;
    SDL_StartTextInput();
    while (running) {
        uint64_t now = SDL_GetPerformanceCounter();
        acc += (now - prev) / (float)pcFreq;
        prev = now;
        if (acc > 0.25f) acc = 0.25f;

        poll_events(&gui, &running);
        if (G.quitRequested) running = false;

        bool mouseGrab = (G.mode == GM_PLAY) && !G.paused && !G.automap;
        SDL_SetRelativeMouseMode(mouseGrab ? SDL_TRUE : SDL_FALSE);

        while (acc >= DT) {
            sim_step(&IN, DT);
            acc -= DT;
        }

        render_frame();
        gui_present(&gui);
        SDL_Delay(1);
    }
    SDL_StopTextInput();
    audio_quit();
    SDL_DestroyTexture(gui.tex);
    SDL_DestroyRenderer(gui.ren);
    SDL_DestroyWindow(gui.win);
    SDL_Quit();
    return 0;
}

// real main lives here; dispatches modes
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) {
            tex_init();
            return selftest_run();
        }
        if (!strcmp(argv[i], "--shots") && i + 1 < argc) {
            shots_tour(argv[++i]);
            return 0;
        }
        if (!strcmp(argv[i], "--icon") && i + 1 < argc) {
            make_icon(argv[++i]);
            return 0;
        }
        if (!strcmp(argv[i], "--version")) {
            printf("doom-mac %s\n", VERSION_STR);
            return 0;
        }
    }
    return app_main(argc, argv);
}
