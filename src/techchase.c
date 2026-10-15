#include "../MexTK/mex.h"
#include "events.h"
#include "techchase_logic.h"

static EventMenu Menu_Main;
static EventMenu Menu_Chances;

static Vec3 saved_cam_pos;
static Vec3 saved_cam_rot;

static void Exit(GOBJ *menu);
static void StartMoveCPU(GOBJ *menu);
static void FinishMoveCPU(GOBJ *menu);
static void StartMoveHMN(GOBJ *menu);
static void FinishMoveHMN(GOBJ *menu);
static void StartMoveCam(GOBJ *menu);
static void FinishMoveCam(GOBJ *menu);
static void ChangeCam(GOBJ *menu_gobj, int _new_val);
static void ChangePreset(GOBJ *menu_gobj, int value);
static void ChangeLocation(GOBJ *menu_gobj, int value);
static void ChangeHeight(GOBJ *menu_gobj, int value);
static void ChangeDistance(GOBJ *menu_gobj, int value);
static void ChangeReactionOSD(GOBJ *menu_gobj, int value);
static void ChangeTimingOSD(GOBJ *menu_gobj, int value);
static void ChangeReset(GOBJ *menu_gobj, int value);
static void ChangeInvisibility(GOBJ *menu_gobj, int value);
static void ChangeSpotdodge(GOBJ *menu_gobj, int value);
static void ChangeCPUColor(GOBJ *menu_gobj, int value);
static void ChangeTechInPlaceChance (GOBJ *menu_gobj, int _new_val);
static void ChangeTechAwayChance    (GOBJ *menu_gobj, int _new_val);
static void ChangeTechTowardChance  (GOBJ *menu_gobj, int _new_val);
static void ChangeMissTechChance    (GOBJ *menu_gobj, int _new_val);
static void ChangeStandChance       (GOBJ *menu_gobj, int _new_val);
static void ChangeRollAwayChance    (GOBJ *menu_gobj, int _new_val);
static void ChangeRollTowardChance  (GOBJ *menu_gobj, int _new_val);
static void ChangeGetupAttackChance (GOBJ *menu_gobj, int _new_val);

static const char *Options_Invis[] = { "None", "After Reaction", "Flash Reaction" };
enum {
    OPTINVIS_NONE,
    OPTINVIS_POST_REACTION,
    OPTINVIS_FLASH,

    OPTINVIS_COUNT
};

static const EventOption Option_MoveCPU = {
    .kind = OPTKIND_FUNC,
    .name = "Move CPU",
    .desc = {"Manually set the CPU's position.", "Press A to change direction."},
    .OnSelect = StartMoveCPU,
};

static const EventOption Option_FinishMoveCPU = {
    .kind = OPTKIND_FUNC,
    .name = "Finish Moving CPU",
    .desc = {"Finish setting the CPU's position."},
    .OnSelect = FinishMoveCPU,
};

static const EventOption Option_MoveHMN = {
    .kind = OPTKIND_FUNC,
    .name = "Move HMN",
    .desc = {"Manually set the HMN's position.", "Press A to change direction."},
    .OnSelect = StartMoveHMN,
};

static const EventOption Option_FinishMoveHMN = {
    .kind = OPTKIND_FUNC,
    .name = "Finish Moving HMN",
    .desc = {"Finish setting the HMN's position."},
    .OnSelect = FinishMoveHMN,
};

static const EventOption Option_MoveCam = {
    .kind = OPTKIND_FUNC,
    .name = "Set Custom Camera",
    .desc = {"Adjust the custom camera's behavior.",
             "Use C-Stick while holding",
             "A/B/Y to pan, rotate and zoom, respectively."},
    .OnSelect = StartMoveCam,
    .disable = true
};

static const EventOption Option_FinishMoveCam = {
    .kind = OPTKIND_FUNC,
    .name = "Finish Moving Camera",
    .desc = {"Finish setting the camera position.",
             "Use C-Stick while holding",
             "A/B/Y to pan, rotate and zoom, respectively."},
    .OnSelect = FinishMoveCam
};

static float GameSpeeds[] = {1.f, 5.f/6.f, 2.f/3.f, 1.f/2.f, 1.f/4.f};
static const char *Options_GameSpeedText[] = {"1", "5/6", "2/3", "1/2", "1/4"};

static const char *Options_Reset[] = {"Fast", "Slow", "None"};
static int ResetDurations[] = { 30, 60, 2147483647 };
static const char *Options_CamMode[] = {"Normal", "Zoom", "Custom"};
static const char *Options_Preset[] = {
    "Original", "Even", "In Place Only", "Away Only", "Toward Only", "Miss Tech Only",
    "In Place / Toward", "Away / Toward", "Tyro setting"
};
static const char *Options_Location[] = {"Center Stage", "Left Plat", "Right Plat", "Top Plat"};
static const char *Options_Height[] = {"Normal", "Short", "Tall", "Double"};
static const char *Options_Distance[] = {"Right Next To", "Close", "Medium", "Past Tech Roll"};
static const char *Options_LocationDisplay[TC_LOCATION_COUNT];
static const char *Options_CPUColor[] = {TM_CPU_INVINCIBILITY_NAMES};

