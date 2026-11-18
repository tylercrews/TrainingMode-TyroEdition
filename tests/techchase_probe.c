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
static GOBJ test_cpu, test_hmn;
static FighterData test_cpu_data, test_hmn_data;
static Figatree test_animation;
static int native_calls;
static float test_roll;
static int test_coin, randi_calls, randf_calls, randi_max;

int GrColl_CheckIfLineEnabled(int line) { return test_enabled[line]; }
int HSD_Randi(int max) { ++randi_calls; randi_max = max; return test_coin % max; }
float HSD_Randf(void) { ++randf_calls; return test_roll; }
int Stage_GetExternalID(void) { return test_stage_id; }
GOBJ *Fighter_GetGObj(int player) { return player ? &test_cpu : &test_hmn; }
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
    test_hmn.userdata = &test_hmn_data;
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
int TestTCPreset(void) { return Options_Chances[OPTCHANCE_PRESET].val; }
int TestTCLoadSaved(int saved) {
    InitializeChances(saved);
    return Options_Chances[OPTCHANCE_PRESET].val;
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
int TestTCChangePlacement(int height, int distance) {
    Options_Main[OPT_HEIGHT].val = height;
    Options_Main[OPT_HEIGHT].OnChange((GOBJ *)1, height);
    Options_Main[OPT_DISTANCE].val = distance;
    Options_Main[OPT_DISTANCE].OnChange((GOBJ *)1, distance);
    ApplyStartingLocation();
    return writes;
}
int TestTCChangeSide(int side) {
    Options_Main[OPT_SIDE].val = side;
    Options_Main[OPT_SIDE].OnChange((GOBJ *)1, side);
    ApplyStartingLocation();
    return writes;
}
// Choose the side without the menu callback or a save, as a loaded setting.
int TestTCSetSide(int side) { Options_Main[OPT_SIDE].val = side; return side; }
int TestTCReloadPlacement(void) {
    Options_Main[OPT_HEIGHT].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_HEIGHT);
    Options_Main[OPT_DISTANCE].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_DISTANCE);
    Options_Main[OPT_SIDE].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_SIDE);
    ApplyStartingLocation();
    return automatic_position;
}
// The next HSD_Randf (roll / 1000000) and HSD_Randi (coin % max) results.
int TestTCRandom(int roll, int coin) {
    test_roll = roll / 1000000.f; test_coin = coin; randi_calls = randf_calls = 0;
    return 1;
}
int TestTCRandomCalls(int floats) { return floats ? randf_calls : randi_calls; }
int TestTCApply(void) { ApplyStartingLocation(); return Options_Main[OPT_LOCATION].val; }
int TestTCDPad(int down) { ReadDPad(down); return Options_Main[OPT_SIDE].val; }
int TestTCSideRow(void) { return Options_Main[OPT_SIDE].val | Options_Main[OPT_SIDE].val_prev << 8; }
int TestTCAutomatic(int set) { if (set >= 0) automatic_position = set; return automatic_position; }
int TestTCRowCount(int distance) { return Options_Main[distance ? OPT_DISTANCE : OPT_SIDE].value_num; }
unsigned TestTCRowValue(int distance, int i) { return (unsigned)(distance ? Options_Distance : Options_Side)[i]; }
unsigned TestTCRowName(int distance) { return (unsigned)Options_Main[distance ? OPT_DISTANCE : OPT_SIDE].name; }
int TestTCRowIndex(int distance) { return distance ? OPT_DISTANCE : OPT_SIDE; }
int TestTCPlacementSaved(int index) { return saved_preferences[index]; }
unsigned TestTCResetAddress(void) { return (unsigned)Reset; }
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
    attempt_followup = enabled ? TC_FOLLOWUP_SPOTDODGE : TC_FOLLOWUP_NONE;
    test_cpu_data.state_id = category == TC_STATE_WAIT ? ASID_WAIT : category == TC_STATE_OPTION ? ASID_PASSIVESTANDF :
        category == TC_STATE_DODGE ? ASID_ESCAPE : ASID_DAMAGEFALL;
    test_cpu_data.cpu.held = 0; test_cpu_data.cpu.lstickY = 0;
    test_cpu_data.input.held = 0; test_cpu_data.input.lstick.Y = 0;
    test_cpu_data.input.timer_lstick_tilt_y = 100;
    return RunFollowup(&test_cpu);
}
int TestTCDodgePhase(void) { return punish_phase; }
int TestTCDodgeCalls(void) { return native_calls; }
int TestTCDodgeState(void) { return test_cpu_data.state_id == ASID_ESCAPE; }
int TestTCDodgeInputs(void) {
    return test_cpu_data.cpu.held == PAD_TRIGGER_R && test_cpu_data.cpu.lstickY == -127 &&
        test_cpu_data.input.held == (PAD_TRIGGER_R | 0x80000000u) && test_cpu_data.input.lstick.Y == -1 &&
        test_cpu_data.input.timer_lstick_tilt_y == 0;
}
int TestTCNativeColorsUnchanged(void) {
    test_cpu_data.state_id = ASID_WAIT;
    Options_Main[OPT_INVIS].val = OPTINVIS_NONE;
    Options_Main[OPT_CPU_COLOR].val = TM_CPU_INVINCIBILITY_OFF;
    u8 *bytes = (void *)&test_cpu_data.color[0];
    for (unsigned i = 0; i < 2 * sizeof(ColorOverlay); ++i) bytes[i] = i ^ 0xA5;
    Event_PostThink(0);
    for (unsigned i = 0; i < 2 * sizeof(ColorOverlay); ++i) if (bytes[i] != (u8)(i ^ 0xA5)) return 0;
    return 1;
}
int TestTCMenuCPUColorCount(void) { return Options_Main[OPT_CPU_COLOR].value_num; }
unsigned TestTCChancesTitle(void) { return (unsigned)Menu_Chances.name; }
int TestTCSetCPUColor(int value) {
    Options_Main[OPT_CPU_COLOR].val = value;
    ChangeCPUColor((GOBJ *)1, value);
    return saved_preferences[TM_TECHCHASE_CPU_COLOR];
}

