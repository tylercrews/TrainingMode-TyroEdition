#include "events.h"
#include "ledgechase_logic.h"

static EventMenu Menu_Main, Menu_Chances;
static void Exit(GOBJ *menu);
static void ChangeSide(GOBJ *menu, int value);
static void ChangeCPUPercent(GOBJ *menu, int value);
static void ChangePlayerPercent(GOBJ *menu, int value);
static void ChangePreset(GOBJ *menu, int value);
static void ChangeDistance(GOBJ *menu, int value);
static void ChangeColor(GOBJ *menu, int value);
static void ChangeFollowup(GOBJ *menu, int value);
static void ChangeGetupChance(GOBJ *menu, int value);
static void ChangeJumpChance(GOBJ *menu, int value);
static void ChangeRollChance(GOBJ *menu, int value);
static void ChangeAttackChance(GOBJ *menu, int value);

static const char *SideNames[] = {"Left", "Right", "Swap on Success", "Random"};
static const char *PercentNames[] = {"0%", "25%", "50%", "75%", "100%", "125%", "150%"};
static const char *PresetNames[] = {"Even", "Getup Only", "Jump Only", "Roll Only", "Attack Only", "Tyro setting"};
static const char *DistanceNames[] = {"At Ledge", "Close", "Mid", "Far", "Safe"};
static const char *ColorNames[] = {TM_CPU_INVINCIBILITY_NAMES};
static const char *FollowupNames[] = {"None", "Spotdodge", "Roll", "Dash", "Smash"};

enum { OPT_CHANCES, OPT_SIDE, OPT_CPU_PERCENT, OPT_PLAYER_PERCENT, OPT_DISTANCE, OPT_COLOR, OPT_FOLLOWUP, OPT_EXIT };
enum { CHANCE_PRESET, CHANCE_GETUP, CHANCE_JUMP, CHANCE_ROLL, CHANCE_ATTACK, CHANCE_HOP, CHANCE_DASH };

static EventOption Options_Main[] = {
    {.kind = OPTKIND_MENU, .name = "Ledge Option Chances", .menu = &Menu_Chances,
     .desc = {"Choose a saved preset or edit chances", "for this session."}},
    {.kind = OPTKIND_STRING, .name = "Ledge Side", .values = SideNames, .value_num = LC_SIDE_COUNT,
     .desc = {"Choose a ledge. Swap changes sides only", "after you hit or grab the CPU."}, .OnChange = ChangeSide},
    {.kind = OPTKIND_TOGGLE, .name = "CPU above 100%",
     .desc = {"Off starts the CPU at 0%. On starts at 100%", "for the native slow ledge options."}, .OnChange = ChangeCPUPercent},
    {.kind = OPTKIND_STRING, .name = "Player Percent", .values = PercentNames, .value_num = countof(PercentNames),
     .desc = {"Choose your starting damage, from 0% to 150%."}, .OnChange = ChangePlayerPercent},
    {.kind = OPTKIND_STRING, .name = "Player Distance", .values = DistanceNames, .value_num = LC_DISTANCE_COUNT,
     .val = 1, .desc = {"Choose how far inside the ledge you start.", "Safe gives the largest spacing toward center."}, .OnChange = ChangeDistance},
    {.kind = OPTKIND_STRING, .name = "CPU Invincibility Overlay", .values = ColorNames, .value_num = TM_CPU_INVINCIBILITY_COUNT,
     .val = TM_CPU_INVINCIBILITY_DEFAULT,
     .desc = {"Choose a color for the CPU's natural", "invincibility and intangibility."}, .OnChange = ChangeColor},
    {.kind = OPTKIND_STRING, .name = "CPU Action", .values = FollowupNames, .value_num = LC_FOLLOWUP_COUNT,
     .desc = {"CPU acts on its first legal grounded frame", "after the ledge option. Jump always fastfalls."}, .OnChange = ChangeFollowup},
    {.kind = OPTKIND_FUNC, .name = "Exit", .desc = {"Save settings and return to Event Select."}, .OnSelect = Exit},
};
static EventOption Options_Chances[] = {
    {.kind = OPTKIND_STRING, .name = "Preset", .values = PresetNames, .value_num = LC_PRESET_COUNT,
     .desc = {"The preset is saved. Manual chance changes", "below last only for this session."}, .OnChange = ChangePreset},
    {.kind = OPTKIND_INT, .name = "Getup Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of a normal ledge getup."}, .OnChange = ChangeGetupChance},
    {.kind = OPTKIND_INT, .name = "Jump Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of a ledge jump, then fastfall."}, .OnChange = ChangeJumpChance},
    {.kind = OPTKIND_INT, .name = "Roll Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of a ledge roll."}, .OnChange = ChangeRollChance},
    {.kind = OPTKIND_INT, .name = "Attack Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of a ledge getup attack."}, .OnChange = ChangeAttackChance},
    {.kind = OPTKIND_INFO, .name = "Ledge Hop Chance", .value_string = "N/A", .disable = true,
     .desc = {"Ledgehop is not available yet."}},
    {.kind = OPTKIND_INFO, .name = "Ledge Dash Chance", .value_string = "N/A", .disable = true,
     .desc = {"Ledgedash is not available yet."}},
};
static EventMenu Menu_Main = {.name = "Ledgechase Training", .options = Options_Main, .option_num = countof(Options_Main)};
static EventMenu Menu_Chances = {.name = "Ledge Option Chances", .options = Options_Chances, .option_num = countof(Options_Chances)};
EventMenu *Event_Menu = &Menu_Main;

