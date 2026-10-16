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
static unsigned preferences[2] = {1, 0}, serial;
static int paused, text_count, text_rows[2], text_result[2], writes;
static u32 text_colors[2][2];
static const s16 *suppression;
static int percent_hud, ended;
static ftCommonData common;
void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
static u32 Read(unsigned field, unsigned index) { return preferences[index]; }
static void Write(unsigned field, unsigned index, u32 value) { preferences[index] = value; ++writes; }
static const TMSettingsAPI api = {Read, Write, 0};
static unsigned Serial(void) { return serial; }
static void Suppress(const s16 *enabled) { suppression = enabled; }
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
    text_result[text - texts] = fmt[0] == '-' ? -1 : va_arg(args, int);
    va_end(args);
}
int TestLaserInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event; vars.menu_gobj = &menu_object;
    vars.settings = &api; vars.get_restore_serial = Serial; vars.set_osd_suppression = Suppress;
    event.userdata = &data; menu_object.userdata = &menu_data;
    for (int i = 0; i < 2; ++i) {
        fighters[i].userdata = &ft[i]; fighters[i].hsd_object = &joints[i]; ft[i].ply = i;
        joints[i].child = &model_joints[i];
    }
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
void TestLaserPause(int value) { paused = value ? 2 : 0; menu_data.mode = value ? MenuMode_Paused : 0; }
void TestLaserChoice(int row, int value) {
    options[row].val = value;
    if (options[row].OnChange) options[row].OnChange(0, value);
    else options[row].OnSelect(0);
}
void TestLaserHit(int percent) {
    /* Native collision owns percent. The event must neither restore nor zero it. */
    ft[1].dmg.percent += percent;
    ft[1].phys.pos.X += 12; ft[1].phys.kb_vel.X = 9; ft[1].phys.nudge_vel.X = 3;
}
void TestLaserReenter(void) { text_count = 0; Event_Init(&event); }
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
    }
    return -999;
}
