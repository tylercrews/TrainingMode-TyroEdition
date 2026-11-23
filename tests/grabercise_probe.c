/* Production Grabs-ercise, with a small engine at the native boundaries: the
 * CPU's stick and tilt timer; the Wait, Walk, Turn, Dash, Run, TurnRun,
 * Guard and roll transitions as the decomp has them (see src/grabercise.c);
 * ground speed; floors that end, with rolls and run turnarounds stopping at
 * the end and everything else falling off; a knockback landing's tech or
 * knockdown and the getups, with the L/R and A timers they read; and
 * Battlefield's or Final Destination's floors. Each frame runs the game's
 * order around the event's think: animation and scripts (s_link 1), then input
 * and interrupts (3) and physics (4). */
#include "../src/grabercise.c"
#include <stdarg.h>

enum { HMN_FT, CPU_FT };
#define REDASH_FRAME 20.f // PlCo 0x4C: past this dash frame, any fresh smash turns or dashes again.
static EventVars vars;
static GOBJ event, fighters[2], nana_gobj;
static FighterData ft[2], nana;
static JOBJ joints[2], nana_joint;
static ftCommonData common;
static int stale[11];
static unsigned prefs[TM_GRABS_PREF_COUNT];
static unsigned counter_pref;
static int retries, ended, post_sfx, records, error_sfx, pref_writes;
static int stage_kind;
static unsigned seed = 12345;
static Text texts[3];
static int text_count;

void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
float sqrtf(float value) {
    if (value <= 0) return 0;
    float root = value > 1 ? value : 1;
    for (int i = 0; i < 24; ++i) root = (root + value / root) / 2;
    return root;
}
int sprintf(char *out, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char *start = out;
    for (; *format; ++format) {
        if (format[0] == '%' && format[1] == 's') {
            for (const char *s = va_arg(args, const char *); *s; ++s) *out++ = *s;
            ++format;
        } else *out++ = *format;
    }
    *out = 0;
    va_end(args);
    return out - start;
}
static unsigned NextRand(void) { seed = seed * 1103515245 + 12345; return seed >> 16 & 0x7FFF; }
int HSD_Randi(int max) { return max > 0 ? NextRand() % max : 0; }
float HSD_Randf(void) { return NextRand() / 32768.f; }

static u32 Read(unsigned field, unsigned index) {
    if (field == TM_SETTING_GRABERCISE && index < TM_GRABS_PREF_COUNT) return prefs[index];
    if (field == TM_SETTING_EGGS_COUNTER) return counter_pref;
    return 0;
}
static void Write(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_GRABERCISE && index < TM_GRABS_PREF_COUNT) { prefs[index] = value; pref_writes++; }
    if (field == TM_SETTING_EGGS_COUNTER) counter_pref = value;
}
static const TMSettingsAPI api = {Read, Write, 0};

// Stage ---------------------------------------------------------------------

static CollDataStage stage;
static CollLine lines[8];
static CollLineDesc descs[8];
static CollVert verts[16];
static int vert_count;
static void AddLine(float x1, float y1, float x2, float y2, int floor, int platform) {
    int i = stage.line_num++;
    lines[i].desc = &descs[i];
    descs[i] = (CollLineDesc){0};
    descs[i].vert_prev = vert_count;
    descs[i].vert_next = vert_count + 1;
    descs[i].is_floor = floor;
    if (platform) ((u8 *)&descs[i])[0xE] = 1; /* LINE_FLAG_PLATFORM */
    verts[vert_count++].pos_curr = (Vec2){x1, y1};
    verts[vert_count++].pos_curr = (Vec2){x2, y2};
}
/* Battlefield: its ground in two halves that meet in the middle, a wall
 * under one ledge, and its three platforms. Final Destination: the ground. */
static void BuildStage(int kind) {
    stage_kind = kind;
    stage = (CollDataStage){0};
    vert_count = 0;
    float edge = kind ? 85.6f : 68.4f;
    AddLine(-edge, 0, 0, 0, 1, 0);
    AddLine(0, 0, edge, 0, 1, 0);
    if (!kind) {
        AddLine(-edge, 0, -edge, -20, 0, 0);
        AddLine(-57.6f, 27.2f, -20, 27.2f, 1, 1);
        AddLine(20, 27.2f, 57.6f, 27.2f, 1, 1);
        AddLine(-18.8f, 54.4f, 18.8f, 54.4f, 1, 1);
    }
    stage.vert_num = vert_count;
    *stc_colldata = &stage;
    *stc_collline = lines;
    *stc_collvert = verts;
}
int Stage_GetExternalID(void) { return stage_kind ? GRKINDEXT_FD : GRKINDEXT_BATTLE; }
int GrColl_CheckIfLineEnabled(int line) { return 1; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *slope, int a, int b, int c, void *d,
    float x1, float y1, float x2, float y2, float e) { return 0; }

// Engine stubs ----------------------------------------------------------------

typedef struct Char {
    float walk_max, walk_acc, friction, dash_init, acc_a, acc_b, terminal;
    int dash_len, dash_flag, turnrun_len, turnrun_flag, roll_len;
    float roll_speed, walk_init;
    int turn_len;
} Char;
/* Fox, Marth, Game & Watch (the shortest dash, 18 frames) and Luigi (the
 * least traction), from their PlXx.dat attributes and scripts. */
