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
static void ChangeSide(GOBJ *menu_gobj, int value);
static void ChangeReactionOSD(GOBJ *menu_gobj, int value);
static void ChangeTimingOSD(GOBJ *menu_gobj, int value);
static void ChangeReset(GOBJ *menu_gobj, int value);
static void ChangeInvisibility(GOBJ *menu_gobj, int value);
static void ChangeFollowup(GOBJ *menu_gobj, int value);
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
    "Getup Attack Only", "In Place / Toward", "Away / Toward", "Tyro setting"
};
static const char *Options_Location[] = {"Center Stage", "Left Plat", "Right Plat", "Top Plat"};
static const char *Options_Height[] = {"Normal", "Short", "Tall", "Double"};
static const char *Options_Distance[] = {"Right Next To", "Close", "Medium", "Past Tech Roll", "Random"};
static const char *Options_Side[] = {"Left", "Right", "Random"};
static const char *Options_LocationDisplay[TC_LOCATION_COUNT];
static const char *Options_Followup[] = {"None", "Spotdodge", "Roll Away", "Roll In", "Shine", "Random"};
static const char *Options_FollowupDisplay[TC_FOLLOWUP_COUNT];
static const char *Options_CPUColor[] = {TM_CPU_INVINCIBILITY_NAMES};
typedef char TCMenuMatchesSave[(countof(Options_Preset) == TC_PRESET_COUNT &&
    TC_PRESET_COUNT == TM_TECHCHASE_PRESET_COUNT && countof(Options_Distance) == TC_DISTANCE_COUNT &&
    TC_DISTANCE_COUNT == TM_TECHCHASE_DISTANCE_COUNT && countof(Options_Side) == TC_SIDE_COUNT &&
    TC_SIDE_COUNT == TM_TECHCHASE_SIDE_COUNT && countof(Options_Followup) == TC_FOLLOWUP_COUNT &&
    TC_FOLLOWUP_COUNT == TM_TECHCHASE_FOLLOWUP_COUNT) ? 1 : -1];