typedef struct LedgechaseData {
    EventDesc *desc; // Common event prefix.
    int initialized, side, option, act_frame, legal_frames, vulnerable_frames;
    int option_started, elapsed, followup_started[2], punish_timer;
    int reset_timer, success;
    Vec3 ledge;
    int ledge_line, inward;
} LedgechaseData;
typedef char ledgechase_data_size[(sizeof(LedgechaseData) <= EVENT_DATASIZE) ? 1 : -1];

static LedgechaseData *Data(void) { return event_vars->event_gobj->userdata; }
static GOBJ *FighterBody(int player, int ms) {
    GOBJ *leader = Fighter_GetGObj(player);
    if (!ms) return leader;
    // Zelda/Sheik also have a secondary GOBJ, but it is a sleeping alternate
    // form. Only Ice Climbers' active follower participates in an attempt.
    if (!leader || !((FighterData *)leader->userdata)->flags.has_follower) return 0;
    return Fighter_GetSubcharGObj(player, ms);
}
static void ApplyPreset(int preset) {
    for (int i = 0; i < LC_OPTION_COUNT; ++i)
        Options_Chances[CHANCE_GETUP + i].val = LC_PresetChance(preset, i);
}
static void ReboundChances(int changed) {
    int chances[LC_OPTION_COUNT];
    for (int i = 0; i < LC_OPTION_COUNT; ++i) chances[i] = Options_Chances[CHANCE_GETUP + i].val;
    LC_Rebound(chances, changed);
    for (int i = 0; i < LC_OPTION_COUNT; ++i) Options_Chances[CHANCE_GETUP + i].val = chances[i];
}
static void ChangeGetupChance(GOBJ *menu, int value) { ReboundChances(LC_GETUP); }
static void ChangeJumpChance(GOBJ *menu, int value) { ReboundChances(LC_JUMP); }
static void ChangeRollChance(GOBJ *menu, int value) { ReboundChances(LC_ROLL); }
static void ChangeAttackChance(GOBJ *menu, int value) { ReboundChances(LC_ATTACK); }

