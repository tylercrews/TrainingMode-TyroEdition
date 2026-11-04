/* Production Edgeguard launch and side-B logic, with fixtures only at native
 * engine boundaries. Python passes and reads floats as thousandths. */
#include "../src/edgeguard.c"
static EventVars vars;
static GOBJ gobjs[2];
static FighterData fighters[2];
static ftCommonData common;
static float attributes[32];
static u32 preferences[TM_EG_PREF_COUNT];
static float rand_float;
static int rand_int;
static float launch_mag, launch_angle, launch_distance;
static int launch_percent;
void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
static u32 Read(unsigned field, unsigned index) { return preferences[index]; }
static void Write(unsigned field, unsigned index, u32 value) { preferences[index] = value; }
static const TMSettingsAPI api = {Read, Write, 0};
float HSD_Randf(void) { return rand_float; }
int HSD_Randi(int max) { return rand_int % max; }
GOBJ *Fighter_GetGObj(int ply) { return &gobjs[ply]; }
float sqrtf(float v) {
    float x = v > 1.f ? v : 1.f;
    if (v <= 0.f) return 0.f;
    for (int i = 0; i < 24; ++i) x = .5f * (x + v / x);
    return x;
}
float sin(float a) {
    float a2 = a * a;
    return a * (1.f - a2 / 6.f * (1.f - a2 / 20.f * (1.f - a2 / 42.f * (1.f - a2 / 72.f))));
}
float cos(float a) {
    float a2 = a * a;
    return 1.f - a2 / 2.f * (1.f - a2 / 12.f * (1.f - a2 / 30.f * (1.f - a2 / 56.f)));
}
float atan2(float y, float x) {
    float t = y / x;
    for (int i = 0; i < 2; ++i) t = t / (1.f + sqrtf(1.f + t * t));
    float t2 = t * t;
    return 4.f * t * (1.f - t2 * (1.f / 3.f - t2 * (.2f - t2 / 7.f)));
}
static float Milli(int value) { return value / 1000.f; }
void TestEdgeguardInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.settings = &api;
    *stc_ftcommon = &common;
    common.force_applied_to_kb_mag_multiplier = .03f;
    common.x154 = .4f;
    common.kb_frameDecay = .051f;
    gobjs[0].userdata = &fighters[0];
    gobjs[1].userdata = &fighters[1];
    fighters[0].phys.air_state = 1;
    FighterData *ft = &fighters[1];
    ft->kind = FTKIND_FOX;
    ft->special_attributes = attributes;
    attributes[0x24 / 4] = 15.f;
    attributes[0x28 / 4] = 1.5f;
    attributes[0x2C / 4] = .05f;
    attributes[0x30 / 4] = 1.f / 60.f;
    attributes[0x3C / 4] = 2.f;
    attributes[0x40 / 4] = .07f;
    attributes[0x44 / 4] = 5.f;
    attributes[0x48 / 4] = .08f;
    ft->attr.model_scaling = .96f;
    ft->scale.Y = 1.f;
    ft->attr.jump_v_initial_velocity = 3.68f;
    ft->attr.air_jump_v_multiplier = 1.2f;
    ft->attr.max_jumps = 2;
    ft->attr.gravity = .23f;
    ft->attr.terminal_velocity = 2.8f;
    ft->attr.aerial_drift_stick_mult = .06f;
    ft->attr.aerial_drift_base = .02f;
    ft->attr.aerial_drift_max = .83f;
    ft->attr.aerial_friction = .02f;
    ft->coll_data.cliffgrab_width = 11.f;
    ft->coll_data.cliffgrab_y_offset = 13.f;
    ft->coll_data.cliffgrab_height = 9.f;
    ft->coll_data.ecbCurr_bot.Y = 2.f;
    ft->jump.jumps_used = 2;
}
void TestCPU(int gravity, int terminal) {
    fighters[1].attr.gravity = Milli(gravity);
    fighters[1].attr.terminal_velocity = Milli(terminal);
}
void TestLedgeHeight(int y) { ledge_positions[0].Y = ledge_positions[1].Y = Milli(y); }
void TestRand(int value, int choice) { rand_float = Milli(value); rand_int = choice; }
void TestRows(int height, int distance) {
    Options_Main[OPT_MAIN_KB_HEIGHT].val = height;
    Options_Main[OPT_MAIN_KB_DISTANCE].val = distance;
}
void TestCustom(int row, int value) { Options_CustomKnockback[row].val = value; }
void TestLaunch(int side_idx, int inland) {
    launch_distance = inland;
    launch_percent = ChooseLaunch(&fighters[1], side_idx, &launch_distance, &launch_mag, &launch_angle);
}
int TestLaunchInfo(int key) {
    switch (key) {
        case 0: return launch_mag * 1000.f;
        case 1: return launch_angle * 1000.f;
        case 2: return launch_distance * 1000.f;
    }
    return launch_percent * 1000;
}
void TestPreference(int index, int value) { preferences[index] = value; }
int TestReadPreference(int index) { return preferences[index]; }
void TestLoadKnockback(void) { LoadKnockback(); }
int TestRow(int key) {
    switch (key) {
        case 0: return Options_Main[OPT_MAIN_KB_HEIGHT].val;
        case 1: return Options_Main[OPT_MAIN_KB_DISTANCE].val;
    }
    return Options_Main[OPT_MAIN_KB_DISTANCE].disable;
}
void TestChangeHeight(int value) {
    Options_Main[OPT_MAIN_KB_HEIGHT].val = value;
    ChangeKnockbackHeight(0, value);
}
void TestChangeCustom(void) { ChangeCustomKB_MagMin(0, 0); }

