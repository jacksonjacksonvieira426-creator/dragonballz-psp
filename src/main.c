/*
 * Dragon Ball PSP - jogo de luta 2D (fan game)
 * Compila com o pspsdk (psp-gcc). Veja README.md.
 */
#ifdef HOST_TEST
#include "host_stub.h"
#else
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <psputils.h>
#include <malloc.h>
PSP_MODULE_INFO("DragonBallPSP", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Constantes                                                          */
/* ------------------------------------------------------------------ */
#define SCR_W 480
#define SCR_H 272
#define BUF_W 512
#define PAGE_W 512
#define PAGE_BYTES (PAGE_W * PAGE_W * 2)
#define GROUND_Y 246
#define WORLD_W 880
#define NANIM 16
#define NCHAR 28
#define NSTAGE 8
#define HP_MAX 1000
#define KI_MAX 1000
#define BLAST_COST 80
#define SUPER_COST 500
#define GRAV 0.55f
#define ROUND_SECS 90

#define RGBA(r, g, b, a) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(g) << 8) | (u32)(r))
#define WHITE 0xFFFFFFFFu

enum { A_IDLE, A_WALK, A_JUMP, A_GUARD, A_DASH, A_ATK1, A_ATK2, A_ATK3, A_BLAST,
       A_SUPER, A_HIT, A_HIT2, A_FALL, A_LIE, A_WIN, A_AIR };

enum { S_IDLE, S_WALK, S_JUMP, S_GUARD, S_DASH, S_ATK, S_BLAST, S_SUPER, S_HIT,
       S_FALL, S_LIE, S_GETUP, S_WIN };

enum { IN_L = 1, IN_R = 2, IN_U = 4, IN_D = 8, IN_P = 16, IN_K = 32, IN_B = 64,
       IN_S = 128, IN_C = 256, IN_DASH = 512 };

enum { SC_TITLE, SC_SELECT, SC_FIGHT, SC_RESULT };
enum { P_INTRO, P_FIGHT, P_KO, P_END };

typedef struct { const char *name; u32 color; float speed; float power; } CharInfo;
static const CharInfo CH[NCHAR] = {
    {"GOKU",          RGBA(255, 170, 60, 255),  2.6f, 1.00f},
    {"GOKU SSJ",      RGBA(255, 230, 80, 255),  2.7f, 1.05f},
    {"GOKU SSJ GOD",  RGBA(255, 80, 80, 255),   2.8f, 1.08f},
    {"GOKU SSJ BLUE", RGBA(90, 200, 255, 255),  2.8f, 1.10f},
    {"VEGETA",        RGBA(110, 150, 255, 255), 2.5f, 1.00f},
    {"VEGETA SSJ",    RGBA(255, 230, 90, 255),  2.6f, 1.05f},
    {"TEEN GOHAN",    RGBA(255, 190, 90, 255),  2.6f, 0.95f},
    {"GOHAN SSJ2",    RGBA(255, 240, 100, 255), 2.7f, 1.08f},
    {"GOHAN ADULTO",  RGBA(200, 160, 255, 255), 2.6f, 1.05f},
    {"FUTURE TRUNKS", RGBA(170, 130, 255, 255), 2.7f, 1.00f},
    {"PICCOLO",       RGBA(120, 255, 120, 255), 2.4f, 1.00f},
    {"KRILLIN",       RGBA(255, 200, 120, 255), 2.8f, 0.85f},
    {"FRIEZA",        RGBA(230, 120, 255, 255), 2.6f, 1.05f},
    {"CELL",          RGBA(130, 255, 130, 255), 2.5f, 1.05f},
    {"MAJIN BUU",     RGBA(255, 150, 200, 255), 2.2f, 1.15f},
    {"ANDROID 18",    RGBA(255, 255, 160, 255), 2.7f, 0.95f},
    {"NAPPA",         RGBA(255, 200, 120, 255), 2.3f, 1.05f},
    {"RADITZ",        RGBA(255, 160, 100, 255), 2.4f, 1.00f},
    {"BARDOCK",       RGBA(255, 120, 90, 255),  2.6f, 1.00f},
    {"BROLY",         RGBA(120, 255, 150, 255), 2.2f, 1.20f},
    {"BEERUS",        RGBA(200, 120, 255, 255), 2.6f, 1.15f},
    {"EVIL GOKU",     RGBA(255, 70, 60, 255),   2.6f, 1.00f},
    {"GOKU BLACK",    RGBA(255, 120, 210, 255), 2.5f, 1.05f},
    {"VEGETTO",       RGBA(90, 170, 255, 255),  2.5f, 1.00f},
    {"GOKU SSJ4",     RGBA(255, 210, 70, 255),  2.7f, 1.00f},
    {"IZE",           RGBA(190, 100, 255, 255), 2.4f, 1.10f},
    {"KEFLA",         RGBA(120, 255, 170, 255), 2.8f, 0.95f},
    {"HITTO",         RGBA(170, 210, 255, 255), 2.9f, 0.95f},
};
static const char *STAGE_NAME[NSTAGE + 1] = {
    "GRASSLANDS", "VOLCANIC SUNSET", "WEST CITY", "DEEP SPACE", "MEADOW",
    "TEAL CANYON", "MOONLIT PLAINS", "OCEAN SKIES", "RANDOM"};

typedef struct { int dmg, reach, kb, stun, knock; } AtkP;
static const AtkP ATKP[5] = {
    {0, 0, 0, 0, 0},
    {38, 66, 3, 16, 0},   /* 1: soco */
    {52, 72, 5, 20, 0},   /* 2: uppercut */
    {90, 86, 9, 26, 1},   /* 3: chute forte */
    {46, 68, 4, 18, 0},   /* 4: golpe aereo */
};

/* ------------------------------------------------------------------ */
/* Graficos                                                            */
/* ------------------------------------------------------------------ */
typedef struct { unsigned short u, v; u32 col; short x, y, z; } VT;
typedef struct { u32 col; short x, y, z; } VC;
#define VT_FMT (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D)
#define VC_FMT (GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D)

static unsigned int __attribute__((aligned(64))) gu_list[262144];
static const void *g_tex = 0;
static int g_tpsm = -1, g_tw = 0, g_th = 0, g_filter = -1, g_wrap = -1;

static void bind_tex(const void *p, int psm, int w, int h) {
    if (p == g_tex && psm == g_tpsm && w == g_tw && h == g_th) return;
    g_tex = p; g_tpsm = psm; g_tw = w; g_th = h;
    sceGuTexMode(psm, 0, 0, 0);
    sceGuTexImage(0, w, h, w, p);
}
static void set_filter(int linear) {
    if (g_filter == linear) return;
    g_filter = linear;
    sceGuTexFilter(linear ? GU_LINEAR : GU_NEAREST, linear ? GU_LINEAR : GU_NEAREST);
}
static void set_wrap(int rep) {
    if (g_wrap == rep) return;
    g_wrap = rep;
    sceGuTexWrap(rep ? GU_REPEAT : GU_CLAMP, rep ? GU_REPEAT : GU_CLAMP);
}
static void blend_normal(void) { sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); }
static void blend_add(void) { sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF); }