enum {
    OPT_CHANCE_MENU,
    OPT_REACTION_OSD,
    OPT_TIMING_OSD,
    OPT_RESET,
    OPT_INVIS,
    OPT_LOCATION,
    OPT_HEIGHT,
    OPT_DISTANCE,
    OPT_SPOTDODGE,
    OPT_CPU_COLOR,
    OPT_SPEED,
    OPT_MOVE_CPU,
    OPT_MOVE_HMN,
    OPT_CAM,
    OPT_MOVE_CAM,
    OPT_EXIT,

    OPT_COUNT
};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_MENU,
        .menu = &Menu_Chances,
        .name = "Tech Chances",
        .desc = {"Adjust tech and getup option chances."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Reaction Frame OSD",
        .desc = {"Check which frame you reacted on."},
        .OnChange = ChangeReactionOSD,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Timing OSD",
        .desc = {"Check how early or late you were."},
        .val = true,
        .OnChange = ChangeTimingOSD,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Reset",
        .desc = {"Change how quickly the event resets."},
        .values = Options_Reset,
        .value_num = countof(Options_Reset),
        .OnChange = ChangeReset,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Tech Invisibility",
        .desc = {"Toggle the CPU turning invisible during tech", "animations."},
        .values = Options_Invis,
        .value_num = countof(Options_Invis),
        .OnChange = ChangeInvisibility,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Starting Location",
        .desc = {"CPU falls onto the selected platform.", "HMN starts on the ground nearby.", "Unavailable platforms are skipped."},
        .values = Options_LocationDisplay,
        .value_num = TC_LOCATION_COUNT,
        .OnChange = ChangeLocation,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "CPU Drop Height",
        .desc = {"Choose how far above the target CPU starts.", "Double gives slow characters more time to climb.", "This setting is saved."},
        .values = Options_Height,
        .value_num = TC_HEIGHT_COUNT,
        .OnChange = ChangeHeight,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Player Distance",
        .desc = {"Choose horizontal spacing from the CPU.", "HMN stays on the ground, facing the target.", "This setting is saved."},
        .values = Options_Distance,
        .value_num = TC_DISTANCE_COUNT,
        .OnChange = ChangeDistance,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Spotdodge Punish",
        .desc = {"CPU spotdodges as soon as its tech/getup ends.", "Punish its recovery; reset waits until it ends."},
        .OnChange = ChangeSpotdodge,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "CPU Invincibility Overlay",
        .desc = {"Choose a color for the CPU's natural", "invincibility and intangibility."},
        .values = Options_CPUColor,
        .value_num = TM_CPU_INVINCIBILITY_COUNT,
        .val = TM_CPU_INVINCIBILITY_DEFAULT,
        .OnChange = ChangeCPUColor,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = countof(Options_GameSpeedText),
        .name = "Game Speed",
        .desc = {"Change how fast the game engine runs."},
        .values = Options_GameSpeedText,
    },
    Option_MoveCPU,
    Option_MoveHMN,
    {
        .kind = OPTKIND_STRING,
        .name = "Camera Mode",
        .desc = {"Choose what camera type to use."},
        .values = Options_CamMode,
        .value_num = countof(Options_CamMode),
        .OnChange = ChangeCam
    },
    Option_MoveCam,
    {
        .kind = OPTKIND_FUNC,
        .name = "Exit",
        .desc = {"Return to the Event Select Screen."},
        .OnSelect = Exit,
    },
};

static EventMenu Menu_Main = {
    .name = "Techchase Training",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};

EventMenu *Event_Menu = &Menu_Main;

enum {
    OPTCHANCE_PRESET,
    OPTCHANCE_TECHINPLACE,
    OPTCHANCE_TECHAWAY,
    OPTCHANCE_TECHTOWARD,
    OPTCHANCE_MISSTECH,
    OPTCHANCE_GETUPWAIT,
    OPTCHANCE_GETUPSTAND,
    OPTCHANCE_GETUPAWAY,
    OPTCHANCE_GETUPTOWARD,
    OPTCHANCE_GETUPATTACK,

    OPTCHANCE_COUNT
};

enum {
    OPTCAM_NORMAL,
    OPTCAM_ZOOM,
    OPTCAM_CUSTOM,

    OPTCAM_COUNT
};

static EventOption Options_Chances[] = {
    {
        .kind = OPTKIND_STRING,
        .name = "Preset",
        .desc = {"Choose the four tech chances. This is saved.", "Percentage edits below last for this session."},
        .values = Options_Preset,
        .value_num = TC_PRESET_COUNT,
        .OnChange = ChangePreset,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 40,
        .name = "Tech in Place Chance",
        .desc = {"Adjust the chance the CPU will tech in place."},
        .format = "%d%%",
        .OnChange = ChangeTechInPlaceChance,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 30,
        .name = "Tech Away Chance",
        .desc = {"Adjust the chance the CPU will tech away."},
        .format = "%d%%",
        .OnChange = ChangeTechAwayChance,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 30,
        .name = "Tech Toward Chance",
        .desc = {"Adjust the chance the CPU will tech toward."},
        .format = "%d%%",
        .OnChange = ChangeTechTowardChance,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 0,
        .name = "Miss Tech Chance",
        .desc = {"Adjust the chance the CPU will miss tech."},
        .format = "%d%%",
        .OnChange = ChangeMissTechChance,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 0,
        .name = "Miss Tech Wait Chance",
        .desc = {"Adjust the chance the CPU will wait 15 frames",
                 "after a missed tech."},
        .format = "%d%%",
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 25,
        .name = "Stand Chance",
        .desc = {"Adjust the chance the CPU will stand."},
        .format = "%d%%",
        .OnChange = ChangeStandChance,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 25,
        .name = "Roll Away Chance",
        .desc = {"Adjust the chance the CPU will roll away."},
        .format = "%d%%",
        .OnChange = ChangeRollAwayChance,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 25,
        .name = "Roll Toward Chance",
        .desc = {"Adjust the chance the CPU will roll toward."},
        .format = "%d%%",
        .OnChange = ChangeRollTowardChance,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 101,
        .val = 25,
        .name = "Getup Attack Chance",
        .desc = {"Adjust the chance the CPU will getup attack."},
        .format = "%d%%",
        .OnChange = ChangeGetupAttackChance,
    },
};

