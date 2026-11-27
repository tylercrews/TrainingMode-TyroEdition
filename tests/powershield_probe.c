/* Production event with fixtures only at engine/render/save boundaries. */
#include "../src/powershield.c"
#include <stdarg.h>
static EventVars vars;
static GOBJ event, fighter[3], menu_object;
static FighterData ft[3];
static JOBJ joint[3];
static CmSubject camera[3];
static PowershieldData data;
static MenuData menu_data;
static Text text[2];
static int paused, text_count, rows[2], result[2], saved, ended, rng_bound, overlay_color;
static float animation_end;
static u32 text_colors[2][3];
static char labels[2][3][80];
static unsigned preferences[TM_PS_PREF_COUNT] = {0,0,20,0,0,0,1,1,0,1,0};
static const s16 *suppression;
static void (*contact_cb)(FighterData *, int, int, int);
static void (*timing_cb)(FighterData *, int, int);
static void (*clear_cb)(void);
static u32 Read(unsigned field, unsigned index) { return preferences[index]; }
static void Write(unsigned field, unsigned index, u32 value) { preferences[index] = value; }
static const TMSettingsAPI settings_api = {Read, Write, 0};
void memset(void *to, int value, int size) { u8 *p = to; while (size--) *p++ = value; }
void memcpy(void *to, const void *from, int size) { u8 *d = to; const u8 *s = from; while(size--) *d++ = *s++; }
int sprintf(char *out, const char *format, ...) {
    va_list args;
    va_start(args, format);
    for (; *format; ++format) {
        if (format[0] == '%' && format[1] == 's') {
            for (const char *s = va_arg(args, const char *); *s; ++s) *out++ = *s;
            ++format;
        } else *out++ = *format;
    }
    *out = 0;
    va_end(args);
    return 0;
}
static void Observe(void (*contact)(FighterData *, int, int, int), void (*timing)(FighterData *, int, int), void (*clear)(void)) {
    contact_cb = contact; timing_cb = timing; clear_cb = clear;
}
static void Suppress(const s16 *value) { suppression = value; }
static void Overlay(GOBJ *obj, const GXColor *color) { overlay_color = color ? *(u32 *)color : 0; }
GOBJ *Fighter_GetGObj(int ply) { return &fighter[ply]; }
GOBJ *Fighter_GetSubcharGObj(int ply, int sub) { return &fighter[sub ? 2 : ply]; }
void Fighter_ZeroCPUInputs(FighterData *data) { memset(&data->cpu, 0, sizeof(data->cpu)); }
void Fighter_EnterSleep(GOBJ *object, int sub) { ((FighterData *)object->userdata)->state_id = ASID_SLEEP; }
void Fighter_EnterDeadDown(GOBJ *object) { ((FighterData *)object->userdata)->state_id = ASID_DEADDOWN; }
void Fighter_EnterWait(GOBJ *object) {
    FighterData *f = object->userdata;
    f->state_id = ASID_WAIT; f->TM.state_frame = 0;
}
void Fighter_SetGrounded(FighterData *data) { data->phys.air_state = 0; }
void Fighter_SetPosition(int ply, int sub, Vec3 *position) { }
void Fighter_UpdateCameraBox(GOBJ *object) { }
void Fighter_SetAllHurtboxState(GOBJ *object, int kind) { }
float Fighter_GetCurrentAnimLength(GOBJ *object) { return animation_end; }
void JOBJ_SetMtxDirtySub(JOBJ *object) { }
void EnvironmentCollision_WaitLanding(GOBJ *object) { }
void Match_CorrectCamera(void) { }
void Match_EndVS(void) { ended = 1; }
void Memcard_SaveIfChanged(void) { ++saved; }
/* A fixed draw, or a repeating list of draws, each taken modulo the bound. */
static int rng_values[16], rng_count, rng_next;
int HSD_Randi(int bound) {
    rng_bound = bound;
    if (!rng_count || bound <= 0) return 0;
    int value = rng_values[rng_next++ % rng_count];
    return value % bound;
}
void __assert(const char *file, int line, const char *message) { }
int Pause_CheckStatus(int arg) { return paused; }
int SFX_PlayCommon(int sfx) { return 0; }
int Fighter_GetExternalID(int ply) {
    if (ply) return ft[1].kind == FTKIND_SAMUS ? CKIND_SAMUS : CKIND_FALCO;
    return CKIND_FOX;
}
int Fighter_GetCostumeID(int ply) { return 0; }
/* The savestate keeps where each fighter stands; a mirrored load flips it. */
static struct { Vec3 pos; float facing; int state, line; } snapshot[2];
static int saves, loads, last_load_flags;
static int Save(Savestate_v1 *state, int flags) {
    for (int i = 0; i < 2; ++i)
        snapshot[i].pos = ft[i].phys.pos, snapshot[i].facing = ft[i].facing_direction,
        snapshot[i].state = ft[i].state_id, snapshot[i].line = ft[i].coll_data.ground_index;
    ++saves;
    return 1;
}
static int Load(Savestate_v1 *state, int flags) {
    if (clear_cb) clear_cb();
    ++loads; last_load_flags = flags;
    if (!saves) return 1;
    for (int i = 0; i < 2; ++i) {
        ft[i].phys.pos = snapshot[i].pos; ft[i].facing_direction = snapshot[i].facing;
        ft[i].state_id = snapshot[i].state; ft[i].coll_data.ground_index = snapshot[i].line;
        if (flags & Savestate_Mirror) { ft[i].phys.pos.X *= -1; ft[i].facing_direction *= -1; }
    }
    return 1;
}
int Text_CreateCanvas(int a, GOBJ *b, int c, int d, int e, int f, int g, int h) { return 1; }
Text *Text_CreateText(int sis, int canvas) { return &text[text_count++]; }
int Text_AddSubtext(Text *t, float x, float y, char *format, ...) {
    unsigned which = t - text;
    if (rows[which] == 0) {
        unsigned i = 0;
        for (; format[i] && i < 79; ++i) labels[which][0][i] = format[i];
        labels[which][0][i] = 0;
    }
    return rows[which]++;
}
void Text_SetColor(Text *t, int row, GXColor *color) { text_colors[t - text][row] = *(u32 *)color; }
void Text_SetText(Text *t, int row, const char *format, ...) {
    unsigned which = t - text;
    va_list args; va_start(args, format);
    const char *source = format;
    if (format[0] == '%' && format[1] == 's') source = va_arg(args, const char *);
    else if (row == 1) result[which] = format[0] == '-' ? -1 : va_arg(args, int);
    unsigned i = 0;
    for (; source[i] && i < 79; ++i) labels[which][row][i] = source[i];
    labels[which][row][i] = 0;
    va_end(args);
}