static const Char chars[] = {
    {1.6f, .1f, .08f, 1.9f, .1f, .02f, 2.2f, 22, 12, 20, 9, 32, 1.4f, .2f, 12},
    {1.6f, 0, .06f, 1.5f, .06f, 0, 1.8f, 28, 16, 30, 9, 36, 1.2f, .15f, 12},
    {1.1f, .1f, .06f, 1.5f, .06f, .02f, 1.5f, 18, 9, 18, 9, 36, 1.0f, .1f, 10},
    {1.1f, .1f, .025f, 1.3f, .06f, .02f, 1.34f, 22, 11, 22, 18, 32, 1.2f, .1f, 12},
};
static struct engine {
    const Char *c;
    int anim_len, dash_arg;
    int smash_turn, turned, just_turned, turn_after;
    int turnrun_from, paused, unpaused;
    int roll_dir, signed_timer;
    float pending;   // A new dash's push, applied by its first physics (xE8).
    int dash_entered;
    int down_wait;   // DownWait's frames left before it stands up on its own (PlCo 0x424).
} eng;
/* Tech and getup lengths; techs and getup rolls move ROLL_SPEED a frame for
 * their first ROLL_FRAMES, forward (F) or back (B) from the way they face. */
enum { DOWNBOUND_LEN = 26, DOWNWAIT_LEN = 220, GETUP_LEN = 30, TECH_LEN = 40, ROLL_FRAMES = 20 };
#define ROLL_SPEED 1.5f
static struct stats {
    int frames, fell, runs, redashes, dash_starts, dash_turns, run_turns, rolls, walk_dashes;
    // Run mode's crouch turnaround: crouches, dashes out of a crouch, and
    // drops through a platform (which it mustn't).
    int crouches, squat_dashes, passes;
    // Dash legs: frames in each dash, long (6+) or short (3 or fewer), and
    // the short ones that went back against the last long one and moved back.
    int long_legs, short_legs, short_back, moved_back, longest_leg;
    // Every dash leg: how many, how many went the other way from the one
    // before, and after the first, the longest and shortest in frames and
    // distance (the first is from standing).
    int legs, alternations, longest_after_first, shortest_after_first;
    float farthest_after_first, nearest_after_first;
    int dir_changes, last_dir, inputs;
    float top_walk, top_run, top_dash, min_x, max_x, min_y, max_y;
    // The tech chase: the first tech or knockdown, how far it moved, and its getup.
    int chase_state, getup_state;
    float chase_x, chase_last_x;
} st;

