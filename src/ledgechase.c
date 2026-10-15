#include "events.h"
#include "ledgechase_logic.h"
#include "cpu_actions.h"
#include "cpu_air_attacks.h"

static EventMenu Menu_Main, Menu_Chances;
static void Exit(GOBJ *menu);
static void ChangeSide(GOBJ *menu, int value);
static void ChangeCPUPercent(GOBJ *menu, int value);
static void ChangePlayerPercent(GOBJ *menu, int value);
static void ChangePreset(GOBJ *menu, int value);
static void ChangeDistance(GOBJ *menu, int value);
static void ChangeColor(GOBJ *menu, int value);
static void ChangeFollowup(GOBJ *menu, int value);
static void ChangeAirAttack(GOBJ *menu, int value);
static void ChangeGetupChance(GOBJ *menu, int value);
static void ChangeJumpChance(GOBJ *menu, int value);
static void ChangeRollChance(GOBJ *menu, int value);
static void ChangeAttackChance(GOBJ *menu, int value);
static void ChangeHopChance(GOBJ *menu, int value);
static void ChangeDashChance(GOBJ *menu, int value);
static bool CheckHasJump(GOBJ *cpu);

static const char *SideNames[] = {"Left", "Right", "Swap on Success", "Random"};
static const char *PercentNames[] = {"0%", "25%", "50%", "75%", "100%", "125%", "150%"};
static const char *PresetNames[] = {"Even", "Getup Only", "Jump Only", "Roll Only", "Attack Only", "Tyro setting", "Getup / Roll",
    "Ledge Hop Only", "Ledge Dash Only", "Regular Four + Hop", "Regular Four + Dash", "All Six Even",
    "Dash 75 / Jump 25", "Hop / Dash"};
static const char *DistanceNames[] = {"At Ledge", "Close", "Mid", "Far", "Safe"};
static const char *ColorNames[] = {TM_CPU_INVINCIBILITY_NAMES};
static const char *FollowupNames[] = {CPU_GROUND_ACTIONS(CPU_GROUND_ACTION_NAME) "Random"};
static const u8 FollowupActions[] = {CPU_GROUND_ACTIONS(CPU_GROUND_ACTION_ID) CPUACT_RANDOM};
static const char *AirNames[] = {CPU_AIR_ATTACKS(CPU_GROUND_ACTION_NAME) "Random"};
static const u8 AirActions[] = {CPU_AIR_ATTACKS(CPU_GROUND_ACTION_ID) CPUACT_RANDOM};
static int custom_chances;
typedef char followup_count_matches[(countof(FollowupActions) == CPU_GROUND_ACTION_COUNT) ? 1 : -1];
typedef char air_count_matches[(countof(AirActions) == CPU_AIR_ATTACK_COUNT) ? 1 : -1];
typedef char preset_count_matches[(countof(PresetNames) == LC_PRESET_COUNT) ? 1 : -1];

enum { OPT_CHANCES, OPT_SIDE, OPT_CPU_PERCENT, OPT_PLAYER_PERCENT, OPT_DISTANCE, OPT_COLOR, OPT_FOLLOWUP, OPT_AIR, OPT_EXIT };
enum { CHANCE_PRESET, CHANCE_GETUP, CHANCE_JUMP, CHANCE_ROLL, CHANCE_ATTACK, CHANCE_HOP, CHANCE_DASH };