static void draw_tex(int u, int v, int tw, int th, int x, int y, int w, int h, int flip, u32 col) {
    VT *vt = (VT *)sceGuGetMemory(2 * sizeof(VT));
    int u0 = flip ? u + tw : u, u1 = flip ? u : u + tw;
    vt[0].u = (unsigned short)u0; vt[0].v = (unsigned short)v; vt[0].col = col;
    vt[0].x = (short)x; vt[0].y = (short)y; vt[0].z = 0;
    vt[1].u = (unsigned short)u1; vt[1].v = (unsigned short)(v + th); vt[1].col = col;
    vt[1].x = (short)(x + w); vt[1].y = (short)(y + h); vt[1].z = 0;
    sceGuDrawArray(GU_SPRITES, VT_FMT, 2, 0, vt);
}
static void rect(int x, int y, int w, int h, u32 col) {
    VC *vc;
    if (w <= 0 || h <= 0) return;
    sceGuDisable(GU_TEXTURE_2D);
    vc = (VC *)sceGuGetMemory(2 * sizeof(VC));
    vc[0].col = col; vc[0].x = (short)x; vc[0].y = (short)y; vc[0].z = 0;
    vc[1].col = col; vc[1].x = (short)(x + w); vc[1].y = (short)(y + h); vc[1].z = 0;
    sceGuDrawArray(GU_SPRITES, VC_FMT, 2, 0, vc);
    sceGuEnable(GU_TEXTURE_2D);
}

/* textura de brilho (gerada em codigo) */
static unsigned short __attribute__((aligned(16))) g_glow[32 * 32];
static void make_glow(void) {
    int x, y;
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            float dx = (x - 15.5f) / 15.5f, dy = (y - 15.5f) / 15.5f;
            float r = sqrtf(dx * dx + dy * dy), a;
            if (r >= 1.0f) a = 0; else { a = 1.0f - r; a = a * a * (3.0f - 2.0f * a); }
            {
                int ai = (int)(a * 15.0f + 0.5f);
                g_glow[y * 32 + x] = (unsigned short)(0x0FFF | (ai << 12));
            }
        }
}
static void glow(float cx, float cy, float rx, float ry, u32 col, int additive) {
    bind_tex(g_glow, GU_PSM_4444, 32, 32);
    set_filter(1);
    if (additive) blend_add();
    draw_tex(0, 0, 32, 32, (int)(cx - rx), (int)(cy - ry), (int)(rx * 2), (int)(ry * 2), 0, col);
    if (additive) blend_normal();
}

/* fonte */
static unsigned short __attribute__((aligned(16))) g_font[256 * 128];
static unsigned char g_adv[96];
static void build_font_metrics(void) {
    int i, x, y;
    for (i = 0; i < 96; i++) {
        int cx = (i & 15) * 16, cy = (i >> 4) * 16, maxx = -1;
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++)
                if ((g_font[(cy + y) * 256 + cx + x] >> 12) > 3 && x > maxx) maxx = x;
        g_adv[i] = (unsigned char)(maxx < 0 ? 5 : maxx + 3);
    }
}
static int text_w(const char *s, float sc) {
    float w = 0;
    for (; *s; s++) if (*s >= 32 && *s < 128) w += g_adv[*s - 32] * sc;
    return (int)w;
}
static void text_raw(int x, int y, float sc, u32 col, const char *s) {
    float fx = (float)x;
    bind_tex(g_font, GU_PSM_4444, 256, 128);
    set_filter(sc != 1.0f);
    for (; *s; s++) {
        int c = *s;
        if (c < 32 || c >= 128) continue;
        c -= 32;
        if (c != 0)
            draw_tex((c & 15) * 16, (c >> 4) * 16, 16, 16, (int)fx, y, (int)(16 * sc), (int)(16 * sc), 0, col);
        fx += g_adv[c] * sc;
    }
}
static void text(int x, int y, float sc, u32 col, const char *s) {
    text_raw(x + 1, y + 1, sc, 0xFF000000u, s);
    text_raw(x, y, sc, col, s);
}
static void text_c(int cx, int y, float sc, u32 col, const char *s) {
    text(cx - text_w(s, sc) / 2, y, sc, col, s);
}

/* ------------------------------------------------------------------ */
/* Arquivos / dados                                                    */
/* ------------------------------------------------------------------ */
static char g_base[256];

typedef struct { unsigned short page, x, y, w, h, ax; } Rec;
typedef struct { unsigned short start; unsigned char count, ticks; } AnimRec;
typedef struct {
    Rec *recs; AnimRec anim[NANIM]; unsigned char *pages; int npages, nrecs;
} Sprites;

static unsigned short rd16(const unsigned char *p) { return (unsigned short)(p[0] | (p[1] << 8)); }

static int load_sprites(int idx, Sprites *s) {
    char path[300];
    unsigned char hdr[8 + NANIM * 4], *rb;
    FILE *f;
    int i, nrec, npages;
    long meta, off;
    memset(s, 0, sizeof(*s));
    snprintf(path, sizeof path, "%sdata/chr_%d.dat", g_base, idx);
    f = fopen(path, "rb");
    if (!f) return 0;
    if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr || memcmp(hdr, "DBCH", 4)) { fclose(f); return 0; }
    npages = rd16(hdr + 4); nrec = rd16(hdr + 6);
    for (i = 0; i < NANIM; i++) {
        s->anim[i].start = rd16(hdr + 8 + i * 4);
        s->anim[i].count = hdr[10 + i * 4];
        s->anim[i].ticks = hdr[11 + i * 4];
        if (s->anim[i].count == 0) s->anim[i].count = 1;
        if (s->anim[i].ticks == 0) s->anim[i].ticks = 4;
    }
    s->recs = (Rec *)malloc(sizeof(Rec) * nrec);
    rb = (unsigned char *)malloc(nrec * 12);
    if (!s->recs || !rb || fread(rb, 1, nrec * 12, f) != (size_t)(nrec * 12)) { fclose(f); free(rb); return 0; }
    for (i = 0; i < nrec; i++) {
        s->recs[i].page = rd16(rb + i * 12); s->recs[i].x = rd16(rb + i * 12 + 2);
        s->recs[i].y = rd16(rb + i * 12 + 4); s->recs[i].w = rd16(rb + i * 12 + 6);
        s->recs[i].h = rd16(rb + i * 12 + 8); s->recs[i].ax = rd16(rb + i * 12 + 10);
    }
    free(rb);
    meta = 8 + NANIM * 4 + (long)nrec * 12;
    off = (meta + 63) & ~63L;
    fseek(f, off, SEEK_SET);
    s->pages = (unsigned char *)memalign(64, (size_t)npages * PAGE_BYTES);
    if (!s->pages || fread(s->pages, 1, (size_t)npages * PAGE_BYTES, f) != (size_t)npages * PAGE_BYTES) {
        fclose(f); return 0;
    }
    fclose(f);
    s->npages = npages; s->nrecs = nrec;
    sceKernelDcacheWritebackAll();
    return 1;
}
static void free_sprites(Sprites *s) {
    free(s->recs); free(s->pages); memset(s, 0, sizeof(*s));
}

static unsigned short *g_port;           /* NCHAR * 128*128 */
typedef struct { unsigned short *sky, *ground; int id; } Stage;
static Stage g_stage = {0, 0, -1};