enum {
    OPT_CHANCE_MENU,
    OPT_REACTION_OSD,
    OPT_TIMING_OSD,
    OPT_RESET,
    OPT_INVIS,
    OPT_LOCATION,
    OPT_HEIGHT,
    OPT_DISTANCE,
    OPT_SIDE,
    OPT_FOLLOWUP,
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
        .desc = {"Choose horizontal spacing from the CPU.",
                 "Random picks a new spacing each reset, anywhere",
                 "from Right Next To through Medium.",
                 "HMN stays on the ground. This setting is saved."},
        .values = Options_Distance,
        .value_num = TC_DISTANCE_COUNT,
        .OnChange = ChangeDistance,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Player Side",
        .desc = {"Which side of the CPU HMN starts on.",
                 "Random picks again each reset. This is saved.",
                 "D-Pad left/right also picks a side and restarts."},
        .values = Options_Side,
        .value_num = TC_SIDE_COUNT,
        .OnChange = ChangeSide,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "CPU Followup Action",
        .desc = {"CPU acts once its tech/getup ends; punish it.",
                 "Rolls go away from or in toward stage center.",
                 "Shine (Fox/Falco) jumps out and wavelands.",
                 "Random picks again each reset. This is saved."},
        .values = Options_FollowupDisplay,
        .value_num = TC_FOLLOWUP_COUNT,
        .OnChange = ChangeFollowup,
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
    .on_exit = Exit,
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
        .desc = {"Choose the four tech chances. This is saved.",
                 "Getup Attack Only also sets the getups.",
                 "Percentage edits below last for this session."},
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

static const s16 DefaultChances[] = {40, 30, 30, 0, 0, 25, 25, 25, 25};
static bool preset_set_getups; // The getups are Getup Attack Only's, not the defaults or edits.

static void SetTechPresetChances(int preset) {
    for (int i = 0; i < 4; ++i)
        Options_Chances[OPTCHANCE_TECHINPLACE + i].val = TC_PresetChance(preset, i);
    // The other presets keep this session's getups, but leaving
    // Getup Attack Only puts back the defaults.
    bool sets_getups = TC_PresetGetupChance(preset, 0) >= 0;
    if (sets_getups || preset_set_getups) {
        for (int i = 0; i < 4; ++i) {
            Options_Chances[OPTCHANCE_GETUPSTAND + i].val = sets_getups ? TC_PresetGetupChance(preset, i) :
                DefaultChances[OPTCHANCE_GETUPSTAND - OPTCHANCE_TECHINPLACE + i];
        }
    }
    preset_set_getups = sets_getups;
}

static void InitializeChances(int saved) {
    int preset = TC_PresetFromSaved(saved);
    for (unsigned i = 0; i < countof(DefaultChances); ++i)
        Options_Chances[OPTCHANCE_TECHINPLACE + i].val = DefaultChances[i];
    Options_Chances[OPTCHANCE_PRESET].val = preset;
    preset_set_getups = false;
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
// Frames in the followup's last, punishable state, for the Timing OSD.
static int followup_length = 0;
// This attempt's followup, with Random already picked.
static int attempt_followup = TC_FOLLOWUP_NONE;
// This match's CPU is Fox or Falco, so it can shine.
static bool can_shine = false;
// Which way this attempt's shine wavelands, picked while it's grounded.
static int waveland_direction = 0;

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
// This attempt's side and spacing; Random rolls them again on every reset.
static int player_direction = -1;
static float player_distance = 10;

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

// The leftmost and rightmost ends of the enabled solid floors.
static int SolidGroundEdges(float *left, float *right) {
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
        float lo = a.X < b.X ? a.X : b.X, hi = a.X > b.X ? a.X : b.X;
        if (!found || lo < *left) *left = lo;
        if (!found || hi > *right) *right = hi;
        found = 1;
    }
    return found;
}

// HMN's ground spot for a CPU above cpu_x, on this attempt's side.
static int FindPlayerGround(float cpu_x, Vec3 *ground) {
    float left, right;
    int line_index;
    float x = cpu_x + player_direction * player_distance;
    if (SolidGroundEdges(&left, &right)) x = TC_PlayerX(cpu_x, player_direction, player_distance, left, right);
    if (FindSolidGround(x, ground, &line_index)) return 1;
    *ground = (Vec3){x, 0, 0};
    return 0;
}

static int FindLocationSurface(int location, Vec3 *position) {
    int line_index;
    if (location == TC_CENTER) return FindSolidGround(-5.f * player_direction, position, &line_index);
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
        if (!TC_SurfaceTarget(a.X, a.Y, b.X, b.Y, IsPlatformLine(line), location, player_direction, &x, &y)) continue;
        Vec3 ground;
        if (!FindPlayerGround(x, &ground) || y <= ground.Y + 4) continue;
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

// Settle this attempt's side and spacing, rolling whichever is Random.
static void RollPlacement(void) {
    int side = Options_Main[OPT_SIDE].val;
    player_direction = TC_PlayerDirection(side, side == TC_SIDE_RANDOM ? HSD_Randi(2) : 0);
    int distance = Options_Main[OPT_DISTANCE].val;
    player_distance = TC_PlayerDistance(distance, distance == TC_DISTANCE_RANDOM ? HSD_Randf() : 0);
}

/* Shine is only for a Fox or Falco CPU. Elsewhere it's labeled N/A and the
 * menu skips it, as it skips missing platforms. */
static void RefreshFollowupChoices(void) {
    int kind = ((FighterData *)Fighter_GetGObj(1)->userdata)->kind;
    can_shine = kind == FTKIND_FOX || kind == FTKIND_FALCO;
    for (int i = 0; i < TC_FOLLOWUP_COUNT; ++i) Options_FollowupDisplay[i] = Options_Followup[i];
    if (!can_shine) Options_FollowupDisplay[TC_FOLLOWUP_SHINE] = "Shine (N/A)";
}

// A saved Shine falls back to None for another CPU, without a save write.
static void LoadFollowup(void) {
    RefreshFollowupChoices();
    int followup = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_FOLLOWUP);
    Options_Main[OPT_FOLLOWUP].val = TC_FollowupAvailable(followup, can_shine) ? followup : TC_FOLLOWUP_NONE;
}

// Settle this attempt's followup, picking again if it's Random.
static void RollFollowup(void) {
    int followup = Options_Main[OPT_FOLLOWUP].val;
    int pick = followup == TC_FOLLOWUP_RANDOM ? HSD_Randi(TC_RandomFollowupCount(can_shine)) : 0;
    attempt_followup = TC_ResolveFollowup(followup, can_shine, pick);
}

static void ApplyStartingLocation(void) {
    RollPlacement();
    RefreshLocationChoices();
    int location = Options_Main[OPT_LOCATION].val;
    Vec3 surface, ground;
    if (!location_available[location] || !FindLocationSurface(location, &surface)) {
        location = TC_CENTER;
        Options_Main[OPT_LOCATION].val = location;
        if (!FindLocationSurface(location, &surface)) surface = (Vec3){-5.f * player_direction, 0, 0};
    }
    previous_location = location;
    FindPlayerGround(surface.X, &ground);
    hmn_pos = ground;
    cpu_pos = (Vec3){surface.X, surface.Y + TC_DropHeight(Options_Main[OPT_HEIGHT].val), 0};
    // Face each other, even where the ledge squeezes HMN in under the CPU.
    hmn_facing_direction = -player_direction;
    cpu_facing_direction = player_direction;
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
    followup_length = 0;
    waveland_direction = 0;
    RollFollowup();
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

/* Fox's and Falco's grounded reflector states, as osd_context.c numbers them. */
enum { TC_SHINE_START = 360, TC_SHINE_LOOP, TC_SHINE_HIT, TC_SHINE_END, TC_SHINE_TURN };
// The waveland's air dodge: 45 degrees down, as Attack on Shield's Wavedash Away.
#define TC_WAVELAND_STICK 90

/* CPU input processing already ran at priority 2. Supply this tick's
 * processed input, which the IASA reads, and the raw input the next frame
 * decodes, as Ledgechase's and Attack on Shield's adapters do. A button is
 * pressed (input.down) only if it wasn't already held; L/R also carries the
 * synthesized HSD_PAD_LR bit, and grounded specials read cached stick
 * classifications rather than the stick. */
static void QueueInput(FighterData *data, unsigned held, int x, int y) {
    data->cpu.held = held;
    data->cpu.lstickX = x;
    data->cpu.lstickY = y;
    unsigned processed = held;
    if (held & (PAD_TRIGGER_L | PAD_TRIGGER_R)) processed |= 0x80000000u;
    data->input.down = processed & ~data->input.held;
    data->input.held = processed;
    data->input.lstick = (Vec2){x / 127.f, y / 127.f};
    data->input.trigger = processed & 0x80000000u ? 1 : 0;
    data->input.timer_specialhi = data->input.timer_speciallw =
        data->input.timer_specials = data->input.timer_specialn = 255;
    if (data->input.down & PAD_BUTTON_B) {
        ftCommonData *common = *stc_ftcommon;
        float sx = fabs(data->input.lstick.X), sy = data->input.lstick.Y;
        if (sy >= common->x21c) data->input.timer_specialhi = 0;
        if (sy < -common->x21c) data->input.timer_speciallw = 0;
        if (sx >= common->x218) data->input.timer_specials = 0;
        if (sx < common->x218 && fabs(sy) < common->x21c) data->input.timer_specialn = 0;
    }
    // A stick pushed from neutral is a fresh tilt (a smash, at full).
    if (x) data->input.timer_lstick_tilt_x = 0;
    if (y) data->input.timer_lstick_tilt_y = 0;
    if (data->input.down & 0x80000000u) data->input.timer_trigger_analog = data->input.timer_trigger_digital = 0;
}

// The current state's native IASA, which makes the regular eligibility checks.
static void RunIASA(GOBJ *cpu) {
    FighterData *data = cpu->userdata;
    if (data->flags.dead || data->flags.freeze || data->flags.hitlag || data->dmg.hitlag_frames > 0) return;
    if (data->cb.IASA) data->cb.IASA(cpu);
}

// The first state a followup's input puts the CPU in, from Wait.
static bool FollowupStarted(int followup, int state) {
    switch (followup) {
        case TC_FOLLOWUP_SPOTDODGE: return state == ASID_ESCAPE;
        case TC_FOLLOWUP_ROLL_AWAY:
        case TC_FOLLOWUP_ROLL_IN:
            return (ASID_GUARDON <= state && state <= ASID_GUARDREFLECT) || state == ASID_ESCAPEF || state == ASID_ESCAPEB;
        case TC_FOLLOWUP_SHINE: return TC_SHINE_START <= state && state <= TC_SHINE_TURN;
    }
    return false;
}

// A followup's last state, which ends in Wait: the dodge, or the waveland's landing.
static bool FollowupEndState(int state) {
    return state == ASID_ESCAPE || state == ASID_ESCAPEF || state == ASID_ESCAPEB || state == ASID_LANDINGFALLSPECIAL;
}

static void NoteFollowupLength(FighterData *data) {
    if (punish_phase != TC_PUNISH_DODGING || !FollowupEndState(data->state_id)) return;
    // A special landing's animation is slowed to last its landing lag.
    float frames = data->figatree_curr->frame_num, rate = data->state.rate;
    followup_length = rate > 0 ? (int)(frames / rate + .5f) : (int)frames;
}

/* How far the CPU's floor reaches from it toward `dir`: to its platform's
 * end, or to the end of the solid ground. Unknown counts as room. */
static float FloorRoom(FighterData *data, int dir) {
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    float x = data->phys.pos.X, left, right;
    int i = data->coll_data.ground_index;
    CollLineDesc *line = stage && lines && verts && i >= 0 && i < stage->line_num ? lines[i].desc : 0;
    if (line && IsPlatformLine(line) && line->vert_prev >= 0 && line->vert_next >= 0 &&
        line->vert_prev < stage->vert_num && line->vert_next < stage->vert_num) {
        Vec2 a = verts[line->vert_prev].pos_curr, b = verts[line->vert_next].pos_curr;
        left = a.X < b.X ? a.X : b.X;
        right = a.X > b.X ? a.X : b.X;
    } else if (!SolidGroundEdges(&left, &right)) {
        return TC_WAVELAND_ROOM;
    }
    return dir > 0 ? right - x : x - left;
}

// Away from the player, unless that would slide the CPU off its floor.
static int WavelandDirection(FighterData *data) {
    float gap = data->phys.pos.X - ((FighterData *)Fighter_GetGObj(0)->userdata)->phys.pos.X;
    int away = gap > 0 ? 1 : gap < 0 ? -1 : -player_direction;
    return TC_WavelandDirection(away, FloorRoom(data, away), FloorRoom(data, -away));
}

/* The followup's first input, on the CPU's first Wait frame. Native Wait
 * IASA makes the regular eligibility check; no tech/getup state or animation
 * length is forcibly shortened. */
static void StartFollowup(GOBJ *cpu) {
    FighterData *data = cpu->userdata;
    switch (attempt_followup) {
        case TC_FOLLOWUP_SPOTDODGE:
            // Shield and a fresh down tilt.
            data->cpu.held = PAD_TRIGGER_R;
            data->cpu.lstickX = 0;
            data->cpu.lstickY = -127;
            // Native fighter input also carries the synthesized HSD_PAD_LR bit;
            // the raw R button alone is insufficient for the spotdodge check.
            data->input.held = PAD_TRIGGER_R | 0x80000000u;
            data->input.lstick = (Vec2){0, -1};
            data->input.trigger = 1;
            data->input.timer_lstick_tilt_y = 0;
            break;
        case TC_FOLLOWUP_ROLL_AWAY:
        case TC_FOLLOWUP_ROLL_IN:
            /* Wait can't roll, only shield (Interrupt_Shield): the roll comes
             * out of GuardOn on the next frame. No fresh digital press, so it's
             * a plain shield, not a powershield (GuardReflect). */
            QueueInput(data, PAD_TRIGGER_R, 0, 0);
            data->input.down = 0;
            break;
        case TC_FOLLOWUP_SHINE:
            QueueInput(data, PAD_BUTTON_B, 0, -127); // Down B.
            break;
        default:
            return;
    }
    Fighter_IASACheck_AllGrounded(cpu);
}

/* The rest of the followup, one native step a frame: roll out of shield, or
 * hold the shine into its loop, jump out of it and air dodge into the ground
 * on the jump's first airborne frame (a waveland). */
static void ContinueFollowup(GOBJ *cpu) {
    FighterData *data = cpu->userdata;
    int state = data->state_id;
    switch (attempt_followup) {
        case TC_FOLLOWUP_ROLL_AWAY:
        case TC_FOLLOWUP_ROLL_IN:
            // Shield still held, and a fresh smash to the side (Interrupt_Roll).
            if (ASID_GUARDON <= state && state <= ASID_GUARDREFLECT && state != ASID_GUARDOFF) {
                int dir = TC_RollDirection(attempt_followup, data->phys.pos.X, -player_direction);
                QueueInput(data, PAD_TRIGGER_R, dir * 127, 0);
                RunIASA(cpu);
            }
            break;
        case TC_FOLLOWUP_SHINE:
            if (state == TC_SHINE_START) {
                data->cpu.held = PAD_BUTTON_B; // The shine lasts while B is held.
            } else if (state == TC_SHINE_LOOP) {
                waveland_direction = WavelandDirection(data);
                QueueInput(data, PAD_BUTTON_B | PAD_BUTTON_X, 0, 0); // Jump out of the shine.
                RunIASA(cpu);
            } else if ((state == ASID_JUMPF || state == ASID_JUMPB) && data->phys.air_state) {
                int x = waveland_direction * TC_WAVELAND_STICK;
                QueueInput(data, PAD_TRIGGER_R, x, x ? -TC_WAVELAND_STICK : -127);
                RunIASA(cpu);
            }
            break;
    }
}

/* Intangible on the last frame of a tech or getup, standing, or falling into
 * the tech, except while a followup is punishable: from its first state
 * until the CPU stands again, when it's intangible as after a tech. */
static bool ProtectCPU(FighterData *data) {
    int state = data->state_id;
    if (punish_phase == TC_PUNISH_DODGING) return false;
    return (ASID_DOWNSPOTD <= state && state <= ASID_PASSIVESTANDB &&
            data->state.frame + 1 == data->figatree_curr->frame_num) || // a tech/getup's last frame
        state == ASID_WAIT || state == ASID_DAMAGEFALL;
}

static int RunFollowup(GOBJ *cpu) {
    FighterData *data = cpu->userdata;
    int state = data->state_id;
    int category = ASID_DOWNSPOTD <= state && state <= ASID_PASSIVESTANDB ? TC_STATE_OPTION :
        state == ASID_WAIT ? TC_STATE_WAIT : FollowupStarted(attempt_followup, state) ? TC_STATE_DODGE : TC_STATE_OTHER;
    int action = TC_PunishStep(&punish_phase, attempt_followup != TC_FOLLOWUP_NONE, category);
    if (action == TC_ACTION_DODGE) {
        StartFollowup(cpu);
        if (FollowupStarted(attempt_followup, data->state_id)) punish_phase = TC_PUNISH_DODGING;
    } else if (punish_phase == TC_PUNISH_DODGING) {
        ContinueFollowup(cpu);
    }
    NoteFollowupLength(data);
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

/* D-Pad left/right picks HMN's side and restarts, like the Player Side row.
 * Pressing the side already chosen only restarts, as D-Pad left always has,
 * so manually moved fighters stay where they were put. */
static void ReadDPad(int down) {
    int side = down & HSD_BUTTON_DPAD_LEFT ? TC_SIDE_LEFT : down & HSD_BUTTON_DPAD_RIGHT ? TC_SIDE_RIGHT : -1;
    if (side < 0) return;
    if (side == Options_Main[OPT_SIDE].val) {
        Reset();
        return;
    }
    Options_Main[OPT_SIDE].val = Options_Main[OPT_SIDE].val_prev = side;
    ChangeSide(0, side);
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

    int punish_action = reset_timer == -1 ? RunFollowup(cpu) : TC_ACTION_NONE;
    int state_id = cpu_data->state_id;
    /* The followup is punishable from its first state until the CPU stands
     * again. Then, like a tech that ends, it's intangible. */
    bool punish_recovery = punish_phase == TC_PUNISH_DODGING;
    
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
    if (ProtectCPU(cpu_data)) {
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
        && (attempt_followup == TC_FOLLOWUP_NONE || punish_phase == TC_PUNISH_FINISHED)
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
        // tech if airborne (not the shine's waveland jump)
        if (cpu_data->phys.air_state == 1 && punish_phase < TC_PUNISH_DODGING) {
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
                    // A hit earlier in the followup (the shine or its jump) has no timing.
                    if ((punish_recovery && FollowupEndState(prev_state_id)) ||
                        (!punish_recovery && ASID_DOWNSPOTD <= prev_state_id && prev_state_id <= ASID_PASSIVESTANDB)) {
                        frames_taken = cpu_data->TM.state_prev_frames[i];
                        break;
                    }
                }

                if (frames_taken != -1) {
                    int option_length = punish_recovery ? followup_length : getup_option_length;
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

    ReadDPad(pad->down);

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
    Options_Main[OPT_SIDE].val = TM_GetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_SIDE);
    LoadFollowup();
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
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_PRESET, TC_PresetToSaved(value));
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
static void ChangeSide(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_SIDE, value);
    automatic_position = true;
    Reset();
}
static void ChangeFollowup(GOBJ *menu, int value) {
    RefreshFollowupChoices();
    value = TC_StepFollowup(value, Options_Main[OPT_FOLLOWUP].val_prev, can_shine);
    Options_Main[OPT_FOLLOWUP].val = value;
    TM_SetSetting(TM_SETTING_TECHCHASE, TM_TECHCHASE_FOLLOWUP, value);
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