static EventMenu Menu_Chances = {
    .name = "Tech Chances",
    .option_num = countof(Options_Chances),
    .options = Options_Chances,
};

static void SetTechPresetChances(int preset) {
    for (int i = 0; i < 4; ++i)
        Options_Chances[OPTCHANCE_TECHINPLACE + i].val = TC_PresetChance(preset, i);
}

static void InitializeChances(int preset) {
    static const s16 defaults[] = {40, 30, 30, 0, 0, 25, 25, 25, 25};
    for (unsigned i = 0; i < countof(defaults); ++i)
        Options_Chances[OPTCHANCE_TECHINPLACE + i].val = defaults[i];
    Options_Chances[OPTCHANCE_PRESET].val = preset;
    SetTechPresetChances(preset);
}

static void UpdatePosition(GOBJ *fighter) {
    FighterData *data = fighter->userdata;

    Vec3 pos = data->phys.pos;
    data->coll_data.topN_Curr = pos;
    data->coll_data.topN_CurrCorrect = pos;
    data->coll_data.topN_Prev = pos;
    data->coll_data.topN_Proj = pos;
    data->coll_data.coll_test = *stc_colltest;

    JOBJ *jobj = fighter->hsd_object;
    jobj->trans = data->phys.pos;
    JOBJ_SetMtxDirtySub(jobj);

    // Update static hmn block coords
    Fighter_SetPosition(data->ply, data->flags.ms, &data->phys.pos);
}

static bool FindGroundNearPlayer(GOBJ *fighter, Vec3 *pos, int *line_idx) {
    FighterData *data = fighter->userdata;
    float x = data->phys.pos.X;
    float y1 = data->phys.pos.Y + 4;
    float y2 = data->phys.pos.Y - 4;

    Vec3 _line_unk;
    int _line_kind;

    return GrColl_RaycastGround(pos, line_idx, &_line_kind, &_line_unk,
            -1, -1, -1, 0, x, y1, x, y2, 0);
}

static void UpdateCameraBox(GOBJ *fighter) {
    Fighter_UpdateCameraBox(fighter);

    FighterData *data = fighter->userdata;
    CmSubject *subject = data->camera_subject;
    subject->boundleft_curr = subject->boundleft_proj;
    subject->boundright_curr = subject->boundright_proj;
}

static int reset_timer = -1;
static int missed_tech_getup_timer = -1;
static int first_action_frame = -1;
static bool put_out_hitbox = false;
static int punish_phase = TC_PUNISH_START;
static int spotdodge_length = 0;

// figatree_curr->frame_num for last getup animation.
// We can't easily request this on demand so we store it here pre-emptively.
static int getup_option_length = 0;

static int hmn_pad_index;
static int null_pad_index;

static Vec3 cpu_pos = { 5, 27, 0 };
static Vec3 hmn_pos = { -5, 0, 0 };
static float cpu_facing_direction = -1;
static float hmn_facing_direction = 1;
static bool automatic_position = true;
static int location_available[TC_LOCATION_COUNT];
static int previous_location = TC_CENTER;

static int IsPlatformLine(const CollLineDesc *line) {
    /* Native MapLine.lo_flags LINE_FLAG_PLATFORM is 0x0100. The SDK's
     * legacy is_drop label addresses 0x0400 (dynamic collision), not this bit. */
    return ((const u8 *)line)[0xE] & 1;
}

/* Use enabled, current collision vertices rather than fixed stage coordinates.
 * This follows Fountain's moving platforms and grounds HMN under the platform. */
static int FindSolidGround(float x, Vec3 *position, int *index) {
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts) return 0;
    int found = 0;
    for (int i = 0; i < stage->line_num; ++i) {
        CollLineDesc *line = lines[i].desc;
        if (!line || !line->is_floor || IsPlatformLine(line) || line->disabled || !GrColl_CheckIfLineEnabled(i) ||
            line->vert_prev < 0 || line->vert_next < 0 ||
            line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num) continue;
        Vec2 a = verts[line->vert_prev].pos_curr, b = verts[line->vert_next].pos_curr;
        float left = a.X < b.X ? a.X : b.X, right = a.X > b.X ? a.X : b.X;
        if (right - left < 0.01f || x < left || x > right) continue;
        float y = a.Y + (b.Y - a.Y) * (x - a.X) / (b.X - a.X);
        if (!found || y > position->Y) { *position = (Vec3){x, y, 0}; *index = i; found = 1; }
    }
    return found;
}

