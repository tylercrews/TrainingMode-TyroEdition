/* Production Escape IC Grab event, with fixtures only at native engine
 * boundaries: Popo's grab, the grab timer the game sets when it connects, and
 * the capture states' per-frame tick (1 a frame, 6 a mash input, out at 0). */
#include "../src/escapeic.c"
#include <stdarg.h>

enum { YOU, POPO, NANA, BODIES };
static EventVars vars;
static GOBJ event, fighters[BODIES];
static FighterData ft[BODIES];
static JOBJ joints[BODIES];
static ftCommonData common;
static unsigned preferences[TM_ESC_IC_PREF_COUNT];
static int loads, saves, sfx, hud_percent[2], panel_rows, randi, placing, handicap = 9;
static char panel_label[4][24], panel_info[4][24], big_text_shown[16];
static GXColor big_color;
static int grab_misses, catch_frame, captured_frames, mash_each, mash_frames;
#define GRAB_FRAME 7    // Popo's grab is out on its seventh frame.
#define CATCH_FRAMES 30 // A missed grab's animation.
#define PULLED_FRAMES 4 // CapturePulled, then CaptureWait, which can let go.

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
/* The event's formats: %d, %s and %.1f. */
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
static u32 Read(unsigned field, unsigned index) { return field == TM_SETTING_ESCAPE_IC ? preferences[index] : 0; }
static void Write(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_ESCAPE_IC && index < TM_ESC_IC_PREF_COUNT) { preferences[index] = value; saves++; }
}
static const TMSettingsAPI api = {Read, Write, 0};
static int Save(void *state, int flags) { return 1; }
/* The real load rewinds game_timer and puts everyone back falling, at 0%. */
static int Load(void *state, int flags) {
    ++loads;
    vars.game_timer = 1;
    for (int i = 0; i < BODIES; ++i) {
        ft[i].dmg.percent = ft[i].dmg.percent_temp = 0;
        ft[i].state_id = ASID_FALL;
        ft[i].phys.pos = (Vec3){i * 20 - 20, 20, 0};
        ft[i].input.held = 0;
        ft[i].grab.grab_timer = 0;
    }
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

GOBJ *Fighter_GetGObj(int player) { return player == 0 ? &fighters[YOU] : player == 1 ? &fighters[POPO] : 0; }
GOBJ *Fighter_GetSubcharGObj(int ply, int ms) { return ply == 1 && ms == 1 ? &fighters[NANA] : 0; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
void Fighter_SetAllHurtboxState(GOBJ *f, int kind) { }
void Fighter_UpdateCameraBox(GOBJ *f) { }
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) {
    FighterData *data = f->userdata;
    data->state_id = ASID_WAIT;
    data->cb.IASA = WaitIASA;
}
/* Final Destination: y = 0 from -85.6 to 85.6. */
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *unk, int a, int b, int c, void *cb,
    float fromX, float fromY, float toX, float toY, float unk5) {
    if (fromX < -85.6f || fromX > 85.6f) return 0;
    *pos = (Vec3){fromX, 0, 0};
    *line = 1;
    return 1;
}
void Match_CorrectCamera(void) { }
void Fighter_SetHUDDamage(int player, u16 percent) { if (player < 2) hud_percent[player] = percent; }
void Fighter_ZeroCPUInputs(FighterData *f) {
    f->cpu.held = 0;
    f->cpu.lstickX = f->cpu.lstickY = 0;
}
int Pause_CheckStatus(int kind) { return 0; }
int SFX_Play(int id) { sfx = id; return 0; }
int HSD_Randi(int max) { return max ? randi % max : 0; }
int Fighter_GetCurrentPlacing(int ply) { return placing; }
int Fighter_GetHandicap(int ply) { return handicap; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { }
void GObj_AddGXLink(GOBJ *gobj, void *cb, int gx_link, int gx_pri) { }
GOBJ *GObj_Create(int a, int b, int c) { return &event; }
GOBJProc *GObj_AddProc(GOBJ *gobj, void *cb, int pri) { return 0; }

int TestICInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event;
    vars.settings = &api;
    vars.Savestate_Save_v1 = (void *)Save;
    vars.Savestate_Load_v1 = (void *)Load;
    vars.HUD_DrawInfoPanel = Panel;
    vars.HUD_DrawTextEx = DrawTextEx;
    // PlCo.dat's grab numbers (scripts read them from the ISO; see the notes).
    common.grab_mash_min = 30;
    common.grab_mash_per_handicap = 8;
    common.grab_max_handicap = 9;
    common.grab_placing_mult = 15;
    common.grab_placing_max = 4;
    common.grab_mash_mult = 1.6f;
    common.grab_mash_per_frame = 1;
    common.grab_mash_per_input = 6;
    *stc_ftcommon = &common;
    for (int i = 0; i < BODIES; ++i) {
        fighters[i].userdata = &ft[i];
        fighters[i].hsd_object = &joints[i];
        ft[i].ply = i == NANA ? 1 : i;
        ft[i].state.rate = 1;
        ft[i].pad_index = i == YOU ? 0 : 1;
    }
    ft[POPO].flags.has_follower = 1;
    ft[NANA].flags.ms = 1;
    Event_Init(&event);
    return 0;
}

/* One frame, in the game's order: the capture tick (an animation callback,
 * s_link 1), the event's think and the fighters' input (3), the grab's hit
 * detection (13), then the event's s_link 15 proc. */