/* Live collision lines, as in the Low Laser probe. */
static CollDataStage test_stage;
static CollLine test_lines[180];
static CollLineDesc test_descriptors[180];
static CollVert test_vertices[360];
static int test_enabled[180], test_stage_id = GRKINDEXT_FD, restarted = -1, restart_cpu = -2;
int Stage_GetExternalID(void) { return test_stage_id; }
int GrColl_CheckIfLineEnabled(int line) { return test_enabled[line]; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *normal,
    int a, int b, int c, void *cb, float x, float y, float x2, float y2, float e) {
    *pos = (Vec3){x, 0, 0}; *line = -1; return 1;
}
static void RestartStage(int stage) { restarted = stage; }
static int RestartFighters(int hmn, int hmn_costume, int cpu, int cpu_costume) { restart_cpu = cpu; return 1; }
int TestPSStage(int stage) { test_stage_id = stage; test_stage.line_num = test_stage.vert_num = 0; return stage; }
int TestPSAddLine(int ax, int ay, int bx, int by, int native_flags, int enabled) {
    int i = test_stage.line_num++;
    test_stage.vert_num += 2;
    test_lines[i].desc = &test_descriptors[i];
    memset(&test_descriptors[i], 0, sizeof(test_descriptors[i]));
    test_descriptors[i].vert_prev = 2 * i;
    test_descriptors[i].vert_next = 2 * i + 1;
    test_descriptors[i].is_floor = 1;
    ((u8 *)&test_descriptors[i])[0xE] = native_flags >> 8;
    ((u8 *)&test_descriptors[i])[0xF] = native_flags;
    test_enabled[i] = enabled;
    test_vertices[2 * i].pos_curr = (Vec2){ax / 1000.f, ay / 1000.f};
    test_vertices[2 * i + 1].pos_curr = (Vec2){bx / 1000.f, by / 1000.f};
    return i;
}
/* Move a live line, as Fountain's platforms do between frames. */
void TestPSMoveLine(int line, int dy, int enabled) {
    test_vertices[2 * line].pos_curr.Y += dy / 1000.f;
    test_vertices[2 * line + 1].pos_curr.Y += dy / 1000.f;
    test_enabled[line] = enabled;
}