static EventOption Options_Main[] = {
    {.kind = OPTKIND_MENU, .name = "Ledge Option Chances", .menu = &Menu_Chances,
     .desc = {"Choose a saved preset or edit chances", "for this session."}},
    {.kind = OPTKIND_STRING, .name = "Ledge Side", .values = SideNames, .value_num = LC_SIDE_COUNT,
     .desc = {"Choose a ledge. Swap changes sides only", "after you hit or grab the CPU."}, .OnChange = ChangeSide},
    {.kind = OPTKIND_TOGGLE, .name = "CPU 100%",
     .desc = {"Off starts the CPU at 0%. On starts at 100%", "for the native slow ledge options."}, .OnChange = ChangeCPUPercent},
    {.kind = OPTKIND_STRING, .name = "Player Percent", .values = PercentNames, .value_num = countof(PercentNames),
     .desc = {"Choose your starting damage, from 0% to 150%."}, .OnChange = ChangePlayerPercent},
    {.kind = OPTKIND_STRING, .name = "Player Distance", .values = DistanceNames, .value_num = LC_DISTANCE_COUNT,
     .val = 1, .desc = {"Choose how far inside the ledge you start.", "Safe gives the largest spacing toward center."}, .OnChange = ChangeDistance},
    {.kind = OPTKIND_STRING, .name = "CPU Invincibility Overlay", .values = ColorNames, .value_num = TM_CPU_INVINCIBILITY_COUNT,
     .val = TM_CPU_INVINCIBILITY_DEFAULT,
     .desc = {"Choose a color for the CPU's natural", "invincibility and intangibility."}, .OnChange = ChangeColor},
    {.kind = OPTKIND_STRING, .name = "CPU Action", .values = FollowupNames, .value_num = countof(FollowupNames),
     .desc = {"CPU acts on its first legal grounded frame", "after the ledge option and any aerial attack."}, .OnChange = ChangeFollowup},
    {.kind = OPTKIND_STRING, .name = "Aerial Attack When Jumping", .values = AirNames, .value_num = countof(AirNames),
     .desc = {"Attack after ledge-jump recovery or during a ledge hop.",
              "Hop waits until rising safely over stage. Specials vary.",
              "Neutral B repeats; Fox/Falco request two laser loops.",
              "Grounded CPU Action still follows after landing."}, .OnChange = ChangeAirAttack},
    {.kind = OPTKIND_FUNC, .name = "Exit", .desc = {"Save settings and return to Event Select."}, .OnSelect = Exit},
};
static EventOption Options_Chances[] = {
    {.kind = OPTKIND_STRING, .name = "Preset", .values = PresetNames, .value_num = LC_PRESET_COUNT,
     .desc = {"The preset is saved. Manual chance changes",
              "below last only for this session. All Six is equal;",
              "its displayed percentages are rounded. Editing one",
              "switches to the displayed whole-percent chances."}, .OnChange = ChangePreset},
    {.kind = OPTKIND_INT, .name = "Getup Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of a normal ledge getup."}, .OnChange = ChangeGetupChance},
    {.kind = OPTKIND_INT, .name = "Jump Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of the native ledge jump, then aerial action", "and fastfall once the action permits."}, .OnChange = ChangeJumpChance},
    {.kind = OPTKIND_INT, .name = "Roll Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of a ledge roll."}, .OnChange = ChangeRollChance},
    {.kind = OPTKIND_INT, .name = "Attack Chance", .val = 25, .value_num = 101, .format = "%d%%",
     .desc = {"Chance of a ledge getup attack."}, .OnChange = ChangeAttackChance},
    {.kind = OPTKIND_INT, .name = "Ledge Hop Chance", .value_num = 101, .format = "%d%%",
     .desc = {"Drop, double jump inward, then fastfall to stage.",
              "Uses the character's native aerial jump."}, .OnChange = ChangeHopChance},
    {.kind = OPTKIND_INT, .name = "Ledge Dash Chance", .value_num = 101, .format = "%d%%",
     .desc = {"Experimental: drop, double jump, then waveland.",
              "Waits for clearance; does not optimize GALINT.",
              "Needs in-game roster testing. Misses retry normally.",
              "Set to 100% to test the selected character."}, .OnChange = ChangeDashChance},
};
static EventMenu Menu_Main = {.name = "Ledgechase Training", .options = Options_Main, .option_num = countof(Options_Main), .on_exit = Exit};
static EventMenu Menu_Chances = {.name = "Ledge Option Chances", .options = Options_Chances, .option_num = countof(Options_Chances)};
EventMenu *Event_Menu = &Menu_Main;