void TestICThink(int frames) {
    while (frames--) {
        FighterData *you = &ft[YOU], *popo = &ft[POPO];
        for (int i = 0; i < BODIES; ++i) {
            ft[i].dmg.percent += ft[i].dmg.percent_temp;
            ft[i].dmg.percent_temp = 0;
        }
        if (IsCaptured(you)) {
            int inputs = mash_frames > 0 ? mash_each : 0;
            if (mash_frames > 0) mash_frames--;
            you->grab.grab_timer -= common.grab_mash_per_frame + common.grab_mash_per_input * inputs;
            if (++captured_frames == PULLED_FRAMES) you->state_id = ASID_CAPTUREWAITHI;
            if (you->state_id == ASID_CAPTUREWAITHI && you->grab.grab_timer <= 0) {
                you->state_id = ASID_CAPTURECUT;
                popo->state_id = ASID_CATCHCUT;
            }
        }
        ++vars.game_timer;
        Event_Think(&event);
        // Popo's input: a fresh Z from Wait is a grab.
        if (popo->state_id == ASID_WAIT && (popo->cpu.held & ~popo->input.held & PAD_TRIGGER_Z)) {
            popo->state_id = ASID_CATCH;
            catch_frame = 0;
        }
        popo->input.held = popo->cpu.held;
        if (popo->state_id == ASID_CATCH && ++catch_frame == GRAB_FRAME && !grab_misses) {
            // The grab connects: the timer as 800DA8E4 sets it.
            you->state_id = ASID_CAPTUREPULLEDHI;
            you->grab.grab_timer = common.grab_mash_min + common.grab_mash_per_handicap * (common.grab_max_handicap - handicap)
                + common.grab_placing_mult * (common.grab_placing_max - (placing + 1)) + common.grab_mash_mult * you->dmg.percent;
            popo->state_id = ASID_CATCHWAIT;
            captured_frames = 0;
        } else if (popo->state_id == ASID_CATCH && catch_frame >= CATCH_FRAMES) {
            popo->state_id = ASID_WAIT;
        }
        MeasureGrab(&event);
        big_text_shown[0] = 0;
        DrawHUD(&event, 2);
    }
}
void TestICMash(int each, int frames) { mash_each = each; mash_frames = frames; }
void TestICMissGrabs(int value) { grab_misses = value; }
void TestICPlacing(int place, int cap) { placing = place; handicap = cap; }
void TestICRandom(int value) { randi = value; }
void TestICDPad(int buttons) {
    stc_engine_pads->pad[0].down = buttons;
    stc_engine_pads->pad[0].held = buttons;
}
void TestICChoose(int row, int value) {
    Options_Main[row].val = value;
    Options_Main[row].OnChange(0, value);
}
void TestICReWait(void) { Fighter_EnterWait(&fighters[YOU]); }
void TestICKill(int who) { ft[who].flags.dead = 1; }
static int Tenths(float x) { return (int)(x * 10 + (x < 0 ? -.5f : .5f)); }
int TestICInfo(int key) {
    switch (key) {
    case 0: return loads;
    case 1: return phase;
    case 2: return Tenths(ft[YOU].phys.pos.X);
    case 3: return Tenths(ft[POPO].phys.pos.X);
    case 4: return Tenths(ft[NANA].phys.pos.X);
    case 5: return ft[YOU].facing_direction > 0 ? 1 : -1;
    case 6: return ft[POPO].facing_direction > 0 ? 1 : -1;
    case 7: return ft[NANA].facing_direction > 0 ? 1 : -1;
    case 8: return ft[YOU].state_id;
    case 9: return ft[POPO].state_id;
    case 10: return ft[YOU].cb.IASA != 0;
    case 11: return ft[POPO].cpu.held;
    case 12: return ft[POPO].cpu.ai * 100 + ft[NANA].cpu.ai;
    case 13: return grab.frames;
    case 14: return grab.inputs;
    case 15: return grab.done * 10 + grab.new_best;
    case 16: return Tenths(grab.timer);
    case 17: return bests[Options_Main[OPT_PERCENT].val].frames;
    case 18: return bests[Options_Main[OPT_PERCENT].val].inputs;
    case 19: return sfx;
    case 20: return preferences[TM_ESC_IC_PERCENT];
    case 21: return saves;
    case 22: return hud_percent[0];
    case 23: return (int)ft[YOU].dmg.percent;
    case 24: return big_color.r << 16 | big_color.g << 8 | big_color.b;
    case 25: return panel_rows;
    case 26: return Options_Main[OPT_HANDOFF].disable;
    case 27: return Options_Main[OPT_HANDOFF].value_num;
    case 28: return Options_Main[OPT_PERCENT].val;
    case 29: return ft[YOU].input.held;
    case 30: return countof(Options_Bests);
    }
    return -1;
}
int TestICHold(int percent) { return NoMashHold(percent); }
char *TestICText(int key) {
    if (key < 4) return panel_label[key];
    if (key < 8) return panel_info[key - 4];
    if (key == 8) return big_text_shown;
    if (key == 9) return (char *)Options_Main[OPT_HANDOFF].values[0];
    if (key == 10) return Options_Main[OPT_HANDOFF].name;
    return best_text[key - 11]; /* 11.. the Session Bests rows */
}
char *TestICBestName(int row) { return Options_Bests[row].name; }