static int load_file(const char *name, void *dst, size_t n) {
    char path[300];
    FILE *f;
    size_t r;
    snprintf(path, sizeof path, "%sdata/%s", g_base, name);
    f = fopen(path, "rb");
    if (!f) return 0;
    r = fread(dst, 1, n, f);
    fclose(f);
    return r == n;
}
static int load_stage(int id) {
    char nm[32];
    FILE *f;
    char path[300];
    if (g_stage.id == id) return 1;
    free(g_stage.sky); free(g_stage.ground);
    g_stage.sky = (unsigned short *)memalign(64, 256 * 256 * 2);
    g_stage.ground = (unsigned short *)memalign(64, 512 * 256 * 2);
    g_stage.id = -1;
    snprintf(nm, sizeof nm, "stage_%d.dat", id);
    snprintf(path, sizeof path, "%sdata/%s", g_base, nm);
    f = fopen(path, "rb");
    if (!f) return 0;
    if (fread(g_stage.sky, 1, 256 * 256 * 2, f) != 256 * 256 * 2 ||
        fread(g_stage.ground, 1, 512 * 256 * 2, f) != 512 * 256 * 2) { fclose(f); return 0; }
    fclose(f);
    sceKernelDcacheWritebackAll();
    g_stage.id = id;
    return 1;
}
static void draw_stage(float cam) {
    int ox;
    if (g_stage.id < 0) return;
    bind_tex(g_stage.sky, GU_PSM_5650, 256, 256);
    set_filter(1); set_wrap(0);
    draw_tex(0, 0, 256, 256, 0, 0, SCR_W, SCR_H, 0, WHITE);
    bind_tex(g_stage.ground, GU_PSM_5551, 512, 256);
    set_filter(0); set_wrap(1);
    ox = ((int)(cam * 0.5f)) & 511;
    draw_tex(ox, 0, SCR_W, 256, 0, SCR_H - 256, SCR_W, 256, 0, WHITE);
    set_wrap(0);
}

/* ------------------------------------------------------------------ */
/* Lutadores                                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    int ch;
    Sprites *spr;
    float x, h, vx, vh;
    int face, st, anim, af, at, done;
    int hp, ki, stun, atkk, hit_done, next, jt, dash_t, cool, invul, lie, bounced;
    int hitalt, fired, charging, hflash, combo, combo_t, wins, ko, guardflash;
    float hp_disp;
    int is_cpu, ai_t, ai_mode, ai_cd;
} Fighter;

typedef struct { int on; float x, hgt, vx; int owner, dmg, kind, life; float r; u32 col; } Proj;
typedef struct { int on; float x, y, size; int t, life; u32 col; } Spark;

static Sprites g_spr[2];
static Fighter F[2];
static Proj g_proj[12];
static Spark g_spark[24];
static float g_cam = 0;
static int g_shake = 0, g_flash = 0, g_hitstop = 0;

static void set_anim(Fighter *f, int a) { if (f->anim != a) { f->anim = a; f->af = 0; f->at = 0; f->done = 0; } }
static void force_anim(Fighter *f, int a) { f->anim = a; f->af = 0; f->at = 0; f->done = 0; }
static AnimRec *arec(Fighter *f) { return &f->spr->anim[f->anim]; }
static int tick_anim(Fighter *f, int loop) {
    AnimRec *ar = arec(f);
    if (f->done && !loop) return 1;
    f->at++;
    if (f->at >= ar->ticks) {
        f->at = 0;
        if (f->af + 1 >= ar->count) {
            if (loop) f->af = 0; else { f->af = ar->count - 1; f->done = 1; }
        } else f->af++;
    }
    return f->done;
}

static void add_spark(float x, float y, float size, u32 col, int life) {
    int i;
    for (i = 0; i < 24; i++)
        if (!g_spark[i].on) {
            g_spark[i].on = 1; g_spark[i].x = x; g_spark[i].y = y; g_spark[i].size = size;
            g_spark[i].t = 0; g_spark[i].life = life; g_spark[i].col = col;
            return;
        }
}

/* resultado: 0 = ignorado, 1 = acertou, 2 = defendeu */
static int apply_hit(Fighter *t, Fighter *a, int dmg, int kb, int stun, int knock) {
    int dir = (t->x >= a->x) ? 1 : -1;
    float sy = GROUND_Y - t->h - 62;
    if (t->invul > 0 || t->st == S_LIE || t->st == S_GETUP || t->hp <= 0) return 0;
    if (t->st == S_GUARD && (a->x - t->x) * t->face > 0) {
        dmg = dmg / 5; if (dmg < 1) dmg = 1;
        t->hp -= dmg; if (t->hp < 1) t->hp = 1;
        t->vx = dir * 2.5f; t->guardflash = 8;
        t->ki += 15; if (t->ki > KI_MAX) t->ki = KI_MAX;
        add_spark(t->x - g_cam + t->face * 14, sy, 22, RGBA(120, 200, 255, 255), 8);
        g_hitstop = 2;
        return 2;
    }
    t->hp -= dmg;
    a->ki += dmg / 3 + 6; if (a->ki > KI_MAX) a->ki = KI_MAX;
    t->ki += 10; if (t->ki > KI_MAX) t->ki = KI_MAX;
    a->combo++; a->combo_t = 70;
    t->hflash = 6; t->charging = 0;
    t->face = -dir;
    if (t->hp <= 0) { t->hp = 0; t->ko = 1; knock = 1; }
    add_spark(t->x - g_cam, sy, knock ? 46.f : 30.f, RGBA(255, 240, 160, 255), 10);
    g_hitstop = knock ? 6 : 3;
    if (knock) g_shake = 8;
    if (knock || t->h > 0.5f || t->ko) {
        t->st = S_FALL; t->vx = dir * (kb + 1.5f); t->vh = t->h > 0.5f ? 5.0f : 6.5f;
        if (t->h < 0.5f) t->h = 0.5f;
        t->bounced = 0; force_anim(t, A_FALL);
    } else {
        t->st = S_HIT; t->stun = stun; t->vx = dir * kb * 0.7f;
        t->hitalt ^= 1; force_anim(t, t->hitalt ? A_HIT2 : A_HIT);
    }
    return 1;
}

static void fire_proj(Fighter *f, int idx, int kind) {
    int i;
    for (i = 0; i < 12; i++)
        if (!g_proj[i].on) {
            Proj *p = &g_proj[i];
            p->on = 1; p->x = f->x + f->face * 40; p->hgt = f->h + 58;
            p->vx = f->face * (kind ? 11.0f : 7.5f);
            p->owner = idx; p->dmg = (int)((kind ? 260 : 55) * CH[f->ch].power);
            p->kind = kind; p->life = 140; p->r = kind ? 30.f : 13.f; p->col = CH[f->ch].color;
            return;
        }
}

static void start_attack(Fighter *f, int kind) {
    f->st = S_ATK; f->atkk = kind; f->hit_done = 0; f->next = 0;
    force_anim(f, kind == 1 ? A_ATK1 : kind == 2 ? A_ATK2 : kind == 3 ? A_ATK3 : A_AIR);
    if (kind != 4) f->vx = 0;
}