typedef struct LedgechaseData {
    EventDesc *desc; // Common event prefix.
    int initialized, side, option, act_frame, legal_frames, vulnerable_frames;
    int option_started, elapsed, followup_started[2], punish_timer;
    int reset_timer, success;
    int followup_active[2], followup_pending[2], followup_state[2], followup_action[2];
    Vec3 ledge;
    int ledge_line, inward;
    int return_phase[2], return_jumps[2];
    struct {
        int action, started, pending, state, repeat_tick, loops, last_state;
        float last_frame;
    } air[2];
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
    custom_chances = 0;
    for (int i = 0; i < LC_OPTION_COUNT; ++i)
        Options_Chances[CHANCE_GETUP + i].val = LC_PresetChance(preset, i);
}
static void ReboundChances(int changed) {
    custom_chances = 1;
    int chances[LC_OPTION_COUNT];
    for (int i = 0; i < LC_OPTION_COUNT; ++i) chances[i] = Options_Chances[CHANCE_GETUP + i].val;
    LC_Rebound(chances, changed);
    for (int i = 0; i < LC_OPTION_COUNT; ++i) Options_Chances[CHANCE_GETUP + i].val = chances[i];
}
static void ChangeGetupChance(GOBJ *menu, int value) { ReboundChances(LC_GETUP); }
static void ChangeJumpChance(GOBJ *menu, int value) { ReboundChances(LC_JUMP); }
static void ChangeRollChance(GOBJ *menu, int value) { ReboundChances(LC_ROLL); }
static void ChangeAttackChance(GOBJ *menu, int value) { ReboundChances(LC_ATTACK); }
static void ChangeHopChance(GOBJ *menu, int value) { ReboundChances(LC_HOP); }
static void ChangeDashChance(GOBJ *menu, int value) { ReboundChances(LC_DASH); }

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
    // This field is pending damage to add, not a copy of the current percent.
    ft->dmg.percent_temp = 0;
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
    data->option = !custom_chances && Options_Chances[CHANCE_PRESET].val == LC_PRESET_ALL ?
        HSD_Randi(LC_OPTION_COUNT) : LC_SelectOption(chances, HSD_Randi(100));
    data->act_frame = LC_ActFrame(HSD_Randi(100), HSD_Randi((*stc_ftcommon)->cliff_invuln_time + 5),
        (*stc_ftcommon)->cliff_invuln_time + 1);
    data->legal_frames = data->vulnerable_frames = data->option_started = data->elapsed = 0;
    data->followup_started[0] = data->followup_started[1] = 0;
    for (int ms = 0; ms < 2; ++ms) {
        data->followup_active[ms] = data->followup_pending[ms] = 0;
        data->return_phase[ms] = 0;
        int action = FollowupActions[Options_Main[OPT_FOLLOWUP].val];
        if (action == CPUACT_RANDOM)
            action = CPUCounterActionsGroundRandom[HSD_Randi(countof(CPUCounterActionsGroundRandom))];
        data->followup_action[ms] = action;
        memset(&data->air[ms], 0, sizeof(data->air[ms]));
        int aerial = AirActions[Options_Main[OPT_AIR].val];
        if (aerial == CPUACT_RANDOM) aerial = AirActions[1 + HSD_Randi(countof(AirActions) - 2)];
        data->air[ms].action = aerial;
    }
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
    if ((option == LC_GETUP || option == LC_HOP || option == LC_DASH) && !ft->state_var.state_var3) return;
    if (option == LC_GETUP) QueueInput(cpu, 0, inward * 127, 0);
    else if (option == LC_JUMP) QueueInput(cpu, PAD_BUTTON_X, 0, 0);
    else if (option == LC_ROLL) QueueInput(cpu, PAD_TRIGGER_R, 0, 0);
    else if (option == LC_HOP || option == LC_DASH) QueueInput(cpu, 0, -inward * 127, 0);
    else QueueInput(cpu, PAD_BUTTON_A, 0, 0);
    RunNativeIASA(cpu);
    if (IsLedgeOption(ft->state_id)) NeutralCPU(cpu);
}

enum { LC_RETURN_LEDGE, LC_RETURN_DROP, LC_RETURN_JUMP, LC_RETURN_DODGE, LC_RETURN_LANDED };
// Fox and Falco share the native blaster motion-state IDs.
enum { LC_BLASTER_START = 344, LC_BLASTER_LOOP, LC_BLASTER_END };
static int DelayLaserFastfall(LedgechaseData *data, FighterData *ft) {
    int ms = ft->flags.ms;
    return (data->option == LC_JUMP || data->option == LC_HOP) &&
        data->air[ms].action == CPUACT_NEUTRALB && (ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO) &&
        (!data->air[ms].started || (ft->state_id != LC_BLASTER_END && data->air[ms].loops < 2) ||
         ft->state_id == LC_BLASTER_LOOP);
}
/* Observe native transitions, including ones consumed between event callbacks.
 * Never change velocity, collision, jump count, or protection to make a return
 * succeed. Each Ice Climber owns a separate sequence. */