static int FindLocationSurface(int location, Vec3 *position) {
    int line_index;
    if (location == TC_CENTER) return FindSolidGround(5, position, &line_index);
    int stage_id = Stage_GetExternalID();
    if (stage_id != GRKINDEXT_STORY && stage_id != GRKINDEXT_IZUMI &&
        stage_id != GRKINDEXT_FD && stage_id != GRKINDEXT_PSTAD &&
        stage_id != GRKINDEXT_BATTLE && stage_id != GRKINDEXT_OLDPU) return 0;
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts) return 0;
    float best = -100000;
    int found = 0;
    for (int i = 0; i < stage->line_num; ++i) {
        CollLineDesc *line = lines[i].desc;
        if (!line || !line->is_floor || line->disabled || !GrColl_CheckIfLineEnabled(i) ||
            line->vert_prev < 0 || line->vert_next < 0 ||
            line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num) continue;
        Vec2 a = verts[line->vert_prev].pos_curr, b = verts[line->vert_next].pos_curr;
        float x, y;
        if (!TC_SurfaceTarget(a.X, a.Y, b.X, b.Y, IsPlatformLine(line), location, &x, &y)) continue;
        float distance = TC_PlayerDistance(Options_Main[OPT_DISTANCE].val);
        float hmn_x = x < 0 ? x + distance : x - distance;
        Vec3 ground;
        if (!FindSolidGround(hmn_x, &ground, &line_index) || y <= ground.Y + 4) continue;
        float score = location == TC_TOP_PLATFORM ? y : -fabs(x);
        if (!found || score > best) { *position = (Vec3){x, y, 0}; best = score; found = 1; }
    }
    return found;
}

static void RefreshLocationChoices(void) {
    static const char *unavailable[] = {"Center Stage", "Left Plat (N/A)", "Right Plat (N/A)", "Top Plat (N/A)"};
    Vec3 position;
    for (int i = 0; i < TC_LOCATION_COUNT; ++i) {
        location_available[i] = FindLocationSurface(i, &position);
        Options_LocationDisplay[i] = location_available[i] ? Options_Location[i] : unavailable[i];
    }
    // Center retains the original placement if stage geometry is unavailable.
    location_available[TC_CENTER] = 1;
    Options_Main[OPT_LOCATION].disable = !location_available[TC_LEFT_PLATFORM] &&
        !location_available[TC_RIGHT_PLATFORM] && !location_available[TC_TOP_PLATFORM];
}

static void ApplyStartingLocation(void) {
    RefreshLocationChoices();
    int location = Options_Main[OPT_LOCATION].val;
    Vec3 surface, ground;
    int line_index;
    if (!location_available[location] || !FindLocationSurface(location, &surface)) {
        location = TC_CENTER;
        Options_Main[OPT_LOCATION].val = location;
        if (!FindLocationSurface(location, &surface)) surface = (Vec3){5, 0, 0};
    }
    previous_location = location;
    float distance = TC_PlayerDistance(Options_Main[OPT_DISTANCE].val);
    float hmn_x = surface.X < 0 ? surface.X + distance : surface.X - distance;
    if (!FindSolidGround(hmn_x, &ground, &line_index)) ground = (Vec3){hmn_x, 0, 0};
    hmn_pos = ground;
    cpu_pos = (Vec3){surface.X, surface.Y + TC_DropHeight(Options_Main[OPT_HEIGHT].val), 0};
    hmn_facing_direction = surface.X > hmn_x ? 1 : -1;
    cpu_facing_direction = -hmn_facing_direction;
}

static void Reset(void) {
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    if (automatic_position) ApplyStartingLocation();

    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;

    hmn_data->facing_direction = hmn_facing_direction;
    cpu_data->facing_direction = cpu_facing_direction;

    hmn_data->phys.pos = hmn_pos;
    cpu_data->phys.pos = cpu_pos;

    UpdatePosition(hmn);
    UpdatePosition(cpu);
    
    Vec3 grounded_hmn_pos;
    int ground_index;
    if (FindGroundNearPlayer(hmn, &grounded_hmn_pos, &ground_index)) {
        hmn_data->phys.pos = grounded_hmn_pos;
        hmn_data->coll_data.ground_index = ground_index;
        UpdatePosition(hmn);
        EnvironmentCollision_WaitLanding(hmn);
        Fighter_SetGrounded(hmn_data);
        Fighter_EnterWait(hmn);
    } else {
        Fighter_EnterFall(hmn);
    }

    Fighter_EnterDamageFall(cpu);

    UpdateCameraBox(hmn);
    UpdateCameraBox(cpu);

    Match_CorrectCamera();

    reset_timer = -1;
    missed_tech_getup_timer = -1;
    first_action_frame = -1;
    put_out_hitbox = false;
    punish_phase = TC_PUNISH_START;
    spotdodge_length = 0;
}

static int tech_frame_distinguishable[27] = {
     8, // Mario
     4, // Fox
     6, // Captain Falcon
     9, // Donkey Kong
     3, // Kirby
     1, // Bowser
     6, // Link
     8, // Sheik
     8, // Ness
     3, // Peach
     9, // Popo (Ice Climbers)
     9, // Nana (Ice Climbers)
     7, // Pikachu
     6, // Samus
     9, // Yoshi
     3, // Jigglypuff
    16, // Mewtwo
     8, // Luigi
     7, // Marth
     6, // Zelda
     6, // Young Link
     8, // Dr. Mario
     4, // Falco
     8, // Pichu
     3, // Game & Watch
     6, // Ganondorf
     7, // Roy
};

