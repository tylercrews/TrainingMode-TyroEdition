/* Production Escape Samus Up-B event, with fixtures only at native engine
 * boundaries. The native SDI callback (8008E4F0) is a stub that moves you the
 * way an accepted SDI input does whenever a test asks for one. */
#include "../src/events.h"
static void TestNativeSDI(GOBJ *fighter);
#define ESCAPE_SAMUS_SDI TestNativeSDI
#include "../src/escapesamus.c"
#include <stdarg.h>

enum { YOU, SAMUS, PLAYERS };
static EventVars vars;
static GOBJ event, fighters[PLAYERS];
static FighterData ft[PLAYERS];
static JOBJ joints[PLAYERS];
static unsigned preferences[TM_ESC_SAMUS_PREF_COUNT] = { 0, TM_ESC_DEFAULT_DISTANCE, 0, 0 };
static int loads, saves, sfx, hud_percent[PLAYERS], panel_rows, randi, enter_waits[PLAYERS];
static float randf = .5f;
static char panel_label[4][24], panel_info[4][24], big_text_shown[16];
static GXColor big_color;
// The hit the test asks for, applied after the event's think like hit detection.
static int pending_hit, pending_damage, pending_launcher;
static int hitlag_left, sdi_queue, sdi_dir = 1, sdi_calls;

void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
static void Copy(char *out, const char *in) { while ((*out++ = *in++)) { } }
static char *Number(char *out, int value) {
    char digits[12];
    int count = 0;
    if (value < 0) { *out++ = '-'; value = -value; }
    do { digits[count++] = '0' + value % 10; value /= 10; } while (value);
    while (count) *out++ = digits[--count];
    return out;
}
/* The event's formats: %d, %s, %.1f and %%. */
int sprintf(char *out, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (; *fmt; ++fmt) {
        if (*fmt != '%') { *out++ = *fmt; continue; }
        ++fmt;
        if (*fmt == 'd') out = Number(out, va_arg(args, int));
        else if (*fmt == 's') { const char *s = va_arg(args, const char *); while (*s) *out++ = *s++; }
        else if (*fmt == '%') *out++ = '%';
        else {
            int tenths = (int)(va_arg(args, double) * 10 + .5);
            fmt += 2; /* ".1f" */
            out = Number(out, tenths / 10);
            *out++ = '.';
            *out++ = '0' + tenths % 10;
        }
    }
    *out = 0;
    va_end(args);
    return 0;
}
static u32 Read(unsigned field, unsigned index) { return field == TM_SETTING_ESCAPE_SAMUS ? preferences[index] : 0; }
static void Write(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_ESCAPE_SAMUS && index < TM_ESC_SAMUS_PREF_COUNT) { preferences[index] = value; saves++; }
}
static const TMSettingsAPI api = {Read, Write, 0};
static int Save(void *state, int flags) { return 1; }
/* The real load rewinds game_timer and puts both of you back where the match
 * began, falling, at 0%. */
static int Load(void *state, int flags) {
    ++loads;
    vars.game_timer = 1;
    for (int i = 0; i < PLAYERS; ++i) {
        ft[i].dmg.percent = ft[i].dmg.percent_temp = 0;
        ft[i].state_id = ASID_FALL;
        ft[i].state.frame = 0;
        ft[i].phys.pos = (Vec3){i ? 30 : -30, 20, 0};
        ft[i].cb.EveryHitlag = 0;
        ft[i].flags.hitlag = ft[i].flags.dead = 0;
        ft[i].input.held = 0;
        for (int h = 0; h < 4; ++h) ft[i].hitbox[h].active = 0;
    }
    hitlag_left = 0;
    return 1;
}
static void Panel(const char **label, const char **info, int count) {
    panel_rows = count;
    for (int i = 0; i < count && i < 4; ++i) { Copy(panel_label[i], label[i]); Copy(panel_info[i], info[i]); }
}
static void DrawTextEx(const char *text, Rect *pos, float size, GXColor text_color, GXColor border_color,
    float border_offset, int align) {
    Copy(big_text_shown, text);
    big_color = text_color;
}
static int WaitIASA(GOBJ *f) { return 0; }
static void StatePhys(GOBJ *f) { }

