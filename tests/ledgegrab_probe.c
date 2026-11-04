/* Production event, with fixtures only at native engine boundaries. */
#include "../src/ledgegrab.c"
#include <stdarg.h>
enum { SUBTEXTS = ROW_COUNT * ROW_SUBTEXTS };
static EventVars vars;
static GOBJ event, fighters[2], menu_object;
static FighterData ft[2];
static JOBJ joints[2];
static CmSubject subject;
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
void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
static int Save(void *state, int flags) { ++saves; return 1; }
static int Load(void *state, int flags) { ++loads; return 1; }
static void Restart(int stage) { restarted = stage; }
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
void Fighter_UpdateCameraBox(GOBJ *f) { }
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) { ((FighterData *)f->userdata)->state_id = ASID_WAIT; }
void Fighter_EnterSleep(GOBJ *f, int ms) { }
void Fighter_EnterRebirth(GOBJ *f) { ((FighterData *)f->userdata)->flags.dead = 0; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *normal,
    int a, int b, int c, void *cb, float x, float y, float x2, float y2, float e) {
    *pos = (Vec3){x, 0, 0}; *line = 1; return 1;
}
void Match_CorrectCamera(void) { }
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
/* The three row formats: "%s: %d", "%s: DNF" and "%s: -". */
void Text_SetText(Text *t, int subtext, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    shown_label[subtext] = *va_arg(args, const char *);
    shown_value[subtext] = fmt[4] == '%' ? va_arg(args, int) : fmt[4] == 'D' ? -2 : -1;
    va_end(args);
    ++text_writes;
}
int TestLedgeInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event; vars.menu_gobj = &menu_object;
    vars.Savestate_Save_v1 = (void *)Save; vars.Savestate_Load_v1 = (void *)Load;
    vars.restart_on_stage = Restart; vars.carry_store = CarryStore; vars.carry_take = CarryTake;
    menu_object.userdata = &menu_data;
    for (int i = 0; i < 2; ++i) {
        fighters[i].userdata = &ft[i]; fighters[i].hsd_object = &joints[i];
        ft[i].camera_subject = &subject;
    }
    *stc_colldata = &test_stage; *stc_collline = test_lines; *stc_collvert = test_vertices;
    text_rows = 0;
    Event_Init(&event);
    return (unsigned)&event;
}
void TestLedgeReset(void) { text_rows = 0; vars.game_timer = 0; }
unsigned TestLedgeObject(void) { return (unsigned)&event; }
void TestLedgeTimer(void) { ++vars.game_timer; }
void TestLedgeThink(int frames) {
    while (frames--) { ++vars.game_timer; Event_Think(&event); Event_Update(); }
}
void TestLedgeState(int state, int x) { ft[0].state_id = state; ft[0].phys.pos.X = x; }
void TestLedgeDown(int buttons) { ft[0].input.down = buttons; }
void TestLedgeDead(void) { ft[0].flags.dead = 1; }
void TestLedgePause(int value) { paused = value ? 2 : 0; menu_data.mode = value ? MenuMode_Paused : 0; }
void TestLedgeNana(int value) { has_nana = value; ft[0].flags.has_follower = value; }
void TestLedgeChoice(int row, int value) {
    options[row].val = value;
    if (options[row].OnChange) options[row].OnChange(0, value);
}
void TestLedgeStageSwap(int row) { StageSwap_Options[row].OnSelect(0); }
void TestLedgeExit(void) { Exit(0); }
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
    }
    return -1;
}
