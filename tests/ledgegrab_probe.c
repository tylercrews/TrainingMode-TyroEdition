/* Production event, with fixtures only at native engine boundaries. */
#include "../src/ledgegrab.c"
#include <stdarg.h>
enum { SUBTEXTS = ROW_COUNT * ROW_SUBTEXTS };
static EventVars vars;
static GOBJ event, fighters[2], menu_object;
static FighterData ft[2];
static JOBJ joints[2];
static CmSubject subject, ledge_subjects[2];
static int ledge_subject_count;
static int gfx_count, gfx_shape, gfx_no_depth, gx_link, gx_priority;
static void (*gx_callback)(GOBJ *, int);
static GOBJ marker_gobj;
static MenuData menu_data;
static Text text;
static CollDataStage test_stage;
static CollLine test_lines[180];
static CollLineDesc test_descriptors[180];
static CollVert test_vertices[360];
static int test_enabled[180], test_stage_id = GRKINDEXT_BATTLE;
static int paused, has_nana, saves, loads, restarted = -1, ended, text_rows, text_writes;
static int shown_value[SUBTEXTS], shown_label[SUBTEXTS], row_y_shown[SUBTEXTS];
static u32 subtext_colors[SUBTEXTS];
static u32 carry_tag, carry_size;
static u8 carry_data[32];
/* 0x10 here means the fighter's previous buttons come from held. Without it they
 * come from x664, once, as on the fighter's very first input frame. */
#define INPUT_FLAGS 0x221D
#define INPUT_STARTED 0x10
static struct input saved_input;
static u8 saved_input_flags;
static int fighter_reads_first, camera_snaps;
static u32 saved_reset_speed = TM_LEDGEGRAB_RESET_SHORT;
static const char *counter_format;
static int counter_time[2];
static const char *best_format;
static char best_text[2][8];
static int shown_right[SUBTEXTS];
void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
/* The real savestate keeps the fighter's input and flags, which is what made a
 * held D-pad press look new again after every restart. */
static int Save(void *state, int flags) {
    saved_input = ft[0].input; saved_input_flags = ((u8 *)&ft[0])[INPUT_FLAGS];
    ++saves;
    return 1;
}
static int Load(void *state, int flags) {
    ft[0].input = saved_input; ((u8 *)&ft[0])[INPUT_FLAGS] = saved_input_flags;
    ++loads;
    return 1;
}
/* PlayerThink_Interrupt (0x8006AD10): reads the engine pad, makes down an edge
 * against the previous buttons, and takes a taunt from it straight away. */