GOBJ *Fighter_GetGObj(int player) { return player < PLAYERS ? &fighters[player] : 0; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
void Fighter_SetAirborne(FighterData *f) { f->phys.air_state = 1; }
void Fighter_SetAllHurtboxState(GOBJ *f, int kind) { }
void Fighter_UpdateCameraBox(GOBJ *f) { }
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) {
    FighterData *data = f->userdata;
    data->state_id = ASID_WAIT;
    data->cb.IASA = WaitIASA;
    data->cb.Phys = StatePhys;
    enter_waits[f - fighters]++;
}
void ActionStateChange(float startFrame, float animSpeed, float animBlend, GOBJ *f, int state, int flags1, GOBJ *alt) {
    FighterData *data = f->userdata;
    data->state_id = state;
    data->state.frame = startFrame;
    data->cb.IASA = WaitIASA;
    data->cb.Phys = StatePhys;
}
/* Battlefield's main stage: y = 0 from -68.4 to 68.4. */
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *unk, int a, int b, int c, void *cb,
    float fromX, float fromY, float toX, float toY, float unk5) {
    if (fromX < -68.4f || fromX > 68.4f) return 0;
    *pos = (Vec3){fromX, 0, 0};
    *line = 2;
    return 1;
}
void Match_CorrectCamera(void) { }
void Fighter_SetHUDDamage(int player, u16 percent) { hud_percent[player] = percent; }
void Fighter_ZeroCPUInputs(FighterData *f) {
    f->cpu.held = 0;
    f->cpu.lstickX = f->cpu.lstickY = 0;
}
int Pause_CheckStatus(int kind) { return 0; }
int SFX_Play(int id) { sfx = id; return 0; }
int HSD_Randi(int max) { return max ? randi % max : 0; }
float HSD_Randf(void) { return randf; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { }
void GObj_AddGXLink(GOBJ *gobj, void *cb, int gx_link, int gx_pri) { }
GOBJ *GObj_Create(int a, int b, int c) { return &event; }
GOBJProc *GObj_AddProc(GOBJ *gobj, void *cb, int pri) { return 0; }

/* An accepted SDI moves you 6 along the stick; the rest don't move you. */
static void TestNativeSDI(GOBJ *fighter) {
    FighterData *data = fighter->userdata;
    sdi_calls++;
    if (!data->flags.hitlag || !sdi_queue) return;
    sdi_queue--;
    data->phys.pos.X += 6 * sdi_dir;
}

int TestEscInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event;
    vars.settings = &api;
    vars.Savestate_Save_v1 = (void *)Save;
    vars.Savestate_Load_v1 = (void *)Load;
    vars.HUD_DrawInfoPanel = Panel;
    vars.HUD_DrawTextEx = DrawTextEx;
    for (int i = 0; i < PLAYERS; ++i) {
        fighters[i].userdata = &ft[i];
        fighters[i].hsd_object = &joints[i];
        ft[i].ply = i;
        ft[i].state.rate = 1;
        ft[i].pad_index = i;
    }
    ft[SAMUS].kind = FTKIND_SAMUS;
    Event_Init(&event);
    return 0;
}

/* One frame, in the game's order: Samus's animation (s_link 1), the event's
 * think and the fighters' input (3), physics with its hitlag callback (4),
 * hit detection and damage (13, 14), then the event's s_link 15 proc. */
