/* Production Edgeguard launch and side-B logic, with fixtures only at native
 * engine boundaries. Python passes and reads floats as thousandths. */
#include "../src/edgeguard.c"
static EventVars vars;
static GOBJ gobjs[2];
static FighterData fighters[2];
static ftCommonData common;
static float attributes[48];
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
static u32 NextRand(void);
static u32 lcg;
float HSD_Randf(void) { return lcg ? (NextRand() & 0xFFFF) / 65536.f : rand_float; }
int HSD_Randi(int max) { return lcg ? NextRand() % max : rand_int % max; }
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
void TestRand(int value, int choice) { rand_float = Milli(value); rand_int = choice; lcg = 0; }
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

/* Closed-loop recovery: tests/recovery_sim.py runs a fighter's physics frame by
 * frame and this runs the production Recovery_Think on it. The stage is real
 * lines read back through the production stage readers. */
typedef struct SimIn {
    int state, frame, air, hitlag, jumps, flag2, cyclone_used, opt_flags, held;
    float x, y, vx, vy, kbx, kby, hitstun, facing, front, bottom;
} SimIn;
typedef struct SimOut { int held, stick_x, stick_y; } SimOut;
SimIn sim_in;
SimOut sim_out;
/* jump_v, air_v, air_h, gravity, terminal, drift_mult, drift_base, drift_max,
 * friction, fastfall, model_scale, then the fighter's snap x, y and height. */
float sim_attr[14];
/* Per Fox-style special attribute, from 0x24 on. */
float sim_special[32];
/* ax, ay, bx, by, then 1 floor, 2 ceiling, 4 right wall, 8 left wall. */
float sim_segs[64][5];
float sim_ledge[2][2];
float sim_blast_bottom;
static CollDataStage sim_stage;
static CollDataStage *sim_stage_ptr;
static CollLine sim_lines[64];
static CollLineDesc sim_descs[64];
static CollVert sim_verts[128];

