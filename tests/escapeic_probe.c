/* Production Escape IC Grab event, with fixtures only at native engine
 * boundaries: Popo's grab and pull, the grab timer the game sets when it
 * connects, CaptureWait's per-frame tick (1 a frame, 6 a mash input, out at
 * 0), the two releases, and the global cue's ActionCues_Remaining. */
#include "../src/escapeic.c"
#include "../src/action_cues.c"
#include <stdarg.h>

enum { YOU, POPO, NANA, BODIES };
static EventVars vars;
static GOBJ event, fighters[BODIES];
static FighterData ft[BODIES];
static JOBJ joints[BODIES];
static ftCommonData common;
static unsigned preferences[TM_ESC_IC_PREF_COUNT], flash_preference = 1;
static int loads, saves, flash_saves, sfx, hud_percent[2], panel_rows, randi, placing, handicap = 9;
static char panel_label[4][24], panel_info[4][24], big_text_shown[16];
static GXColor big_color;
static unsigned overlay; // Your timing overlay's RGB, 0 for none.
static int grab_misses, catch_frame, pull_frames, mash_each, mash_frames, air_release;
#define GRAB_FRAME 7    // Popo's grab is out on its seventh frame.
#define CATCH_FRAMES 30 // A missed grab's animation.
#define PULL_FRAMES 2   // His CatchPull ends on Catch frame 8, when its script's 0x50 runs.
#define CUT_FRAMES 30   // CaptureCut: no interrupt, then Wait.
#define JUMP_FRAMES 50  // CaptureJump: aerial actions from frame 30 (PlCo 0x3B8), then Fall.
#define JUMP_ACT 30

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
static u32 Read(unsigned field, unsigned index) {
    if (field == TM_SETTING_ESCAPE_IC_FLASH) return index == 0 ? flash_preference : 0;
    return field == TM_SETTING_ESCAPE_IC ? preferences[index] : 0;
}
static void Write(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_ESCAPE_IC && index < TM_ESC_IC_PREF_COUNT) { preferences[index] = value; saves++; }
    if (field == TM_SETTING_ESCAPE_IC_FLASH && index == 0) { flash_preference = value; flash_saves++; }
}
static const TMSettingsAPI api = {Read, Write, 0};
static int Save(void *state, int flags) { return 1; }
// CaptureJump and CaptureWait keep their frame counts in state_var1, a float.
static float Var1(FighterData *f) { float v; memcpy(&v, &f->state_var.state_var1, sizeof(v)); return v; }
static void SetVar1(FighterData *f, float v) { memcpy(&f->state_var.state_var1, &v, sizeof(v)); }
static void Enter(FighterData *f, int state) {
    f->state_id = state;
    f->state.frame = 0;
    SetVar1(f, 0);
}
/* The real load rewinds game_timer and puts everyone back falling, at 0%. */
static int Load(void *state, int flags) {
    ++loads;
    vars.game_timer = 1;
    for (int i = 0; i < BODIES; ++i) {
        ft[i].dmg.percent = ft[i].dmg.percent_temp = 0;
        Enter(&ft[i], ASID_FALL);
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
static void TimingOverlay(GOBJ *f, const GXColor *color) {
    if (f == &fighters[YOU]) overlay = color ? color->r << 16 | color->g << 8 | color->b : 0;
}
/* ActionCues_Remaining for the states this event sees: neutral ones can act,
 * the releases end as the game's do, and nothing else is cued. */
static int Remaining(GOBJ *f) {
    FighterData *data = f->userdata;
    int state = data->state_id;
    if (state == ASID_WAIT || (state >= ASID_JUMPF && state <= ASID_FALLAERIALB)) return 0;
    if (state == ASID_CAPTURECUT) return TMCue_Remaining(data->state.frame, 1, CUT_FRAMES);
    if (state != ASID_CAPTUREJUMP) return -1;
    int fall = TMCue_Remaining(data->state.frame, 1, JUMP_FRAMES), act = TMCue_Remaining(Var1(data), 1, JUMP_ACT);
    return act < fall ? act : fall;
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
    Enter(data, ASID_WAIT);
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
    vars.set_timing_overlay = TimingOverlay;
    vars.cue_remaining = Remaining;
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

/* One frame, in the game's order: the animation callbacks (s_link 1), yours
 * before Popo's, whose CatchPull end puts you in CaptureWait; the event's think
 * and the fighters' input (3); the grab's hit detection (13); then the event's
 * s_link 15 proc. Nothing ticks while you're pulled in. */
void TestICThink(int frames) {
    while (frames--) {
        FighterData *you = &ft[YOU], *popo = &ft[POPO];
        for (int i = 0; i < BODIES; ++i) {
            ft[i].dmg.percent += ft[i].dmg.percent_temp;
            ft[i].dmg.percent_temp = 0;
        }
        if (you->state_id == ASID_CAPTUREWAITHI) {
            int inputs = mash_frames > 0 ? mash_each : 0;
            if (mash_frames > 0) mash_frames--;
            SetVar1(you, Var1(you) + 1);
            you->grab.grab_timer -= common.grab_mash_per_frame + common.grab_mash_per_input * inputs;
            if (you->grab.grab_timer <= 0) {
                Enter(you, air_release ? ASID_CAPTUREJUMP : ASID_CAPTURECUT);
                Enter(popo, ASID_CATCHCUT);
            }
        } else if (you->state_id == ASID_CAPTURECUT) {
            if (++you->state.frame >= CUT_FRAMES) Fighter_EnterWait(&fighters[YOU]);
        } else if (you->state_id == ASID_CAPTUREJUMP) {
            SetVar1(you, Var1(you) + 1);
            if (++you->state.frame >= JUMP_FRAMES) Enter(you, ASID_FALL);
        } else if (you->state_id != ASID_CAPTUREPULLEDHI) {
            ++you->state.frame;
        }
        if (popo->state_id == ASID_CATCHPULL && ++pull_frames == PULL_FRAMES) {
            Enter(popo, ASID_CATCHWAIT);
            you->state_id = ASID_CAPTUREWAITHI; // Its frame count goes on from the grab's.
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
            // The grab connects: the timer as 800DA8E4 sets it, and the frame count zeroed.
            Enter(you, ASID_CAPTUREPULLEDHI);
            you->grab.grab_timer = common.grab_mash_min + common.grab_mash_per_handicap * (common.grab_max_handicap - handicap)
                + common.grab_placing_mult * (common.grab_placing_max - (placing + 1)) + common.grab_mash_mult * you->dmg.percent;
            popo->state_id = ASID_CATCHPULL;
            pull_frames = 0;
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
void TestICAirRelease(int value) { air_release = value; }
void TestICAct(int state) { Enter(&ft[YOU], state); } // You act: an interrupt into this state.
void TestICHitlag(int value) { ft[YOU].flags.hitlag = value; }
void TestICDPad(int buttons) {
    stc_engine_pads->pad[0].down = buttons;
    stc_engine_pads->pad[0].held = buttons;
}
void TestICChoose(int row, int value) {
    Options_Main[row].val = value;
    Options_Main[row].OnChange(0, value);
}
void TestICFlashPreference(int value) { flash_preference = value; }
void TestICLoadSettings(void) { LoadSettings(); }
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
    case 31: return countof(Options_Help);
    case 32: return Options_Main[OPT_HELP].kind == OPTKIND_MENU && Options_Main[OPT_HELP].menu == &Menu_Help;
    case 33: return countof(Options_Main);
    case 34: return overlay;
    case 35: return Options_Main[OPT_FLASH].val;
    case 36: return flash_preference;
    case 37: return flash_saves;
    case 38: return flash.armed;
    case 39: return Options_Main[OPT_FLASH].kind == OPTKIND_TOGGLE;
    case 40: return Tenths(Var1(&ft[YOU]));
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
char *TestICMainName(int row) { return Options_Main[row].name; }
char *TestICMainLine(int row, int line) { return Options_Main[row].desc[line]; }
char *TestICHelpName(int row) { return Options_Help[row].name; }
char *TestICHelpLine(int row, int line) { return Options_Help[row].desc[line]; }