static int RunSpotdodgePunish(GOBJ *cpu) {
    FighterData *data = cpu->userdata;
    int state = data->state_id;
    int category = ASID_DOWNSPOTD <= state && state <= ASID_PASSIVESTANDB ? TC_STATE_OPTION :
        state == ASID_WAIT ? TC_STATE_WAIT : state == ASID_ESCAPE ? TC_STATE_DODGE : TC_STATE_OTHER;
    int action = TC_PunishStep(&punish_phase, Options_Main[OPT_SPOTDODGE].val, category);
    if (state == ASID_ESCAPE) spotdodge_length = data->figatree_curr->frame_num;
    if (action == TC_ACTION_DODGE) {
        /* CPU input processing already ran at priority 2. Supply this tick's
         * processed input, with a fresh down tilt, before native Wait IASA.
         * Native Wait IASA performs the regular spotdodge eligibility check;
         * no tech/getup state or animation length is forcibly shortened. */
        data->cpu.held = PAD_TRIGGER_R;
        data->cpu.lstickX = 0;
        data->cpu.lstickY = -127;
        // Native fighter input also carries the synthesized HSD_PAD_LR bit;
        // the raw R button alone is insufficient for the spotdodge check.
        data->input.held = PAD_TRIGGER_R | 0x80000000u;
        data->input.lstick = (Vec2){0, -1};
        data->input.trigger = 1;
        data->input.timer_lstick_tilt_y = 0;
        Fighter_IASACheck_AllGrounded(cpu);
        if (data->state_id == ASID_ESCAPE) {
            punish_phase = TC_PUNISH_DODGING;
            spotdodge_length = data->figatree_curr->frame_num;
        }
    }
    return action;
}

void Event_Update(GOBJ *menu) {
    if (Pause_CheckStatus(1) != 2) {
        float speed = GameSpeeds[Options_Main[OPT_SPEED].val];
        HSD_SetSpeedEasy(speed);
    } else {
        HSD_SetSpeedEasy(1.0);
    }

}

void MoveFighter(GOBJ *ft, Vec3 *pos, float *facing_direction) {
    HSD_Pad *pad = PadGetMaster(hmn_pad_index);
    FighterData *ft_data = ft->userdata;
    
    if (pad->down & HSD_BUTTON_A)
        *facing_direction = -*facing_direction;
    ft_data->facing_direction = *facing_direction;
    ft_data->facing_direction_prev = *facing_direction;
        
    float x = pad->fstickX;
    float y = pad->fstickY;
    float deadzone = 0.2750f;
    if (fabs(x) < deadzone) x = 0.f;
    if (fabs(y) < deadzone) y = 0.f;
    pos->X += x * 1.5f;
    pos->Y += y * 1.5f;
    ft_data->phys.pos = *pos;
    UpdatePosition(ft);
}