void TestEscThink(int frames) {
    while (frames--) {
        for (int i = 0; i < PLAYERS; ++i) {
            ft[i].dmg.percent += ft[i].dmg.percent_temp;
            ft[i].dmg.percent_temp = 0;
        }
        FighterData *samus = &ft[SAMUS], *you = &ft[YOU];
        if (InUpB(samus) && !samus->flags.hitlag && ++samus->state.frame >= 50) samus->state_id = ASID_FALLSPECIAL;
        ++vars.game_timer;
        Event_Think(&event);
        // Samus's input: a fresh B with the stick up from Wait is her up-B.
        if (samus->state_id == ASID_WAIT && (samus->cpu.held & ~samus->input.held & PAD_BUTTON_B) && samus->cpu.lstickY > 64) {
            samus->state_id = SAMUS_UPB_GROUND;
            samus->state.frame = 0;
        }
        samus->input.held = samus->cpu.held;
        // Physics: the hitlag callback, then hitlag running out.
        if (you->flags.hitlag) {
            if (you->cb.EveryHitlag) you->cb.EveryHitlag(&fighters[YOU]);
            if (--hitlag_left <= 0) you->flags.hitlag = 0;
        }
        for (int h = 0; h < 4; ++h) samus->hitbox[h].active = 0;
        if (pending_hit) {
            pending_hit = 0;
            samus->hitbox[0].active = 1;
            samus->hitbox[0].wdsk = pending_launcher ? 80 : 0;
            you->dmg.percent_temp += pending_damage;
            you->state_id = ASID_DAMAGEFLYN;
            you->cb.IASA = WaitIASA;
            you->cb.Phys = StatePhys;
            you->cb.EveryHitlag = TestNativeSDI; // as damage entry does
            you->flags.hitlag = 1;
            hitlag_left = 3;
        }
        AfterHits(&event);
        big_text_shown[0] = 0;
        DrawHUD(&event, 2);
    }
}
void TestEscHit(int damage, int launcher) { pending_hit = 1; pending_damage = damage; pending_launcher = launcher; }
void TestEscSDI(int count, int dir) { sdi_queue = count; sdi_dir = dir; }
void TestEscRandom(int value, int fraction_percent) { randi = value; randf = fraction_percent / 100.f; }
void TestEscDPad(int buttons) {
    stc_engine_pads->pad[0].down = buttons;
    stc_engine_pads->pad[0].held = buttons;
}
void TestEscChoose(int row, int value) {
    Options_Main[row].val = value;
    Options_Main[row].OnChange(0, value);
}
void TestEscState(int who, int state) { ft[who].state_id = state; }
void TestEscRestart(void) { restart = true; }
void TestEscReWait(void) { Fighter_EnterWait(&fighters[YOU]); }
void TestEscKill(int who) { ft[who].flags.dead = 1; }
static int Tenths(float x) { return (int)(x * 10 + (x < 0 ? -.5f : .5f)); }
int TestEscInfo(int key) {
    switch (key) {
    case 0: return loads;
    case 1: return phase;
    case 2: return Tenths(ft[YOU].phys.pos.X);
    case 3: return Tenths(ft[YOU].phys.pos.Y);
    case 4: return Tenths(ft[SAMUS].phys.pos.X);
    case 5: return Tenths(ft[SAMUS].phys.pos.Y);
    case 6: return ft[YOU].facing_direction > 0 ? 1 : -1;
    case 7: return ft[SAMUS].facing_direction > 0 ? 1 : -1;
    case 8: return ft[YOU].state_id;
    case 9: return ft[SAMUS].state_id;
    case 10: return ft[YOU].cb.IASA != 0;
    case 11: return ft[YOU].cb.Phys == HoldInAir;
    case 12: return ft[YOU].phys.air_state;
    case 13: return hud_percent[YOU];
    case 14: return (int)ft[YOU].dmg.percent;
    case 15: return attempt.hits;
    case 16: return attempt.sdi;
    case 17: return attempt.launched;
    case 18: return stats.attempts;
    case 19: return stats.escapes;
    case 20: return stats.last;
    case 21: return sfx;
    case 22: return ft[YOU].cb.EveryHitlag == CountSDI;
    case 23: return sdi_calls;
    case 24: return panel_rows;
    case 25: return big_color.r << 16 | big_color.g << 8 | big_color.b;
    case 26: return preferences[TM_ESC_SAMUS_POSITION];
    case 27: return preferences[TM_ESC_SAMUS_DISTANCE];
    case 28: return preferences[TM_ESC_SAMUS_SIDE];
    case 29: return preferences[TM_ESC_SAMUS_PERCENT];
    case 30: return saves;
    case 31: return ft[YOU].jump.jumps_used;
    case 32: return ft[SAMUS].cpu.ai;
    case 33: return ft[YOU].input.held;
    }
    return -1;
}
char *TestEscText(int key) {
    return key < 4 ? panel_label[key] : key < 8 ? panel_info[key - 4] : big_text_shown;
}