/* CPU Followup Action. Each native state entry the tests reach is redirected
 * here: it records the entry and sets the state, as the real one would. */
static int enter_calls;
static Vec2 airdodge_stick;
static void Entered(GOBJ *cpu, int state) { ((FighterData *)cpu->userdata)->state_id = state; ++enter_calls; }
__attribute__((noinline)) void TestTCEnterSpotdodge(GOBJ *cpu) { Entered(cpu, ASID_ESCAPE); }
// GuardOn's script turns the shield bubble on; its IASA drops a shield without it.
__attribute__((noinline)) void TestTCEnterGuardOn(GOBJ *cpu) {
    Entered(cpu, ASID_GUARDON);
    ((FighterData *)cpu->userdata)->flags.shield_enable = 1;
}
__attribute__((noinline)) void TestTCEnterGuardReflect(GOBJ *cpu) { Entered(cpu, ASID_GUARDREFLECT); }
__attribute__((noinline)) void TestTCEnterRoll(GOBJ *cpu, int state) { Entered(cpu, state); }
__attribute__((noinline)) void TestTCEnterShine(GOBJ *cpu) { Entered(cpu, TC_SHINE_START); }
__attribute__((noinline)) void TestTCEnterKneebend(GOBJ *cpu) { Entered(cpu, ASID_KNEEBEND); }
__attribute__((noinline)) void TestTCEnterAirdodge(GOBJ *cpu) {
    Entered(cpu, ASID_ESCAPEAIR);
    airdodge_stick = ((FighterData *)cpu->userdata)->input.lstick;
}