/* A model of Samus for the event's raw inputs, read the next frame as the game
 * reads a CPU's. Its numbers are PlSs.dat's and PlCo.dat's (see the doc): a
 * 3-frame jumpsquat into a 1.7 short hop or 2.1 full hop (whose first frame
 * skips gravity), 0.066 gravity to 1.4, and her air ECB's bottom 3.4 over her
 * feet. B with the stick 0.6 or more sideways (and less than 0.55 up or down)
 * is side-B, a super missile if the stick crossed 0.8 fewer than 3 frames
 * before; it spawns 9.92 over her feet on its 16th frame and lasts 50. A smash
 * down (0.66 within 6 frames) on a platform squats 2 frames, then Pass drops
 * through at -0.5. Landing lasts until she acts. */
#define SIM_HOP 1.7f
#define SIM_JUMP 2.1f
#define SIM_GRAVITY .066f
#define SIM_TERMINAL 1.4f
#define SIM_ECB 3.4f
#define SIM_RISE 9.92f
enum { SIM_HOMING = 351, SIM_SUPER = 352 };
typedef struct SimMissile { int height, x, dir, smash, descending, from_platform, frame; } SimMissile;
static SimMissile missiles[64];
static int missile_count, wrong_moves, fastfalls, platform_landings, drops, ground_landings, sim_frames;
static struct {
    int held, x, y, x_cross, y_cross, frame, squat, pass_line, first_air, from_platform;
} sim = { .x_cross = 255, .y_cross = 255, .pass_line = -1 };
static void SimState(FighterData *f, int state) { f->state_id = state; f->TM.state_frame = 0; sim.frame = 0; }
static float SimFloor(float x, int solid_only, int *index) {
    float best = -1000;
    *index = -1;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        float y;
        if (!FloorLine(i, &a, &b, &platform) || (solid_only && platform) || !LineY(i, x, &y)) continue;
        if (y > best) { best = y; *index = i; }
    }
    return best;
}
static void SimLand(FighterData *f, int line, float y) {
    Vec2 a, b;
    int platform = 0;
    FloorLine(line, &a, &b, &platform);
    f->phys.pos.Y = y; f->phys.self_vel.Y = 0; f->phys.air_state = 0;
    f->coll_data.ground_index = line;
    if (platform) ++platform_landings; else ++ground_landings;
    SimState(f, ASID_LANDING);
}
void TestPSEngineTick(void) {
    FighterData *f = &ft[1];
    int held = f->cpu.held, x = f->cpu.lstickX, y = f->cpu.lstickY;
    int pressed = held & ~sim.held;
    float sx = x / 127.f, sy = y / 127.f, px = sim.x / 127.f, py = sim.y / 127.f;
    sim.x_cross = fabs(sx) >= .8f && fabs(px) < .8f ? 0 : sim.x_cross < 255 ? sim.x_cross + 1 : 255;
    sim.y_cross = sy <= -.66f && py > -.66f ? 0 : sim.y_cross < 255 ? sim.y_cross + 1 : 255;
    sim.held = held; sim.x = x; sim.y = y;
    int state = f->state_id, air = f->phys.air_state;
    ++sim_frames;
    if (!air) {
        Vec2 a, b;
        int platform = 0;
        FloorLine(f->coll_data.ground_index, &a, &b, &platform);
        if (state == ASID_KNEEBEND) {
            if (sim.frame >= 2) {
                f->phys.self_vel.Y = held & PAD_BUTTON_Y ? SIM_JUMP : SIM_HOP;
                f->phys.air_state = 1; sim.first_air = 1; sim.from_platform = 0;
                SimState(f, ASID_JUMPF);
            }
        } else if (state == ASID_SQUAT) {
            if (sim.squat && --sim.squat == 0) {
                f->phys.air_state = 1; f->phys.self_vel.Y = -.5f; sim.pass_line = f->coll_data.ground_index;
                sim.from_platform = 1; ++drops;
                SimState(f, ASID_PASS);
            } else if (!sim.squat && sy > -.6875f) SimState(f, ASID_WAIT);
        } else if (state == ASID_WAIT || (state == ASID_LANDING && f->TM.state_frame >= 4)) {
            if (pressed & PAD_BUTTON_Y) SimState(f, ASID_KNEEBEND);
            else if (pressed & PAD_BUTTON_B) ++wrong_moves;
            else if (sy < -.6875f) {
                SimState(f, ASID_SQUAT);
                sim.squat = platform && sy <= -.66f && sim.y_cross < 6 ? 2 : 0;
            } else if (state == ASID_LANDING && f->TM.state_frame >= 29) SimState(f, ASID_WAIT);
        }
    } else {
        int can_act = state == ASID_JUMPF || state == ASID_FALL || state == ASID_PASS;
        if (can_act && (pressed & PAD_BUTTON_B)) {
            if (fabs(sy) >= .55f || fabs(sx) < .6f) ++wrong_moves;
            else {
                if (sx * f->facing_direction < -.2f) f->facing_direction = sx > 0 ? 1 : -1;
                f->phys.self_vel.X /= 2;
                SimState(f, sim.x_cross < 3 ? SIM_SUPER : SIM_HOMING);
            }
        } else if (can_act && f->phys.self_vel.Y < 0 && sy <= -.66f && sim.y_cross < 4) ++fastfalls;
        state = f->state_id;
        /* Physics: the jump's first frame rises its whole speed. */
        if (state == ASID_JUMPF && sim.first_air) sim.first_air = 0;
        else {
            f->phys.self_vel.Y -= SIM_GRAVITY;
            if (f->phys.self_vel.Y < -SIM_TERMINAL) f->phys.self_vel.Y = -SIM_TERMINAL;
        }
        sim.first_air = 0;
        float before = f->phys.pos.Y + SIM_ECB;
        f->phys.pos.Y += f->phys.self_vel.Y;
        float after = f->phys.pos.Y + SIM_ECB;
        if ((state == SIM_SUPER && sim.frame == 15) || (state == SIM_HOMING && sim.frame == 24)) {
            int line;
            float floor = SimFloor(f->phys.pos.X, 1, &line);
            if (missile_count < 64) missiles[missile_count++] = (SimMissile){
                (f->phys.pos.Y + SIM_RISE - floor) * 1000, f->phys.pos.X * 1000, f->facing_direction,
                state == SIM_SUPER, f->phys.self_vel.Y < 0, sim.from_platform, sim_frames };
        }
        if (f->phys.self_vel.Y < 0) {
            /* Land on a floor crossed from above; a dropped platform lets her through. */
            int best = -1;
            float best_y = -1000;
            for (int i = 0; i < LineCount(); ++i) {
                float line_y;
                if (i == sim.pass_line || !LineY(i, f->phys.pos.X, &line_y)) continue;
                if (before >= line_y && after < line_y && line_y > best_y) { best = i; best_y = line_y; }
            }
            if (best >= 0) { SimLand(f, best, best_y); sim.pass_line = -1; return; }
            float pass_y;
            if (sim.pass_line >= 0 && (!LineY(sim.pass_line, f->phys.pos.X, &pass_y) || after < pass_y - 1))
                sim.pass_line = -1;
        }
        int end = state == SIM_SUPER ? 50 : state == SIM_HOMING ? 60 : state == ASID_JUMPF ? 40 : state == ASID_PASS ? 30 : 0;
        if (end && sim.frame + 1 >= end) { SimState(f, ASID_FALL); return; }
    }
    ++sim.frame;
    ++f->TM.state_frame;
}
/* One frame: the engine, then the event's think and update. */
void TestPSRun(int frames) {
    for (int i = 0; i < frames; ++i) {
        if (vars.game_timer) TestPSEngineTick();
        ++vars.game_timer;
        ++stc_match->time_frames;
        Event_Think(&menu_object);
        Event_Update();
    }
}
/* A fresh match with this CPU on the current stage, as the restart would load it. */
void TestPSMatch(int samus) {
    ft[1].kind = samus ? FTKIND_SAMUS : FTKIND_FALCO;
    ft[1].attr.gravity = samus ? SIM_GRAVITY : .17f;
    ft[1].attr.terminal_velocity = samus ? SIM_TERMINAL : 3.1f;
    for (int i = 0; i < 2; ++i) {
        ft[i].phys.pos = (Vec3){i ? 30 : -30, 10, 0}; ft[i].phys.self_vel = (Vec3){0};
        ft[i].phys.air_state = 0; ft[i].state_id = ASID_WAIT; ft[i].facing_direction = i ? -1 : 1;
        ft[i].coll_data.ground_index = -1;
    }
    memset(&sim, 0, sizeof(sim)); sim.x_cross = sim.y_cross = 255; sim.pass_line = -1;
    missile_count = wrong_moves = fastfalls = platform_landings = drops = ground_landings = sim_frames = 0;
    saves = loads = 0; vars.game_timer = 0; restarted = -1; restart_cpu = -2;
    text_count = rows[0] = rows[1] = 0;
    Event_Init(&event); Event_Update();
}
void TestPSSetPreference(int index, int value) { preferences[index] = value; }
void TestPSRandom(int count, int a, int b, int c, int d) {
    rng_values[0] = a; rng_values[1] = b; rng_values[2] = c; rng_values[3] = d;
    rng_count = count; rng_next = 0;
}
int TestPSMissile(int index, int key) {
    if (index >= missile_count) return -99999;
    SimMissile *m = &missiles[index];
    return (int[]){m->height, m->x, m->dir, m->smash, m->descending, m->from_platform, m->frame}[key];
}
int TestPSSim(int key) {
    switch (key) {
    case 0: return missile_count;
    case 1: return wrong_moves;
    case 2: return fastfalls;
    case 3: return platform_landings;
    case 4: return drops;
    case 5: return ground_landings;
    case 6: return ft[1].phys.pos.X * 1000;
    case 7: return ft[1].phys.pos.Y * 1000;
    case 8: return ft[0].phys.pos.X * 1000;
    case 9: return ft[0].phys.pos.Y * 1000;
    case 10: return ft[0].facing_direction;
    case 11: return ft[1].facing_direction;
    case 12: return restarted;
    case 13: return ft[1].state_id;
    case 14: return ft[1].cpu.held | (u8)ft[1].cpu.lstickX << 8 | (u8)ft[1].cpu.lstickY << 16;
    case 15: return saves;
    case 16: return last_load_flags;
    case 17: return ft[1].coll_data.ground_index;
    case 18: return contact_cb != 0;
    case 19: return suppression ? *suppression : -1;
    }
    return 0;
}
/* Menu rows: their kind, gating, values and labels. */
int TestPSRow(int row, int key) {
    EventOption *o = &Options_Main[row];
    switch (key) {
    case 0: return o->disable;
    case 1: return o->val;
    case 2: return (unsigned)o->name;
    case 3: return (unsigned)o->values;
    case 4: return o->value_num;
    case 5: return o->kind == OPTKIND_MENU ? (unsigned)o->menu : 0;
    case 6: return (unsigned)o->desc[0];
    case 7: return (unsigned)o->desc[1];
    }
    return 0;
}
unsigned TestPSRowValue(int row, int value) { return (unsigned)Options_Main[row].values[value]; }
int TestPSPrefRow(int index) { return preference_rows[index]; }
int TestPSSwap(int key) {
    switch (key) {
    case 0: return FighterSwap_Options[1].value_num;
    case 1: return (unsigned)FighterSwap_Options[1].values[0];
    case 2: return (unsigned)FighterSwap_Options[1].values[1];
    case 3: return FighterSwap_Options[1].disable;
    case 4: return FighterSwap_Options[0].disable;
    case 5: return (unsigned)&StageSwap_Menu;
    case 6: return (unsigned)&Menu_Characters;
    case 7: { int mask = 0; for (int i = 0; i < STAGE_SWAP_COUNT; ++i) mask |= StageSwap_Options[i].disable << i; return mask; }
    case 8: return restart_cpu;
    case 9: return FighterSwap_Options[1].val;
    }
    return 0;
}
/* Pick a CPU on the CPU Character row, then press A. */
int TestPSSwapCPU(int index) {
    FighterSwap_Options[1].val = index;
    FighterSwap_Options[1].OnChange(0, index);
    return FighterSwap_RowHooks.input(0, &FighterSwap_Options[1], HSD_BUTTON_A);
}
void TestPSStageSwap(int row) { StageSwap_Options[row].OnSelect(0); }

void TestPSInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event; vars.menu_gobj = &menu_object; vars.settings = &settings_api;
    vars.set_powershield_observer = Observe; vars.set_osd_suppression = Suppress; vars.set_timing_overlay = Overlay;
    vars.Savestate_Save_v1 = Save; vars.Savestate_Load_v1 = Load;
    vars.restart_on_stage = RestartStage; vars.restart_on_fighters = RestartFighters;
    event.userdata = &data; menu_object.userdata = &menu_data;
    *stc_colldata = &test_stage; *stc_collline = test_lines; *stc_collvert = test_vertices;
    for (int i = 0; i < 3; ++i) {
        fighter[i].userdata = &ft[i]; fighter[i].hsd_object = &joint[i];
        ft[i].fighter = &fighter[i]; ft[i].ply = i == 2 ? 0 : i; ft[i].flags.ms = i == 2;
        ft[i].camera_subject = &camera[i]; ft[i].state_id = ASID_WAIT;
    }
    ft[1].kind = FTKIND_FALCO;
    Event_Init(&event); Event_Update();
}
void TestPSReenter(void) { text_count = rows[0] = rows[1] = 0; TestPSInit(); }
void TestPSContact(int kind, int frame, int reason, int slot) { contact_cb(&ft[slot], kind, frame, reason); Event_Update(); }
void TestPSTiming(int phase, int next) { timing_cb(&ft[0], phase, next); Event_Update(); }
void TestPSSample(int tick, int state, int stun, int frozen) {
    stc_match->time_frames = tick; ft[0].state_id = state;
    float remaining = stun; memcpy(&ft[0].state_var.state_var1, &remaining, 4);
    ft[0].flags.hitstun = stun > 0; ft[0].flags.hitlag = frozen; ft[0].flags.freeze = frozen;
}
/* index is a saved preference (TM_PS_*), changed on its menu row. */
void TestPSChoice(unsigned index, int value) {
    unsigned row = preference_rows[index];
    Options_Main[row].val = value;
    if (Options_Main[row].OnChange) Options_Main[row].OnChange(0, value);
    else if (Options_Main[row].OnSelect) Options_Main[row].OnSelect(0);
    Event_Update();
}
void TestPSPause(int value) { paused = value ? 2 : 0; menu_data.mode = value; Event_Update(); }
void TestPSRestore(void) { clear_cb(); Event_Update(); }
void TestPSYoshi(void) { ft[0].kind = FTKIND_YOSHI; }
void TestPSAnimation(int frame_tenths, int rate_tenths, int end_tenths) {
    ft[0].state.frame = frame_tenths / 10.f;
    ft[0].state.rate = rate_tenths / 10.f;
    animation_end = end_tenths / 10.f;
}
void TestPSExit(void) { Exit(0); }
int TestPSEscape(int which) { return (int[]){ASID_ESCAPEF, ASID_ESCAPEB, ASID_ESCAPE}[which]; }
int TestPSDelay(int low, int high) {
    Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val = low;
    Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val = high;
    GetRandomLaserDelay(); return rng_bound;
}
int TestPSInfo(unsigned field) {
    switch (field) {
    case 0: return data.act_frame;
    case 1: return data.act_kind;
    case 2: return overlay_color;
    case 3: return data.opportunities;
    case 4: return text[0].hidden;
    case 5: return text[1].hidden;
    case 6: return suppression ? *suppression : -1;
    case 7: return text_colors[0][1];
    case 8: return text_colors[1][1];
    case 9: return data.ps_valid;
    case 10: return contact_cb != 0;
    case 11: return saved;
    case 12: return ended;
    case 13: return text_count;
    case 14: return result[1];
    case 15: return (unsigned)&Options_Main;
    case 16: return (unsigned)&help_menu;
    case 17: return (unsigned)&ft[0];
    }
    return 0;
}
unsigned TestPSLabel(unsigned which, unsigned row, unsigned index) { return labels[which][row][index]; }
unsigned TestPSPreference(unsigned index) { return preferences[index]; }