void Event_Think(GOBJ *menu) {
    if (event_vars->game_timer == 1) {
        event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
        Reset();
    }

    HSD_Pad *pad = PadGetMaster(hmn_pad_index);
    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;
    
    // move CPU
    if (Options_Main[OPT_MOVE_CPU].name == Option_FinishMoveCPU.name) {
        MoveFighter(cpu, &cpu_pos, &cpu_facing_direction);
        Fighter_EnterDamageFall(cpu);
        return;
    }
    
    // move HMN
    else if (Options_Main[OPT_MOVE_HMN].name == Option_FinishMoveHMN.name) {
        MoveFighter(hmn, &hmn_pos, &hmn_facing_direction);

        Vec3 grounded_hmn_pos;
        int ground_index;
        if (FindGroundNearPlayer(hmn, &grounded_hmn_pos, &ground_index)) {
            hmn_data->phys.pos = grounded_hmn_pos;
            hmn_data->coll_data.ground_index = ground_index;
        }
        Fighter_EnterFall(hmn);
    
        return;
    }

    // move Camera
    else if (Options_Main[OPT_MOVE_CAM].name == Option_FinishMoveCam.name) {
        return;
    }

    if (Options_Main[OPT_CAM].val == OPTCAM_CUSTOM && Options_Main[OPT_MOVE_CAM].name == Option_MoveCam.name){
        stc_matchcam->devcam_pos = saved_cam_pos;
        stc_matchcam->devcam_rot = saved_cam_rot;
    }

    cpu_data->cpu.ai = 15;
    cpu_data->cpu.held = 0;
    cpu_data->cpu.lstickX = 0;
    cpu_data->cpu.lstickY = 0;
    cpu_data->cpu.cstickX = 0;
    cpu_data->cpu.cstickY = 0;

    int punish_action = reset_timer == -1 ? RunSpotdodgePunish(cpu) : TC_ACTION_NONE;
    int state_id = cpu_data->state_id;
    bool punish_recovery = Options_Main[OPT_SPOTDODGE].val && punish_phase >= TC_PUNISH_DODGING;
    
    // cache getup_option_length
    if (ASID_DOWNSPOTD <= state_id && state_id <= ASID_PASSIVESTANDB)
        getup_option_length = cpu_data->figatree_curr->frame_num;

    // check for action
    if (
        Options_Main[OPT_REACTION_OSD].val
        && ASID_DOWNSPOTD <= state_id && state_id <= ASID_PASSIVESTANDB
        && first_action_frame == -1
    ) {
        HSD_Pad *engine_pad = PadGetEngine(hmn_data->pad_index);
        
        int input = engine_pad->held | engine_pad->stickX | engine_pad->stickY | engine_pad->substickX | engine_pad->substickY;
        if (input != 0) {
            first_action_frame = cpu_data->TM.state_frame;
            int frame_distinguishable = tech_frame_distinguishable[cpu_data->kind];

            event_vars->Message_Display(
                15, hmn_data->ply, MSGCOLOR_WHITE, 
                "Reaction: %if\n", first_action_frame - frame_distinguishable
            );
        }
    }

    // set intangibility
    if (!punish_recovery && (
        (
            // make invincible on last frame of getup/tech option
            ASID_DOWNSPOTD <= state_id && state_id <= ASID_PASSIVESTANDB
            && cpu_data->state.frame + 1 == cpu_data->figatree_curr->frame_num
        )
        || state_id == ASID_WAIT
        || state_id == ASID_DAMAGEFALL
    )) {
        cpu_data->hurt.intang_frames.ledge = 1;
        cpu_data->hurt.kind_game = 1;
    } else {
        cpu_data->hurt.intang_frames.ledge = 0;
        cpu_data->hurt.kind_game = 0;
    }
    
    // Check for late attack and show Timing OSD
    if (
        Options_Main[OPT_TIMING_OSD].val
        && state_id == ASID_WAIT 
        && (!Options_Main[OPT_SPOTDODGE].val || punish_phase == TC_PUNISH_FINISHED)
        && hmn_data->flags.hitbox_active
        && !put_out_hitbox
    ) { 
        put_out_hitbox = true;
        event_vars->Message_Display(
            16, hmn_data->ply, MSGCOLOR_RED, 
            "%if Late\n", cpu_data->TM.state_frame
        );
    }

    if (reset_timer == -1) {
        // tech if airborne
        if (cpu_data->phys.air_state == 1) {
            cpu_data->input.timer_LR = 0;
            
            int rng = HSD_Randi(100);
            s8 x = 0;
            char LR = 0;
            if ((rng -= Options_Chances[OPTCHANCE_TECHINPLACE].val) < 0) {

            } else if ((rng -= Options_Chances[OPTCHANCE_TECHAWAY].val) < 0) {
                x = 40 * (int)sign(cpu_data->phys.pos.X - hmn_data->phys.pos.X);
            } else if ((rng -= Options_Chances[OPTCHANCE_TECHTOWARD].val) < 0) {
                x = 40 * (int)sign(hmn_data->phys.pos.X - cpu_data->phys.pos.X);
            } else {
                LR = -1;
            }
            cpu_data->cpu.lstickX = x;
            cpu_data->input.timer_LR = LR;
        }
        
        // missed tech getup
        if (missed_tech_getup_timer == -1) {
            if (state_id == ASID_DOWNWAITU || state_id == ASID_DOWNWAITD) {
                int rng = HSD_Randi(100);
                if (rng < Options_Chances[OPTCHANCE_GETUPWAIT].val) {
                    missed_tech_getup_timer = 15;
                } else {
                    rng = HSD_Randi(100);
                    s8 x = 0;
                    s8 y = 0;
                    int held = 0;
                    if ((rng -= Options_Chances[OPTCHANCE_GETUPSTAND].val) < 0) {
                        y = 127;
                    } else if ((rng -= Options_Chances[OPTCHANCE_GETUPAWAY].val) < 0) {
                        x = 40 * (int)sign(cpu_data->phys.pos.X - hmn_data->phys.pos.X);
                    } else if ((rng -= Options_Chances[OPTCHANCE_GETUPTOWARD].val) < 0) {
                        x = 40 * (int)sign(hmn_data->phys.pos.X - cpu_data->phys.pos.X);
                    } else {
                        held = PAD_BUTTON_A;
                    }
                    cpu_data->cpu.lstickX = x;
                    cpu_data->cpu.lstickY = y;
                    cpu_data->cpu.held = held;
                }
            }
        } else {
            missed_tech_getup_timer--;
        }

        // fail if finished tech 
        if (state_id == ASID_WAIT && punish_action == TC_ACTION_RESET) {
            // SFX_Play(0xAF);
            reset_timer = ResetDurations[Options_Main[OPT_RESET].val];
        }
        // succeed if grabbed or hit 
        else if (
            (ASID_CAPTUREPULLEDHI <= state_id && state_id <= ASID_CAPTUREFOOT)
            || (ASID_DAMAGEHI1 <= state_id && state_id <= ASID_DAMAGEFLYROLL)
            || (cpu_data->dmg.hitlag_frames > 0 && cpu_data->flags.hitlag_victim)
        ) {
            // Show Timing OSD
            if (Options_Main[OPT_TIMING_OSD].val && !put_out_hitbox) { 
                put_out_hitbox = true;
                
                int frames_taken = -1;
                int prev_state_id = -1;
                for (int i = 0; i < 6; ++i) {
                    prev_state_id = cpu_data->TM.state_prev[i];
                    if ((punish_recovery && prev_state_id == ASID_ESCAPE) ||
                        (!punish_recovery && ASID_DOWNSPOTD <= prev_state_id && prev_state_id <= ASID_PASSIVESTANDB)) {
                        frames_taken = cpu_data->TM.state_prev_frames[i];
                        break;
                    }
                }

                if (frames_taken != -1) {
                    int option_length = punish_recovery ? spotdodge_length : getup_option_length;
                    event_vars->Message_Display(
                        16, hmn_data->ply, MSGCOLOR_GREEN, 
                        "%if Early\n", option_length - frames_taken - 2
                    );
                }
            }
        
            SFX_Play(0xAD);
            reset_timer = ResetDurations[Options_Main[OPT_RESET].val];
        }
    }

    if (pad->down & HSD_BUTTON_DPAD_LEFT)
        Reset();

    if (reset_timer >= 0 && reset_timer-- == 0) {
        const float deadzone = 0.2750f;
        const float triggerDeadzone = 0.3f;
        bool busy = false;
        if (fabs(pad->fstickX) > deadzone) busy = true;
        if (fabs(pad->fstickY) > deadzone) busy = true;
        if (fabs(pad->fsubstickX) > deadzone) busy = true;
        if (fabs(pad->fsubstickY) > deadzone) busy = true;
        if (pad->ftriggerLeft  > triggerDeadzone) busy = true;
        if (pad->ftriggerRight > triggerDeadzone) busy = true;
        if (pad->down != 0) busy = true;
        if (pad->held != 0) busy = true;

        if (!busy) {
            Reset();
        } else {
            reset_timer++;
        }
    }
}