static void fighter_update(Fighter *f, Fighter *e, int idx, unsigned held, unsigned press, int can_act) {
    AnimRec *ar;
    int dir, n;
    if (f->invul > 0) f->invul--;
    if (f->cool > 0) f->cool--;
    if (f->hflash > 0) f->hflash--;
    if (f->guardflash > 0) f->guardflash--;
    if (f->combo_t > 0 && --f->combo_t == 0) f->combo = 0;
    f->charging = 0;
    if (!can_act) { held = 0; press = 0; }

    switch (f->st) {
    case S_IDLE: case S_WALK: case S_GUARD:
        f->face = (e->x >= f->x) ? 1 : -1;
        f->vx = 0;
        if (f->ko) { f->st = S_IDLE; break; }
        if (f->cool == 0 && (press & IN_S) && f->ki >= SUPER_COST) {
            f->ki -= SUPER_COST; f->st = S_SUPER; force_anim(f, A_SUPER); f->fired = 0; break;
        }
        if (f->cool == 0 && (press & IN_B) && f->ki >= BLAST_COST) {
            f->ki -= BLAST_COST; f->st = S_BLAST; force_anim(f, A_BLAST); f->fired = 0; break;
        }
        if (f->cool == 0 && (press & IN_K)) { start_attack(f, 3); break; }
        if (f->cool == 0 && (press & IN_P)) { start_attack(f, 1); break; }
        dir = ((held & IN_R) ? 1 : 0) - ((held & IN_L) ? 1 : 0);
        if (press & IN_U) {
            f->st = S_JUMP; f->vh = 11.5f; f->vx = dir * CH[f->ch].speed * 1.3f; f->jt = 0; f->h = 0.1f;
            force_anim(f, A_JUMP); break;
        }
        if (press & IN_DASH) {
            int d = dir ? dir : f->face;
            f->st = S_DASH; f->dash_t = 12; f->vx = d * 8.0f; f->invul = 6;
            if (d != f->face) f->vx = d * 6.5f;
            force_anim(f, A_DASH); break;
        }
        if (held & IN_C) {
            f->charging = 1; f->ki += 5; if (f->ki > KI_MAX) f->ki = KI_MAX;
            f->st = S_IDLE; set_anim(f, A_IDLE); tick_anim(f, 1); break;
        }
        if (held & IN_D) { f->st = S_GUARD; set_anim(f, A_GUARD); tick_anim(f, 0); break; }
        if (dir) { f->st = S_WALK; f->vx = dir * CH[f->ch].speed; set_anim(f, A_WALK); tick_anim(f, 1); }
        else { f->st = S_IDLE; set_anim(f, A_IDLE); tick_anim(f, 1); }
        if (f->ki < KI_MAX) f->ki += (f->ki % 3 == 0);
        break;

    case S_JUMP:
        f->jt++;
        if (press & (IN_P | IN_K)) { start_attack(f, 4); break; }
        ar = arec(f);
        f->af = f->jt * ar->count / 44; if (f->af >= ar->count) f->af = ar->count - 1;
        f->at = 0;
        break;

    case S_DASH:
        set_anim(f, A_DASH); tick_anim(f, 1);
        if (--f->dash_t <= 0) { f->st = S_IDLE; f->vx = 0; f->cool = 4; }
        break;

    case S_ATK: {
        const AtkP *p = &ATKP[f->atkk];
        int fa, fb, active, dn;
        ar = arec(f); n = ar->count;
        fa = n / 3; fb = (n * 2) / 3; if (fb < fa) fb = fa;
        active = (f->af >= fa && f->af <= fb);
        if (can_act && f->h <= 0.5f) {
            if ((press & IN_P) && f->atkk >= 1 && f->atkk <= 2) f->next = f->atkk + 1;
            if ((press & IN_K) && f->atkk < 3) f->next = 3;
        }
        if (f->atkk != 4 && f->af < fa) f->vx = f->face * 1.6f; else if (f->atkk != 4) f->vx = 0;
        if (active && !f->hit_done) {
            float dx = (e->x - f->x) * f->face;
            if (dx > -8 && dx < p->reach + 20 && fabsf(f->h - e->h) < 90) {
                int r = apply_hit(e, f, (int)(p->dmg * CH[f->ch].power), p->kb, p->stun, p->knock);
                if (r) f->hit_done = 1;
            }
        }
        dn = tick_anim(f, 0);
        if (f->hit_done && f->next && f->af > fb && f->h <= 0.5f) { start_attack(f, f->next); break; }
        if (dn) {
            if (f->next && f->h <= 0.5f) start_attack(f, f->next);
            else if (f->h > 0.5f) { f->st = S_JUMP; f->jt = 24; force_anim(f, A_JUMP); f->af = arec(f)->count - 1; }
            else { f->st = S_IDLE; f->cool = 5; f->vx = 0; }
        }
        break;
    }

    case S_BLAST:
        ar = arec(f); n = ar->count; f->vx = 0;
        if (!(f->fired & 1) && f->af >= n / 2) { f->fired |= 1; fire_proj(f, idx, 0); }
        if (n >= 5 && !(f->fired & 2) && f->af >= n - 2) { f->fired |= 2; fire_proj(f, idx, 0); }
        if (tick_anim(f, 0)) { f->st = S_IDLE; f->cool = 8; }
        break;

    case S_SUPER:
        ar = arec(f); n = ar->count; f->vx = 0;
        if (!(f->fired & 1) && f->af >= (n * 55) / 100) {
            f->fired |= 1; fire_proj(f, idx, 1); g_flash = 10; g_shake = 10;
        }
        if (!(f->fired & 1)) f->charging = 1;
        if (tick_anim(f, 0)) { f->st = S_IDLE; f->cool = 12; }
        break;

    case S_HIT:
        f->vx *= 0.85f;
        tick_anim(f, 0);
        if (--f->stun <= 0) { f->st = S_IDLE; f->vx = 0; }
        break;

    case S_FALL:
        ar = arec(f);
        f->af = (f->vh > 0) ? 0 : 1; if (f->af >= ar->count) f->af = ar->count - 1;
        f->at = 0;
        break;

    case S_LIE:
        ar = arec(f); f->af = ar->count - 1; f->at = 0; f->vx = 0;
        if (!f->ko && --f->lie <= 0) { f->st = S_GETUP; force_anim(f, A_LIE); }
        break;

    case S_GETUP:
        f->invul = 2; f->vx = 0;
        if (tick_anim(f, 0)) { f->st = S_IDLE; f->invul = 20; f->done = 0; }
        break;

    case S_WIN:
        f->vx = 0; set_anim(f, A_WIN); tick_anim(f, 1);
        break;
    }

    /* fisica */
    f->x += f->vx;
    if (f->st == S_JUMP || f->st == S_FALL || (f->st == S_ATK && f->h > 0.5f) || f->h > 0.01f) {
        f->h += f->vh; f->vh -= GRAV;
        if (f->h <= 0) {
            f->h = 0;
            if (f->st == S_FALL) {
                if (!f->bounced && fabsf(f->vh) > 5.0f) { f->bounced = 1; f->vh = -f->vh * 0.35f; f->vx *= 0.5f; g_shake = 5; }
                else { f->vh = 0; f->vx = 0; f->st = S_LIE; f->lie = 45; g_shake = 6; }
            } else {
                f->vh = 0; f->vx = 0;
                if (f->st == S_JUMP || f->st == S_ATK) { f->st = S_IDLE; f->cool = 3; }
            }
        }
    }
    if (f->st == S_FALL && f->h <= 0 && f->vh == 0) f->st = S_LIE;
    if (f->st != S_FALL && f->st != S_LIE) { /* limites do mundo */ }
    if (f->x < 28) f->x = 28;
    if (f->x > WORLD_W - 28) f->x = WORLD_W - 28;
    if (f->ki > KI_MAX) f->ki = KI_MAX;
    if (f->hp_disp > f->hp) f->hp_disp -= 3.0f; else f->hp_disp = (float)f->hp;
    if (f->hp_disp < f->hp) f->hp_disp = (float)f->hp;
}