static void FighterInput(void) {
    FighterData *f = &ft[0];
    u8 *input_flags = (u8 *)f + INPUT_FLAGS;
    f->input.held_prev = *input_flags & INPUT_STARTED ? f->input.held : f->input.x664;
    *input_flags |= INPUT_STARTED;
    f->input.held = PadGetEngine(0)->held;
    f->input.down = f->input.held & ~f->input.held_prev;
    if (f->state_id == ASID_WAIT && f->input.down & HSD_BUTTON_DPAD_UP)
        f->state_id = ASID_APPEALR;
}
static void Restart(int stage) { restarted = stage; }
static u32 SettingGet(unsigned field, unsigned index) {
    return field == TM_SETTING_LEDGEGRAB_RESET && index == 0 ? saved_reset_speed : 0;
}
static void SettingSet(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_LEDGEGRAB_RESET && index == 0) saved_reset_speed = value;
}
static const TMSettingsAPI settings_api = {SettingGet, SettingSet, 0};
static void GFXStart(u16 count, GFX_Params params) {
    gfx_count = count; gfx_shape = params.shape; gfx_no_depth = params.no_depth;
}
GOBJ *GObj_Create(int entity_class, int p_link, int p_priority) { return &marker_gobj; }
void GObj_AddGXLink(GOBJ *gobj, void *cb, int link, int priority) {
    gx_callback = cb; gx_link = link; gx_priority = priority;
}
CmSubject *CameraSubject_Alloc() { return &ledge_subjects[ledge_subject_count++ % 2]; }
static void *CarryStore(u32 tag, u32 size) { carry_tag = tag; carry_size = size; return carry_data; }
static const void *CarryTake(u32 tag, u32 size) {
    if (!tag || tag != carry_tag || size != carry_size) return 0;
    carry_tag = 0;
    return carry_data;
}
GOBJ *Fighter_GetGObj(int player) { return &fighters[0]; }
GOBJ *Fighter_GetSubcharGObj(int player, int sub) { return sub && has_nana ? &fighters[1] : 0; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
int GrColl_CheckIfLineEnabled(int line) { return test_enabled[line]; }
int Stage_GetExternalID(void) { return test_stage_id; }
void Fighter_SetAllHurtboxState(GOBJ *f, int kind) { }
/* As native (0x800761C8), for Fox on a stage that widens his front by 1.5. */
void Fighter_UpdateCameraBox(GOBJ *f) {
    FighterData *fighter = f->userdata;
    CmSubject *cam = fighter->camera_subject;
    int right = fighter->facing_direction > 0;
    cam->direction = right ? 1 : -1;
    cam->boundleft_proj = right ? -9 : -33;
    cam->boundright_proj = right ? 33 : 9;
    cam->cam_pos = fighter->phys.pos;
    cam->cam_pos.Y += 10;
}
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) { ((FighterData *)f->userdata)->state_id = ASID_WAIT; }
void Fighter_EnterSleep(GOBJ *f, int ms) { }
void Fighter_EnterRebirth(GOBJ *f) { ((FighterData *)f->userdata)->flags.dead = 0; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *normal,
    int a, int b, int c, void *cb, float x, float y, float x2, float y2, float e) {
    *pos = (Vec3){x, 0, 0}; *line = 1; return 1;
}
void Match_CorrectCamera(void) { ++camera_snaps; }
int Pause_CheckStatus(int kind) { return paused; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { ended = 1; }
int Text_CreateCanvas(int sis, GOBJ *cam, int entity, int plink, int priority, int gxlink, int gxpri, int cobjpri) { return 1; }
Text *Text_CreateText(int sis, int canvas) { return &text; }
int Text_AddSubtext(Text *t, float x, float y, char *fmt, ...) {
    row_y_shown[text_rows] = y;
    return text_rows++;
}
void Text_SetColor(Text *t, int subtext, GXColor *color) { subtext_colors[subtext] = *(u32 *)color; }
void Text_SetScale(Text *t, int subtext, float x, float y) { }
/* Only BestText's "%d". */
int sprintf(char *out, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int value = va_arg(args, int);
    va_end(args);
    char digits[12];
    int count = 0;
    do { digits[count++] = '0' + value % 10; value /= 10; } while (value);
    while (count) *out++ = digits[--count];
    *out = 0;
    return 0;
}
static int ShownBest(int side, const char *text) {
    int value = 0;
    for (int i = 0; (best_text[side][i] = text[i]); ++i)
        value = value * 10 + text[i] - '0';
    return text[0] == '-' ? -1 : value;
}
/* The row formats: "%s: %d", "%s: DNF", "%s: --", and Session Best's two times. */
void Text_SetText(Text *t, int subtext, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    if (fmt[0] == 'S') {
        best_format = fmt;
        shown_label[subtext] = 'S';
        shown_value[subtext] = ShownBest(0, va_arg(args, const char *));
        shown_right[subtext] = ShownBest(1, va_arg(args, const char *));
    } else {
        shown_label[subtext] = *va_arg(args, const char *);
        shown_value[subtext] = fmt[4] == '%' ? va_arg(args, int) : fmt[4] == 'D' ? -2 : -1;
        if (fmt[4] == '%' && fmt[7]) { // The counter's time.
            counter_format = fmt;
            for (int i = 0; i < 2; ++i) counter_time[i] = va_arg(args, int);
        }
    }
    va_end(args);
    ++text_writes;
}
unsigned TestLedgeBestText(int which) {
    return which == 2 ? (unsigned)best_format : (unsigned)best_text[which];
}
unsigned TestLedgeCounterFormat(void) { return (unsigned)counter_format; }
void TestLedgeSavedResetSpeed(int value) { saved_reset_speed = value; }
int TestLedgeInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event; vars.menu_gobj = &menu_object;
    vars.Savestate_Save_v1 = (void *)Save; vars.Savestate_Load_v1 = (void *)Load;
    vars.GFX_Start = GFXStart;
    vars.settings = &settings_api;
    vars.restart_on_stage = Restart; vars.carry_store = CarryStore; vars.carry_take = CarryTake;
    menu_object.userdata = &menu_data;
    for (int i = 0; i < 2; ++i) {
        fighters[i].userdata = &ft[i]; fighters[i].hsd_object = &joints[i];
        ft[i].camera_subject = &subject;
    }
    subject.boundtop_proj = 16; subject.boundbottom_proj = -9;
    *stc_colldata = &test_stage; *stc_collline = test_lines; *stc_collvert = test_vertices;
    text_rows = 0;
    Event_Init(&event);
    return (unsigned)&event;
}
void TestLedgeReset(void) { text_rows = 0; vars.game_timer = 0; }
unsigned TestLedgeObject(void) { return (unsigned)&event; }
void TestLedgeTimer(void) { ++vars.game_timer; }
void TestLedgeThink(int frames) {
    while (frames--) {
        ++vars.game_timer;
        HSD_Pad *pad = PadGetEngine(0);
        pad->down = pad->held & ~pad->heldPrev;
        pad->heldPrev = pad->held;
        if (fighter_reads_first && !paused) FighterInput();
        Event_Think(&event);
        if (!fighter_reads_first && !paused) FighterInput();
        Event_Update();
    }
}
void TestLedgeFighterReadsFirst(int value) { fighter_reads_first = value; }
void TestLedgeState(int state, int x) { ft[0].state_id = state; ft[0].phys.pos.X = x; }
/* A release can land between frames, so two presses in a row stay two presses. */
void TestLedgeHold(int buttons) {
    HSD_Pad *pad = PadGetEngine(0);
    pad->held = buttons;
    pad->heldPrev &= buttons;
}
void TestLedgeDead(void) { ft[0].flags.dead = 1; }
void TestLedgePause(int value) { paused = value ? 2 : 0; menu_data.mode = value ? MenuMode_Paused : 0; }
void TestLedgeNana(int value) { has_nana = value; ft[0].flags.has_follower = value; }
void TestLedgeChoice(int row, int value) {
    options[row].val = value;
    if (options[row].OnChange) options[row].OnChange(0, value);
}
void TestLedgeStageSwap(int row) { StageSwap_Options[row].OnSelect(0); }
void TestLedgeExit(void) { Exit(0); }
void TestLedgeDraw(int pass) { gx_callback(&marker_gobj, pass); }
unsigned TestLedgeDesc(int row, int line) { return (unsigned)options[row].desc[line]; }
int TestLedgeOptionCount(void) { return countof(options); }
int TestLedgeSubject(int ledge, int key) {
    CmSubject *cam = &ledge_subjects[ledge];
    switch (key) {
    case 0: return cam->cam_pos.X * 1000;
    case 1: return cam->cam_pos.Y * 1000;
    case 2: return cam->kind;
    case 3: return cam->boundleft_proj * 1000;
    case 4: return cam->boundright_proj * 1000;
    case 5: return cam->boundtop_proj * 1000;
    case 6: return cam->boundbottom_proj * 1000;
    case 7: return cam->boundleft_curr == cam->boundleft_proj && cam->boundright_curr == cam->boundright_proj &&
        cam->boundtop_curr == cam->boundtop_proj && cam->boundbottom_curr == cam->boundbottom_proj;
    }
    return -1;
}
int TestLedgeStage(int stage) { test_stage_id = stage; test_stage.line_num = test_stage.vert_num = 0; return stage; }
int TestLedgeAddLine(int ax, int ay, int bx, int by, int native_flags, int enabled) {
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
void TestLedgeEnableLine(int line, int enabled) { test_enabled[line] = enabled; }
int TestLedgeInfo(int key) {
    switch (key) {
    case 0: return shown_value[ROW_COUNTER * ROW_SUBTEXTS];
    case 1: return shown_value[ROW_BEST * ROW_SUBTEXTS];
    case 2: return shown_value[ROW_PREVIOUS * ROW_SUBTEXTS];
    case 3: return subtext_colors[ROW_FOREGROUND];
    case 4: return text.hidden;
    case 5: return ft[0].phys.pos.X * 1000;
    case 6: return ft[0].phys.pos.Y * 1000;
    case 7: return ft[0].facing_direction;
    case 8: return ft[0].coll_data.ground_index;
    case 9: return loads;
    case 10: return saves;
    case 11: return restarted;
    case 12: return ended;
    case 13: return options[OPT_GOAL].val;
    case 14: return options[OPT_START].val;
    case 15: return options[OPT_FACING].val;
    case 16: { int mask = 0; for (int i = 0; i < START_COUNT; ++i) mask |= start_available[i] << i; return mask; }
    case 17: return text_writes;
    case 18: return text_rows;
    case 19: return shown_label[ROW_COUNTER * ROW_SUBTEXTS];
    case 20: return shown_label[ROW_BEST * ROW_SUBTEXTS];
    case 21: return shown_label[ROW_PREVIOUS * ROW_SUBTEXTS];
    case 22: return row_y_shown[ROW_FOREGROUND];
    case 23: return row_y_shown[ROW_BEST * ROW_SUBTEXTS + ROW_FOREGROUND];
    case 24: return row_y_shown[ROW_PREVIOUS * ROW_SUBTEXTS + ROW_FOREGROUND];
    case 25: return sizeof(records);
    case 26: return (unsigned char)start_display[START_TOP][0] | (unsigned char)start_display[START_TOP][10] << 8;
    case 27: return ft[1].phys.pos.X * 1000;
    case 28: return ft[1].cpu.leader_log[0].pos.X * 1000;
    case 29: return run.frames;
    case 30: return ft[0].state_id;
    case 31: return ft[0].input.down;
    case 32: return gfx_count;
    case 33: return gfx_shape;
    case 34: return gfx_no_depth;
    case 35: return gx_link;
    case 36: return gx_priority;
    case 37: return ledge_subject_count;
    case 38: return options[OPT_CAMERA].val;
    case 39: return camera_snaps;
    case 40: return shown_right[ROW_BEST * ROW_SUBTEXTS];
    case 41: return options[OPT_RESET].val;
    case 42: return saved_reset_speed;
    case 43: return counter_time[0];
    case 44: return counter_time[1];
    }
    return -1;
}