static void Event_PostThink(GOBJ *menu) {
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;
    int state_id = cpu_data->state_id;

    int frame_distinguishable = tech_frame_distinguishable[cpu_data->kind];
    switch (Options_Main[OPT_INVIS].val) {
        case OPTINVIS_NONE: {
            cpu_data->flags.invisible = false;
        } break;
        case OPTINVIS_POST_REACTION: {
            bool post_reaction =
                ASID_PASSIVE <= state_id && state_id <= ASID_PASSIVESTANDB
                && frame_distinguishable < cpu_data->TM.state_frame && cpu_data->TM.state_frame < 20;
            cpu_data->flags.invisible = post_reaction;
        } break;
        case OPTINVIS_FLASH: {
            bool on_reaction = (
                    ASID_PASSIVE <= state_id && state_id <= ASID_PASSIVESTANDB
                    && cpu_data->TM.state_frame + 1 != frame_distinguishable
                    && cpu_data->TM.state_frame < 20
                ) 
                || state_id == ASID_DAMAGEFALL;
            cpu_data->flags.invisible = on_reaction;
        } break;
    }

}

void Event_Init(GOBJ *gobj) {
    GObj_AddProc(gobj, Event_PostThink, 20);
    
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    hmn_pad_index = hmn_data->pad_index;
    null_pad_index = (hmn_pad_index + 1) % 4;
    automatic_position = true;
    InitializeChances(TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_PRESET));
    Options_Main[OPT_REACTION_OSD].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_REACTION_OSD);
    Options_Main[OPT_TIMING_OSD].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_TIMING_OSD);
    Options_Main[OPT_RESET].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_RESET);
    Options_Main[OPT_INVIS].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_INVISIBILITY);
    Options_Main[OPT_LOCATION].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_LOCATION);
    Options_Main[OPT_HEIGHT].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_HEIGHT);
    Options_Main[OPT_DISTANCE].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_DISTANCE);
    Options_Main[OPT_SPOTDODGE].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_SPOTDODGE);
    Options_Main[OPT_CPU_COLOR].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_CPU_COLOR);
    event_vars->set_cpu_invincibility_overlay(&Options_Main[OPT_CPU_COLOR].val);
    Options_Main[OPT_CAM].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_CAMERA);
    ApplyStartingLocation();
    ChangeCam(0, Options_Main[OPT_CAM].val);
    for (unsigned i = 0; i < countof(Options_Main); ++i) Options_Main[i].val_prev = Options_Main[i].val;
    for (unsigned i = 0; i < countof(Options_Chances); ++i) Options_Chances[i].val_prev = Options_Chances[i].val;
}

static void Exit(GOBJ *menu) {
    event_vars->set_cpu_invincibility_overlay(0);
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

static void ChangePreset(GOBJ *menu, int value) {
    SetTechPresetChances(value);
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_PRESET, value);
}
static void ChangeReactionOSD(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_REACTION_OSD, value); }
static void ChangeTimingOSD(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_TIMING_OSD, value); }
static void ChangeReset(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_RESET, value); }
static void ChangeInvisibility(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_INVISIBILITY, value); }
static void ChangeCPUColor(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_CPU_COLOR, value); }
static void ChangeHeight(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_HEIGHT, value);
    automatic_position = true;
    Reset();
}
static void ChangeDistance(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_DISTANCE, value);
    automatic_position = true;
    Reset();
}
static void ChangeSpotdodge(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_SPOTDODGE, value);
    Reset();
}
static void ChangeLocation(GOBJ *menu, int value) {
    RefreshLocationChoices();
    // Left/right changes wrap around the selector; keep moving in that same
    // direction past missing platforms (FD and Stadium's missing top platform).
    int direction = (value - previous_location + TC_LOCATION_COUNT) % TC_LOCATION_COUNT == 1 ? 1 : -1;
    for (int i = 0; i < TC_LOCATION_COUNT && !location_available[value]; ++i)
        value = (value + direction + TC_LOCATION_COUNT) % TC_LOCATION_COUNT;
    Options_Main[OPT_LOCATION].val = value;
    previous_location = value;
    automatic_position = true;
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_LOCATION, value);
    Reset();
}

// Clean up percentages so the total is always 100.
// The next percentage is modified.
static void ReboundChances(s16 *chances[], unsigned int chance_count, int just_changed_option) {
    if (chance_count == 0) return;

    int sum = 0;
    for (u32 t = 0; t < chance_count; ++t)
        sum += *chances[t];

    int delta = 100 - sum;
    // keep adding/removing from next option chance until all needed change to revert to 100% is applied
    while (delta)
    {
        int rebound_option = (just_changed_option + 1) % chance_count;

        s16 *opt_val = chances[rebound_option];
        int prev_chance = (int)*opt_val;

        int new_chance = prev_chance + delta;
        if (new_chance < 0) {
            *opt_val = 0;
            delta = new_chance;
        } else if (new_chance > 100) {
            *opt_val = 100;
            delta = new_chance - 100;
        } else {
            *opt_val = (u16)new_chance;
            delta = 0;
        }

        just_changed_option = rebound_option;
    }
}

