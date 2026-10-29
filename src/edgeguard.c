#include "edgeguard.h"
#include "recovery.c"

// how far inland the cpu starts, and the hmn with Player Position on Close
#define DISTANCE_FROM_LEDGE 15
// how far inland the hmn starts with Player Position on At Ledge
#define AT_LEDGE_DISTANCE 5

// how many frames it takes to reset after the cpu dies or successfully recovers
#define RESET_DELAY 30

static int reset_timer = -1;
static int edgeguard_timer = 0;
static u32 saved_recovery = 0;
EventMenu *Event_Menu = &Menu_Main;

// Saved settings ---------------------------------------

/* Saved field for each Custom Hit Strength row, in row order. */
static const u8 CustomHitStrength_Prefs[OPT_CUSTOMHIT_COUNT] = {
    TM_EG_KB_MIN, TM_EG_KB_MAX, TM_EG_ANGLE_MIN, TM_EG_ANGLE_MAX,
    TM_EG_CPU_PERCENT_MIN, TM_EG_CPU_PERCENT_MAX,
};

static int RecoveryPref(EventMenu *menu) {
    if (menu == &RecMenu_Fox) return TM_EG_RECOVERY_FOX;
    if (menu == &RecMenu_Falco) return TM_EG_RECOVERY_FALCO;
    if (menu == &RecMenu_Sheik) return TM_EG_RECOVERY_SHEIK;
    if (menu == &RecMenu_Falcon) return TM_EG_RECOVERY_FALCON;
    if (menu == &RecMenu_Marth) return TM_EG_RECOVERY_MARTH;
    if (menu == &RecMenu_Yoshi) return TM_EG_RECOVERY_YOSHI;
    if (menu == &RecMenu_Peach) return TM_EG_RECOVERY_PEACH;
    if (menu == &RecMenu_Luigi) return TM_EG_RECOVERY_LUIGI;
    if (menu == &RecMenu_DrMario) return TM_EG_RECOVERY_DRMARIO;
    if (menu == &RecMenu_IceClimbers) return TM_EG_RECOVERY_ICECLIMBERS;
    if (menu == &RecMenu_Puff) return TM_EG_RECOVERY_PUFF;
    if (menu == &RecMenu_Samus) return TM_EG_RECOVERY_SAMUS;
    return -1;
}

static void SaveSettings(void) {
    TM_SetSetting(TM_SETTING_EDGEGUARD, TM_EG_PLAYER_PERCENT, Options_Main[OPT_MAIN_PERCENT].val);
    TM_SetSetting(TM_SETTING_EDGEGUARD, TM_EG_PLAYER_POSITION, Options_Main[OPT_MAIN_POSITION].val);
    TM_SetSetting(TM_SETTING_EDGEGUARD, TM_EG_HIT_STRENGTH, Options_Main[OPT_MAIN_HITSTRENGTH].val);
    for (int i = 0; i < OPT_CUSTOMHIT_COUNT; ++i)
        TM_SetSetting(TM_SETTING_EDGEGUARD, CustomHitStrength_Prefs[i], Options_CustomHitStrength[i].val);
}

static void LoadCustomKBRange(void) {
    HitStrength_KBRange[KBVALS_CUSTOM].mag_min = Options_CustomHitStrength[OPT_CUSTOMHIT_MAGMIN].val;
    HitStrength_KBRange[KBVALS_CUSTOM].mag_max = Options_CustomHitStrength[OPT_CUSTOMHIT_MAGMAX].val;
    HitStrength_KBRange[KBVALS_CUSTOM].ang_min = Options_CustomHitStrength[OPT_CUSTOMHIT_ANGMIN].val;
    HitStrength_KBRange[KBVALS_CUSTOM].ang_max = Options_CustomHitStrength[OPT_CUSTOMHIT_ANGMAX].val;
    HitStrength_KBRange[KBVALS_CUSTOM].dmg_min = Options_CustomHitStrength[OPT_CUSTOMHIT_DMGMIN].val;
    HitStrength_KBRange[KBVALS_CUSTOM].dmg_max = Options_CustomHitStrength[OPT_CUSTOMHIT_DMGMAX].val;
}