/* ------------------------------------------------------------------ */
/* IA                                                                  */
/* ------------------------------------------------------------------ */
static int g_diff = 1; /* 0 facil, 1 normal, 2 dificil */

static void ai_think(Fighter *f, Fighter *e, unsigned *held, unsigned *press) {
    float dist = fabsf(e->x - f->x);
    unsigned to = (e->x > f->x) ? IN_R : IN_L, away = (to == IN_R) ? IN_L : IN_R;
    int enemy_atk = (e->st == S_ATK || e->st == S_BLAST || e->st == S_SUPER);
    *held = 0; *press = 0;
    if (f->st >= S_HIT) return;
    if (f->ai_cd > 0) f->ai_cd--;
    if (f->ai_t <= 0) {
        int r = rand() % 100;
        int guardp = g_diff == 0 ? 10 : g_diff == 1 ? 30 : 55;
        f->ai_t = (g_diff == 0 ? 28 : g_diff == 1 ? 18 : 10) + rand() % 20;
        if (enemy_atk && dist < 130 && r < guardp) f->ai_mode = 3;
        else if (e->st == S_SUPER && dist < 400) f->ai_mode = 3;
        else if (dist > 260) {
            if (f->ki >= SUPER_COST && r < 35) f->ai_mode = 6;
            else if (f->ki >= BLAST_COST && r < 55) f->ai_mode = 4;
            else if (f->ki < 250 && r < 70) { f->ai_mode = 5; f->ai_t = 40; }
            else f->ai_mode = 0;
        } else if (dist > 100) {
            if (f->ki >= SUPER_COST && r < 25) f->ai_mode = 6;
            else if (f->ki >= BLAST_COST && r < 35) f->ai_mode = 4;
            else if (r < 45) f->ai_mode = 7;
            else f->ai_mode = 0;
        } else {
            if (r < 60) f->ai_mode = 1;
            else if (r < 75) f->ai_mode = 3;
            else if (r < 88) f->ai_mode = 2;
            else f->ai_mode = 7;
        }
    }
    f->ai_t--;
    switch (f->ai_mode) {
    case 0: *held |= to; if (g_diff == 2 && dist > 200 && f->ai_cd == 0 && rand() % 40 == 0) { *press |= IN_DASH; f->ai_cd = 40; } break;
    case 1:
        if (dist > 80) *held |= to;
        if (f->ai_cd == 0) {
            int r = rand() % 100;
            if (r < 55) *press |= IN_P; else if (r < 85) *press |= IN_K;
            f->ai_cd = g_diff == 0 ? 24 : g_diff == 1 ? 14 : 8;
        }
        if (g_diff > 0 && f->st == S_ATK && f->hit_done && rand() % 3 == 0) *press |= IN_P;
        break;
    case 2: *held |= away; if (rand() % 30 == 0) *press |= IN_U; break;
    case 3: *held |= IN_D; break;
    case 4: *press |= IN_B; f->ai_mode = 0; f->ai_t = 25; break;
    case 5: *held |= IN_C; break;
    case 6: *press |= IN_S; f->ai_mode = 0; f->ai_t = 40; break;
    case 7: *held |= to; if (f->ai_cd == 0) { *press |= IN_U; f->ai_cd = 50; } break;
    }
}

/* ------------------------------------------------------------------ */
/* Partida                                                             */
/* ------------------------------------------------------------------ */
static int g_scene = SC_TITLE;
static int g_phase = P_INTRO, g_phase_t = 0, g_round = 1, g_timer = 0, g_paused = 0;
static int g_sel = 0, g_stage_sel = NSTAGE, g_menu = 0, g_p1 = 0, g_p2 = 1, g_stage_id = 0;
static int g_result_win = 0, g_frame = 0, g_loading = 0, g_pending = 0;
static unsigned g_prev_buttons = 0, g_prev_held = 0;

static void start_round(void) {
    int i;
    float mid = WORLD_W / 2.0f;
    for (i = 0; i < 2; i++) {
        Fighter *f = &F[i];
        f->x = mid + (i ? 110.f : -110.f); f->h = 0; f->vx = f->vh = 0;
        f->face = i ? -1 : 1; f->st = S_IDLE; f->hp = HP_MAX; f->hp_disp = HP_MAX;
        f->ki = 200; f->stun = 0; f->invul = 0; f->ko = 0; f->cool = 0; f->combo = 0;
        f->ai_t = 30; f->ai_mode = 0; f->ai_cd = 0; f->charging = 0; f->hflash = 0;
        force_anim(f, A_IDLE);
    }
    memset(g_proj, 0, sizeof g_proj); memset(g_spark, 0, sizeof g_spark);
    g_cam = mid - SCR_W / 2.0f;
    g_phase = P_INTRO; g_phase_t = 0; g_timer = ROUND_SECS * 60; g_flash = 0; g_shake = 0;
}

static int start_match(int p1, int p2, int stage) {
    int i;
    if (!load_sprites(p1, &g_spr[0]) || !load_sprites(p2, &g_spr[1])) return 0;
    if (!load_stage(stage)) return 0;
    memset(F, 0, sizeof F);
    for (i = 0; i < 2; i++) {
        F[i].ch = i ? p2 : p1; F[i].spr = &g_spr[i]; F[i].is_cpu = i;
    }
    g_round = 1; g_stage_id = stage;
    start_round();
    g_scene = SC_FIGHT; g_paused = 0;
    return 1;
}
static void end_match(void) {
    free_sprites(&g_spr[0]); free_sprites(&g_spr[1]);
}

static void separate_and_limit(void) {
    float dx = F[1].x - F[0].x, ad = fabsf(dx), mid;
    float tcam;
    if (ad < 30.f && F[0].h < 40 && F[1].h < 40 && F[0].st != S_LIE && F[1].st != S_LIE) {
        float push = (30.f - ad) * 0.5f, s = (dx >= 0) ? 1.f : -1.f;
        F[0].x -= s * push; F[1].x += s * push;
    }
    if (ad > 410.f) {
        float over = (ad - 410.f) * 0.5f, s = (dx >= 0) ? 1.f : -1.f;
        F[0].x += s * over; F[1].x -= s * over;
    }
    mid = (F[0].x + F[1].x) * 0.5f;
    tcam = mid - SCR_W / 2.0f;
    if (tcam < 0) tcam = 0;
    if (tcam > WORLD_W - SCR_W) tcam = WORLD_W - SCR_W;
    g_cam += (tcam - g_cam) * 0.18f;
    {
        int i;
        for (i = 0; i < 2; i++) {
            if (F[i].x < g_cam + 26) F[i].x = g_cam + 26;
            if (F[i].x > g_cam + SCR_W - 26) F[i].x = g_cam + SCR_W - 26;
        }
    }
}