int TestTCSetCPUKind(int kind) { test_cpu_data.kind = kind; RefreshFollowupChoices(); return can_shine; }
unsigned TestTCFollowupLabel(int i) { return (unsigned)Options_FollowupDisplay[i]; }
unsigned TestTCFollowupRowName(void) { return (unsigned)Options_Main[OPT_FOLLOWUP].name; }
int TestTCFollowupRowCount(void) { return Options_Main[OPT_FOLLOWUP].value_num; }
// A step or pick on the menu row: the menu sets val_prev to the old value first.
int TestTCChangeFollowup(int value) {
    EventOption *row = &Options_Main[OPT_FOLLOWUP];
    row->val_prev = row->val;
    row->val = value;
    row->OnChange((GOBJ *)1, value);
    return row->val;
}
int TestTCSetFollowupRow(int value) { Options_Main[OPT_FOLLOWUP].val = value; return value; }
int TestTCFollowupSaved(void) { return saved_preferences[TM_TECHCHASE_FOLLOWUP]; }
int TestTCLoadFollowup(int saved) {
    saved_preferences[TM_TECHCHASE_FOLLOWUP] = saved;
    LoadFollowup();
    return Options_Main[OPT_FOLLOWUP].val;
}
// The next HSD_Randi is coin % max; returns the attempt's followup.
int TestTCRollFollowup(int coin) {
    test_coin = coin; randi_calls = 0; randi_max = 0;
    RollFollowup();
    return attempt_followup;
}
int TestTCRandiMax(void) { return randi_calls ? randi_max : 0; }

/* The input proc, simplified: the raw CPU input the last tick queued becomes
 * this frame's processed input. A button is down on the frame it's first held;
 * an axis's tilt timer is 0 on a fresh crossing of 0.25, counts up while it
 * stays past, and reads 254 inside. */
static void TestTilt(char *timer, float before, float now) {
    if (fabs(now) < .25f) *timer = 254;
    else if (fabs(before) < .25f || before * now < 0) *timer = 0;
    else if ((u8)*timer < 254) ++*timer;
}
static void TestDecode(FighterData *d) {
    unsigned held = d->cpu.held;
    if (held & (PAD_TRIGGER_L | PAD_TRIGGER_R)) held |= 0x80000000u;
    d->input.down = held & ~d->input.held;
    d->input.held = held;
    Vec2 before = d->input.lstick;
    d->input.lstick = (Vec2){d->cpu.lstickX / 127.f, d->cpu.lstickY / 127.f};
    TestTilt(&d->input.timer_lstick_tilt_x, before.X, d->input.lstick.X);
    TestTilt(&d->input.timer_lstick_tilt_y, before.Y, d->input.lstick.Y);
    d->input.trigger = held & 0x80000000u ? 1 : 0;
    if (d->input.down & 0x80000000u) d->input.timer_trigger_analog = d->input.timer_trigger_digital = 0;
    else if ((u8)d->input.timer_trigger_digital < 254) ++d->input.timer_trigger_analog, ++d->input.timer_trigger_digital;
    d->input.timer_specialhi = d->input.timer_speciallw = d->input.timer_specials = d->input.timer_specialn = 255;
}
// A fresh attempt at `followup`, the CPU at x on line `ground` (-1 none), facing right.
int TestTCFollowupBegin(int followup, int cpu_x, int hmn_x, int ground) {
    attempt_followup = followup;
    punish_phase = TC_PUNISH_START;
    native_calls = enter_calls = 0;
    waveland_direction = 0;
    followup_length = 0;
    player_direction = hmn_x > cpu_x ? 1 : -1;
    FighterData *d = &test_cpu_data;
    d->phys.pos.X = cpu_x / 1000.f;
    test_hmn_data.phys.pos.X = hmn_x / 1000.f;
    d->coll_data.ground_index = ground;
    d->facing_direction = 1;
    d->shield.health = 60;
    d->flags.shield_enable = 0;
    d->cpu.held = 0; d->cpu.lstickX = d->cpu.lstickY = 0;
    d->input.held = d->input.down = 0;
    d->input.lstick = (Vec2){0, 0};
    d->input.timer_lstick_tilt_x = d->input.timer_lstick_tilt_y = 254;
    d->input.timer_trigger_analog = d->input.timer_trigger_digital = 254;
    return 1;
}
/* One frame: the input proc, then the event's tick, which zeroes the raw
 * input before the followup. `state` < 0 keeps the state an entry left;
 * `iasa` is the state's IASA (0 for none). Returns RunFollowup's action. */