static void LoadSettings(EventMenu *recovery_menu) {
    Options_Main[OPT_MAIN_PERCENT].val = TM_GetSetting(TM_SETTING_EDGEGUARD, TM_EG_PLAYER_PERCENT);
    Options_Main[OPT_MAIN_POSITION].val = TM_GetSetting(TM_SETTING_EDGEGUARD, TM_EG_PLAYER_POSITION);
    Options_Main[OPT_MAIN_HITSTRENGTH].val = TM_GetSetting(TM_SETTING_EDGEGUARD, TM_EG_HIT_STRENGTH);
    for (int i = 0; i < OPT_CUSTOMHIT_COUNT; ++i)
        Options_CustomHitStrength[i].val = TM_GetSetting(TM_SETTING_EDGEGUARD, CustomHitStrength_Prefs[i]);
    LoadCustomKBRange();
    Options_Main[OPT_MAIN_BLAST_ZONES].val = Options_Main[OPT_MAIN_BLAST_ZONES].val_prev =
        !!TM_GetSetting(TM_SETTING_FLAG, TM_FLAG_BLAST_ZONES);

    // bit N of the saved mask is row N of this CPU's recovery menu
    int pref = RecoveryPref(recovery_menu);
    if (pref < 0)
        return;
    u32 mask = TM_GetSetting(TM_SETTING_EDGEGUARD, pref);
    for (int opt = 0; opt < recovery_menu->option_num; ++opt)
        recovery_menu->options[opt].val = (mask >> opt) & 1;
}

/* The recovery toggles are shared with the Lab, so they have no OnChange.
 * Save them whenever the CPU's flags change instead. */
static void SaveRecovery(GOBJ *cpu, u32 opt_flags) {
    if (opt_flags == saved_recovery)
        return;
    saved_recovery = opt_flags;
    FighterData *cpu_data = cpu->userdata;
    int pref = RecoveryPref(RecoveryMenus[cpu_data->kind]);
    if (pref >= 0)
        TM_SetSetting(TM_SETTING_EDGEGUARD, pref, opt_flags);
}

/* Toggles changed in the pause menu before any Think frame. */
static void FlushRecovery(void) {
    GOBJ *cpu = Fighter_GetGObj(1);
    SaveRecovery(cpu, RecoveryMenuOptFlags(cpu));
}

// Event ------------------------------------------------

static void UpdatePosition(GOBJ *fighter) {
    FighterData *data = fighter->userdata;

    Vec3 pos = data->phys.pos;
    data->coll_data.topN_Curr = pos;
    data->coll_data.topN_CurrCorrect = pos;
    data->coll_data.topN_Prev = pos;
    data->coll_data.topN_Proj = pos;
    data->coll_data.coll_test = *stc_colltest;
}

static void UpdateCameraBox(GOBJ *fighter) {
    Fighter_UpdateCameraBox(fighter);

    FighterData *data = fighter->userdata;
    CmSubject *subject = data->camera_subject;
    subject->boundleft_curr = subject->boundleft_proj;
    subject->boundright_curr = subject->boundright_proj;

    Match_CorrectCamera();
}

static bool IsGroundActionable(GOBJ *fighter) {
    FighterData *data = fighter->userdata;

    // ensure grounded
    if (data->phys.air_state == 1)
        return false;

    int state = data->state_id;

    if (InHitstunAnim(state) && HitstunEnded(fighter))
        return true;

    if (state == ASID_LANDING && data->state.frame >= data->attr.normal_landing_lag)
        return true;
        
    return state == ASID_WAIT
        || state == ASID_WALKSLOW
        || state == ASID_WALKMIDDLE
        || state == ASID_WALKFAST
        || state == ASID_RUN
        || state == ASID_SQUATWAIT
        || state == ASID_OTTOTTOWAIT
        || state == ASID_GUARD;
}