static void UpdatePosition(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    Vec3 pos = ft->phys.pos;
    ft->phys.pos_prev = pos;
    ft->coll_data.topN_Curr = pos;
    ft->coll_data.topN_CurrCorrect = pos;
    ft->coll_data.topN_Prev = pos;
    ft->coll_data.topN_Proj = pos;
    ft->coll_data.coll_test = *stc_colltest;
    JOBJ *joint = fighter->hsd_object;
    joint->trans = pos;
    JOBJ_SetMtxDirtySub(joint);
    Fighter_SetPosition(ft->ply, ft->flags.ms, &pos);
}
static void ClearProtection(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->hurt.kind_game = 0;
    ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = 0;
    ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
}
static void ClearVelocity(FighterData *ft) {
    ft->phys.self_vel = (Vec3){0, 0, 0};
    ft->phys.kb_vel = (Vec3){0, 0, 0};
    ft->phys.self_vel_ground = (Vec3){0, 0, 0};
    ft->phys.nudge_vel = (Vec2){0, 0};
}
static void SetPercent(GOBJ *fighter, int percent) {
    FighterData *ft = fighter->userdata;
    ft->dmg.percent = percent;
    ft->dmg.percent_temp = percent;
    Fighter_SetHUDDamage(ft->ply, percent);
}
static void SeedFollower(GOBJ *sub, GOBJ *leader) {
    FighterData *ft = sub->userdata, *main = leader->userdata;
    for (unsigned i = 0; i < countof(ft->cpu.leader_log); ++i) {
        ft->cpu.leader_log[i].pos = main->phys.pos;
        ft->cpu.leader_log[i].facing_direction = main->facing_direction;
    }
}
static void PlaceHuman(GOBJ *fighter, LedgechaseData *data, int follower) {
    FighterData *ft = fighter->userdata;
    float distance = LC_Distance(Options_Main[OPT_DISTANCE].val) + follower * 3;
    ft->phys.pos = data->ledge;
    ft->phys.pos.X += data->inward * distance;
    ft->facing_direction = -data->inward;
    ft->facing_direction_prev = ft->facing_direction;
    ClearVelocity(ft);
    Vec3 ground, slope;
    int line, kind;
    float x = ft->phys.pos.X, y = data->ledge.Y;
    if (GrColl_RaycastGround(&ground, &line, &kind, &slope, -1, -1, -1, 0, x, y + 10, x, y - 20, 0)) {
        ft->phys.pos = ground;
        ft->coll_data.ground_index = line;
    }
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(ft);
    Fighter_EnterWait(fighter);
    ClearProtection(fighter);
    SetPercent(fighter, Options_Main[OPT_PLAYER_PERCENT].val * 25);
    Fighter_UpdateCameraBox(fighter);
}
static void PlaceCPU(GOBJ *fighter, LedgechaseData *data) {
    FighterData *ft = fighter->userdata;
    Fighter_EnterSleep(fighter, ft->flags.ms);
    Fighter_EnterRebirth(fighter);
    ft->facing_direction = data->inward;
    ft->facing_direction_prev = data->inward;
    ClearVelocity(ft);
    ClearProtection(fighter);
    SetPercent(fighter, Options_Main[OPT_CPU_PERCENT].val ? 100 : 0);
    ft->state_var.state_var1 = data->ledge_line;
    Fighter_EnterCliffWait(fighter);
    ft->state_var.state_var3 = 0;
    ft->state_var.state_var2 = 0x40000000; // Native float hang timeout = 2.0f.
    Fighter_SetAirborne(ft);
    Fighter_EnableCollUpdate(ft);
    Coll_CheckLedge(&ft->coll_data);
    Fighter_MoveToCliff(fighter);
    UpdatePosition(fighter);
    Fighter_ApplyIntang(fighter, (*stc_ftcommon)->cliff_invuln_time + 1);
    ft->cpu.ai = 15;
    Fighter_UpdateCameraBox(fighter);
}

static void ResetAttempt(int success) {
    LedgechaseData *data = Data();
    if (!data->initialized) return;
    int setting = Options_Main[OPT_SIDE].val;
    unsigned random_side = setting == LC_SIDE_RANDOM ? HSD_Randi(2) : 0;
    int next_side = LC_NextSide(setting, data->side, success, random_side);
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    // The common savestate restores the complete event userdata too. Keep
    // attempt ownership and the selected side across that baseline restore.
    data->initialized = 1;
    data->side = next_side;
    data->ledge_line = data->side ? 2 : 0; // Final Destination's two native ledge floors.
    data->inward = data->side ? -1 : 1;
    if (data->side) Stage_GetRightOfLineCoordinates(data->ledge_line, &data->ledge);
    else Stage_GetLeftOfLineCoordinates(data->ledge_line, &data->ledge);
    for (int player = 0; player < 2; ++player) {
        GOBJ *leader = Fighter_GetGObj(player);
        for (int ms = 0; ms < 2; ++ms) {
            GOBJ *fighter = FighterBody(player, ms);
            if (!fighter) continue;
            if (player) PlaceCPU(fighter, data);
            else PlaceHuman(fighter, data, ms);
            if (ms) SeedFollower(fighter, leader);
        }
    }
    int chances[LC_OPTION_COUNT];
    for (int i = 0; i < LC_OPTION_COUNT; ++i) chances[i] = Options_Chances[CHANCE_GETUP + i].val;
    data->option = LC_SelectOption(chances, HSD_Randi(100));
    data->act_frame = LC_ActFrame(HSD_Randi(100), HSD_Randi((*stc_ftcommon)->cliff_invuln_time + 5),
        (*stc_ftcommon)->cliff_invuln_time + 1);
    data->legal_frames = data->vulnerable_frames = data->option_started = data->elapsed = 0;
    data->followup_started[0] = data->followup_started[1] = 0;
    data->punish_timer = data->reset_timer = -1;
    data->success = 0;
    Match_CorrectCamera();
}