static void update_projectiles(void) {
    int i, j;
    for (i = 0; i < 12; i++) {
        Proj *p = &g_proj[i];
        Fighter *t;
        if (!p->on) continue;
        p->x += p->vx; p->life--;
        if (p->life <= 0 || p->x < g_cam - 80 || p->x > g_cam + SCR_W + 80) { p->on = 0; continue; }
        t = &F[1 - p->owner];
        if (fabsf(p->x - t->x) < 22 + p->r && p->hgt > t->h - 15 && p->hgt < t->h + 120 + p->r) {
            int r = apply_hit(t, &F[p->owner], p->dmg, p->kind ? 10 : 6, 22, p->kind);
            if (r) { p->on = 0; add_spark(p->x - g_cam, GROUND_Y - p->hgt, p->kind ? 70.f : 36.f, p->col, 12); continue; }
        }
        for (j = i + 1; j < 12; j++) {
            Proj *q = &g_proj[j];
            if (q->on && q->owner != p->owner && fabsf(q->x - p->x) < p->r + q->r && fabsf(q->hgt - p->hgt) < p->r + q->r) {
                add_spark((p->x + q->x) * 0.5f - g_cam, GROUND_Y - p->hgt, 40.f, RGBA(255, 255, 255, 255), 10);
                if (p->dmg >= q->dmg) q->on = 0;
                if (q->dmg >= p->dmg) p->on = 0;
                if (!p->on) break;
            }
        }
    }
    for (i = 0; i < 24; i++)
        if (g_spark[i].on && ++g_spark[i].t >= g_spark[i].life) g_spark[i].on = 0;
}

static void check_round_end(void) {
    int i;
    if (g_phase != P_FIGHT) return;
    if (F[0].hp <= 0 || F[1].hp <= 0 || g_timer <= 0) {
        int w = -1;
        if (F[0].hp <= 0 && F[1].hp <= 0) w = -1;
        else if (F[0].hp <= 0) w = 1;
        else if (F[1].hp <= 0) w = 0;
        else if (F[0].hp > F[1].hp) w = 0;
        else if (F[1].hp > F[0].hp) w = 1;
        if (w >= 0) F[w].wins++;
        g_phase = P_KO; g_phase_t = 0;
        for (i = 0; i < 2; i++) {
            if (i == w && F[i].st != S_FALL && F[i].st != S_LIE) { F[i].st = S_WIN; force_anim(&F[i], A_WIN); }
        }
        g_pending = w;
    }
}

