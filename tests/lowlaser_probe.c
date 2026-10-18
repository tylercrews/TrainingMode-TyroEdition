/* Production event, with fixtures only at native engine boundaries. */
#include "../src/lowlaser.c"
#include <stdarg.h>
static EventVars vars;
static GOBJ event, fighters[2], menu_object;
static FighterData ft[2];
static JOBJ joints[2];
static JOBJ model_joints[2];
static LowLaserData data;
static MenuData menu_data;
static Text texts[2];
static unsigned preferences[TM_LOW_LASER_PREF_COUNT] = {1, 0, 0, 0}, serial;
static CollDataStage test_stage;
static CollLine test_lines[180];
static CollLineDesc test_descriptors[180];
static CollVert test_vertices[360];
static int test_enabled[180], test_stage_id = GRKINDEXT_FD;
static FtMultiJumpDesc puff_jumps;
static int label_neutral_b;
static int paused, text_count, text_rows[2], text_result[2], writes;
static u32 text_colors[2][2];
static const s16 *suppression;
static int percent_hud, ended, restarted = -1;
static ftCommonData common;
void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
static u32 Read(unsigned field, unsigned index) { return preferences[index]; }
static void Write(unsigned field, unsigned index, u32 value) { preferences[index] = value; ++writes; }
static const TMSettingsAPI api = {Read, Write, 0};
static unsigned Serial(void) { return serial; }
static void Suppress(const s16 *enabled) { suppression = enabled; }
static void Restart(int stage) { restarted = stage; }
GOBJ *Fighter_GetGObj(int player) { return &fighters[player]; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) {
    joint->flags |= JOBJ_MTX_DIRTY;
    if (joint->child) JOBJ_SetMtxDirtySub(joint->child);
}
void JOBJ_SetupMtxSub(JOBJ *joint) {
    joint->flags &= ~JOBJ_MTX_DIRTY;
    if (joint->child) JOBJ_SetupMtxSub(joint->child);
}
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
void Fighter_SetAirborne(FighterData *f) { f->phys.air_state = 1; }
int GrColl_CheckIfLineEnabled(int line) { return test_enabled[line]; }
int Stage_GetExternalID(void) { return test_stage_id; }
void Fighter_SetAllHurtboxState(GOBJ *f, int kind) { }
void Fighter_UpdateCameraBox(GOBJ *f) { }
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) { ((FighterData *)f->userdata)->state_id = ASID_WAIT; }
void Fighter_EnterSleep(GOBJ *f, int ms) { }
void Fighter_EnterRebirth(GOBJ *f) { ((FighterData *)f->userdata)->flags.dead = 0; }
void Fighter_ZeroCPUInputs(FighterData *f) { memset(&f->input, 0, sizeof(f->input)); }
void ActionStateChange(float frame, float speed, float blend, GOBJ *f, int state, int flags, GOBJ *source) {
    FighterData *p = f->userdata;
    p->state_id = state; p->state.frame = frame; p->state.rate = speed;
    p->cb.Coll = 0; /* Native ActionStateChange installs the new state's callbacks. */
    /* Record the native blend choice; visual cache invalidation is separate. */
    p->state.blend = blend == -1 ? 0 : blend == 0 ? 6 : blend;
    if (p->state.blend == 0) ((JOBJ *)f->hsd_object)->child->trans.Y = 0;
}
void Fighter_SubactionFastForward(GOBJ *f) { }
void Fighter_SetHUDDamage(int player, u16 value) { percent_hud = value; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *normal,
    int a, int b, int c, void *cb, float x, float y, float x2, float y2, float e) {
    *pos = (Vec3){x, 0, 0}; *line = 1; return 1;
}
void Match_CorrectCamera(void) { }
int Pause_CheckStatus(int kind) { return paused; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { ended = 1; }
int Text_CreateCanvas(int sis, GOBJ *cam, int entity, int plink, int priority, int gxlink, int gxpri, int cobjpri) { return 1; }
Text *Text_CreateText(int sis, int canvas) { return &texts[text_count++]; }
int Text_AddSubtext(Text *text, float x, float y, char *fmt, ...) { return text_rows[text - texts]++; }
void Text_SetColor(Text *text, int row, GXColor *color) { text_colors[text - texts][row] = *(u32 *)color; }
void Text_SetText(Text *text, int row, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    if (row == 0) { label_neutral_b = fmt[0] == 'N'; va_end(args); return; }
    text_result[text - texts] = fmt[0] == '-' ? -1 : va_arg(args, int);
    va_end(args);
}
int TestLaserInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event; vars.menu_gobj = &menu_object;
    vars.settings = &api; vars.get_restore_serial = Serial; vars.set_osd_suppression = Suppress;
    vars.restart_on_stage = Restart;
    event.userdata = &data; menu_object.userdata = &menu_data;
    for (int i = 0; i < 2; ++i) {
        fighters[i].userdata = &ft[i]; fighters[i].hsd_object = &joints[i]; ft[i].ply = i;
        joints[i].child = &model_joints[i];
    }
    *stc_colldata = &test_stage; *stc_collline = test_lines; *stc_collvert = test_vertices;
    /* Jigglypuff's PlPr.dat values: gravity, hop, jump, generic air multiplier 0,
     * and the first entry of its multi-jump table at special attributes 0x14. */
    ft[0].kind = FTKIND_FOX; ft[1].kind = FTKIND_JIGGLYPUFF;
    ft[1].attr.gravity = .064f; ft[1].attr.hop_v_initial_velocity = 1.05f;
    ft[1].attr.jump_v_initial_velocity = 1.6f; ft[1].attr.air_jump_v_multiplier = 0;
    puff_jumps.jump_vel_y[0] = 1.65f; ft[1].special_attributes = &puff_jumps;
    Event_Init(&event);
    return (unsigned)&event;
}
void TestLaserEngineTick(void) {
    if (paused) return;
    /* Real order: physics callback at priority 4, native model processing at
     * 6..16, then this event at priority 16. A paused frame skips both. */
    if (ft[1].cb.Phys) ft[1].cb.Phys(&fighters[1]);
    joints[1].trans = ft[1].phys.pos;
    JOBJ_SetupMtxSub(&joints[1]);
}
void TestLaserThink(void) { TestLaserEngineTick(); Event_Think(&event); Event_Update(); }
void TestLaserSample(int state, int airborne, int fastfall, int eligible, int frozen) {
    ft[0].state_id = state; ft[0].phys.air_state = airborne;
    ft[0].flags.is_fastfall = fastfall; ft[0].TM.can_fastfall_frames = eligible;
    ft[0].flags.freeze = frozen;
}
void TestLaserMove(int kind, int atk) { ft[0].kind = kind; ft[0].atk_kind = atk; }
int TestLaserStage(int stage) { test_stage_id = stage; test_stage.line_num = test_stage.vert_num = 0; return stage; }
int TestLaserAddLine(int ax, int ay, int bx, int by, int native_flags, int enabled) {
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
void TestLaserMoveLine(int line, int dy, int enabled) {
    test_vertices[2 * line].pos_curr.Y += dy / 1000.f;
    test_vertices[2 * line + 1].pos_curr.Y += dy / 1000.f;
    test_enabled[line] = enabled;
}
void TestLaserSetPreference(int index, int value) { preferences[index] = value; }
void TestLaserPause(int value) { paused = value ? 2 : 0; menu_data.mode = value ? MenuMode_Paused : 0; }
void TestLaserChoice(int row, int value) {
    options[row].val = value;
    if (options[row].OnChange) options[row].OnChange(0, value);
    else options[row].OnSelect(0);
}
void TestLaserStageSwap(int row) { StageSwap_Options[row].OnSelect(0); }
void TestLaserHit(int percent) {
    /* Native collision owns percent. The event must neither restore nor zero it. */
    ft[1].dmg.percent += percent;
    ft[1].phys.pos.X += 12; ft[1].phys.kb_vel.X = 9; ft[1].phys.nudge_vel.X = 3;
}
void TestLaserReenter(void) { text_count = 0; label_neutral_b = 0; Event_Init(&event); }
void TestLaserPrepare(void) { text_count = 0; text_rows[0] = text_rows[1] = 0; }
void TestLaserRestore(void) { ++serial; }
void TestLaserNativeInput(int down) {
    *stc_ftcommon = &common;
    common.x88 = .7f;
    *(int *)((u8 *)&common + 0x8C) = 4;
    ft[0].phys.self_vel.Y = -1;
    ft[0].input.lstick.Y = down ? -1 : 0;
    ft[0].input.timer_lstick_tilt_y = 0;
}
unsigned TestLaserObject(void) { return (unsigned)&event; }
unsigned TestLaserFighter(int player) { return (unsigned)&ft[player]; }
unsigned TestLaserText(int i) { return (unsigned)&texts[i]; }
int TestLaserInfo(int key) {
    switch (key) {
    case 0: return data.laser_frame;
    case 1: return data.fastfall_frame;
    case 2: return data.air_frame;
    case 3: return ft[1].dmg.percent;
    case 4: return ft[1].phys.pos.X;
    case 5: return ft[0].phys.pos.X;
    case 6: return ft[1].state_id;
    case 7: return ft[1].state.frame * 100;
    case 8: return ft[1].state.rate * 100;
    case 9: return ft[1].flags.no_reaction_always && ft[1].flags.nudge_disable && ft[1].grab.vuln == 0x1FF;
    case 10: return ft[1].cb.Anim == 0 && ft[1].cb.IASA == 0 && ft[1].cb.Phys == HoldPuff;
    case 11: return suppression && *suppression;
    case 12: return texts[0].hidden || texts[1].hidden;
    case 13: return text_colors[0][1];
    case 14: return text_colors[1][1];
    case 15: return writes;
    case 16: return percent_hud;
    case 17: return ended;
    case 18: return ft[1].phys.kb_vel.X == 0 && ft[1].phys.nudge_vel.X == 0;
    case 19: return text_result[0];
    case 20: return text_result[1];
    case 21: return (joints[1].trans.Y + model_joints[1].trans.Y) * 100;
    case 22: return ft[1].phys.pos.Y * 100;
    case 23: return ft[1].state.blend * 100;
    case 24: return !!(joints[1].flags & JOBJ_MTX_DIRTY);
    case 25: return !!(model_joints[1].flags & JOBJ_MTX_DIRTY);
    case 26: return ft[1].phys.pos.X * 1000;
    case 27: return ft[1].phys.pos.Y * 1000;
    case 28: return ft[0].phys.pos.X * 1000;
    case 29: return ft[0].phys.pos.Y * 1000;
    case 30: return ft[1].phys.air_state;
    case 31: return ft[1].cb.Coll == HoldPuffCollision;
    case 32: return options[OPT_POSITION].val;
    case 33: return options[OPT_HEIGHT].val;
    case 34: return preferences[TM_LOW_LASER_POSITION];
    case 35: return preferences[TM_LOW_LASER_HEIGHT];
    case 36: return label_neutral_b;
    case 37: return ft[1].coll_data.ground_index;
    case 38: return ft[0].facing_direction;
    case 39: return position_available[0] | position_available[1] << 1 | position_available[2] << 2 |
        position_available[3] << 3 | position_available[4] << 4 | position_available[5] << 5 |
        position_available[6] << 6 | position_available[7] << 7 | position_available[8] << 8 |
        position_available[9] << 9;
    case 40: return data.puff_height * 1000;
    case 41: return ft[1].state_id == ASID_FALL;
    case 42: return ft[1].state_id == ASID_SQUAT;
    case 43: return ft[0].phys.air_state;
    case 44: { int mask = 0; for (int i = 0; i < STAGE_SWAP_COUNT; ++i) mask |= StageSwap_Options[i].disable << i; return mask; }
    case 45: return StageSwap_Menu.cursor;
    case 46: return restarted;
    case 47: return options[OPT_STAGE].kind == OPTKIND_MENU && options[OPT_STAGE].menu == &StageSwap_Menu;
    case 48: { int mask = 0; for (int i = 0; i < STAGE_SWAP_COUNT; ++i) mask |= !!StageSwap_Options[i].value_string << i; return mask; }
    }
    return -999;
}