/* AI is disabled and both raw and processed inputs are controlled. The event
 * runs after CPU input decoding; changing only the raw input would add a frame. */
static void NeutralCPU(GOBJ *cpu) {
    FighterData *ft = cpu->userdata;
    ft->cpu.ai = 15;
    ft->cpu.held = 0;
    ft->cpu.lstickX = ft->cpu.lstickY = ft->cpu.cstickX = ft->cpu.cstickY = 0;
    ft->input.held = ft->input.down = 0;
    ft->input.lstick = ft->input.cstick = (Vec2){0, 0};
    ft->input.trigger = 0;
}
static void QueueInput(GOBJ *cpu, unsigned held, int x, int y) {
    FighterData *ft = cpu->userdata;
    ft->cpu.held = held;
    ft->cpu.lstickX = x;
    ft->cpu.lstickY = y;
    unsigned processed = held;
    if (held & (PAD_TRIGGER_R | PAD_TRIGGER_L)) processed |= 0x80000000u; // HSD_PAD_LR
    ft->input.held = ft->input.down = processed;
    ft->input.lstick = (Vec2){x / 127.f, y / 127.f};
    ft->input.timer_lstick_tilt_x = ft->input.timer_lstick_tilt_y = 0;
    if (processed & 0x80000000u) {
        ft->input.trigger = 1;
        ft->input.timer_trigger_analog = ft->input.timer_trigger_digital = 0;
    }
}
static void RunNativeIASA(GOBJ *cpu) {
    FighterData *ft = cpu->userdata;
    if (ft->flags.dead || ft->flags.freeze || ft->flags.hitlag || ft->dmg.hitlag_frames > 0) return;
    if (ft->cb.IASA) ft->cb.IASA(cpu);
}
static int IsLedgeOption(int state) {
    return state >= ASID_CLIFFCLIMBSLOW && state <= ASID_CLIFFJUMPQUICK2;
}
static void RunLedgeOption(GOBJ *cpu, int option, int inward) {
    FighterData *ft = cpu->userdata;
    if (ft->state_id != ASID_CLIFFWAIT) return;
    // Normal getup requires the native stick-release latch. A neutral first
    // frame arms it; never force an option before that native requirement.
    if (option == LC_GETUP && !ft->state_var.state_var3) return;
    if (option == LC_GETUP) QueueInput(cpu, 0, inward * 127, 0);
    else if (option == LC_JUMP) QueueInput(cpu, PAD_BUTTON_X, 0, 0);
    else if (option == LC_ROLL) QueueInput(cpu, PAD_TRIGGER_R, 0, 0);
    else QueueInput(cpu, PAD_BUTTON_A, 0, 0);
    RunNativeIASA(cpu);
    if (IsLedgeOption(ft->state_id)) NeutralCPU(cpu);
}
static int GroundReady(FighterData *ft) {
    return ft->phys.air_state == 0 && (ft->state_id == ASID_WAIT ||
        (ft->state_id == ASID_LANDING && ft->state.frame >= ft->attr.normal_landing_lag));
}
static int FollowupStarted(FighterData *ft, int action) {
    int state = ft->state_id;
    if (action == LC_SPOTDODGE) return state == ASID_ESCAPE;
    if (action == LC_FOLLOWUP_ROLL) return state == ASID_ESCAPEF || state == ASID_ESCAPEB;
    if (action == LC_DASH) return state == ASID_DASH || state == ASID_RUN;
    return (state >= ASID_ATTACKS4HI && state <= ASID_ATTACKS4LW) ||
        (ft->kind == FTKIND_NESS && state == 341) ||
        (ft->kind == FTKIND_GAW && state == 346) ||
        (ft->kind == FTKIND_PEACH && state >= 349 && state <= 351);
}
static int RunFollowup(GOBJ *cpu, GOBJ *human, int action, int inward) {
    FighterData *ft = cpu->userdata, *hmn = human->userdata;
    if (action == LC_NONE) return GroundReady(ft);
    int state = ft->state_id;
    // Native landing/turn/shield IASA performs its own readiness check. Never
    // invoke an action directly or interrupt a ledge option or aerial state.
    int eligible = state == ASID_WAIT || state == ASID_LANDING || state == ASID_TURN ||
        state == ASID_GUARDON || state == ASID_GUARD || state == ASID_GUARDREFLECT ||
        (ft->kind == FTKIND_YOSHI && ft->flags.shield_enable);
    if (!eligible || ft->phys.air_state) return 0;
    int toward = hmn->phys.pos.X > ft->phys.pos.X ? 1 : hmn->phys.pos.X < ft->phys.pos.X ? -1 : inward;
    if (action == LC_SPOTDODGE) QueueInput(cpu, PAD_TRIGGER_R, 0, -127);
    else if (action == LC_FOLLOWUP_ROLL) QueueInput(cpu, PAD_TRIGGER_R, inward * 127, 0);
    else if (action == LC_DASH) QueueInput(cpu, 0, toward * 127, 0);
    else QueueInput(cpu, PAD_BUTTON_A, toward * 127, 0);
    RunNativeIASA(cpu);
    int started = FollowupStarted(ft, action);
    // The native transition has consumed the input. Do not leave a roll's
    // trigger/stick queued for the next CPU input pass or subsequent IASA.
    if (started) NeutralCPU(cpu);
    return started;
}
static void FastfallJump(GOBJ *cpu) {
    FighterData *ft = cpu->userdata;
    if (ft->flags.dead || ft->flags.freeze || ft->flags.hitlag || ft->dmg.hitlag_frames > 0) return;
    if (!ft->phys.air_state || ft->phys.self_vel.Y >= 0) return;
    QueueInput(cpu, 0, 0, -127);
    Fighter_PhysAir_CheckFastfall(ft);
}
static int WasHit(FighterData *ft) {
    return ft->flags.hitstun || (ft->dmg.hitlag_frames > 0 && ft->flags.hitlag_victim) ||
        (ft->state_id >= ASID_CAPTUREPULLEDHI && ft->state_id <= ASID_CAPTUREFOOT) ||
        (ft->state_id >= ASID_DAMAGEHI1 && ft->state_id <= ASID_DAMAGEFLYROLL);
}