static void RunLedgeReturn(GOBJ *cpu, LedgechaseData *data) {
    FighterData *ft = cpu->userdata;
    int *phase = &data->return_phase[ft->flags.ms];
    if (ft->state_id == ASID_CLIFFWAIT && *phase == LC_RETURN_LEDGE) {
        RunLedgeOption(cpu, data->option, data->inward);
        if (ft->state_id == ASID_FALL) {
            *phase = LC_RETURN_DROP;
            data->return_jumps[ft->flags.ms] = ft->jump.jumps_used;
            NeutralCPU(cpu); // Do not keep the outward release input for physics.
        }
        return; // Drop and jump must be separate simulation frames.
    }
    if (!ft->phys.air_state) {
        *phase = LC_RETURN_LANDED;
        return;
    }
    if (*phase == LC_RETURN_DROP) {
        if (ft->jump.jumps_used <= data->return_jumps[ft->flags.ms] &&
            ft->state_id == ASID_FALL && CheckHasJump(cpu)) {
            QueueInput(cpu, PAD_BUTTON_X, data->inward * 127, 0);
            RunNativeIASA(cpu);
        }
        // Multijump characters use character-specific action states. Observe
        // the consumed jump count instead of assuming JumpAerialF/B IDs.
        if (ft->jump.jumps_used > data->return_jumps[ft->flags.ms]) {
            *phase = LC_RETURN_JUMP;
            // Release immediately after acceptance. Held jump can float with
            // Peach or request another Kirby/Puff jump; animation-driven
            // double jumps do not need the button held to keep their rise.
            QueueInput(cpu, 0, data->inward * 127, 0);
        }
        return;
    }
    if (*phase != LC_RETURN_JUMP) return;
    if (ft->state_id == ASID_CLIFFCATCH || ft->state_id == ASID_CLIFFWAIT) return;
    if (ft->state_id == ASID_ESCAPEAIR) {
        *phase = LC_RETURN_DODGE;
        return;
    }
    float inside = (ft->phys.pos.X - data->ledge.X) * data->inward;
    float height = ft->phys.pos.Y - data->ledge.Y;
    if (data->option == LC_DASH && LC_CanWaveland(inside, height, height + ft->coll_data.ecbCurr_bot.Y)) {
        QueueInput(cpu, PAD_TRIGGER_R, data->inward * 90, -90);
        RunNativeIASA(cpu);
        if (ft->state_id == ASID_ESCAPEAIR) {
            *phase = LC_RETURN_DODGE;
            NeutralCPU(cpu);
        }
    } else {
        QueueInput(cpu, 0, inside < 12 ? data->inward * 127 : 0, 0);
        if (data->option == LC_HOP && ft->phys.self_vel.Y < 0 && inside >= 3 && !DelayLaserFastfall(data, ft)) {
            QueueInput(cpu, 0, inside < 12 ? data->inward * 127 : 0, -127);
            Fighter_PhysAir_CheckFastfall(ft);
        }
    }
}
static int GroundReady(FighterData *ft) {
    return ft->phys.air_state == 0 && (ft->state_id == ASID_WAIT ||
        (ft->state_id == ASID_LANDING && ft->state.frame >= ft->attr.normal_landing_lag));
}
static bool CheckHasJump(GOBJ *cpu) {
    FighterData *ft = cpu->userdata;
    return ft->jump.jumps_used < ft->attr.max_jumps;
}
static int FollowupMatches(FighterData *ft, int state) {
    int current = ft->state_id;
    if (state == ASID_ACTIONABLEGROUND || (state == ASID_ACTIONABLE && !ft->phys.air_state))
        return !ft->phys.air_state && (GroundReady(ft) || current == ASID_WALKSLOW ||
            current == ASID_WALKMIDDLE || current == ASID_WALKFAST || current == ASID_RUN ||
            current == ASID_SQUATWAIT || current == ASID_OTTOTTOWAIT ||
            current == ASID_GUARD || current == ASID_GUARDREFLECT);
    if (state == ASID_ACTIONABLEAIR || state == ASID_ACTIONABLE)
        return ft->phys.air_state && (current == ASID_JUMPF || current == ASID_JUMPB ||
            current == ASID_JUMPAERIALF || current == ASID_JUMPAERIALB ||
            current == ASID_FALL || current == ASID_FALLAERIALF || current == ASID_FALLAERIALB);
    if (state == ASID_ACTIONABLESHIELD)
        return current == ASID_GUARDON || current == ASID_GUARD || current == ASID_GUARDREFLECT ||
            (ft->kind == FTKIND_YOSHI && ft->flags.shield_enable);
    return current == state;
}
static int RunFollowup(GOBJ *cpu, GOBJ *human, int action, int inward) {
    LedgechaseData *data = Data();
    FighterData *ft = cpu->userdata, *hmn = human->userdata;
    int ms = ft->flags.ms;
    // A terminal input can be consumed by the native IASA between event ticks.
    if (data->followup_pending[ms] && ft->state_id != data->followup_state[ms]) {
        NeutralCPU(cpu);
        return 1;
    }
    if (!data->followup_active[ms]) {
        if (!FollowupMatches(ft, ASID_ACTIONABLEGROUND) && ft->state_id != ASID_GUARDON) return 0;
        data->followup_active[ms] = 1;
    }
    if (action == CPUACT_NONE) return 1;
    CPUAction *commands = Lab_CPUActions[action];
    for (CPUAction *command = commands; command->state != 0xFFFF; ++command) {
        if (!FollowupMatches(ft, command->state) || ft->state.frame < command->frameLow ||
            (command->custom_check && !command->custom_check(cpu))) continue;
        int toward = hmn->phys.pos.X > ft->phys.pos.X ? 1 : hmn->phys.pos.X < ft->phys.pos.X ? -1 : inward;
        int dir = 1;
        switch (command->stickDir) {
            case STCKDIR_TOWARD: dir = toward; break;
            case STCKDIR_AWAY: dir = -toward; break;
            case STCKDIR_FRONT: dir = ft->facing_direction; break;
            case STCKDIR_BACK: dir = -ft->facing_direction; break;
            case STICKDIR_RDM: dir = HSD_Randi(2) ? -1 : 1; break;
        }
        QueueInput(cpu, command->input, dir * command->stickX, command->stickY);
        ft->cpu.cstickX = dir * command->cstickX;
        ft->cpu.cstickY = command->cstickY;
        ft->input.cstick = (Vec2){ft->cpu.cstickX / 127.f, ft->cpu.cstickY / 127.f};
        int state = ft->state_id;
        RunNativeIASA(cpu);
        // Some sequences finish by observing a state (for example Spotdodge).
        for (CPUAction *end = commands; end->state != 0xFFFF; ++end)
            if (end->isLast && !end->input && !end->stickX && !end->stickY &&
                !end->cstickX && !end->cstickY && FollowupMatches(ft, end->state) &&
                ft->state.frame >= end->frameLow) {
                NeutralCPU(cpu);
                return 1;
            }
        if (command->isLast) {
            if (ft->state_id != state || (!command->input && !command->stickX &&
                !command->stickY && !command->cstickX && !command->cstickY) ||
                (state == ASID_DASH && !command->input && !command->cstickX && !command->cstickY)) {
                NeutralCPU(cpu);
                return 1;
            }
            data->followup_pending[ms] = 1;
            data->followup_state[ms] = state;
        }
        return 0;
    }
    return 0;
}
static int AirReady(FighterData *ft) {
    return FollowupMatches(ft, ASID_ACTIONABLEAIR) || (ft->phys.air_state && ft->flags.is_multijump &&
        ft->state_id >= 341 && ft->state_id <= 345);
}
static void RunAirAttack(GOBJ *cpu, GOBJ *human, LedgechaseData *data) {
    FighterData *ft = cpu->userdata, *hmn = human->userdata;
    int ms = ft->flags.ms, action = data->air[ms].action;
    if ((data->option != LC_JUMP && data->option != LC_HOP) || !ft->phys.air_state ||
        data->followup_active[ms] || action == CPUACT_NONE || IsLedgeOption(ft->state_id) ||
        ft->state_id == ASID_CLIFFWAIT || ft->state_id == ASID_CLIFFCATCH) return;
    int blaster = (ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO) && action == CPUACT_NEUTRALB;
    if (data->air[ms].pending && ft->state_id != data->air[ms].state && !AirReady(ft)) {
        data->air[ms].started = 1;
        data->air[ms].pending = 0;
    }
    if (blaster) {
        if (ft->state_id == LC_BLASTER_LOOP && (data->air[ms].last_state != LC_BLASTER_LOOP ||
            ft->state.frame < data->air[ms].last_frame)) ++data->air[ms].loops;
        data->air[ms].last_state = ft->state_id;
        data->air[ms].last_frame = ft->state.frame;
    }
    if (data->air[ms].started) {
        if (action != CPUACT_NEUTRALB) return;
        // Native blaster IASA accepts fresh B presses during its script's loop
        // window. Feed pulses throughout start/first loop; release in the
        // second loop so the native animation ends after two shots.
        if (blaster && (data->air[ms].loops >= 2 || ft->state_id == LC_BLASTER_END)) return;
        if (!blaster || ft->state_id == LC_BLASTER_START || ft->state_id == LC_BLASTER_LOOP || AirReady(ft)) {
            int press = !(data->air[ms].repeat_tick++ & 1);
            QueueInput(cpu, press ? PAD_BUTTON_B : 0, data->inward * 127, 0);
            // A fresh neutral special must keep a neutral stick for selection.
            if (AirReady(ft)) QueueInput(cpu, press ? PAD_BUTTON_B : 0, 0, 0);
            if (press) RunNativeIASA(cpu);
        }
        return;
    }
    if (!AirReady(ft)) return;
    float inside = (ft->phys.pos.X - data->ledge.X) * data->inward;
    float height = ft->phys.pos.Y - data->ledge.Y;
    // Cancelling an animation-driven jump below the lip can kill its rise.
    // Start hop attacks only when rising with the origin and ECB over stage.
    if (inside < 1 || height < 0 || height + ft->coll_data.ecbCurr_bot.Y < 0) return;
    if (data->option == LC_HOP && (data->return_phase[ms] != LC_RETURN_JUMP || height < 4 || ft->phys.self_vel.Y <= 0)) return;
    for (CPUAction *command = Lab_CPUActions[action]; command->state != 0xFFFF; ++command) {
        if (command->state != ASID_ACTIONABLEAIR && command->state != ASID_ACTIONABLE) continue;
        int toward = hmn->phys.pos.X > ft->phys.pos.X ? 1 : hmn->phys.pos.X < ft->phys.pos.X ? -1 : data->inward;
        int dir = command->stickDir == STCKDIR_TOWARD ? toward : command->stickDir == STCKDIR_AWAY ? -toward :
            command->stickDir == STCKDIR_FRONT ? ft->facing_direction : command->stickDir == STCKDIR_BACK ? -ft->facing_direction : 1;
        QueueInput(cpu, command->input, dir * command->stickX, command->stickY);
        ft->cpu.cstickX = dir * command->cstickX;
        ft->cpu.cstickY = command->cstickY;
        ft->input.cstick = (Vec2){ft->cpu.cstickX / 127.f, ft->cpu.cstickY / 127.f};
        int state = ft->state_id;
        RunNativeIASA(cpu);
        if (ft->state_id != state) {
            data->air[ms].started = 1;
            data->air[ms].pending = 0;
            QueueInput(cpu, 0, data->inward * 127, 0);
        } else {
            data->air[ms].pending = 1;
            data->air[ms].state = state;
        }
        return;
    }
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
        TM_LEDGECHASE_PLAYER_PERCENT, TM_LEDGECHASE_DISTANCE, TM_LEDGECHASE_CPU_COLOR, TM_LEDGECHASE_CPU_ACTION, TM_LEDGECHASE_AIR_ATTACK};
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
            int manual_return = data->option == LC_HOP || data->option == LC_DASH;
            int legal = (data->option != LC_GETUP && !manual_return) || ft->state_var.state_var3;
            if (legal && LC_ReadyToAct(&data->legal_frames, &data->vulnerable_frames,
                                     event_vars->is_protected(cpu), data->act_frame)) {
                for (int ms = 0; ms < 2; ++ms) {
                    GOBJ *fighter = FighterBody(1, ms);
                    if (fighter) {
                        if (manual_return) RunLedgeReturn(fighter, data);
                        else RunLedgeOption(fighter, data->option, data->inward);
                    }
                }
            }
        }
        if (IsLedgeOption(ft->state_id) || data->return_phase[0] != LC_RETURN_LEDGE) data->option_started = 1;
    } else {
        int all_finished = 1;
        for (int ms = 0; ms < 2; ++ms) {
            GOBJ *fighter = FighterBody(1, ms);
            if (!fighter) continue;
            FighterData *sub = fighter->userdata;
            if ((data->option == LC_HOP || data->option == LC_DASH) && !data->followup_active[ms])
                RunLedgeReturn(fighter, data);
            else if (sub->state_id == ASID_CLIFFWAIT) RunLedgeOption(fighter, data->option, data->inward);
            if (data->option == LC_JUMP && !data->followup_active[ms] && !DelayLaserFastfall(data, sub)) FastfallJump(fighter);
            RunAirAttack(fighter, human, data);
            if (!data->followup_started[ms])
                data->followup_started[ms] = RunFollowup(fighter, human, data->followup_action[ms], data->inward);
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
static void ChangeAirAttack(GOBJ *menu, int value) { ChangePreference(TM_LEDGECHASE_AIR_ATTACK, value, 1); }
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