static void fight_update(unsigned held0, unsigned press0) {
    unsigned h1, p1;
    int can = (g_phase == P_FIGHT);
    g_frame++;
    if (g_hitstop > 0) { g_hitstop--; return; }
    if (g_phase == P_KO && g_phase_t < 70 && (g_frame & 1)) { g_phase_t++; return; }
    if (g_shake > 0) g_shake--;
    if (g_flash > 0) g_flash--;
    g_phase_t++;
    if (g_phase == P_INTRO && g_phase_t >= 100) { g_phase = P_FIGHT; g_phase_t = 0; }
    if (g_phase == P_FIGHT && g_timer > 0) g_timer--;
    if (can) ai_think(&F[1], &F[0], &h1, &p1); else { h1 = 0; p1 = 0; }
    fighter_update(&F[0], &F[1], 0, held0, press0, can);
    fighter_update(&F[1], &F[0], 1, h1, p1, can);
    /* se o ultimo golpe derrubou, F[1] ja foi atualizado; ok */
    separate_and_limit();
    update_projectiles();
    check_round_end();
    if (g_phase == P_KO && g_phase_t >= 200) {
        int i;
        if (F[0].wins >= 2 || F[1].wins >= 2) {
            g_result_win = (F[0].wins >= 2); g_scene = SC_RESULT;
        } else {
            if (g_pending >= 0 || 1) g_round++;
            start_round();
            for (i = 0; i < 2; i++) { }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Renderizacao                                                        */
/* ------------------------------------------------------------------ */
static void draw_fighter(Fighter *f, float camx) {
    AnimRec *ar = arec(f);
    Rec *r = &f->spr->recs[ar->start + (f->af < ar->count ? f->af : ar->count - 1)];
    int sx = (int)(f->x - camx), sy = GROUND_Y - (int)f->h;
    int x0 = sx - (f->face > 0 ? r->ax : (r->w - r->ax));
    int y0 = sy - r->h;
    u32 tint = WHITE;
    if (f->hflash > 0 && (f->hflash & 2)) tint = RGBA(255, 160, 160, 255);
    if (f->invul > 2 && f->st == S_GETUP && (g_frame & 2)) tint = RGBA(255, 255, 255, 140);
    bind_tex(f->spr->pages + (size_t)r->page * PAGE_BYTES, GU_PSM_5551, PAGE_W, PAGE_W);
    set_filter(0); set_wrap(0);
    draw_tex(r->x, r->y, r->w, r->h, x0, y0, r->w, r->h, f->face < 0, tint);
}

static void draw_shadow(Fighter *f, float camx) {
    float s = 1.0f - f->h / 260.0f;
    if (s < 0.3f) s = 0.3f;
    glow(f->x - camx, GROUND_Y + 2, 34 * s, 8 * s, RGBA(0, 0, 0, 150), 0);
}

static void draw_aura(Fighter *f, float camx, int t) {
    float pulse = 1.0f + 0.12f * sinf(t * 0.5f);
    float sx = f->x - camx, sy = GROUND_Y - f->h - 55;
    u32 c = CH[f->ch].color; c = (c & 0x00FFFFFFu) | (150u << 24);
    glow(sx, sy, 62 * pulse, 84 * pulse, c, 1);
}

static void draw_projectiles(float camx) {
    int i;
    for (i = 0; i < 12; i++) {
        Proj *p = &g_proj[i];
        float sx, sy;
        if (!p->on) continue;
        sx = p->x - camx; sy = GROUND_Y - p->hgt;
        if (p->kind) {
            glow(sx - p->vx * 3.0f, sy, p->r * 4.2f, p->r * 1.5f, (p->col & 0x00FFFFFFu) | (200u << 24), 1);
            glow(sx, sy, p->r * 1.9f, p->r * 1.9f, p->col, 1);
            glow(sx, sy, p->r * 1.0f, p->r * 1.0f, WHITE, 1);
        } else {
            glow(sx, sy, p->r * 2.0f, p->r * 2.0f, p->col, 1);
            glow(sx, sy, p->r * 1.0f, p->r * 1.0f, WHITE, 1);
        }
    }
}
static void draw_sparks(void) {
    int i;
    for (i = 0; i < 24; i++) {
        Spark *s = &g_spark[i];
        float k;
        u32 a;
        if (!s->on) continue;
        k = (float)s->t / (float)s->life;
        a = (u32)(255 * (1.0f - k));
        glow(s->x, s->y, s->size * (0.6f + k), s->size * (0.6f + k), (s->col & 0x00FFFFFFu) | (a << 24), 1);
    }
}

static u32 hp_color(float r) {
    if (r > 0.5f) return RGBA(80, 220, 60, 255);
    if (r > 0.25f) return RGBA(240, 210, 40, 255);
    return RGBA(240, 60, 40, 255);
}

static void draw_bar(int x, int y, int w, int h, float v, float trail, int right, u32 col, u32 tcol) {
    int fw = (int)(w * v), tw = (int)(w * trail);
    rect(x - 2, y - 2, w + 4, h + 4, RGBA(0, 0, 0, 200));
    rect(x, y, w, h, RGBA(50, 50, 50, 255));
    if (right) {
        rect(x + w - tw, y, tw, h, tcol);
        rect(x + w - fw, y, fw, h, col);
    } else {
        rect(x, y, tw, h, tcol);
        rect(x, y, fw, h, col);
    }
}

static void draw_hud(void) {
    char buf[48];
    int i, secs = g_timer / 60;
    for (i = 0; i < 2; i++) {
        Fighter *f = &F[i];
        float v = f->hp / (float)HP_MAX, tr = f->hp_disp / (float)HP_MAX;
        int x = i ? 276 : 14;
        draw_bar(x, 12, 190, 14, v, tr, i, hp_color(v), RGBA(255, 255, 255, 220));
        draw_bar(x, 30, 150, 6, f->ki / (float)KI_MAX, f->ki / (float)KI_MAX, i,
                 f->ki >= SUPER_COST ? RGBA(255, 255, 120, 255) : RGBA(60, 200, 255, 255), 0);
        if (i) text(x + 190 - text_w(CH[f->ch].name, 0.75f), 38, 0.75f, WHITE, CH[f->ch].name);
        else text(x, 38, 0.75f, WHITE, CH[f->ch].name);
        {
            int k;
            for (k = 0; k < 2; k++) {
                int px = i ? 252 + k * 14 : 214 - k * 14 + 0;
                rect(px, 33, 10, 10, RGBA(0, 0, 0, 200));
                rect(px + 1, 34, 8, 8, k < f->wins ? RGBA(255, 215, 0, 255) : RGBA(70, 70, 70, 255));
            }
        }
    }
    snprintf(buf, sizeof buf, "%02d", secs);
    rect(222, 8, 36, 26, RGBA(0, 0, 0, 170));
    text_c(240, 9, 1.3f, secs <= 10 ? RGBA(255, 80, 60, 255) : WHITE, buf);
    if (F[0].combo >= 2) {
        snprintf(buf, sizeof buf, "%d HITS", F[0].combo);
        text(14, 60, 1.2f, RGBA(255, 220, 60, 255), buf);
    }
    if (F[1].combo >= 2) {
        snprintf(buf, sizeof buf, "%d HITS", F[1].combo);
        text(SCR_W - 14 - text_w(buf, 1.2f), 60, 1.2f, RGBA(255, 120, 60, 255), buf);
    }
    if (F[0].ki >= SUPER_COST) text(14, 252, 0.7f, RGBA(255, 255, 120, 255), "SUPER READY (TRIANGLE)");
}

static void draw_fight(void) {
    float cx = g_cam;
    char buf[40];
    if (g_shake > 0) cx += (float)((g_frame * 7) % 5 - 2) * (g_shake > 4 ? 1.5f : 1.0f);
    draw_stage(cx);
    draw_shadow(&F[0], cx); draw_shadow(&F[1], cx);
    if (F[0].charging) draw_aura(&F[0], cx, g_frame);
    if (F[1].charging) draw_aura(&F[1], cx, g_frame + 3);
    /* quem esta atacando fica por cima */
    if (F[0].st == S_ATK || F[0].st == S_SUPER || F[0].st == S_BLAST) { draw_fighter(&F[1], cx); draw_fighter(&F[0], cx); }
    else { draw_fighter(&F[0], cx); draw_fighter(&F[1], cx); }
    draw_projectiles(cx);
    draw_sparks();
    if (g_flash > 0) rect(0, 0, SCR_W, SCR_H, RGBA(255, 255, 255, g_flash * 18));
    draw_hud();
    if (g_phase == P_INTRO) {
        if (g_phase_t < 50) { snprintf(buf, sizeof buf, "ROUND %d", g_round); text_c(240, 100, 2.4f, RGBA(255, 220, 60, 255), buf); }
        else text_c(240, 100, 2.8f, RGBA(255, 90, 50, 255), "FIGHT!");
    } else if (g_phase == P_KO || g_phase == P_END) {
        if (g_timer <= 0 && F[0].hp > 0 && F[1].hp > 0) text_c(240, 100, 2.4f, RGBA(255, 220, 60, 255), "TIME UP");
        else text_c(240, 100, 3.0f, RGBA(255, 60, 40, 255), "K.O.");
        if (g_phase_t > 60 && g_pending < 0) text_c(240, 140, 1.4f, WHITE, "DRAW");
    }
    if (g_paused) {
        rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));
        text_c(240, 90, 2.2f, WHITE, "PAUSED");
        text_c(240, 140, 1.0f, WHITE, "X: RESUME     O: QUIT TO MENU");
    }
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */
static void draw_title(void) {
    const char *dn[3] = {"EASY", "NORMAL", "HARD"};
    char buf[48];
    draw_stage((float)g_frame * 1.2f);
    rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 80));
    text_c(240, 38, 3.2f, RGBA(255, 200, 40, 255), "DRAGON BALL");
    text_c(240, 82, 1.6f, RGBA(255, 255, 255, 255), "PSP FIGHTING GAME");
    snprintf(buf, sizeof buf, "%sSTART BATTLE", g_menu == 0 ? "> " : "  ");
    text_c(240, 150, 1.3f, g_menu == 0 ? RGBA(255, 255, 120, 255) : WHITE, buf);
    snprintf(buf, sizeof buf, "%sDIFFICULTY: %s", g_menu == 1 ? "> " : "  ", dn[g_diff]);
    text_c(240, 178, 1.3f, g_menu == 1 ? RGBA(255, 255, 120, 255) : WHITE, buf);
    text_c(240, 238, 0.7f, WHITE, "DPAD: MOVE  X: PUNCH  []: KICK  O: KI BLAST  /\\: SUPER  R: CHARGE  L: DASH");
    text_c(240, 252, 0.7f, WHITE, "FAN GAME - NOT AFFILIATED WITH BANDAI NAMCO / TOEI");
}

static void draw_select(void) {
    int i;
    char buf[64];
    draw_stage((float)g_frame * 0.8f);
    rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 130));
    text_c(240, 4, 1.4f, RGBA(255, 200, 40, 255), "SELECT YOUR FIGHTER");
    for (i = 0; i < NCHAR; i++) {
        int cx = 14 + (i % 7) * 66, cy = 26 + (i / 7) * 50;
        int sel = (i == g_sel);
        rect(cx - 2, cy - 2, 66, 50, sel ? RGBA(255, 220, 60, 255) : RGBA(0, 0, 0, 200));
        rect(cx, cy, 62, 46, sel ? RGBA(70, 80, 150, 255) : RGBA(25, 25, 50, 255));
        bind_tex(g_port + i * 128 * 128, GU_PSM_5551, 128, 128);
        set_filter(1); set_wrap(0);
        draw_tex(0, 0, 128, 128, cx + 9, cy + 1, 44, 44, 0, WHITE);
    }
    text_c(240, 232, 1.3f, CH[g_sel].color | 0xFF000000u, CH[g_sel].name);
    snprintf(buf, sizeof buf, "STAGE: < %s >  (L / R)", STAGE_NAME[g_stage_sel]);
    text(10, 254, 0.7f, WHITE, buf);
    text(SCR_W - 10 - text_w("X: FIGHT   O: BACK", 0.7f), 254, 0.7f, WHITE, "X: FIGHT   O: BACK");
}