void Event_Init(GOBJ *gobj) {
    LedgechaseData *data = gobj->userdata;
    EventDesc *desc = data->desc;
    memset(data, 0, sizeof(*data));
    data->desc = desc;
    data->reset_timer = data->punish_timer = -1;
    static const unsigned fields[] = {TM_LEDGECHASE_PRESET, TM_LEDGECHASE_SIDE, TM_LEDGECHASE_CPU_100,
        TM_LEDGECHASE_PLAYER_PERCENT, TM_LEDGECHASE_DISTANCE, TM_LEDGECHASE_CPU_COLOR, TM_LEDGECHASE_CPU_ACTION};
    for (int i = 0; i < OPT_EXIT; ++i) {
        if (i == OPT_CHANCES) continue;
        Options_Main[i].val = TM_GetSetting(TM_SETTING_LEDGECHASE, fields[i]);
        Options_Main[i].val_prev = Options_Main[i].val;
    }
    Options_Chances[CHANCE_PRESET].val = TM_GetSetting(TM_SETTING_LEDGECHASE, TM_LEDGECHASE_PRESET);
    ApplyPreset(Options_Chances[CHANCE_PRESET].val);
    for (unsigned i = 0; i < countof(Options_Chances); ++i) Options_Chances[i].val_prev = Options_Chances[i].val;
    event_vars->set_cpu_invincibility_overlay(&Options_Main[OPT_COLOR].val);
}
void Event_Think(GOBJ *gobj) {
    if (Pause_CheckStatus(1) == 2) return;
    LedgechaseData *data = gobj->userdata;
    if (!data->initialized) {
        event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
        data->initialized = 1;
        ResetAttempt(0);
    }
    GOBJ *human = Fighter_GetGObj(0), *cpu = Fighter_GetGObj(1);
    FighterData *hmn = human->userdata, *ft = cpu->userdata;
    HSD_Pad *pad = PadGetMaster(hmn->pad_index);
    if (pad->down & HSD_BUTTON_DPAD_LEFT) { ResetAttempt(0); return; }
    int frozen = 0;
    for (int ms = 0; ms < 2; ++ms) {
        GOBJ *fighter = FighterBody(1, ms);
        if (fighter) {
            FighterData *body = fighter->userdata;
            NeutralCPU(fighter);
            if (body->flags.freeze || body->flags.hitlag || body->dmg.hitlag_frames > 0) frozen = 1;
        }
    }
    if (data->reset_timer >= 0) {
        if (!frozen && --data->reset_timer <= 0) ResetAttempt(data->success);
        return;
    }
    for (int ms = 0; ms < 2; ++ms) {
        GOBJ *fighter = FighterBody(1, ms);
        if (fighter && WasHit(fighter->userdata)) {
            data->success = 1;
            data->reset_timer = LC_HIT_RESET_DELAY;
            SFX_Play(0xAD);
            return;
        }
    }
    // Native procAnim/procIASA skip flags.freeze as well as hitlag. Keep the
    // attempt and feedback clocks frozen for either Ice Climber body.
    if (frozen) return;
    for (int player = 0; player < 2; ++player) {
        for (int ms = 0; ms < 2; ++ms) {
            GOBJ *fighter = FighterBody(player, ms);
            if (fighter && ((FighterData *)fighter->userdata)->flags.dead) { ResetAttempt(0); return; }
        }
    }
    if (!data->option_started) {
        if (ft->state_id == ASID_CLIFFWAIT) {
            ft->state_var.state_var2 = 0x40000000; // Native float hang timeout = 2.0f.
            int legal = data->option != LC_GETUP || ft->state_var.state_var3;
            if (legal && LC_ReadyToAct(&data->legal_frames, &data->vulnerable_frames,
                                     event_vars->is_protected(cpu), data->act_frame)) {
                for (int ms = 0; ms < 2; ++ms) {
                    GOBJ *fighter = FighterBody(1, ms);
                    if (fighter) RunLedgeOption(fighter, data->option, data->inward);
                }
            }
        }
        if (IsLedgeOption(ft->state_id)) data->option_started = 1;
    } else {
        int all_finished = 1;
        for (int ms = 0; ms < 2; ++ms) {
            GOBJ *fighter = FighterBody(1, ms);
            if (!fighter) continue;
            FighterData *sub = fighter->userdata;
            if (sub->state_id == ASID_CLIFFWAIT) RunLedgeOption(fighter, data->option, data->inward);
            if (data->option == LC_JUMP && !data->followup_started[ms]) FastfallJump(fighter);
            if (!data->followup_started[ms])
                data->followup_started[ms] = RunFollowup(fighter, human, Options_Main[OPT_FOLLOWUP].val, data->inward);
            if (!data->followup_started[ms] || !GroundReady(sub)) all_finished = 0;
        }
        if (all_finished && data->punish_timer < 0) data->punish_timer = LC_PUNISH_WINDOW;
        if (data->punish_timer >= 0 && --data->punish_timer <= 0) { ResetAttempt(0); return; }
    }
    if (++data->elapsed >= LC_ATTEMPT_LIMIT + (*stc_ftcommon)->cliff_invuln_time) ResetAttempt(0);
}

