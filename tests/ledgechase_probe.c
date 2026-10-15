/* Execute production event logic. Native physics/animation endpoints are tiny
 * fixtures; Save/Load deliberately restore all 512 event bytes, as Melee does. */
#include "../src/ledgechase.c"

static EventVars test_vars;
static GOBJ test_event, test_fighters[4];
static FighterData test_ft[4], saved_ft[4];
static JOBJ test_joints[4];
static union { LedgechaseData data; unsigned char bytes[EVENT_DATASIZE]; } event_storage;
static unsigned char saved_event[EVENT_DATASIZE];
static Savestate_v1 test_savestate;
static ftCommonData test_common;
// Existing action tests use direct ledge placement and the original hop height.
// New entry/height tests explicitly select the production defaults.
static unsigned preferences[TM_LEDGECHASE_PREF_COUNT] = {0, 0, 0, 0, 1, 7, 0, 0, 0, 2};
static int writes, saves, loads, native_calls, fastfalls, follower_enabled, paused, sounds;
static int hud_percent[2], random_roll;
static int forced_roll = -1;
static const s16 *palette;

void memset(void *dest, int value, int size) {
    u8 *bytes = dest;
    while (size-- > 0) *bytes++ = value;
}
void memcpy(void *dest, const void *source, int size) {
    u8 *out = dest; const u8 *in = source;
    while (size-- > 0) *out++ = *in++;
}
static unsigned TestLCGet(unsigned field, unsigned index) { return preferences[index]; }
static void TestLCSet(unsigned field, unsigned index, unsigned value) { preferences[index] = value; ++writes; }
static const TMSettingsAPI test_api = {TestLCGet, TestLCSet, 0};
static void TestLCSetPalette(const s16 *value) { palette = value; }
static int TestLCProtected(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    return ft->hurt.intang_frames.ledge > 0 || ft->hurt.kind_game || ft->hurt.kind_script;
}
static int TestLCSave(Savestate_v1 *state, int flags) {
    memcpy(saved_event, event_storage.bytes, sizeof(saved_event));
    memcpy(saved_ft, test_ft, sizeof(saved_ft));
    ++saves;
    return 1;
}
static int TestLCLoad(Savestate_v1 *state, int flags) {
    memcpy(event_storage.bytes, saved_event, sizeof(saved_event));
    memcpy(test_ft, saved_ft, sizeof(test_ft));
    ++loads;
    return 1;
}
static int TestLCIASA(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    int slow = ft->dmg.percent >= 100;
    ++native_calls;
    if (ft->state_id == ASID_CLIFFWAIT) {
        if (ft->input.down & PAD_BUTTON_A) ft->state_id = slow ? ASID_CLIFFATTACKSLOW : ASID_CLIFFATTACKQUICK;
        else if ((unsigned)ft->input.down & 0x80000000u) ft->state_id = slow ? ASID_CLIFFESCAPESLOW : ASID_CLIFFESCAPEQUICK;
        else if (ft->input.down & PAD_BUTTON_X) ft->state_id = slow ? ASID_CLIFFJUMPSLOW1 : ASID_CLIFFJUMPQUICK1;
        else if (ft->input.lstick.X * ft->facing_direction > 0.5f && ((FtCliffCatch *)&ft->state_var)->timer)
            ft->state_id = slow ? ASID_CLIFFCLIMBSLOW : ASID_CLIFFCLIMBQUICK;
        else if (ft->input.lstick.X * ft->facing_direction < -0.5f && ((FtCliffCatch *)&ft->state_var)->timer)
            ft->state_id = ASID_FALL;
        else if (ft->input.lstick.X == 0 && ft->input.lstick.Y == 0) ((FtCliffCatch *)&ft->state_var)->timer = 1;
        return 1;
    }
    if (ft->phys.air_state) {
        if (ft->state_id == ASID_FALL && (ft->input.down & PAD_BUTTON_X) && CheckHasJump(fighter)) {
            ++ft->jump.jumps_used;
            ft->flags.is_fastfall = 0; // Native ChangeMotionState clears this on aerial jump.
            ft->state_id = ft->flags.is_multijump ? 341 : ASID_JUMPAERIALF;
        } else if (ft->input.down & PAD_TRIGGER_R) ft->state_id = ASID_ESCAPEAIR;
        else if (AirReady(ft) && (ft->input.down & PAD_BUTTON_B)) ft->state_id = LC_BLASTER_START;
        else if (AirReady(ft) && (ft->input.down & PAD_BUTTON_A || ft->input.cstick.X || ft->input.cstick.Y))
            ft->state_id = ASID_ATTACKAIRN;
        return 1;
    }
    if (ft->state_id == ASID_LANDING && ft->state.frame < ft->attr.normal_landing_lag) return 0;
    if (ft->input.held & 0x80000000u) {
        if (ft->input.lstick.Y < -0.7f) ft->state_id = ASID_ESCAPE;
        else if (ft->state_id == ASID_GUARDON || ft->state_id == ASID_GUARD || ft->state_id == ASID_GUARDREFLECT) ft->state_id = ASID_ESCAPEF;
        else ft->state_id = ASID_GUARDON;
    } else if (fabs(ft->input.cstick.X) > 0.7f || (ft->input.down & PAD_BUTTON_A && fabs(ft->input.lstick.X) > 0.7f)) {
        ft->state_id = ft->kind == FTKIND_NESS ? 341 : ft->kind == FTKIND_GAW ? 346 :
            ft->kind == FTKIND_PEACH ? 349 : ASID_ATTACKS4S;
    } else if (fabs(ft->input.lstick.X) > 0.7f) ft->state_id = ASID_DASH;
    return 1;
}
GOBJ *Fighter_GetGObj(int player) { return &test_fighters[player * 2]; }
GOBJ *Fighter_GetSubcharGObj(int player, int ms) { return ms && !follower_enabled ? 0 : &test_fighters[player * 2 + ms]; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Fighter_SetPosition(int player, int ms, Vec3 *position) { }
void Fighter_SetAllHurtboxState(GOBJ *fighter, int state) {
    FighterData *ft = fighter->userdata;
    for (int i = 0; i < 15; ++i) ft->hurtbox[i].state = state;
}
void Fighter_SetHUDDamage(int player, u16 percent) { hud_percent[player] = percent; }
void Fighter_SetGrounded(FighterData *ft) { ft->phys.air_state = 0; }
void Fighter_SetAirborne(FighterData *ft) { ft->phys.air_state = 1; ft->jump.jumps_used = 1; }
void Fighter_UpdateCameraBox(GOBJ *fighter) { }
void EnvironmentCollision_WaitLanding(GOBJ *fighter) { }
void Fighter_EnterWait(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->state_id = ASID_WAIT;
    ft->cb.IASA = TestLCIASA;
}
void Fighter_EnterFall(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->state_id = ASID_FALL;
    ft->phys.air_state = 1;
    ft->cb.IASA = TestLCIASA;
}
void Fighter_EnterSleep(GOBJ *fighter, int ms) { ((FighterData *)fighter->userdata)->flags.sleep = 1; }
void Fighter_EnterRebirth(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->flags.dead = ft->flags.sleep = 0;
}
void Fighter_EnterCliffWait(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->state_id = ASID_CLIFFWAIT;
    ft->cb.IASA = TestLCIASA;
}
void Fighter_EnableCollUpdate(FighterData *ft) { }
int Coll_CheckLedge(CollData *coll) { return 1; }
void Fighter_MoveToCliff(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->phys.pos = (Vec3){ft->facing_direction > 0 ? -88.5657f : 88.5657f, -7, 0};
}
void Fighter_ApplyIntang(GOBJ *fighter, int frames) {
    FighterData *ft = fighter->userdata;
    ft->hurt.intang_frames.ledge = frames;
    ft->hurt.kind_game = 2;
}
void Stage_GetLeftOfLineCoordinates(int line, Vec3 *position) { *position = (Vec3){-85.5657f, 0, 0}; }
void Stage_GetRightOfLineCoordinates(int line, Vec3 *position) { *position = (Vec3){85.5657f, 0, 0}; }
int GrColl_RaycastGround(Vec3 *position, int *line, int *kind, Vec3 *slope, int a, int b, int c, void *cb,
                         float x, float y, float to_x, float to_y, float e) {
    *position = (Vec3){x, 0, 0}; *line = 1; *kind = 1; *slope = (Vec3){0, 1, 0};
    return 1;
}
void Match_CorrectCamera(void) { }
int Pause_CheckStatus(int kind) { return paused; }
int HSD_Randi(int maximum) {
    if (forced_roll >= 0 && (maximum == 100 || maximum == LC_OPTION_COUNT)) return forced_roll % maximum;
    random_roll = (unsigned)random_roll * 1664525u + 1013904223u;
    return ((unsigned)random_roll >> 16) % maximum;
}
int SFX_Play(int id) { return ++sounds; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { }
__attribute__((noinline)) void Fighter_PhysAir_CheckFastfall(FighterData *ft) {
    ++fastfalls;
    if (ft->phys.self_vel.Y < 0 && ft->input.lstick.Y <= -0.7f && ft->input.timer_lstick_tilt_y == 0)
        ft->flags.is_fastfall = 1;
}

int TestLCInit(void) {
    *event_vars_ptr_loc = &test_vars;
    *stc_ftcommon = &test_common;
    test_common.cliff_invuln_time = 60;
    *(int *)((u8 *)&test_common + 0x488) = 100; // Native percent threshold is an integer.
    test_common.jumpaerial_lsticky = 0.7f;
    test_common.jumpaerial_lsticktimer = 4;
    test_common.ledge_drop_thresh = 0.3f;
    test_common.x88 = 0.7f;
    *(int *)((u8 *)&test_common + 0x8C) = 4;
    ((float *)&test_common)[8] = 0.785398f; // Native x20_radians.
    test_vars.event_gobj = &test_event;
    test_vars.settings = &test_api;
    test_vars.Savestate_Save_v1 = TestLCSave;
    test_vars.Savestate_Load_v1 = TestLCLoad;
    test_vars.savestate = &test_savestate;
    test_vars.is_protected = TestLCProtected;
    test_vars.set_cpu_invincibility_overlay = TestLCSetPalette;
    test_event.userdata = event_storage.bytes;
    for (int i = 0; i < 4; ++i) {
        test_fighters[i].userdata = &test_ft[i];
        test_fighters[i].hsd_object = &test_joints[i];
        test_ft[i].ply = i / 2;
        test_ft[i].flags.ms = i % 2;
        test_ft[i].kind = i % 2 ? FTKIND_NANA : FTKIND_POPO;
        test_ft[i].pad_index = 0;
        test_ft[i].attr.normal_landing_lag = 4;
        test_ft[i].attr.max_jumps = 2;
        test_ft[i].cb.IASA = TestLCIASA;
        test_ft[i].hurt.intang_frames.respawn = 120;
        test_ft[i].hurt.kind_game = 2;
    }
    Event_Init(&test_event);
    return 1;
}
unsigned TestLCObject(void) { return (unsigned)&test_event; }
unsigned TestLCFighter(int index) { return (unsigned)&test_fighters[index]; }
unsigned TestLCFighterData(int index) { return (unsigned)&test_ft[index]; }
unsigned TestLCEventData(void) { return (unsigned)event_storage.bytes; }
unsigned TestLCNativeEntry(GOBJ *fighter, int state) { ((FighterData *)fighter->userdata)->state_id = state; return state; }
unsigned TestLCNativeSpotEnter(GOBJ *fighter) { return TestLCNativeEntry(fighter, ASID_ESCAPE); }
unsigned TestLCNativeDashEnter(GOBJ *fighter) { return TestLCNativeEntry(fighter, ASID_DASH); }
unsigned TestLCNativeSmashEnter(GOBJ *fighter) { return TestLCNativeEntry(fighter, ASID_ATTACKS4S); }
unsigned TestLCNativeGuardEnter(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->cb.IASA = (void *)0x80092758;
    ft->flags.shield_enable = 1;
    return TestLCNativeEntry(fighter, ASID_GUARDON);
}
unsigned TestLCNativeReflectEnter(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->cb.IASA = (void *)0x80093EC0;
    ft->flags.shield_enable = 1;
    return TestLCNativeEntry(fighter, ASID_GUARDREFLECT);
}
int TestLCNativeGroundParameters(void) {
    *(float *)((u8 *)&test_common + 0x3C) = 0.8f;
    *(int *)((u8 *)&test_common + 0x40) = 4;
    *(float *)((u8 *)&test_common + 0x314) = -0.7f;
    *(int *)((u8 *)&test_common + 0x318) = 4;
    *(float *)((u8 *)&test_common + 0x31C) = 0.7f;
    *(int *)((u8 *)&test_common + 0x320) = 4;
    test_ft[2].shield.health = 60;
    return 1;
}
int TestLCEngineTick(void) {
    for (int i = 2; i < 4; ++i) {
        if (i == 3 && !test_ft[2].flags.has_follower) continue;
        FighterData *ft = &test_ft[i];
        if (ft->state_id == ASID_CLIFFWAIT && !ft->flags.freeze && !ft->flags.hitlag &&
            !ft->input.held && !ft->input.lstick.X && !ft->input.lstick.Y)
            ((FtCliffCatch *)&ft->state_var)->timer = 1;
    }
    return 1;
}
int TestLCStart(void) { TestLCEngineTick(); Event_Think(&test_event); return loads; }
int TestLCFollowers(int enabled) {
    follower_enabled = enabled;
    test_ft[0].flags.has_follower = test_ft[2].flags.has_follower = enabled;
    return enabled;
}
int TestLCAlternate(int kind, int secondary) {
    follower_enabled = 1;
    test_ft[2].kind = kind;
    test_ft[2].flags.has_follower = 0;
    test_ft[3].kind = secondary;
    test_ft[3].state_id = ASID_SLEEP;
    test_ft[3].flags.sleep = 1;
    return 1;
}
int TestLCPreference(int index, int value) {
    preferences[index] = value;
    Event_Init(&test_event);
    return writes;
}
int TestLCMenu(int row, int value) {
    Options_Main[row].val = value;
    Options_Main[row].OnChange((GOBJ *)1, value);
    return writes;
}
int TestLCChangePreset(int value) {
    Options_Chances[CHANCE_PRESET].val = value;
    ChangePreset((GOBJ *)1, value);
    return preferences[TM_LEDGECHASE_PRESET];
}
int TestLCChangeChance(int option, int value) {
    EventOption *row = &Options_Chances[CHANCE_GETUP + option];
    row->val = value;
    row->OnChange((GOBJ *)1, value);
    return writes;
}
int TestLCChance(int option) { return Options_Chances[CHANCE_GETUP + option].val; }
int TestLCMenuValue(int row) { return Options_Main[row].val; }
unsigned TestLCMenuName(int row) { return (unsigned)Options_Main[row].name; }
int TestLCChanceDisabled(int row) { return Options_Chances[row].disable; }
unsigned TestLCChanceValueString(int row) { return (unsigned)Options_Chances[row].value_string; }
int TestLCInfo(int index) {
    LedgechaseData *data = Data();
    int values[] = {writes, saves, loads, data->initialized, data->side, data->option, data->act_frame,
        data->option_started, data->legal_frames, data->vulnerable_frames, data->reset_timer,
        data->success, data->followup_started[0], data->punish_timer, fastfalls, native_calls,
        palette ? *palette : -1, hud_percent[0], hud_percent[1], data->ledge_line};
    return values[index];
}
int TestLCPosition(int index, int component) {
    return (int)((component ? test_ft[index].phys.pos.Y : test_ft[index].phys.pos.X) * 1000);
}
int TestLCProtection(int index) { return TestLCProtected(&test_fighters[index]); }
int TestLCReset(int success) { ResetAttempt(success); return loads; }
int TestLCPrepared(int option, int delay) {
    Data()->option = option;
    Data()->entry_complete = 1;
    Data()->act_frame = delay;
    Data()->option_started = Data()->legal_frames = Data()->vulnerable_frames = Data()->elapsed = 0;
    Data()->followup_started[0] = Data()->followup_started[1] = 0;
    Data()->return_phase[0] = Data()->return_phase[1] = 0;
    for (int ms = 0; ms < 2; ++ms) {
        int action = Data()->air[ms].action;
        memset(&Data()->air[ms], 0, sizeof(Data()->air[ms]));
        Data()->air[ms].action = action;
    }
    Data()->reset_timer = Data()->punish_timer = -1;
    for (int i = 2; i < 4; ++i) {
        test_ft[i].state_id = ASID_CLIFFWAIT;
        test_ft[i].phys.air_state = 1;
        test_ft[i].jump.jumps_used = 1;
        ((FtCliffCatch *)&test_ft[i].state_var)->timer = 1;
    }
    return 1;
}
int TestLCSetProtected(int protected) {
    for (int i = 2; i < 4; ++i) {
        test_ft[i].hurt.intang_frames.ledge = protected ? 60 : 0;
        test_ft[i].hurt.kind_game = protected ? 2 : 0;
    }
    return protected;
}
int TestLCState(int index, int state, int air, int frame) {
    test_ft[index].state_id = state;
    test_ft[index].phys.air_state = air;
    test_ft[index].state.frame = frame;
    return state;
}
int TestLCHit(int index, int value) { test_ft[index].flags.hitstun = value; return value; }
int TestLCDead(int index, int value) { test_ft[index].flags.dead = value; return value; }
int TestLCFreeze(int index, int value) { test_ft[index].flags.hitlag = value; return value; }
int TestLCNativeFreeze(int index, int value) { test_ft[index].flags.freeze = value; return value; }
int TestLCPause(int value) { paused = value; return value; }
int TestLCUseNativeIASA(unsigned address) { test_ft[2].cb.IASA = (void *)address; return address; }
int TestLCEngineIASA(void) { RunNativeIASA(&test_fighters[2]); return test_ft[2].state_id; }
int TestLCLedgeInput(int option) { NeutralCPU(&test_fighters[2]); RunLedgeOption(&test_fighters[2], option, 1); return test_ft[2].state_id; }
int TestLCSetPercent(int value) { test_ft[2].dmg.percent = value; return value; }
int TestLCSetKind(int value) { test_ft[2].kind = value; return value; }
int TestLCFollowup(int action) { return RunFollowup(&test_fighters[2], &test_fighters[0], FollowupActions[action], 1); }
int TestLCInputNeutral(void) {
    FighterData *ft = &test_ft[2];
    return !ft->cpu.held && !ft->cpu.lstickX && !ft->cpu.lstickY &&
        !ft->input.held && !ft->input.down && !ft->input.lstick.X && !ft->input.lstick.Y && !ft->input.trigger;
}
int TestLCFastfall(int vertical) {
    test_ft[2].phys.air_state = 1;
    test_ft[2].phys.self_vel.Y = vertical / 1000.f;
    test_ft[2].flags.is_fastfall = 0;
    FastfallJump(&test_fighters[2]);
    return test_ft[2].flags.is_fastfall;
}
int TestLCFollowerLog(int index) {
    FighterData *sub = &test_ft[index], *leader = &test_ft[index & ~1];
    for (int i = 0; i < 30; ++i)
        if (sub->cpu.leader_log[i].pos.X != leader->phys.pos.X ||
            sub->cpu.leader_log[i].pos.Y != leader->phys.pos.Y ||
            sub->cpu.leader_log[i].facing_direction != leader->facing_direction) return 0;
    return 1;
}
int TestLCChooseFrame(int preference, int roll, int protection) { return LC_ActFrame(preference, roll, protection); }
int TestLCNativeMotionState(int state) { return state; }
unsigned TestLCAnyOption(EventMenu *menu, int row) { return (unsigned)&menu->options[row]; }
int TestLCAnyOptionValue(EventOption *option) { return option->val; }
int TestLCAnyOptionChange(EventOption *option, int value) {
    option->val = value;
    option->OnChange((GOBJ *)1, value);
    return writes;
}
unsigned TestLCAnySubmenu(EventOption *option) { return (unsigned)option->menu; }
int TestLCStateValue(int index) { return test_ft[index].state_id; }

int TestLCActionCount(void) { return countof(FollowupNames); }
unsigned TestLCActionName(int index) { return (unsigned)FollowupNames[index]; }
int TestLCActionID(int index) { return FollowupActions[index]; }
int TestLCPendingDamage(int index) { return test_ft[index].dmg.percent_temp; }
int TestLCApplyPendingDamage(int index) {
    test_ft[index].dmg.percent += test_ft[index].dmg.percent_temp;
    test_ft[index].dmg.percent_temp = 0;
    return test_ft[index].dmg.percent;
}
int TestLCNoIASA(GOBJ *fighter) { return 0; }
int TestLCFollowupReset(void) {
    Data()->followup_active[0] = Data()->followup_pending[0] = 0;
    return 0;
}
int TestLCStick(int component) {
    FighterData *ft = &test_ft[2];
    int values[] = {ft->cpu.lstickX,ft->cpu.lstickY,ft->cpu.cstickX,ft->cpu.cstickY,ft->cpu.held};
    return values[component];
}

int TestLCChosenAction(int ms) { return Data()->followup_action[ms]; }

int TestLCHumanSide(int dir) { test_ft[0].phys.pos.X = test_ft[2].phys.pos.X + dir * 20; return dir; }

int TestLCReturnInfo(int ms, int field) {
    FighterData *ft = &test_ft[2 + ms];
    int values[] = {Data()->return_phase[ms], ft->jump.jumps_used, ft->input.down,
        ft->input.held, ft->flags.is_fastfall};
    return values[field];
}
int TestLCReturnPosition(int ms, int inside, int height, int bottom, int vertical) {
    FighterData *ft = &test_ft[2 + ms];
    ft->phys.pos.X = Data()->ledge.X + Data()->inward * inside;
    ft->phys.pos.Y = Data()->ledge.Y + height;
    ft->coll_data.ecbCurr_bot.Y = bottom;
    ft->phys.self_vel.Y = vertical;
    return 1;
}
int TestLCJumpKind(int index, int kind) {
    test_ft[index].kind = kind;
    test_ft[index].flags.is_multijump = kind == FTKIND_KIRBY || kind == FTKIND_JIGGLYPUFF;
    test_ft[index].attr.max_jumps = test_ft[index].flags.is_multijump ? 6 : 2;
    return kind;
}
int TestLCSelect(int a, int b, int c, int d, int e, int f, int roll) {
    int chances[] = {a,b,c,d,e,f};
    return LC_SelectOption(chances, roll);
}
int TestLCNativeFallEnter(GOBJ *fighter) { return TestLCNativeEntry(fighter, ASID_FALL); }
int TestLCNativeJumpEnter(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ++ft->jump.jumps_used;
    ft->flags.is_fastfall = 0;
    return TestLCNativeEntry(fighter, ft->flags.is_multijump ? 341 : ASID_JUMPAERIALF);
}
int TestLCNativeAirDodgeEnter(GOBJ *fighter) { return TestLCNativeEntry(fighter, ASID_ESCAPEAIR); }
int TestLCNativeReturnIASA(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    return ((int (*)(GOBJ *))(ft->state_id == ASID_FALL ? 0x800CB870 : 0x80099A58))(fighter);
}
unsigned TestLCAirName(int row) { return (unsigned)AirNames[row]; }
int TestLCAirCount(void) { return countof(AirNames); }
int TestLCAirInfo(int ms, int field) {
    int values[] = {Data()->air[ms].action, Data()->air[ms].started, Data()->air[ms].loops,
        Data()->air[ms].pending, Data()->air[ms].repeat_tick};
    return values[field];
}
int TestLCAirRun(void) { RunAirAttack(&test_fighters[2], &test_fighters[0], Data()); return Data()->air[0].started; }
int TestLCBlasterWindow(int allowed) {
    test_ft[2].ftcmd_var.flag0 = allowed;
    test_ft[2].state_var.state_var1 = 0;
    return 1;
}
int TestLCBlasterRepeat(void) { return test_ft[2].state_var.state_var1; }
int TestLCCustomChances(void) { return custom_chances; }
int TestLCPresetCount(void) { return countof(PresetNames); }
int TestLCForceRoll(int roll) { forced_roll = roll; return roll; }
int TestLCAirActionID(int row) { return AirActions[row]; }
int TestLCSetup(int row, int value) {
    Options_Setup[row].val = value;
    Options_Setup[row].OnChange((GOBJ *)1, value);
    return writes;
}
int TestLCEntryInfo(int field) {
    int values[] = {Data()->entry_complete, Data()->entry_elapsed, Data()->entry_jump[0], Data()->entry_jump[1], Data()->elapsed};
    return values[field];
}
int TestLCDropInfo(int ms, int field) {
    return field ? Data()->drop_frames[ms] : (int)(Data()->drop_target[ms]*1000);
}
int TestLCJumpAttributes(int velocity, int gravity, int terminal) {
    test_ft[2].attr.jump_v_initial_velocity = velocity/1000.f;
    test_ft[2].attr.air_jump_v_multiplier = 1;
    test_ft[2].attr.gravity = gravity/1000.f;
    test_ft[2].attr.terminal_velocity = terminal/1000.f;
    return 1;
}