int GrColl_CheckIfLineEnabled(int line) { return 1; }
float Stage_GetBlastzoneBottom(void) { return sim_blast_bottom; }
float Vec2_Normalize(Vec2 *v) {
    float length = sqrtf(v->X * v->X + v->Y * v->Y);
    if (length > 0.f) { v->X /= length; v->Y /= length; }
    return length;
}
void TestSeed(int seed) { lcg = seed ? seed : 1; }
static u32 NextRand(void) { lcg = lcg * 1103515245u + 12345u; return lcg >> 8; }
void TestSimConfigure(int kind, int segments) {
    FighterData *ft = &fighters[1];
    ft->kind = kind;
    ft->attr.jump_v_initial_velocity = sim_attr[0];
    ft->attr.air_jump_v_multiplier = sim_attr[1];
    ft->attr.air_jump_h_multiplier = sim_attr[2];
    ft->attr.gravity = sim_attr[3];
    ft->attr.terminal_velocity = sim_attr[4];
    ft->attr.aerial_drift_stick_mult = sim_attr[5];
    ft->attr.aerial_drift_base = sim_attr[6];
    ft->attr.aerial_drift_max = sim_attr[7];
    ft->attr.aerial_friction = sim_attr[8];
    ft->attr.fastfall_velocity = sim_attr[9];
    ft->attr.model_scaling = sim_attr[10];
    ft->coll_data.cliffgrab_width = sim_attr[11];
    ft->coll_data.cliffgrab_y_offset = sim_attr[12];
    ft->coll_data.cliffgrab_height = sim_attr[13];
    ft->attr.max_jumps = 2;
    for (int i = 0; i < 32; ++i)
        attributes[i + 0x24 / 4] = sim_special[i];
    for (int i = 0; i < segments; ++i) {
        float *s = sim_segs[i];
        CollLineDesc *line = &sim_descs[i];
        memset(line, 0, sizeof(*line));
        line->vert_prev = i * 2;
        line->vert_next = i * 2 + 1;
        ((u8 *)line)[0xD] = (int)s[4];
        sim_verts[i * 2].pos_curr = (Vec2) { s[0], s[1] };
        sim_verts[i * 2 + 1].pos_curr = (Vec2) { s[2], s[3] };
        sim_lines[i].desc = line;
    }
    sim_stage.line_num = segments;
    sim_stage.vert_num = segments * 2;
    *stc_colldata = &sim_stage;
    *stc_collline = sim_lines;
    *stc_collvert = sim_verts;
    ledge_positions[0] = (Vec2) { sim_ledge[0][0], sim_ledge[0][1] };
    ledge_positions[1] = (Vec2) { sim_ledge[1][0], sim_ledge[1][1] };
    fighters[0].phys.pos.X = 0.f; /* The player stands mid stage, out of it. */
    fighters[0].phys.air_state = 0;
    fighters[0].hurt.intang_frames.ledge = 0;
}
int TestSimThink(void) {
    FighterData *ft = &fighters[1];
    SimIn *in = &sim_in;
    ft->state_id = in->state;
    ft->TM.state_frame = in->frame;
    ft->phys.air_state = in->air;
    ft->flags.hitlag = in->hitlag;
    ft->jump.jumps_used = in->jumps;
    ft->ftcmd_var.flag2 = in->flag2;
    ft->fighter_var.ft_var1 = in->cyclone_used;
    ft->input.held = in->held;
    ft->phys.pos.X = in->x;
    ft->phys.pos.Y = in->y;
    ft->phys.self_vel.X = in->vx;
    ft->phys.self_vel.Y = in->vy;
    ft->phys.kb_vel.X = in->kbx;
    ft->phys.kb_vel.Y = in->kby;
    ft->facing_direction = in->facing;
    ft->coll_data.ecbCurr_right.X = in->front;
    ft->coll_data.ecbCurr_left.X = -in->front;
    ft->coll_data.ecbCurr_bot.Y = in->bottom;
    memcpy(&ft->state_var.state_var1, &in->hitstun, sizeof(float));
    Recovery_Think(&gobjs[1], in->opt_flags);
    sim_out.held = ft->cpu.held;
    sim_out.stick_x = ft->cpu.lstickX;
    sim_out.stick_y = ft->cpu.lstickY;
    return ft->fighter_var.ft_var1;
}
int TestState(int key) {
    static const int states[] = {
        ASID_FALL, ASID_DAMAGEFALL, ASID_JUMPAERIALF, ASID_JUMPAERIALB, ASID_FALLSPECIAL,
        ASID_DAMAGEFLYN, FTKIND_FOX, FTKIND_FALCO, FTKIND_LUIGI, PAD_BUTTON_B, PAD_BUTTON_Y,
    };
    return states[key];
}
int TestPlan(void) { return fighters[1].kind == FTKIND_LUIGI ? lg_plan.kind : sp_plan.kind; }
int TestPlanValue(int key) {
    float value = key == 0 ? lg_plan.lo : key == 1 ? lg_plan.hi : key == 2 ? lg_plan.out_lo : lg_plan.out_hi;
    return value * 1000.f;
}
/* Row bits: what Edgeguard loads from what it saved, and back. */
int TestLoadRows(int menu) {
    EventMenu *menus[] = { &RecMenu_Fox, &RecMenu_Falco, &RecMenu_Luigi };
    return LoadRecoveryRows(menus[menu], RecoveryPref(menus[menu]));
}
void TestSaveRows(int menu, int rows) {
    EventMenu *menus[] = { &RecMenu_Fox, &RecMenu_Falco, &RecMenu_Luigi };
    SaveRecoveryRows(menus[menu], RecoveryPref(menus[menu]), rows);
}
/* The side-B simulation at a dash height, on the configured stage. */
int TestSideBResult(int out, int h, int kby) {
    SpacieGeo g = TestGeo(out, h, 0, 0, kby);
    g.ledge = ledge_positions[1];
    g.dir = -1.f;
    int frame;
    float dash_h;
    int result = SimulateSideB(&fighters[1], &g, false, &frame, &dash_h);
    return result * 100 + frame;
}
int TestWallInset(int depth) {
    SpacieGeo g = { .ledge = ledge_positions[1], .dir = -1.f };
    return SpacieWallInset(&g, Milli(depth)) * 1000.f;
}
int TestFirefoxTravel(void) { return SpacieFirefoxTravel(&fighters[1]) * 1000.f; }
void TestRefreshCyclone(void) {
    fighters[1].fighter_var.ft_var1 = 0x01000000;
    Recovery_RefreshCyclone(&fighters[1]);
}
int TestCycloneFlag(void) { return fighters[1].fighter_var.ft_var1; }
/* The real Reset_CPU, with the stale flag a match can start with. */
void Fighter_EnterFall(GOBJ *f) {}
void ActionStateChange(float a, float b, float c, GOBJ *f, int s, int fl, GOBJ *o) {}
void Fighter_UpdateStateFrameInfo(GOBJ *f) {}
void Fighter_UpdateCameraBox(GOBJ *f) {}
void Match_CorrectCamera(void) {}
void Fighter_EnableCollUpdate(FighterData *f) {}
void Fighter_SetHUDDamage(int ply, u16 damage) {}
static CmSubject camera_subject;
int TestResetCpuClears(int stale) {
    FighterData *ft = &fighters[1];
    ft->camera_subject = &camera_subject;
    ft->fighter_var.ft_var1 = stale;
    Reset_CPU(&gobjs[1], 1, 60, 100.f, 45.f, 15.f);
    return ft->fighter_var.ft_var1;
}