static void draw_result(void) {
    draw_stage((float)g_frame);
    rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 140));
    if (g_result_win) text_c(240, 80, 3.0f, RGBA(255, 220, 60, 255), "YOU WIN!");
    else text_c(240, 80, 3.0f, RGBA(255, 70, 50, 255), "YOU LOSE");
    text_c(240, 150, 1.2f, WHITE, CH[g_result_win ? F[0].ch : F[1].ch].name);
    text_c(240, 200, 1.0f, WHITE, "PRESS X TO CONTINUE");
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */
static unsigned pad_to_mask(const SceCtrlData *pad) {
    unsigned m = 0, b = pad->Buttons;
    if ((b & PSP_CTRL_LEFT) || pad->Lx < 64) m |= IN_L;
    if ((b & PSP_CTRL_RIGHT) || pad->Lx > 192) m |= IN_R;
    if ((b & PSP_CTRL_UP) || pad->Ly < 64) m |= IN_U;
    if ((b & PSP_CTRL_DOWN) || pad->Ly > 192) m |= IN_D;
    if (b & PSP_CTRL_CROSS) m |= IN_P;
    if (b & PSP_CTRL_SQUARE) m |= IN_K;
    if (b & PSP_CTRL_CIRCLE) m |= IN_B;
    if (b & PSP_CTRL_TRIANGLE) m |= IN_S;
    if (b & PSP_CTRL_RTRIGGER) m |= IN_C;
    if (b & PSP_CTRL_LTRIGGER) m |= IN_DASH;
    return m;
}

static void begin_fight_loading(void) { g_loading = 1; }

static void game_update(const SceCtrlData *pad) {
    unsigned held = pad_to_mask(pad), press = held & ~g_prev_held;
    unsigned bp = pad->Buttons & ~g_prev_buttons;
    g_prev_held = held; g_prev_buttons = pad->Buttons;

    switch (g_scene) {
    case SC_TITLE:
        g_frame++;
        if (bp & PSP_CTRL_UP) g_menu = 0;
        if (bp & PSP_CTRL_DOWN) g_menu = 1;
        if (bp & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
            if (g_menu == 0) g_scene = SC_SELECT; else g_diff = (g_diff + 1) % 3;
        }
        if ((bp & PSP_CTRL_RIGHT) && g_menu == 1) g_diff = (g_diff + 1) % 3;
        if ((bp & PSP_CTRL_LEFT) && g_menu == 1) g_diff = (g_diff + 2) % 3;
        break;
    case SC_SELECT:
        g_frame++;
        if (bp & PSP_CTRL_RIGHT) g_sel = (g_sel + 1) % NCHAR;
        if (bp & PSP_CTRL_LEFT) g_sel = (g_sel + NCHAR - 1) % NCHAR;
        if (bp & PSP_CTRL_DOWN) g_sel = (g_sel + 7) % NCHAR;
        if (bp & PSP_CTRL_UP) g_sel = (g_sel + NCHAR - 7) % NCHAR;
        if (bp & PSP_CTRL_RTRIGGER) g_stage_sel = (g_stage_sel + 1) % (NSTAGE + 1);
        if (bp & PSP_CTRL_LTRIGGER) g_stage_sel = (g_stage_sel + NSTAGE) % (NSTAGE + 1);
        if (bp & PSP_CTRL_CIRCLE) g_scene = SC_TITLE;
        if (bp & PSP_CTRL_CROSS) {
            g_p1 = g_sel;
            do { g_p2 = rand() % NCHAR; } while (g_p2 == g_p1);
            g_stage_id = (g_stage_sel == NSTAGE) ? rand() % NSTAGE : g_stage_sel;
            begin_fight_loading();
        }
        break;
    case SC_FIGHT:
        if (bp & PSP_CTRL_START) g_paused = !g_paused;
        if (g_paused) {
            if (bp & PSP_CTRL_CROSS) g_paused = 0;
            if (bp & PSP_CTRL_CIRCLE) { end_match(); g_paused = 0; g_scene = SC_SELECT; }
            break;
        }
        fight_update(held, press);
        break;
    case SC_RESULT:
        g_frame++;
        if (bp & (PSP_CTRL_CROSS | PSP_CTRL_START)) { end_match(); g_scene = SC_SELECT; }
        break;
    }
}

static void game_render(void) {
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    g_tex = 0; g_filter = -1; g_wrap = -1;
    blend_normal();
    switch (g_scene) {
    case SC_TITLE: draw_title(); break;
    case SC_SELECT: draw_select(); if (g_loading) { rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 200)); text_c(240, 120, 2.0f, WHITE, "LOADING..."); } break;
    case SC_FIGHT: draw_fight(); break;
    case SC_RESULT: draw_fight(); draw_result(); break;
    }
    sceGuFinish();
    sceGuSync(0, 0);
}

static void gfx_init(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)0x88000, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    blend_normal();
    sceGuFrontFace(GU_CW);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static int game_init(void) {
    gfx_init();
    make_glow();
    sceKernelDcacheWritebackAll();
    if (!load_file("font.dat", g_font, sizeof g_font)) return 0;
    build_font_metrics();
    g_port = (unsigned short *)memalign(64, NCHAR * 128 * 128 * 2);
    {
        int i;
        char nm[32];
        for (i = 0; i < NCHAR; i++) {
            snprintf(nm, sizeof nm, "port_%d.dat", i);
            if (!load_file(nm, g_port + i * 128 * 128, 128 * 128 * 2)) return 0;
        }
    }
    sceKernelDcacheWritebackAll();
    if (!load_stage(0)) return 0;
    return 1;
}

static void init_base(int argc, char **argv) {
    char *p;
    g_base[0] = 0;
    if (argc > 0 && argv && argv[0]) {
        strncpy(g_base, argv[0], sizeof g_base - 1);
        g_base[sizeof g_base - 1] = 0;
        p = strrchr(g_base, '/');
        if (p) p[1] = 0; else g_base[0] = 0;
    }
}

static void fatal_screen(const char *msg) {
    int n = 0;
    while (n < 600) {
        sceGuStart(GU_DIRECT, gu_list);
        sceGuClearColor(0xFF000040);
        sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuFinish(); sceGuSync(0, 0);
        sceDisplayWaitVblankStart(); sceGuSwapBuffers();
        n++;
    }
    (void)msg;
}

#ifndef HOST_TEST
static int exit_callback(int arg1, int arg2, void *common) { sceKernelExitGame(); return 0; }
static int callback_thread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

int main(int argc, char **argv) {
    SceCtrlData pad;
    setup_callbacks();
    init_base(argc, argv);
    srand(sceKernelGetSystemTimeLow());
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    if (!game_init()) { fatal_screen("erro ao carregar data/"); sceKernelExitGame(); return 0; }
    for (;;) {
        sceCtrlReadBufferPositive(&pad, 1);
        game_update(&pad);
        game_render();
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
        if (g_loading) {
            /* o quadro "LOADING..." ja foi exibido; carrega agora */
            g_loading = 0;
            sceGuStart(GU_DIRECT, gu_list);
            sceGuClearColor(0xFF000000); sceGuClear(GU_COLOR_BUFFER_BIT);
            sceGuFinish(); sceGuSync(0, 0);
            if (!start_match(g_p1, g_p2, g_stage_id)) g_scene = SC_SELECT;
        }
    }
    return 0;
}
#else
#include "host_main.inc"
#endif