/* One actionable frame of a plan, 40 out unless told otherwise. */
static SpacieGeo TestGeo(int out, int h, int vy, int kbx, int kby) {
    return (SpacieGeo) {
        .dir = 1.f, .out = Milli(out), .h = Milli(h), .vy = Milli(vy), .kbx = Milli(kbx),
        .kby = Milli(kby), .reach = 14.3f,
    };
}
int TestStageThink(int out, int h, int vy, int kbx, int kby) {
    SpacieGeo g = TestGeo(out, h, vy, kbx, kby);
    sp_plan = SpacieMakePlan(&fighters[1], &g, SP_PLAN_TO_STAGE);
    sp_plan.aim = g.out;
    fighters[1].cpu.held = 0;
    SpacieSideBThink(&fighters[1], 0, &g);
    return (fighters[1].cpu.held & PAD_BUTTON_B) != 0;
}
/* Far out above the ledge with a double jump left, or `hmn_grounded` at the ledge. */
int TestStagePlan(int key, int out, int h, int hmn_grounded) {
    SpacieGeo g = TestGeo(out, h, 0, 0, 0);
    fighters[1].jump.jumps_used = 1;
    fighters[0].phys.air_state = !hmn_grounded;
    SpaciePlan p = SpacieMakePlan(&fighters[1], &g, SP_PLAN_TO_STAGE);
    fighters[1].jump.jumps_used = 2;
    fighters[0].phys.air_state = 1;
    float value = key == 0 ? p.lo : key == 1 ? p.hi : p.aim;
    return value * 1000.f;
}
int TestDashHeight(int out, int h, int kbx, int kby) {
    SpacieGeo g = TestGeo(out, h, 0, kbx, kby);
    int frame;
    float dash_h;
    int result = SimulateSideB(&fighters[1], &g, false, &frame, &dash_h);
    return result == SIDEB_LAND_STAGE ? dash_h * 1000.f : -999999;
}
int TestCarryUp(int kbx, int kby) {
    SpacieGeo g = TestGeo(0, 0, 0, kbx, kby);
    return SpacieCarryUp(&g) * 1000.f;
}
