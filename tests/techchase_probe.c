/* Production event metadata, percentages, positioning, and spotdodge adapter.
 * Native eligibility is modeled only at the Wait IASA boundary below. */
#define Fighter_IASACheck_AllGrounded TestTCNativeWaitIASA
#include "../src/techchase.c"
#undef Fighter_IASACheck_AllGrounded

static EventVars test_vars;
static unsigned saved_preferences[TM_TECHCHASE_PREF_COUNT];
static unsigned writes;
static unsigned last_field, last_index;
static unsigned TestTCGet(unsigned field, unsigned index) { return saved_preferences[index]; }
static void TestTCSet(unsigned field, unsigned index, unsigned value) {
    last_field = field; last_index = index; saved_preferences[index] = value; ++writes;
}
static const TMSettingsAPI test_api = {TestTCGet, TestTCSet, 0};
static CollDataStage test_stage;
static CollLine test_lines[180];
static CollLineDesc test_descriptors[180];
static CollVert test_vertices[360];
static int test_enabled[180];
static int test_stage_id;
static GOBJ test_cpu;
static FighterData test_cpu_data;
static Figatree test_animation;
static int native_calls;

int GrColl_CheckIfLineEnabled(int line) { return test_enabled[line]; }
int Stage_GetExternalID(void) { return test_stage_id; }
__attribute__((noinline)) int TestTCNativeWaitIASA(GOBJ *cpu) {
    FighterData *data = cpu->userdata;
    ++native_calls;
    if (data->state_id == ASID_WAIT && data->input.held & 0x80000000u &&
        data->input.lstick.Y <= -0.7f && data->input.timer_lstick_tilt_y < 4)
        data->state_id = ASID_ESCAPE;
    return 0;
}
__attribute__((noinline)) int TestTCNativeSpotdodgeEnter(GOBJ *cpu) {
    ((FighterData *)cpu->userdata)->state_id = ASID_ESCAPE;
    return 1;
}
int TestTCInit(void) {
    *event_vars_ptr_loc = &test_vars;
    test_vars.settings = &test_api;
    *stc_colldata = &test_stage; *stc_collline = test_lines; *stc_collvert = test_vertices;
    test_cpu.userdata = &test_cpu_data;
    test_cpu_data.figatree_curr = &test_animation;
    test_animation.frame_num = 24;
    return 1;
}
int TestTCPresetCount(void) { return Options_Chances[OPTCHANCE_PRESET].value_num; }
unsigned TestTCPresetName(int preset) { return (unsigned)Options_Preset[preset]; }
int TestTCChance(int i) { return Options_Chances[OPTCHANCE_TECHINPLACE + i].val; }
int TestTCSetPreset(int preset) {
    Options_Chances[OPTCHANCE_PRESET].val = preset;
    ChangePreset((GOBJ *)1, preset);
    return saved_preferences[TM_TECHCHASE_PRESET];
}
int TestTCReloadChances(void) {
    InitializeChances(saved_preferences[TM_TECHCHASE_PRESET]);
    return writes;
}
int TestTCSetChance(int i, int chance) {
    EventOption *option = &Options_Chances[OPTCHANCE_TECHINPLACE + i];
    option->val = chance;
    if (option->OnChange) option->OnChange((GOBJ *)1, chance);
    return writes;
}
int TestTCWriteCount(void) { return writes; }
int TestTCLastWriteField(void) { return last_field; }
int TestTCLastWriteIndex(void) { return last_index; }
int TestTCStage(int stage) {
    test_stage_id = stage;
    test_stage.line_num = test_stage.vert_num = 0;
    return stage;
}
int TestTCAddLine(int ax, int ay, int bx, int by, int native_flags, int enabled) {
    int i = test_stage.line_num++;
    test_stage.vert_num += 2;
    test_lines[i].desc = &test_descriptors[i];
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
int TestTCChooseLocation(int location) {
    automatic_position = true;
    Options_Main[OPT_LOCATION].val = location;
    ApplyStartingLocation();
    return Options_Main[OPT_LOCATION].val;
}
int TestTCLocationAvailable(int location) { return location_available[location]; }
int TestTCLocationDisabled(void) { return Options_Main[OPT_LOCATION].disable; }
int TestTCPosition(int actor, int component) {
    Vec3 position = actor ? cpu_pos : hmn_pos;
    float value = component ? position.Y : position.X;
    return (int)(value * 1000);
}
int TestTCFacing(int actor) { return actor ? cpu_facing_direction : hmn_facing_direction; }
int TestTCDodgeReset(void) { punish_phase = TC_PUNISH_START; native_calls = 0; return 1; }
int TestTCDodgeStep(int enabled, int category) {
    Options_Main[OPT_SPOTDODGE].val = enabled;
    test_cpu_data.state_id = category == TC_STATE_WAIT ? ASID_WAIT : category == TC_STATE_OPTION ? ASID_PASSIVESTANDF :
        category == TC_STATE_DODGE ? ASID_ESCAPE : ASID_DAMAGEFALL;
    test_cpu_data.cpu.held = 0; test_cpu_data.cpu.lstickY = 0;
    test_cpu_data.input.held = 0; test_cpu_data.input.lstick.Y = 0;
    test_cpu_data.input.timer_lstick_tilt_y = 100;
    return RunSpotdodgePunish(&test_cpu);
}
int TestTCDodgePhase(void) { return punish_phase; }
int TestTCDodgeCalls(void) { return native_calls; }
int TestTCDodgeState(void) { return test_cpu_data.state_id == ASID_ESCAPE; }
int TestTCDodgeInputs(void) {
    return test_cpu_data.cpu.held == PAD_TRIGGER_R && test_cpu_data.cpu.lstickY == -127 &&
        test_cpu_data.input.held == (PAD_TRIGGER_R | 0x80000000u) && test_cpu_data.input.lstick.Y == -1 &&
        test_cpu_data.input.timer_lstick_tilt_y == 0;
}