static void ChangePreference(unsigned field, int value, int reset) {
    TM_SetSetting(TM_SETTING_LEDGECHASE, field, value);
    if (reset) ResetAttempt(0);
}
static void ChangeSide(GOBJ *menu, int value) { ChangePreference(TM_LEDGECHASE_SIDE, value, 1); }
static void ChangeCPUPercent(GOBJ *menu, int value) { ChangePreference(TM_LEDGECHASE_CPU_100, value, 1); }
static void ChangePlayerPercent(GOBJ *menu, int value) { ChangePreference(TM_LEDGECHASE_PLAYER_PERCENT, value, 1); }
static void ChangeDistance(GOBJ *menu, int value) { ChangePreference(TM_LEDGECHASE_DISTANCE, value, 1); }
static void ChangeColor(GOBJ *menu, int value) { ChangePreference(TM_LEDGECHASE_CPU_COLOR, value, 0); }
static void ChangeFollowup(GOBJ *menu, int value) { ChangePreference(TM_LEDGECHASE_CPU_ACTION, value, 1); }
static void ChangePreset(GOBJ *menu, int value) {
    ApplyPreset(value);
    ChangePreference(TM_LEDGECHASE_PRESET, value, 1);
}
static void Exit(GOBJ *menu) {
    event_vars->set_cpu_invincibility_overlay(0);
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}