int TestTCFollowupTick(int state, unsigned iasa, int airborne) {
    FighterData *d = &test_cpu_data;
    TestDecode(d);
    if (state >= 0) d->state_id = state;
    d->cb.IASA = (void *)iasa;
    d->phys.air_state = airborne;
    d->cpu.held = 0;
    d->cpu.lstickX = d->cpu.lstickY = d->cpu.cstickX = d->cpu.cstickY = 0;
    return RunFollowup(&test_cpu);
}
int TestTCFollowupState(void) { return test_cpu_data.state_id; }
int TestTCFollowupPhase(void) { return punish_phase; }
int TestTCEnterCalls(void) { return enter_calls; }
int TestTCProtected(void) { return ProtectCPU(&test_cpu_data); }
int TestTCFollowupLength(void) { return followup_length; }
int TestTCWaveland(void) { return waveland_direction; }
// The air dodge's stick, in CPU stick units (127 full).
int TestTCAirdodgeStick(int y) {
    float v = (y ? airdodge_stick.Y : airdodge_stick.X) * 127;
    return (int)(v < 0 ? v - .5f : v + .5f);
}
// Raw (next frame's) input: held, then stick X and Y in the low bytes.
unsigned TestTCRaw(void) {
    FighterData *d = &test_cpu_data;
    return (unsigned)d->cpu.held << 16 | (u8)d->cpu.lstickX << 8 | (u8)d->cpu.lstickY;
}
unsigned TestTCHeld(int down) { return down ? test_cpu_data.input.down : test_cpu_data.input.held; }
int TestTCStick(int y) { return (int)((y ? test_cpu_data.input.lstick.Y : test_cpu_data.input.lstick.X) * 1000); }
int TestTCTimer(int which) {
    FighterData *d = &test_cpu_data;
    char timers[] = {d->input.timer_lstick_tilt_x, d->input.timer_lstick_tilt_y, d->input.timer_speciallw,
                     d->input.timer_specials, d->input.timer_specialn, d->input.timer_specialhi};
    return (u8)timers[which];
}
int TestTCAnimation(int frames, int rate_thousandths, int frame) {
    test_animation.frame_num = frames;
    test_cpu_data.state.rate = rate_thousandths / 1000.f;
    test_cpu_data.state.frame = frame;
    return frames;
}
// Any other special: a followup must never reach one.
__attribute__((noinline)) void TestTCEnterWrongSpecial(GOBJ *cpu) { Entered(cpu, 9999); }
int TestTCASID(int which) {
    static const int ids[] = {ASID_WAIT, ASID_PASSIVESTANDF, ASID_GUARDON, ASID_GUARDREFLECT, ASID_ESCAPEF,
        ASID_ESCAPEB, ASID_KNEEBEND, ASID_JUMPF, ASID_ESCAPEAIR, ASID_LANDINGFALLSPECIAL, ASID_ESCAPE,
        ASID_DAMAGEFALL, ASID_GUARD, ASID_JUMPB, ASID_DAMAGEHI1};
    return ids[which];
}

/* Practice clock: the time base starts at `tick`, with the bus clock at
 * `bus_clock` (the time base runs at a quarter of it). */
int TestTCClockStart(int tick, int bus_clock) {
    os_info->bus_clock = bus_clock;
    clock_seconds = clock_ticks = 0;
    clock_tick = tick;
    return 1;
}
// One frame at time base tick `now`; returns the whole seconds counted.
int TestTCClockFrame(int now, int paused) { AdvanceClock(now, paused); return clock_seconds; }