static void Exit(GOBJ *menu) {
    FlushRecovery();
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

static void Reset_Subchar(GOBJ *sub, GOBJ *main) {
    // Every subchar has a force that attracts them to the main char's position a few frames ago.
    // When the main character teleports during a reset,
    // we need to reset the past position data of the subchar so that 
    // they aren't attracted to the main char's previous position across the stage.

    FighterData *main_data = main->userdata;
    FighterData *sub_data = sub->userdata;
    Vec3 pos = main_data->phys.pos;
    float facing = main_data->facing_direction;
    for (u32 i = 0; i < countof(sub_data->cpu.leader_log); ++i) {
        CPULeaderLog *log = &sub_data->cpu.leader_log[i];
        log->pos = pos;
        log->facing_direction = facing;
    }
}

/* How far inland the hmn starts. Halfway and Mid Stage scale with the stage:
 * half of, and all of, the distance from this ledge to the stage's center. */
static float PlayerDistance(int side_idx) {
    int position = Options_Main[OPT_MAIN_POSITION].val;
    if (position == TM_EG_POSITION_AT_LEDGE)
        return AT_LEDGE_DISTANCE;
    if (position != TM_EG_POSITION_HALFWAY && position != TM_EG_POSITION_MID_STAGE)
        return DISTANCE_FROM_LEDGE;
    float center = (ledge_positions[0].X + ledge_positions[1].X) * 0.5f;
    float distance = fabs(ledge_positions[side_idx].X - center);
    if (position == TM_EG_POSITION_HALFWAY)
        distance *= 0.5f;
    return distance > DISTANCE_FROM_LEDGE ? distance : DISTANCE_FROM_LEDGE;
}

/* The bair starts 5 units above the ground, or above y = 0 like before when
 * the ground is lower. Mid stage can be higher than the ledges on some stages. */
static float PlayerHeight(float x, int side_idx) {
    Vec3 ground, slope;
    int line, kind;
    float y = ledge_positions[side_idx].Y;
    if (GrColl_RaycastGround(&ground, &line, &kind, &slope, -1, -1, -1, 0, x, y + 10, x, y - 20, 0)
        && ground.Y > 0.f)
        return ground.Y + 5.f;
    return 5.f;
}

static void Reset_HMN(GOBJ *hmn, int side_idx, int dmg, float distance) {
    FighterData *hmn_data = hmn->userdata;

    int side = side_idx * 2 - 1;
    hmn_data->facing_direction = -side;
    float ledge_x = ledge_positions[side_idx].X - distance * side;

    // set phys
    hmn_data->phys.kb_vel.X = 0.f;
    hmn_data->phys.kb_vel.Y = 0.f;
    hmn_data->phys.self_vel.X = 0.f;
    hmn_data->phys.self_vel.Y = 0.f;
    hmn_data->phys.pos.X = ledge_x;
    hmn_data->phys.pos.Y = PlayerHeight(ledge_x, side_idx);
    UpdatePosition(hmn);
    
    hmn_data->jump.jumps_used = 1;
    
    // set hmn action state
    Fighter_EnterAerial(hmn, ASID_ATTACKAIRB);
    Fighter_ApplyAnimation(hmn, 7, 1, 0);
    hmn_data->state.frame = 7;
    hmn_data->script.script_event_timer = 0;
    Fighter_SubactionFastForward(hmn);

    Fighter_UpdateStateFrameInfo(hmn);
    Fighter_HitboxDisableAll(hmn);
    hmn_data->script.script_current = 0;
    
    // fix camera
    UpdateCameraBox(hmn);

    // give hitlag
    hmn_data->dmg.hitlag_frames = 7;
    hmn_data->flags.hitlag = 1;
    hmn_data->flags.hitlag_unk = 1;
    
    // percent
    hmn_data->dmg.percent = dmg;
    Fighter_SetHUDDamage(hmn_data->ply, dmg);
}

static void Reset_CPU(GOBJ *cpu, int side_idx, int dmg, float kb_mag, float kb_angle, float distance) {
    FighterData *cpu_data = cpu->userdata;
    // Nana keeps the game's own AI, which follows Popo's inputs.
    if (!cpu_data->flags.ms)
        cpu_data->cpu.ai = 15;

    int side = side_idx * 2 - 1;
    cpu_data->facing_direction = -side;
    float ledge_x = ledge_positions[side_idx].X - distance * side;
    
    // set phys
    cpu_data->phys.kb_vel.X = 0.f;
    cpu_data->phys.kb_vel.Y = 0.f;
    cpu_data->phys.self_vel.X = 0.f;
    cpu_data->phys.self_vel.Y = 0.f;
    cpu_data->phys.pos.X = ledge_x;
    cpu_data->phys.pos.Y = 0.f;
    UpdatePosition(cpu);
    
    cpu_data->jump.jumps_used = 1;
    
    // set cpu action state
    int state = kb_mag > 130.f ? ASID_DAMAGEFLYHI : ASID_DAMAGEFLYN;
    Fighter_EnterFall(cpu);
    ActionStateChange(0, 1, 0, cpu, state, 0x40, 0);
    Fighter_UpdateStateFrameInfo(cpu);
    
    // fix camera
    UpdateCameraBox(cpu);
    
    // give cpu knockback
    float angle_rad = kb_angle * M_1DEGREE;

    float vel = kb_mag * (*stc_ftcommon)->force_applied_to_kb_mag_multiplier;
    float vel_x = cos(angle_rad) * vel * (float)side;
    float vel_y = sin(angle_rad) * vel;
    cpu_data->phys.kb_vel.X = vel_x;
    cpu_data->phys.kb_vel.Y = vel_y;

    float kb_frames = (float)(int)((*stc_ftcommon)->x154 * kb_mag);
    memcpy(&cpu_data->state_var.state_var1, &kb_frames, sizeof(float));
    cpu_data->flags.hitstun = 1;
    Fighter_EnableCollUpdate(cpu_data);

    // give hitlag
    cpu_data->dmg.hitlag_frames = 7;
    cpu_data->flags.hitlag = 1;
    cpu_data->flags.hitlag_unk = 1;
    
    // percent
    cpu_data->dmg.percent = dmg;
    Fighter_SetHUDDamage(cpu_data->ply, dmg);
}

static void Reset(void) {
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    edgeguard_timer = 0;

    for (int ply = 0; ply < 2; ++ply) {
        MatchHUDElement *hud = &stc_matchhud->element_data[ply];
        if (hud->is_removed == 1)
            Match_CreateHUD(ply);
    }

    int side_idx = HSD_Randi(2);
    KBValues vals = HitStrength_KBRange[Options_Main[OPT_MAIN_HITSTRENGTH].val];
    float kb_mag = vals.mag_min + (vals.mag_max - vals.mag_min) * HSD_Randf();
    float kb_angle = vals.ang_min + (vals.ang_max - vals.ang_min) * HSD_Randf();
    int hmn_dmg = Options_Main[OPT_MAIN_PERCENT].val;
    int cpu_dmg = vals.dmg_min + HSD_Randi(vals.dmg_max - vals.dmg_min);

    // The cpu keeps its usual launch spot, unless the hmn starts closer to the
    // ledge: then the cpu is launched from the hmn's spot, so the bair still
    // sends it out from in front of the hmn.
    float hmn_distance = PlayerDistance(side_idx);
    float cpu_distance = hmn_distance < DISTANCE_FROM_LEDGE ? hmn_distance : DISTANCE_FROM_LEDGE;

    GOBJ *hmn = Fighter_GetSubcharGObj(0, 0);
    GOBJ *cpu = Fighter_GetSubcharGObj(1, 0);
    Reset_HMN(hmn, side_idx, hmn_dmg, hmn_distance);
    Reset_CPU(cpu, side_idx, cpu_dmg, kb_mag, kb_angle, cpu_distance);
    
    GOBJ *hmn_sub = Fighter_GetSubcharGObj(0, 1);
    GOBJ *cpu_sub = Fighter_GetSubcharGObj(1, 1);
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;
    if (hmn_sub && hmn_data->flags.has_follower) {
        Reset_HMN(hmn_sub, side_idx, hmn_dmg, hmn_distance);
        Reset_Subchar(hmn_sub, hmn);
    }
    if (cpu_sub && cpu_data->flags.has_follower) {
        Reset_CPU(cpu_sub, side_idx, cpu_dmg, kb_mag, kb_angle, cpu_distance);
        Reset_Subchar(cpu_sub, cpu);
    }
}

static void ChangePlayerPercent(GOBJ *menu_gobj, int dmg) {
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;

    hmn_data->dmg.percent = dmg;
    Fighter_SetHUDDamage(hmn_data->ply, dmg);
    SaveSettings();
}

static void ChangePlayerPosition(GOBJ *menu_gobj, int value) {
    SaveSettings();
}

static void ChangeHitStrength(GOBJ *menu_gobj, int value) {
    SaveSettings();
}

static void ChangeBlastZones(GOBJ *menu_gobj, int value) {
    TM_SetSetting(TM_SETTING_FLAG, TM_FLAG_BLAST_ZONES, value);
}

static void WriteCustomKBValues(void) {
    LoadCustomKBRange();
    Options_Main[OPT_MAIN_HITSTRENGTH].val = KBVALS_CUSTOM;
    SaveSettings();
}

static void ChangeCustomKB_MagMin(GOBJ *menu_gobj, int value) {
    s16 *min = &Options_CustomHitStrength[OPT_CUSTOMHIT_MAGMIN].val;
    s16 *max = &Options_CustomHitStrength[OPT_CUSTOMHIT_MAGMAX].val;
    if (*min > *max) *max = *min;
    WriteCustomKBValues();
}
static void ChangeCustomKB_MagMax(GOBJ *menu_gobj, int value) {
    s16 *min = &Options_CustomHitStrength[OPT_CUSTOMHIT_MAGMIN].val;
    s16 *max = &Options_CustomHitStrength[OPT_CUSTOMHIT_MAGMAX].val;
    if (*min > *max) *min = *max;
    WriteCustomKBValues();
}
static void ChangeCustomKB_AngMin(GOBJ *menu_gobj, int value) {
    s16 *min = &Options_CustomHitStrength[OPT_CUSTOMHIT_ANGMIN].val;
    s16 *max = &Options_CustomHitStrength[OPT_CUSTOMHIT_ANGMAX].val;
    if (*min > *max) *max = *min;
    WriteCustomKBValues();
}
static void ChangeCustomKB_AngMax(GOBJ *menu_gobj, int value) {
    s16 *min = &Options_CustomHitStrength[OPT_CUSTOMHIT_ANGMIN].val;
    s16 *max = &Options_CustomHitStrength[OPT_CUSTOMHIT_ANGMAX].val;
    if (*min > *max) *min = *max;
    WriteCustomKBValues();
}
static void ChangeCustomKB_DmgMin(GOBJ *menu_gobj, int value) {
    s16 *min = &Options_CustomHitStrength[OPT_CUSTOMHIT_DMGMIN].val;
    s16 *max = &Options_CustomHitStrength[OPT_CUSTOMHIT_DMGMAX].val;
    if (*min > *max) *max = *min;
    WriteCustomKBValues();
}
static void ChangeCustomKB_DmgMax(GOBJ *menu_gobj, int value) {
    s16 *min = &Options_CustomHitStrength[OPT_CUSTOMHIT_DMGMIN].val;
    s16 *max = &Options_CustomHitStrength[OPT_CUSTOMHIT_DMGMAX].val;
    if (*min > *max) *min = *max;
    WriteCustomKBValues();
}

void Event_Init(GOBJ *gobj) {
    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;
    
    // change team - homing missiles don't move towards characters on same team
    hmn_data->team = 0;
    cpu_data->team = 1;

    EventMenu *recovery_menu = RecoveryMenus[cpu_data->kind];
    if (recovery_menu == 0)
        assert("unimplemented character in edgeguard training");
    Options_Main[OPT_MAIN_RECOVERY].menu = recovery_menu;
    LoadSettings(recovery_menu);
    saved_recovery = RecoveryMenuOptFlags(cpu);

    FighterSwap_Restrict(1, Edgeguard_CPUKinds, countof(Edgeguard_CPUKinds));
    FighterSwap_Init();
    StageSwap_Init();

    GetLedgePositions(ledge_positions);
}

/* The restart saves the card. */
static void FighterSwap_BeforeRestart(void) { FlushRecovery(); }
static void StageSwap_BeforeRestart(void) { FlushRecovery(); }

void Event_Think(GOBJ *menu) {
    // For some reason, Savestate_Save crashes in Event_Init.
    // So we need to init on first think frame.
    if (event_vars->game_timer == 1) {
        event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
        Reset();
    }
    
    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;

    if (reset_timer > 0) 
        reset_timer--;

    if (reset_timer == 0) {
        reset_timer = -1;
        Reset();
    }
    
    if (edgeguard_timer < 10) {
        // Ensure the player L-cancels the initial bair.
        //
        // We can't set this in reset because most characters need to L-Cancel later than the reset frame.
        // Not really a good solution other than a timer.
        hmn_data->flags.is_fastfall = 1;
        hmn_data->input.timer_trigger_any_ignore_hitlag = 0;
    }
    edgeguard_timer++;
    
    Vec2 cpu_pos = { cpu_data->phys.pos.X, cpu_data->phys.pos.Y };
    
    if (
        reset_timer == -1
        && (
            cpu_data->flags.dead
            || hmn_data->flags.dead
            || cpu_data->state_id == ASID_CLIFFCATCH
            || cpu_data->state_id == ASID_DOWNWAITU
            || cpu_data->state_id == ASID_DOWNWAITD
            || IsGroundActionable(cpu)
            
            // if the CPU is actionable high above the stage, just reset early.
            || (cpu_pos.Y > 100.f 
                && fabs(cpu_pos.X) < ledge_positions[1].X 
                && IsAirActionable(cpu))
        )
    ) {
        reset_timer = RESET_DELAY;
    }
    
    HSD_Pad *pad = PadGetMaster(hmn_data->pad_index);
    if (pad->down & HSD_BUTTON_DPAD_LEFT)
        reset_timer = 0;
        
    u32 opt_flags = RecoveryMenuOptFlags(cpu);
    SaveRecovery(cpu, opt_flags);
    Recovery_Think(cpu, opt_flags);
}