static void ReboundTechChances(EventOption *tech_menu, int slot_idx_changed) {
    s16 *chances[4];
    int enabled_slots = 0;

    for (int i = 0; i < 4; i++) {
        chances[enabled_slots] = &tech_menu[i].val;
        enabled_slots++;
    }

    ReboundChances(chances, 4, slot_idx_changed);
}

static void ChangeTechInPlaceChance (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_TECHINPLACE], 0); }
static void ChangeTechAwayChance    (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_TECHINPLACE], 1); }
static void ChangeTechTowardChance  (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_TECHINPLACE], 2); }
static void ChangeMissTechChance    (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_TECHINPLACE], 3); }

static void ChangeStandChance       (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_GETUPSTAND], 0); }
static void ChangeRollAwayChance    (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_GETUPSTAND], 1); }
static void ChangeRollTowardChance  (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_GETUPSTAND], 2); }
static void ChangeGetupAttackChance (GOBJ *menu_gobj, int _new_val) { ReboundTechChances(&Options_Chances[OPTCHANCE_GETUPSTAND], 3); }

static void DisableHmnControl(void) {
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    hmn_data->pad_index = null_pad_index;
}

static void EnableHmnControl(void) {
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    hmn_data->pad_index = hmn_pad_index;
}

static void StartMoveCPU(GOBJ *menu) {
    DisableHmnControl();
    Reset();
    automatic_position = false;
    Options_Main[OPT_MOVE_CPU] = Option_FinishMoveCPU;
    Options_Main[OPT_MOVE_HMN].disable = true;
    Options_Main[OPT_MOVE_CAM].disable = true;
    Options_Main[OPT_CAM].disable = true;
    Options_Main[OPT_LOCATION].disable = true;
}

static void FinishMoveCPU(GOBJ *menu) {
    EnableHmnControl();
    Reset();
    Options_Main[OPT_MOVE_CPU] = Option_MoveCPU;
    Options_Main[OPT_MOVE_HMN].disable = false;
    Options_Main[OPT_CAM].disable = false;
    RefreshLocationChoices();
    if (Options_Main[OPT_CAM].val == OPTCAM_CUSTOM){
        Options_Main[OPT_MOVE_CAM].disable = false;
    }
}

static void StartMoveHMN(GOBJ *menu) {
    DisableHmnControl();
    Reset();
    automatic_position = false;
    Options_Main[OPT_MOVE_HMN] = Option_FinishMoveHMN; 
    Options_Main[OPT_MOVE_CPU].disable = true;
    Options_Main[OPT_MOVE_CAM].disable = true;
    Options_Main[OPT_CAM].disable = true;
    Options_Main[OPT_LOCATION].disable = true;
}

static void FinishMoveHMN(GOBJ *menu) {
    EnableHmnControl();
    Reset();
    Options_Main[OPT_MOVE_HMN] = Option_MoveHMN; 
    Options_Main[OPT_MOVE_CPU].disable = false;
    Options_Main[OPT_CAM].disable = false;
    RefreshLocationChoices();
    if (Options_Main[OPT_CAM].val == OPTCAM_CUSTOM){
        Options_Main[OPT_MOVE_CAM].disable = false;
    }
}

static void StartMoveCam(GOBJ *menu) {
    DisableHmnControl();
    Reset();
    Options_Main[OPT_MOVE_CAM] = Option_FinishMoveCam;
    Options_Main[OPT_MOVE_HMN].disable = true;
    Options_Main[OPT_MOVE_CPU].disable = true;
    Options_Main[OPT_CAM].disable = true;
    Options_Main[OPT_LOCATION].disable = true;
}

static void FinishMoveCam(GOBJ *menu) {
    saved_cam_pos = stc_matchcam->devcam_pos;
    saved_cam_rot = stc_matchcam->devcam_rot;
    EnableHmnControl();
    Reset();
    Options_Main[OPT_MOVE_CAM] = Option_MoveCam;
    Options_Main[OPT_MOVE_CAM].disable = false;
    Options_Main[OPT_MOVE_HMN].disable = false;
    Options_Main[OPT_MOVE_CPU].disable = false;
    Options_Main[OPT_CAM].disable = false;
    RefreshLocationChoices();
}

static void ChangeCam(GOBJ *menu_gobj, int value) {
    if (menu_gobj) TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_CAMERA, value);
    Options_Main[OPT_MOVE_CAM].disable = value != OPTCAM_CUSTOM;
    if (value == OPTCAM_NORMAL){
        Match_SetNormalCamera();
    }
    else if (value == OPTCAM_ZOOM){
        Match_SetFreeCamera(0, 3);
        stc_matchcam->freecam_fov.X = 140;
        stc_matchcam->freecam_rotate.Y = 10;
    }
    else if (value == OPTCAM_CUSTOM){
        Match_SetDevelopCamera();
        saved_cam_pos = stc_matchcam->devcam_pos;
        saved_cam_rot = stc_matchcam->devcam_rot;
    }
    Match_CorrectCamera();
}