static void Enter(int state, int length) {
    ft[CPU_FT].state_id = state;
    ft[CPU_FT].state.frame = 0;
    eng.anim_len = length;
}
GOBJ *Fighter_GetGObj(int player) { return player < 2 ? &fighters[player] : 0; }
GOBJ *Fighter_GetSubcharGObj(int player, int ms) {
    if (player >= 2) return 0;
    if (!ms) return &fighters[player];
    return player == CPU_FT && ft[CPU_FT].flags.has_follower ? &nana_gobj : 0;
}
int *Fighter_GetStaleMoveTable(int player) { return stale; }
int Fighter_GetExternalID(int player) { return player ? CKIND_MARTH : CKIND_FOX; }
int Fighter_GetCostumeID(int player) { return 0; }
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void EnvironmentCollision_WaitLanding(GOBJ *fighter) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
void Fighter_SetAllHurtboxState(GOBJ *fighter, int kind) { }
void Fighter_UpdateCameraBox(GOBJ *fighter) { }
void Fighter_SetHUDDamage(int player, u16 damage) { }
void Fighter_ZeroCPUInputs(FighterData *f) {
    f->cpu.held = 0;
    f->cpu.lstickX = f->cpu.lstickY = f->cpu.cstickX = f->cpu.cstickY = 0;
}
void Fighter_EnterSleep(GOBJ *fighter, int ms) { ((FighterData *)fighter->userdata)->state_id = ASID_SLEEP; }
void Fighter_EnterRebirth(GOBJ *fighter) {
    FighterData *f = fighter->userdata;
    f->state_id = ASID_REBIRTH;
    f->phys.air_state = 1;
}
void Fighter_EnterWait(GOBJ *fighter) {
    if (fighter == &fighters[CPU_FT]) Enter(ASID_WAIT, 60);
    else ((FighterData *)fighter->userdata)->state_id = ASID_WAIT;
}
float Fighter_GetCurrentAnimLength(GOBJ *fighter) { return eng.anim_len; }
int Pause_CheckStatus(int kind) { return 0; }
int SFX_PlayCommon(int sfx) { if (sfx == 3) error_sfx++; return 0; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { retries++; }
void Match_EndImmediate(void) { ended++; }
void Match_SetEndGraphic(int kind) { }
void Match_SetPostMatchSFX(int sfx) { post_sfx = sfx; }
void MatchInfo_0x0010_store(int value) { records++; }
int Text_CreateCanvas(int sis, GOBJ *cam, int a, int b, int c, int d, int e, int f) { return 1; }
Text *Text_CreateText(int sis, int canvas) { return &texts[text_count++ % 3]; }
int Text_AddSubtext(Text *text, float x, float y, char *fmt, ...) { return 0; }
void Text_SetColor(Text *text, int row, GXColor *color) { }
void Text_SetText(Text *text, int row, const char *fmt, ...) { }

// The engine ------------------------------------------------------------------

static float Sign(float v) { return v < 0 ? -1 : 1; }
static float Brake(float v, float by) { return fabs(v) <= by ? 0 : v - Sign(v) * by; }
/* ftCo_Dash_Enter: the initial dash speed, added to what's left of the speed
 * the other way, or topping up the speed this way. */
static void DashEnter(int arg) {
    FighterData *f = &ft[CPU_FT];
    if (f->state_id == ASID_DASH) st.redashes++;
    if (arg) st.dash_starts++;
    Enter(ASID_DASH, eng.c->dash_len);
    f->ftcmd_var.flag0 = 0;
    eng.dash_arg = arg;
    float init = f->facing_direction * eng.c->dash_init, v = f->phys.self_vel_ground.X;
    eng.pending = v * f->facing_direction < 0 ? init : init - v;
    eng.dash_entered = 1;
}
static void TurnEnter(int smash) {
    Enter(ASID_TURN, eng.c->turn_len);
    eng.smash_turn = smash;
    eng.turned = eng.just_turned = 0;
    eng.turn_after = -ft[CPU_FT].facing_direction;
}
static int DashCheck(void) { // ftCo_Dash_CheckInput
    FighterData *f = &ft[CPU_FT];
    float x = f->input.lstick.X;
    if (fabs(x) < .8f || (u8)f->input.timer_lstick_tilt_x >= 2) return 0;
    if (x * f->facing_direction < 0) TurnEnter(1);
    else DashEnter(1);
    return 1;
}
static void RunEnter(void) { Enter(ASID_RUN, 20); st.runs++; }

/* The stick's angle, atan2(y, |x|) (ftCo_GetLStickAngle), is under 50 degrees
 * (PlCo 0x20): y under tan(50) |x|. */
static int Level(float x, float y) { return y < 1.1918f * fabs(x); }
static void GetupEnter(int state, int length) { Enter(state, length); st.getup_state = state; }
/* ftCo_Down_CheckInput: a getup roll, forward or back from the way it faces. */
static int DownRoll(void) {
    FighterData *f = &ft[CPU_FT];
    float x = f->input.lstick.X;
    if (fabs(x) < .2f || !Level(x, f->input.lstick.Y)) return 0;
    GetupEnter(x * f->facing_direction >= 0 ? ASID_DOWNFOWARDU : ASID_DOWNBACKU, GETUP_LEN);
    return 1;
}
/* ftCo_80090184: a knockback landing techs (rolling with the stick out) if L/R
 * was pressed under 20 frames ago, 40 or more after the press before it, and
 * is knocked down otherwise. */
static void KnockbackLanding(void) {
    FighterData *f = &ft[CPU_FT];
    float x = f->input.lstick.X;
    if ((u8)f->input.timer_LR < 20 && (u8)f->input.since_rapid_lr >= 40) {
        if (fabs(x) >= .2f) Enter(x * f->facing_direction >= 0 ? ASID_PASSIVESTANDF : ASID_PASSIVESTANDB, TECH_LEN);
        else Enter(ASID_PASSIVE, TECH_LEN);
    } else {
        Enter(ASID_DOWNBOUNDU, DOWNBOUND_LEN);
        f->input.timer_a = f->input.timer_b = 255;
    }
}

/* s_link 0 and 1: hitlag, then animations and their scripts. */
static void Animate(void) {
    FighterData *f = &ft[CPU_FT];
    if (f->dmg.hitlag_frames > 0) {
        f->dmg.hitlag_frames -= 1;
        if (f->dmg.hitlag_frames <= 0) { f->dmg.hitlag_frames = 0; f->flags.hitlag = f->flags.freeze = 0; }
        return;
    }
    int s = f->state_id;
    if (!(s == ASID_TURNRUN && eng.paused)) f->state.frame += 1;
    float frame = f->state.frame, stick = f->input.lstick.X * f->facing_direction;
    switch (s) {
        case ASID_DASH:
            if ((int)frame == eng.c->dash_flag) f->ftcmd_var.flag0 = 1;
            if (frame >= eng.anim_len) Enter(ASID_WAIT, 60);
            break;
        case ASID_TURN:
            if (!eng.turned) { eng.turned = eng.just_turned = 1; f->facing_direction = eng.turn_after; }
            if (frame >= eng.anim_len) Enter(ASID_WAIT, 60);
            break;
        case ASID_TURNRUN:
            if ((int)frame == eng.c->turnrun_flag && !eng.unpaused) eng.paused = 1;
            if (eng.paused && eng.turnrun_from * f->phys.self_vel_ground.X <= .01f) {
                eng.paused = 0;
                eng.unpaused = 1;
                f->facing_direction = -f->facing_direction;
            }
            if (frame >= eng.anim_len) {
                if (f->input.lstick.X * f->facing_direction >= .625f) RunEnter();
                else Enter(ASID_WAIT, 60);
            }
            break;
        case ASID_GUARDON:
            if (frame >= 8) Enter(ASID_GUARD, 1000);
            break;
        case ASID_GUARDSETOFF: // Shield stun, then back in shield while R is held.
            if (frame >= 8) Enter(f->input.held & PAD_TRIGGER_R ? ASID_GUARD : ASID_WAIT, 1000);
            break;
        case ASID_ESCAPEF:
        case ASID_ESCAPEB:
            if (s == ASID_ESCAPEF && (int)frame == eng.anim_len / 2) f->facing_direction = -f->facing_direction;
            if (frame >= eng.anim_len) { f->phys.self_vel_ground.X = 0; Enter(ASID_WAIT, 60); }
            break;
        case ASID_LANDING:
            if (frame >= 4) Enter(ASID_WAIT, 60);
            break;
        case ASID_RUNBRAKE:
            if (f->phys.self_vel_ground.X == 0 || frame >= 20) Enter(ASID_WAIT, 60);
            break;
        case ASID_SQUAT: // Every character's Squat is 8 frames, then SquatWait.
            if (frame >= 8) Enter(ASID_SQUATWAIT, 100);
            break;
        case ASID_DOWNBOUNDU: // ftCo_DownBound_Anim: A or B since it began, a roll, or DownWait.
            if (frame < eng.anim_len) break;
            if ((u8)f->input.timer_a < 60 || (u8)f->input.timer_b < 60) GetupEnter(ASID_DOWNATTACKU, GETUP_LEN);
            else if (!DownRoll()) {
                Enter(ASID_DOWNWAITU, 1000);
                eng.down_wait = DOWNWAIT_LEN;
            }
            break;
        case ASID_DOWNWAITU:
            if (--eng.down_wait <= 0) GetupEnter(ASID_DOWNSTANDU, GETUP_LEN);
            break;
        case ASID_PASSIVE:
        case ASID_PASSIVESTANDF:
        case ASID_PASSIVESTANDB:
        case ASID_DOWNSTANDU:
        case ASID_DOWNATTACKU:
        case ASID_DOWNFOWARDU:
        case ASID_DOWNBACKU:
            if (frame >= eng.anim_len) Enter(ASID_WAIT, 60);
            break;
    }
}

/* s_link 3, first: the CPU_FT's stick and buttons become its input. */
static void Input(void) {
    FighterData *f = &ft[CPU_FT];
    float x = f->cpu.lstickX / 127.f, prev = f->input.lstick.X;
    if (x > 1) x = 1;
    if (x < -1) x = -1;
    int fresh = eng.signed_timer ? (x >= TILT && prev < TILT) || (x <= -TILT && prev > -TILT)
        : fabs(x) >= TILT && fabs(prev) < TILT;
    u8 timer = f->input.timer_lstick_tilt_x;
    if (fresh) timer = 0;
    else if (fabs(x) >= TILT) timer = timer < 254 ? timer + 1 : 254;
    else timer = 254;
    f->input.timer_lstick_tilt_x = timer;
    f->input.lstick.X = x;
    // The stick's y and its tilt timer, the same way (PlCo 0xC is 0.25 too).
    float y = f->cpu.lstickY / 127.f, prev_y = f->input.lstick.Y;
    if (y < -1) y = -1;
    u8 timer_y = f->input.timer_lstick_tilt_y;
    if (fabs(y) >= TILT && fabs(prev_y) < TILT) timer_y = 0;
    else if (fabs(y) >= TILT) timer_y = timer_y < 254 ? timer_y + 1 : 254;
    else timer_y = 254;
    f->input.timer_lstick_tilt_y = timer_y;
    f->input.lstick.Y = y;
    // Newly pressed buttons, and the frames since A, B and L/R were (fighter.c's input proc).
    int held_before = f->input.held;
    f->input.held = f->cpu.held;
    f->input.down = f->input.held & ~held_before;
    if (f->input.down & (PAD_TRIGGER_L | PAD_TRIGGER_R)) {
        f->input.since_rapid_lr = f->input.timer_LR;
        f->input.timer_LR = 0;
    } else if ((u8)f->input.timer_LR < 255) f->input.timer_LR++;
    if (f->input.down & PAD_BUTTON_A) f->input.timer_a = 0;
    else if ((u8)f->input.timer_a < 255) f->input.timer_a++;
    if (f->input.down & PAD_BUTTON_B) f->input.timer_b = 0;
    else if ((u8)f->input.timer_b < 255) f->input.timer_b++;
    if (f->cpu.lstickX || f->cpu.held) st.inputs++;
}

/* s_link 3, then: its interrupts. */
static void Interrupt(void) {
    FighterData *f = &ft[CPU_FT];
    Input();
    if (Frozen(f)) return;
    float x = f->input.lstick.X, y = f->input.lstick.Y, toward = x * f->facing_direction;
    u8 timer = f->input.timer_lstick_tilt_x, timer_y = f->input.timer_lstick_tilt_y;
    int shield = f->cpu.held & PAD_TRIGGER_R;
    switch (f->state_id) {
        case ASID_WAIT:
            if (shield) { Enter(ASID_GUARDON, 8); break; }
            if (DashCheck()) break;
            if (toward <= -.25f) { TurnEnter(0); break; }
            if (toward >= .25f) Enter(ASID_WALKSLOW, 30);
            break;
        case ASID_WALKSLOW:
        case ASID_WALKMIDDLE:
        case ASID_WALKFAST:
            if (shield) { Enter(ASID_GUARDON, 8); break; }
            if (DashCheck()) { st.walk_dashes++; break; }
            // ft_8008A244: a stick back or let go drops to Wait (which turns).
            if (toward < 0 || fabs(x) < .25f) Enter(ASID_WAIT, 60);
            break;
        case ASID_RUNBRAKE: // RunBrake takes a crouch (ftCo_800D5FB0).
            if (y < -.6875f) { Enter(ASID_SQUAT, 8); st.crouches++; }
            break;
        case ASID_SQUAT:
        case ASID_SQUATWAIT: {
            // ftCo_80099F1C: a fresh down on a platform drops through it.
            int line = f->coll_data.ground_index;
            if (y <= -.66f && timer_y < 6 && line >= 0 && IsPlatformLine(lines[line].desc)) {
                st.passes++;
                f->phys.air_state = 1;
                f->phys.pos.Y -= 1;
                f->phys.self_vel = (Vec3){f->phys.self_vel_ground.X, 0, 0};
                Enter(ASID_FALL, 60);
                break;
            }
            // SquatWait takes a dash (forward, or a smash turn back), then stands up.
            if (f->state_id == ASID_SQUATWAIT) {
                if (DashCheck()) { st.squat_dashes++; break; }
                if (y > -.6875f) Enter(ASID_WAIT, 60);
            }
            break;
        }
        case ASID_TURN:
            if (shield) { Enter(ASID_GUARDON, 8); break; }
            if (eng.just_turned && eng.smash_turn && x * eng.turn_after >= .8f) { DashEnter(0); st.dash_turns++; }
            eng.just_turned = 0;
            break;
        case ASID_DASH: {
            /* A turn or a new dash out of a dash brakes the old one to a
             * quarter (PlCo 0x54, 0.75 off), after the new dash's push is set. */
            float frame = f->state.frame;
            if (eng.dash_arg && frame <= 4) break; // Specials, items and attacks only.
            if (frame <= REDASH_FRAME ? toward < 0 && DashCheck() : DashCheck()) {
                f->phys.self_vel_ground.X *= .25f;
                break;
            }
            if (shield) { Enter(ASID_GUARDON, 8); break; }
            if (f->ftcmd_var.flag0 && toward >= .625f) RunEnter();
            break;
        }
        case ASID_RUN:
            if (shield) { Enter(ASID_GUARDON, 8); break; }
            if (toward <= -.375f) {
                Enter(ASID_TURNRUN, eng.c->turnrun_len);
                eng.turnrun_from = f->facing_direction;
                eng.paused = eng.unpaused = 0;
                st.run_turns++;
            } else if (toward < .625f) Enter(ASID_RUNBRAKE, 20);
            break;
        case ASID_GUARDON:
        case ASID_GUARD:
            if (!shield) { Enter(ASID_WAIT, 60); break; }
            if (fabs(x) >= .7f && timer < 4) {
                Enter(toward >= 0 ? ASID_ESCAPEF : ASID_ESCAPEB, eng.c->roll_len);
                eng.roll_dir = Sign(x);
                st.rolls++;
            }
            break;
        case ASID_DOWNWAITU: // ftCo_DownWait_IASA: a fresh A or B, a roll, or the stick up.
            if (f->input.down & (PAD_BUTTON_A | PAD_BUTTON_B)) GetupEnter(ASID_DOWNATTACKU, GETUP_LEN);
            else if (!DownRoll() && y >= .2f && !Level(x, y)) GetupEnter(ASID_DOWNSTANDU, GETUP_LEN);
            break;
    }
}

static int FloorAt(float x, float y, int *line, float *floor_y) {
    for (int i = 0; i < stage.line_num; ++i) {
        float at;
        if (LineY(i, x, &at) && fabs(at - y) < .5f) { *line = i; *floor_y = at; return 1; }
    }
    return 0;
}
/* Dash and run acceleration, toward the stick's share of the top speed. */
static float DashRun(float v, float x) {
    const Char *c = eng.c;
    float target = x * c->terminal;
    if (fabs(x) < .01f) return Brake(v, c->friction);
    float accel = x * c->acc_a + Sign(x) * c->acc_b;
    if ((target - v) * Sign(target) > 0) {
        v += accel;
        if ((v - target) * Sign(target) > 0) v = target;
        return v;
    }
    float slower = Brake(v - target, c->friction) + target;
    return slower;
}

/* s_link 4: speed and movement along the floor. */
static void Physics(void) {
    FighterData *f = &ft[CPU_FT];
    int s = f->state_id;
    if (Frozen(f) || Held(s) || s == ASID_SLEEP || s == ASID_REBIRTH) return;
    const Char *c = eng.c;
    float x = f->input.lstick.X, v = f->phys.self_vel_ground.X, frame = f->state.frame;
    if (f->phys.air_state) {
        f->phys.self_vel.Y -= .23f;
        if (f->phys.self_vel.Y < -2.8f) f->phys.self_vel.Y = -2.8f;
        float from = f->phys.pos.Y, to = from + f->phys.self_vel.Y;
        f->phys.pos.X += f->phys.self_vel.X;
        for (int i = 0; i < stage.line_num; ++i) {
            float y;
            if (LineY(i, f->phys.pos.X, &y) && from >= y && to <= y) {
                f->phys.pos.Y = y;
                f->phys.air_state = 0;
                f->phys.self_vel = (Vec3){0};
                f->coll_data.ground_index = i;
                if (KnockedBack(s)) KnockbackLanding();
                else Enter(ASID_LANDING, 4);
                return;
            }
        }
        f->phys.pos.Y = to;
        return;
    }
    switch (s) {
        case ASID_WALKSLOW:
        case ASID_WALKMIDDLE:
        case ASID_WALKFAST: {
            /* ftWalkCommon_800E0060: the stick times the walk acceleration,
             * plus the initial walk speed, toward the stick's share of the
             * top walk speed. */
            float target = x * c->walk_max, accel = fabs(x) * c->walk_acc + c->walk_init;
            if ((target - v) * Sign(target) > 0) {
                v += Sign(target) * accel;
                if ((v - target) * Sign(target) > 0) v = target;
            } else v = Brake(v - target, c->friction) + target;
            f->state_id = fabs(v) > .8f * c->walk_max ? ASID_WALKFAST : fabs(v) > .4f * c->walk_max ? ASID_WALKMIDDLE : ASID_WALKSLOW;
            if (fabs(v) > st.top_walk) st.top_walk = fabs(v);
            break;
        }
        case ASID_DASH: // The entry frame takes the push instead of accelerating.
            if (frame > 0) v = DashRun(v, x);
            else { v += eng.pending; eng.pending = 0; }
            if (fabs(v) > st.top_dash) st.top_dash = fabs(v);
            break;
        case ASID_RUNBRAKE: // Ground friction (PlCo 0x60 is 1).
            v = Brake(v, c->friction);
            break;
        case ASID_RUN:
            v = DashRun(v, x);
            if (fabs(v) > st.top_run) st.top_run = fabs(v);
            break;
        case ASID_TURNRUN: {
            float accel = x * c->acc_a + Sign(x) * c->acc_b, target = x * c->terminal;
            if (fabs(x) < .01f || eng.turnrun_from * accel >= 0) v = Brake(v, c->friction);
            else {
                v += accel;
                if ((v - target) * Sign(target) > 0) v = target;
            }
            break;
        }
        case ASID_ESCAPEF:
        case ASID_ESCAPEB:
            v = frame >= 4 && frame <= 24 ? eng.roll_dir * c->roll_speed : 0;
            break;
        case ASID_PASSIVESTANDF:
        case ASID_DOWNFOWARDU:
            v = frame <= ROLL_FRAMES ? f->facing_direction * ROLL_SPEED : 0;
            break;
        case ASID_PASSIVESTANDB:
        case ASID_DOWNBACKU:
            v = frame <= ROLL_FRAMES ? -f->facing_direction * ROLL_SPEED : 0;
            break;
        default: // Wait, Turn, Squat: friction, doubled above walk speed (ft_80084F3C).
            v = Brake(v, fabs(v) > c->walk_max ? c->friction * 2 : c->friction);
            break;
    }
    f->phys.self_vel_ground.X = v;
    int line;
    float y, next = f->phys.pos.X + v;
    if (FloorAt(next, f->phys.pos.Y, &line, &y)) {
        f->phys.pos.X = next;
        f->phys.pos.Y = y;
        f->coll_data.ground_index = line;
    } else if (s == ASID_ESCAPEF || s == ASID_ESCAPEB || s == ASID_TURNRUN || s == ASID_PASSIVESTANDF ||
               s == ASID_PASSIVESTANDB || s == ASID_DOWNFOWARDU || s == ASID_DOWNBACKU) {
        // Rolls (tech and getup rolls too) and run turnarounds stop at the end of the floor.
        for (int i = 0; i < 40; ++i) {
            float step = f->phys.pos.X + v * (39 - i) / 40;
            if (FloorAt(step, f->phys.pos.Y, &line, &y)) { f->phys.pos.X = step; break; }
        }
        f->phys.self_vel_ground.X = 0;
    } else {
        f->phys.pos.X = next;
        f->phys.air_state = 1;
        f->phys.self_vel = (Vec3){v, 0, 0};
        Enter(ASID_FALL, 60);
        st.fell++;
    }
}

static int leg_dir, leg_len, last_long, prev_leg_dir;
static float leg_x;
static void FinishLeg(void) {
    float distance = fabs(ft[CPU_FT].phys.pos.X - leg_x);
    st.legs++;
    if (prev_leg_dir && leg_dir == -prev_leg_dir) st.alternations++;
    prev_leg_dir = leg_dir;
    if (st.legs > 1) {
        if (leg_len > st.longest_after_first) st.longest_after_first = leg_len;
        if (!st.shortest_after_first || leg_len < st.shortest_after_first) st.shortest_after_first = leg_len;
        if (distance > st.farthest_after_first) st.farthest_after_first = distance;
        if (!st.nearest_after_first || distance < st.nearest_after_first) st.nearest_after_first = distance;
    }
    if (leg_len >= 6) {
        st.long_legs++;
        last_long = leg_dir;
        if (leg_len > st.longest_leg) st.longest_leg = leg_len;
    } else if (leg_len <= 3) {
        st.short_legs++;
        if (last_long && leg_dir == -last_long) {
            st.short_back++;
            if ((ft[CPU_FT].phys.pos.X - leg_x) * last_long < 0) st.moved_back++;
        }
    }
    leg_len = 0;
}
static void Observe(void) {
    FighterData *f = &ft[CPU_FT];
    st.frames++;
    if (f->state_id == ASID_DASH) {
        if (eng.dash_entered) {
            if (leg_len) FinishLeg();
            leg_dir = f->facing_direction;
            leg_x = f->phys.pos.X - f->phys.self_vel_ground.X;
            eng.dash_entered = 0;
        }
        leg_len++;
    } else if (leg_len) FinishLeg();
    if (f->phys.pos.X < st.min_x) st.min_x = f->phys.pos.X;
    if (f->phys.pos.X > st.max_x) st.max_x = f->phys.pos.X;
    if (f->phys.pos.Y < st.min_y) st.min_y = f->phys.pos.Y;
    if (f->phys.pos.Y > st.max_y) st.max_y = f->phys.pos.Y;
    if (target.dir != st.last_dir) { st.dir_changes++; st.last_dir = target.dir; }
    if (Chased(f->state_id)) {
        if (!st.chase_state) { st.chase_state = f->state_id; st.chase_x = f->phys.pos.X; }
        st.chase_last_x = f->phys.pos.X;
    }
}
static void ResetStats(void) {
    FighterData *f = &ft[CPU_FT];
    st = (struct stats){0};
    leg_len = last_long = prev_leg_dir = 0;
    st.min_x = st.max_x = f->phys.pos.X;
    st.min_y = st.max_y = f->phys.pos.Y;
    st.last_dir = target.dir;
}

// Tests -----------------------------------------------------------------------

void TestGrabsLoaded(void) { }
void TestGrabsPref(int index, int value) { prefs[index] = value; }
/* stage: 0 Battlefield, 1 Final Destination. */
void TestGrabsInit(int stage_id, int character, int signed_timer) {
    *event_vars_ptr_loc = &vars;
    vars.settings = &api;
    *stc_ftcommon = &common;
    common.x260 = 60;
    BuildStage(stage_id);
    for (int i = 0; i < 2; ++i) {
        fighters[i].userdata = &ft[i];
        fighters[i].hsd_object = &joints[i];
        ft[i] = (FighterData){0};
        ft[i].ply = i;
        ft[i].facing_direction = 1;
        ft[i].state_id = ASID_WAIT;
        ft[i].input.timer_lstick_tilt_x = 254;
        // As a spawn leaves them (Fighter_ResetInputData): nothing pressed yet.
        ft[i].input.timer_LR = ft[i].input.since_rapid_lr = ft[i].input.timer_a = ft[i].input.timer_b = 255;
    }
    nana_gobj.userdata = &nana;
    nana_gobj.hsd_object = &nana_joint;
    nana = (FighterData){0};
    ft[HMN_FT].phys.pos = (Vec3){-40, 0, 0};
    eng = (struct engine){0};
    eng.c = &chars[character];
    eng.anim_len = 60;
    eng.signed_timer = signed_timer;
    ft[CPU_FT].attr.ground_friction = eng.c->friction;
    ft[CPU_FT].attr.walk_maximum_velocity = eng.c->walk_max;
    ft[CPU_FT].attr.walk_initial_velocity = eng.c->walk_init;
    ft[CPU_FT].attr.walk_acceleration = eng.c->walk_acc;
    ft[CPU_FT].attr.dashrun_terminal_velocity = eng.c->terminal;
    ft[CPU_FT].attr.dash_initial_velocity = eng.c->dash_init;
    ft[CPU_FT].attr.dashrun_acceleration_a = eng.c->acc_a;
    ft[CPU_FT].attr.dashrun_acceleration_b = eng.c->acc_b;
    target = (struct target){ .ends_line = -1 };
    grab_count = total_hits = 0;
    retries = ended = post_sfx = records = error_sfx = pref_writes = 0;
    stc_match->match.timer = 0;
    stc_match->time_frames = 1;
    Event_Init(&event);
}
void TestGrabsFollower(void) { ft[CPU_FT].flags.has_follower = 1; }
static void Frame(void) {
    Animate();
    Event_Think(&event);
    Interrupt();
    Physics();
    Observe();
}
void TestGrabsRun(int frames) { while (frames-- > 0) Frame(); }
/* Puts the CPU_FT standing on a floor, moving a given way, without a respawn. */
void TestGrabsPlace(int x1000, int line, int move, int dir) {
    FighterData *f = &ft[CPU_FT];
    float y;
    if (!target.placed) Frame(); // The first spawn.
    target.shield = 0;
    LineY(line, x1000 / 1000.f, &y);
    f->phys.pos = (Vec3){x1000 / 1000.f, y, 0};
    f->phys.air_state = 0;
    f->phys.self_vel_ground.X = 0;
    f->coll_data.ground_index = line;
    f->facing_direction = dir;
    f->input.lstick.X = 0;
    f->input.timer_lstick_tilt_x = 254;
    f->input.lstick.Y = 0;
    f->input.timer_lstick_tilt_y = 254;
    target.run_phase = 0;
    Enter(ASID_WAIT, 60);
    target.move = move;
    target.dir = dir;
    ResetStats();
}
void TestGrabsRespawn(int human_x1000) {
    ft[HMN_FT].phys.pos.X = human_x1000 / 1000.f;
    Respawn();
}
void TestGrabsState(int state, int hitlag, int air) {
    FighterData *f = &ft[CPU_FT];
    f->state_id = state;
    f->dmg.hitlag_frames = hitlag;
    f->flags.hitlag = hitlag > 0;
    f->phys.air_state = air;
    f->phys.self_vel = (Vec3){0};
}
void TestGrabsPercent(int percent) { ft[CPU_FT].dmg.percent = percent; }
void TestGrabsMoveCPU(int x1000, int y1000) { ft[CPU_FT].phys.pos.X = x1000 / 1000.f; ft[CPU_FT].phys.pos.Y = y1000 / 1000.f; }
void TestGrabsClock(int frames, int count) { stc_match->time_frames = frames; grab_count = count; }
void TestGrabsBest(int value) { best = value; }
void TestGrabsMenu(int row, int value) {
    Options_Main[row].val_prev = Options_Main[row].val;
    Options_Main[row].val = value;
    if (Options_Main[row].OnChange) Options_Main[row].OnChange(&event, value);
}
void TestGrabsSelect(int row) { Options_Main[row].OnSelect(&event); }
int TestGrabsRow(int row, int key) {
    EventOption *option = &Options_Main[row];
    switch (key) {
        case 0: return option->val;
        case 1: return option->disable;
        case 2: return option->kind;
    }
    return -1;
}
unsigned TestGrabsRowName(int row) { return (unsigned)Options_Main[row].name; }
unsigned TestGrabsRowChoice(int row, int i) { return (unsigned)Options_Main[row].values[i]; }
int TestGrabsChooseMove(void) { return ChooseMove(); }
int TestGrabsChooseShield(void) { return ChooseShield(); }
void TestGrabsShield(int on) { target.shield = on; }
void TestGrabsShieldHealth(int health) { ft[CPU_FT].shield.health = health; }
int TestGrabsSpot(int key) {
    FighterData *f = &ft[CPU_FT];
    switch (key) {
        case 0: return f->phys.pos.X * 1000;
        case 1: return f->phys.pos.Y * 1000;
        case 2: return f->coll_data.ground_index;
        case 3: return target.move;
        case 4: return target.dir;
        case 5: return f->facing_direction;
        case 6: return f->state_id;
        case 7: return f->phys.self_vel_ground.X * 1000;
    }
    return -1;
}
int TestGrabsInfo(int key) {
    FighterData *f = &ft[CPU_FT];
    switch (key) {
        case 0: return grab_count;
        case 1: return best;
        case 2: return target.grabbed;
        case 3: return f->dmg.percent;
        case 4: return retries;
        case 5: return ended;
        case 6: return post_sfx;
        case 7: return records;
        case 8: return error_sfx;
        case 9: return f->cpu.ai;
        case 10: return f->cpu.held;
        case 11: return f->cpu.lstickX;
        case 12: return stc_match->match.timer == MATCH_TIMER_COUNTUP;
        case 13: return nana.state_id;
        case 14: return nana.phys.pos.Y;
        case 15: return f->shield.health;
        case 16: return f->show_hit;
        case 17: return counter_pref;
        case 18: return run_frame;
        case 19: return target.shield;
        case 20: return f->state_id;
        case 21: return target.hit;
        case 22: return total_hits;
        case 23: return target.juggle;
        case 24: return target.chase;
        case 25: return target.landing;
        case 26: return target.getup;
        case 27: return (u8)f->input.timer_LR;
        case 28: return (u8)f->input.since_rapid_lr;
        case 29: return f->cpu.lstickY;
    }
    return -1;
}
int TestGrabsPrefValue(int index) { return prefs[index]; }
int TestGrabsStat(int key) {
    switch (key) {
        case 0: return st.frames;
        case 1: return st.fell;
        case 2: return st.runs;
        case 3: return st.redashes;
        case 4: return st.dash_turns;
        case 5: return st.run_turns;
        case 6: return st.rolls;
        case 7: return st.walk_dashes;
        case 8: return st.dir_changes;
        case 9: return st.inputs;
        case 10: return st.top_walk * 1000;
        case 11: return st.top_run * 1000;
        case 12: return st.top_dash * 1000;
        case 13: return st.min_x * 1000;
        case 14: return st.max_x * 1000;
        case 15: return st.min_y * 1000;
        case 16: return st.max_y * 1000;
        case 17: return st.dash_starts;
        case 18: return st.long_legs;
        case 19: return st.short_legs;
        case 20: return st.short_back;
        case 21: return st.moved_back;
        case 22: return st.longest_leg;
        case 23: return st.legs;
        case 24: return st.alternations;
        case 25: return st.longest_after_first;
        case 26: return st.shortest_after_first;
        case 27: return st.farthest_after_first * 1000;
        case 28: return st.nearest_after_first * 1000;
        case 29: return st.crouches;
        case 30: return st.squat_dashes;
        case 31: return st.passes;
        case 32: return st.chase_state;
        case 33: return (st.chase_last_x - st.chase_x) * 1000;
        case 34: return st.getup_state;
    }
    return -1;
}
int TestGrabsChar(int key) {
    switch (key) {
        case 0: return eng.c->walk_max * 1000;
        case 1: return eng.c->terminal * 1000;
        case 2: return eng.c->dash_init * 1000;
        case 3: return eng.c->dash_flag;
    }
    return -1;
}

// Hit mode's tech chase ----------------------------------------------------------

void TestGrabsPlan(int landing, int getup) { target.landing = landing; target.getup = getup; }
// The event's picks for a roll in [0, 100): techs, or getups.
int TestGrabsPick(int getups, int roll) {
    return getups ? TC_PresetGetup(TECH_PRESET, roll) : TC_PresetTech(TECH_PRESET, roll);
}
void TestGrabsFall(int vy1000, int hitstun) {
    ft[CPU_FT].phys.self_vel.Y = vy1000 / 1000.f;
    ft[CPU_FT].flags.hitstun = hitstun;
}
void TestGrabsHuman(int x1000) { ft[HMN_FT].phys.pos.X = x1000 / 1000.f; }
void TestGrabsFace(int dir) { ft[CPU_FT].facing_direction = dir; }
void TestGrabsResetStats(void) { ResetStats(); }
/* A frame up to the native checks: animation, the event's think and the input
 * proc, without the probe's own interrupts or physics. */
void TestGrabsThinkInput(void) {
    Animate();
    Event_Think(&event);
    Input();
}
unsigned TestGrabsCPU(void) { return (unsigned)&fighters[CPU_FT]; }
/* Where the native checks' state entries go, and the stick angle they compare
 * (ftCo_GetLStickAngle: atan2(y, |x|), to within .005). */
static int native_entered = -1;
void TestGrabsNativeEnter(GOBJ *fighter, int state) { native_entered = state; }
void TestGrabsNativePassive(GOBJ *fighter) { native_entered = ASID_PASSIVE; }
void TestGrabsNativeDownWait(GOBJ *fighter) { native_entered = ASID_DOWNWAITU; }
float TestGrabsStickAngle(FighterData *f) {
    float x = fabs(f->input.lstick.X), y = f->input.lstick.Y, half_pi = 1.5707964f;
    if (x < 1e-6f) return y > 0 ? half_pi : y < 0 ? -half_pi : 0;
    float t = y / x;
    if (fabs(t) <= 1) return t / (1 + .28f * t * t);
    return (t > 0 ? half_pi : -half_pi) - t / (t * t + .28f);
}
int TestGrabsNativeEntered(void) {
    int state = native_entered;
    native_entered = -1;
    return state;
}
void TestGrabsDownBound(void) { // As a knockdown leaves it: its anim, and A and B unpressed since.
    Enter(ASID_DOWNBOUNDU, DOWNBOUND_LEN);
    ft[CPU_FT].input.timer_a = ft[CPU_FT].input.timer_b = 255;
}
void TestGrabsDownWait(void) {
    Enter(ASID_DOWNWAITU, 1000);
    eng.down_wait = DOWNWAIT_LEN;
}
