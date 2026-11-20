#include "events.h"
#include "stage_swap.h"
#include "zoom_camera.h"

/* Up-B Sweetspot: start on an angel platform off one ledge and grab the ledge
 * with your up-B. Marth swings his forward smash or down-tilt as each attempt
 * starts and freezes with the tipper out, so its hitboxes are simply there
 * whenever you arrive and overshooting onto the stage gets you hit. The guides
 * and every number behind them are in notes/UPB-SWEETSPOT-RESEARCH.md. */

// The angel platform's spot, out from the ledge and up from it, for anyone
// Start_Spots leaves out: Ness and the Ice Climbers.
#define PLATFORM_OUT 40
#define PLATFORM_UP 15
// The platform stays at least this far above the bottom blast zone (Game &
// Watch's spot is under Yoshi's Story's) and this far inside the side one
// (Bowser's is 10 short of Yoshi's Story's).
#define PLATFORM_BLAST_MARGIN 35
#define PLATFORM_SIDE_MARGIN 20
// Nana's platform, beside Popo's on the stage side.
#define NANA_PLATFORM_GAP 8

/* Where each fighter's angel platform sits, out from the ledge and up from it
 * (where their feet are), so the spot their up-B sweetspots from is in easy
 * reach. Worked out from the ISO's numbers; see "Start spots" in
 * notes/UPB-SWEETSPOT-RESEARCH.md.
 * - Paths: 20 above the blue dot, and at least 15 out so the platform clears
 *   the stage's wall.
 * - Aimed: 20 above the white X, the full travel out. Fall a little, up-B, and
 *   hold straight toward the stage. Fox's and Falco's charge drops about 6.
 * - Donkey Kong, Bowser, Link and Young Link: their up-Bs go a long way
 *   sideways, farther still with the drift you carry into them. The spots
 *   that sweetspot form a narrow band, farther out the higher you are, so
 *   they start just beyond it: fall holding toward the stage and up-B
 *   (Bowser, who keeps all his drift, after about 10 frames of it). Starting
 *   inside the band, as they did at first, drifting in overshoots. */
typedef struct StartSpot {
    s8 out, up;
} StartSpot;

static const StartSpot Start_Spots[FTKIND_ROY + 1] = {
    [FTKIND_MARIO]     = { 25, -35 },
    [FTKIND_DRMARIO]   = { 25, -35 },
    [FTKIND_LUIGI]     = { 15, -41 },
    [FTKIND_MARTH]     = { 17, -44 },
    [FTKIND_ROY]       = { 19, -42 },
    [FTKIND_FALCON]    = { 18, -35 },
    [FTKIND_GANONDORF] = { 21, -39 },
    [FTKIND_PEACH]     = { 15, -32 },
    [FTKIND_KIRBY]     = { 15, -41 },
    [FTKIND_SAMUS]     = { 15, -37 },
    [FTKIND_GAW]       = { 16, -71 },
    [FTKIND_FOX]       = { 88, 7 },
    [FTKIND_FALCO]     = { 67, 7 },
    [FTKIND_SHEIK]     = { 47, 5 },
    [FTKIND_ZELDA]     = { 91, 10 },
    [FTKIND_MEWTWO]    = { 57, 6 },
    [FTKIND_PIKACHU]   = { 55, 9 },
    [FTKIND_PICHU]     = { 41, 14 },
    [FTKIND_DK]        = { 105, 35 },
    [FTKIND_BOWSER]    = { 110, 15 },
    [FTKIND_LINK]      = { 85, 35 },
    [FTKIND_YOUNGLINK] = { 100, 35 },
};

// Marth's forward smash is out on frames 10 to 13 and his down-tilt from frame
// 7, each with the tipper as hitbox 3 (20% and 10%). He freezes with the whole
// attack out: the forward smash on its last frame, the sword all the way down
// (its script clears the hitboxes at 14), and the down-tilt on its first, as
// Side-B does. The forward smash's tipper is centered this far inland of the
// ledge, 0 being right over it. The down-tilt stands where Side-B's does, so its
// tip reaches past the ledge the same way.
#define MARTH_TIPPER_HITBOX 3
#define MARTH_FSMASH_FRAME 13
#define MARTH_DTILT_FRAME 7
#define MARTH_TIPPER_INLAND 0
#define MARTH_DTILT_INLAND 15

// How far an animated up-B has to rise before its angle is taken. Any closer and
// a frame of drift swings it a long way.
#define PATH_ANGLE_RISE 6

#define RESET_SUCCESS 60
#define RESET_FAILURE 30

// Fox and Falco's charge animation (SpecialHiHoldAir) is 43 frames; the stick
// is read as it ends.
#define SPACIES_CHARGE_FRAMES 43

#define ARC_STEP 4 // Degrees of ring per line.
// Fox's and Falco's ring: 2/3 of a circle, ending 30 degrees short of level
// under the stage so it doesn't stick up into the stage.
#define UNDER_STAGE_SWEEP 240
// Half the length of Fox's and Falco's ticks where holding straight sweetspots.
#define LEVEL_TICK 6
// Degrees either side of each straight mark that a bit of ring runs through
// it, at the mark's distance.
#define STRAIGHT_ARC 2
// B0XX Angles' dots, and how close two spots are to be one dot: holding B
// moves Fox's and Falco's under half a degree.
#define B0XX_DOT_SIZE 30
#define B0XX_SAME_SPOT 1.0f
// Half the width of the X, and of the bar over it where coming down snaps.
#define TARGET_SIZE 2.5f

// The side and the totals survive Change Stage's restart; the menu is saved.
#define UPB_CARRY_TAG 0x55704273 /* "UpBs" */

static Vec2 ledge_positions[2];

static void GetLedgePositions(Vec2 coords_out[2]) {
    static char ledge_ids[34][2] = {
        { 0xFF, 0xFF }, { 0xFF, 0xFF }, { 0x03, 0x07 }, { 0x33, 0x36 },
        { 0x03, 0x0D }, { 0x29, 0x45 }, { 0x05, 0x11 }, { 0x09, 0x1A },
        { 0x02, 0x06 }, { 0x15, 0x17 }, { 0x00, 0x00 }, { 0x43, 0x4C },
        { 0x00, 0x00 }, { 0x00, 0x00 }, { 0x0E, 0x0D }, { 0x00, 0x00 },
        { 0x00, 0x05 }, { 0x1E, 0x2E }, { 0x0C, 0x0E }, { 0x02, 0x04 },
        { 0x03, 0x05 }, { 0x00, 0x00 }, { 0x06, 0x12 }, { 0x00, 0x00 },
        { 0xD7, 0xE2 }, { 0x00, 0x00 }, { 0x00, 0x00 }, { 0x00, 0x00 },
        { 0x03, 0x05 }, { 0x03, 0x0B }, { 0x06, 0x10 }, { 0x00, 0x05 },
        { 0x00, 0x02 }, { 0x01, 0x01 },
    };

    int stage_id = Stage_GetExternalID();
    char left_id = ledge_ids[stage_id][0];
    char right_id = ledge_ids[stage_id][1];

    Vec3 pos;
    Stage_GetLeftOfLineCoordinates(left_id, &pos);
    coords_out[0] = (Vec2) { pos.X, pos.Y };
    Stage_GetRightOfLineCoordinates(right_id, &pos);
    coords_out[1] = (Vec2) { pos.X, pos.Y };
}

static bool IsLegalStage(void) {
    int stage = Stage_GetExternalID();
    for (int i = 0; i < STAGE_SWAP_COUNT; i++)
        if (StageSwap_Stages[i] == stage)
            return true;
    return false;
}

// Fighter helpers -------------------------------------------------------

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

static void PlacePlayerOnGround(GOBJ *fighter) {
    FighterData *data = fighter->userdata;

    Vec3 pos, line_unk;
    int line_idx, line_kind;
    if (GrColl_RaycastGround(&pos, &line_idx, &line_kind, &line_unk, -1, -1, -1, 0,
            data->phys.pos.X, data->phys.pos.Y + 10, data->phys.pos.X, data->phys.pos.Y - 100, 0)) {
        data->phys.pos.X = pos.X;
        data->phys.pos.Y = pos.Y;
        data->coll_data.ground_index = line_idx;
    }
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(data);
}

static void UpdateCameraBox(GOBJ *fighter) {
    Fighter_UpdateCameraBox(fighter);

    FighterData *data = fighter->userdata;
    CmSubject *subject = data->camera_subject;
    subject->boundleft_curr = subject->boundleft_proj;
    subject->boundright_curr = subject->boundright_proj;

    Match_CorrectCamera();
}

static void ClearVelocity(FighterData *ft) {
    ft->phys.self_vel = (Vec3){0, 0, 0};
    ft->phys.kb_vel = (Vec3){0, 0, 0};
    ft->phys.self_vel_ground = (Vec3){0, 0, 0};
    ft->phys.nudge_vel = (Vec2){0, 0};
}

// Nana follows where Popo was a few frames ago; point that history at him.
static void SeedFollower(GOBJ *sub, GOBJ *leader) {
    FighterData *main_data = leader->userdata;
    FighterData *sub_data = sub->userdata;
    for (u32 i = 0; i < countof(sub_data->cpu.leader_log); ++i) {
        CPULeaderLog *log = &sub_data->cpu.leader_log[i];
        log->pos = main_data->phys.pos;
        log->facing_direction = main_data->facing_direction;
    }
}

// RebirthWait counts this down and drops you off at 0; keep it from getting there.
static void RebirthWait_Phys(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->state_var.state_var1 = 2;
}

/* The game's own RebirthWait still decides when you leave the platform, so
 * you can jump, fall or up-B straight off it. */
static void PlaceOnPlatform(GOBJ *fighter, float x, float y, float facing) {
    FighterData *ft = fighter->userdata;
    Fighter_EnterSleep(fighter, ft->flags.ms);
    Fighter_EnterRebirth(fighter);
    ft->facing_direction = facing;
    ClearVelocity(ft);
    ft->phys.pos.X = x;
    ft->phys.pos.Y = y;
    UpdatePosition(fighter);
    Fighter_EnterRebirthWait(fighter);
    ft->cb.Phys = RebirthWait_Phys;
    Fighter_UpdateRebirthPlatformPos(fighter);
    ft->jump.jumps_used = 1; // The double jump is still there after the platform.
    UpdateCameraBox(fighter);
}

// Where the fighter's platform goes, from the ledge on this side.
static Vec2 PlatformSpot(FighterData *ft, Vec2 ledge, float outward) {
    StartSpot spot = { PLATFORM_OUT, PLATFORM_UP };
    if (ft->kind >= 0 && ft->kind <= FTKIND_ROY && Start_Spots[ft->kind].out)
        spot = Start_Spots[ft->kind];
    Vec2 at = { ledge.X + outward * spot.out, ledge.Y + spot.up };
    float lowest = Stage_GetBlastzoneBottom() + PLATFORM_BLAST_MARGIN;
    if (at.Y < lowest)
        at.Y = lowest;
    float side = outward > 0 ? Stage_GetBlastzoneRight() - PLATFORM_SIDE_MARGIN
                             : Stage_GetBlastzoneLeft() + PLATFORM_SIDE_MARGIN;
    if (outward * (at.X - side) > 0)
        at.X = side;
    return at;
}

/* Leaving the platform gives respawn invincibility, which Marth's tipper
 * couldn't hit through. At 1 the game ends it next frame, flashing included. */
static void EndRespawnInvincibility(FighterData *ft) {
    if (ft->hurt.intang_frames.respawn > 1)
        ft->hurt.intang_frames.respawn = 1;
}

// Up-B data -------------------------------------------------------------

enum guide {
    GUIDE_NONE,  // The ledge target only.
    GUIDE_AIMED, // A ring at the travel distance, and the angle you're holding.
    GUIDE_PATH,  // Where an animated path should start.
};

/* Each fighter's up-B. States are its action states from the decompilation,
 * grounded and aerial; clones share their original's numbers. Path numbers
 * are the root motion in the ISO (scripts/fighter_move_data.py) at normal
 * size: the rise and forward movement at the peak, the fastest grab, and at
 * the end of the animation, a slower grab falling back to the ledge. Samus's
 * air Screw Attack is physics instead: 2.5 up against 0.066 gravity.
 * tilt_attr is the special attribute holding how many degrees holding toward
 * the stage turns the move before it locks; FullTilt has Game & Watch's. */
typedef struct UpBInfo {
    u16 first, last;
    u8 guide;
    u8 tilt_attr;          // GUIDE_PATH; 0 for none.
    u16 charge;            // GUIDE_AIMED: the aerial start-up, which reads the stick as it ends.
    float rise, forward;   // GUIDE_PATH
    float end_rise, end_forward;
} UpBInfo;

static const UpBInfo UpB_Info[FTKIND_ROY + 1] = {
    [FTKIND_MARIO]     = { 347, 348, GUIDE_PATH, 0x28, 0, 40.4f, 17.6f, 33.3f, 20.9f },
    [FTKIND_DRMARIO]   = { 347, 348, GUIDE_PATH, 0x28, 0, 40.4f, 17.6f, 33.3f, 20.9f },
    [FTKIND_LUIGI]     = { 355, 356, GUIDE_PATH, 0, 0, 45.9f, 0, 37.9f, 0 },
    [FTKIND_MARTH]     = { 367, 368, GUIDE_PATH, 0x38, 0, 47.3f, 10.2f, 34.5f, 11.5f },
    [FTKIND_ROY]       = { 367, 368, GUIDE_PATH, 0x38, 0, 44.5f, 12.3f, 32.4f, 13.0f },
    [FTKIND_FALCON]    = { 353, 356, GUIDE_PATH, 0, 0, 37.6f, 11.9f, 13.1f, 20.2f },
    [FTKIND_GANONDORF] = { 353, 356, GUIDE_PATH, 0, 0, 41.9f, 13.2f, 14.6f, 22.5f },
    [FTKIND_PEACH]     = { 361, 364, GUIDE_PATH, 0, 0, 36.8f, 3.4f, 34.4f, 4.1f },
    [FTKIND_KIRBY]     = { 385, 392, GUIDE_PATH, 0, 0, 52.2f, 0, 0.3f, 0 },
    [FTKIND_SAMUS]     = { 353, 354, GUIDE_PATH, 0, 0, 47.3f, 0, 44.0f, 0 },
    [FTKIND_GAW]       = { 373, 374, GUIDE_PATH, 0, 0, 75.6f, 9.1f, 65.8f, 11.9f },
    [FTKIND_FOX]       = { 353, 359, GUIDE_AIMED, 0, 354 },
    [FTKIND_FALCO]     = { 353, 359, GUIDE_AIMED, 0, 354 },
    [FTKIND_SHEIK]     = { 355, 360, GUIDE_AIMED, 0, 358 },
    [FTKIND_ZELDA]     = { 349, 354, GUIDE_AIMED, 0, 352 },
    [FTKIND_MEWTWO]    = { 353, 358, GUIDE_AIMED, 0, 356 },
    [FTKIND_PIKACHU]   = { 353, 358, GUIDE_AIMED, 0, 356 },
    [FTKIND_PICHU]     = { 353, 358, GUIDE_AIMED, 0, 356 },
    [FTKIND_DK]        = { 381, 382, GUIDE_NONE },
    [FTKIND_BOWSER]    = { 359, 360, GUIDE_NONE },
    [FTKIND_LINK]      = { 356, 357, GUIDE_NONE },
    [FTKIND_YOUNGLINK] = { 356, 357, GUIDE_NONE },
    [FTKIND_NESS]      = { 358, 366, GUIDE_NONE },
    [FTKIND_POPO]      = { 347, 356, GUIDE_NONE },
    [FTKIND_NANA]      = { 347, 356, GUIDE_NONE },
    // Yoshi and Jigglypuff have no recovery up-B and aren't on the CSS.
};

static const UpBInfo *GetUpBInfo(FighterData *ft) {
    if (ft->kind < 0 || ft->kind > FTKIND_ROY || !UpB_Info[ft->kind].last)
        return 0;
    return &UpB_Info[ft->kind];
}

static bool IsSpacie(FighterData *ft) {
    return ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO;
}

#define ATTR_F(ft, offset) (((float *)(ft)->special_attributes)[(offset) / 4])
#define ATTR_I(ft, offset) (((int *)(ft)->special_attributes)[(offset) / 4])

// Degrees a full stick toward the stage turns a path, or 0 if it can't. Game &
// Watch's attribute is radians per unit of stick past 0x58, not a top angle.
static float FullTilt(FighterData *ft, const UpBInfo *info) {
    if (ft->kind == FTKIND_GAW)
        return (1 - ATTR_F(ft, 0x58)) * ATTR_F(ft, 0x5C) / M_1DEGREE;
    return info->tilt_attr ? ATTR_F(ft, info->tilt_attr) : 0;
}

/* The least stick the aimed up-Bs take before they go straight up. Fox and
 * Falco add the two axes (|x| + |y|); the rest use the stick's length. */
static float AimedThreshold(FighterData *ft) {
    switch (ft->kind) {
        case FTKIND_FOX: case FTKIND_FALCO: return ATTR_F(ft, 0x64);
        case FTKIND_SHEIK: return ATTR_F(ft, 0x40);
        case FTKIND_ZELDA: return ATTR_F(ft, 0x50);
        case FTKIND_MEWTWO: return ATTR_F(ft, 0x58);
        case FTKIND_PIKACHU: case FTKIND_PICHU: return ATTR_F(ft, 0x8C);
    }
    return 1;
}

/* The teleports and Quick Attack's first zip move at a constant speed for a
 * set number of frames: speed = per_tilt * tilt + base. */
static int TravelFrames(FighterData *ft) {
    switch (ft->kind) {
        case FTKIND_SHEIK: return ATTR_I(ft, 0x38);
        case FTKIND_ZELDA: return ATTR_I(ft, 0x48);
        case FTKIND_MEWTWO: return ATTR_I(ft, 0x50);
        case FTKIND_PIKACHU: case FTKIND_PICHU: return ATTR_I(ft, 0x60);
    }
    return 0;
}

static float TravelSpeed(FighterData *ft, float tilt) {
    switch (ft->kind) {
        case FTKIND_SHEIK: return ATTR_F(ft, 0x44) * tilt + ATTR_F(ft, 0x48);
        case FTKIND_ZELDA: return ATTR_F(ft, 0x54) * tilt + ATTR_F(ft, 0x58);
        case FTKIND_MEWTWO: return ATTR_F(ft, 0x5C) * tilt + ATTR_F(ft, 0x60);
        case FTKIND_PIKACHU: case FTKIND_PICHU: return ATTR_F(ft, 0x90) * tilt + ATTR_F(ft, 0x94);
    }
    return 0;
}

/* How far the travel goes, from the live attributes, at a stick length of
 * tilt (the threshold up to 1).
 * - Firefox and Firebird ignore tilt: x74 speed for x68 frames, slowing by x78
 *   a frame from frame x70. Fox 81.5, Falco 60.1.
 * - The rest: Sheik 30-40, Zelda 62-84, Mewtwo 40-50, Pikachu 39.5-49.5,
 *   Pichu 26.4-35.2. */
static float AimedDistance(FighterData *ft, float tilt) {
    if (!IsSpacie(ft))
        return TravelFrames(ft) * TravelSpeed(ft, tilt);
    float speed = ATTR_F(ft, 0x74), slow = ATTR_F(ft, 0x78), distance = 0;
    int frames = ATTR_F(ft, 0x68), slow_from = ATTR_F(ft, 0x70);
    for (int i = 1; i <= frames; i++) {
        if (i >= slow_from)
            speed -= slow;
        distance += speed;
    }
    return distance;
}

// Event state -----------------------------------------------------------

static void Exit(GOBJ *menu);
static void ChangeGuides(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_GUIDES, value);
}
static void ChangeB0XX(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_B0XX, value);
}
static void ChangeLanding(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_LANDING, value);
}
static void ChangeMarth(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_MARTH, value);
}
static void ChangeCamera(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_CAMERA, value);
    ZoomCamera_Apply(value);
}

enum menu_options {
    OPT_GUIDES,
    OPT_B0XX,
    OPT_LANDING,
    OPT_MARTH,
    OPT_CAMERA,
};

static const char *Values_Marth[] = {"Forward Smash", "Down Tilt", "Off"};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Enable Line Guides",
        .val = 1,
        .val_prev = 1,
        .OnChange = ChangeGuides,
        .desc = {"Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu, Pichu:",
                 "up-B on the blue ring, then hold the white line. A",
                 "gap only aims straight, at its tick. Others: up-B",
                 "at the blue dot (the dim one: stick to the stage)."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "B0XX Angles",
        .OnChange = ChangeB0XX,
        .desc = {"Aimed up-Bs: dots where the B0XX's angles sweetspot",
                 "instead of the ring. White: no modifier. Blue: Mod",
                 "X or Mod Y, any C-button. Dim: teleports and Quick",
                 "Attack without B held, which don't go as far."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Landing Line",
        .OnChange = ChangeLanding,
        .desc = {"Fox and Falco: a pink line for the lowest you can",
                 "up-B from and still land on the stage. From above",
                 "it, aim just over the ledge to land. Below it, only",
                 "the ledge is in reach."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Marth",
        .values = Values_Marth,
        .value_num = countof(Values_Marth),
        .OnChange = ChangeMarth,
        .desc = {"Marth swings as each attempt starts and freezes",
                 "with the tipper right over the ledge, so",
                 "overshooting onto the stage gets you hit.",
                 "Takes effect on the next reset."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Camera",
        .value_num = 2,
        .values = (const char *[]) { ZOOM_CAMERA_VALUES },
        .OnChange = ChangeCamera,
        .desc = {"Zoom out to see farther offstage,",
                 "down to the bottom blast zone."},
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Change Stage",
        .desc = {"Restart on another tournament-legal stage.",
                 "Your settings and success rate are kept."},
        .menu = &StageSwap_Menu,
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Help",
        .desc = {"Leave the platform and recover with your up-B.",
                 "Coming down, aim at the bar over the X: you grab there.",
                 "D-Pad left/right resets on that ledge.",
                 "A D-Pad reset doesn't count as an attempt."}
    },
    {
        .kind = OPTKIND_FUNC,
        .name = "Exit",
        .desc = {"Return to the Event Select Screen."},
        .OnSelect = Exit,
    },
};

static EventMenu Menu_Main = {
    .on_exit = Exit,
    .name = "Up-B Sweetspot",
    .option_num = sizeof(Options_Main) / sizeof(EventOption),
    .options = Options_Main,
};

enum attempt_result {
    RESULT_NONE,
    RESULT_SWEETSPOT,
    RESULT_HIT,
    RESULT_FELL,
    RESULT_LANDED,
    RESULT_NO_UPB,
};
static const char *Result_Text[] = {
    "-", "Sweetspot", "Got Hit", "Fell", "Landed", "No Up-B",
};

static int side_idx = 0;     // 0 is the left ledge, 1 the right.
static int reset_timer = 0;  // Counts down to a reset; 0 when off.
static bool restarting = false;

// This attempt. A D-pad reset starts a new one without counting it.
static struct attempt {
    bool counted;
    bool done;               // Counted; waiting for the reset.
    bool has_target;
    Vec2 target;             // Where your position grabs the ledge.
    Vec2 snap;               // Where coming down onto the ledge first grabs it.
    float reach;             // The grab box's reach out from the ledge.
} attempt = { 0 };

static struct upb {
    bool used;
    int frames;              // Since it started.
    Vec2 start;
    // GUIDE_PATH: where the angle it set out at leads, at the path's full length.
    bool path_set;
    Vec2 path_end;
    // GUIDE_AIMED: where the travel starts and where the stick sends it.
    bool aiming, locked;
    Vec2 launch, aim;
    float distance;
} upb = { 0 };

static struct marth {
    bool swinging, frozen, measured;
    bool dtilt;              // This swing's attack; the menu waits for a reset.
} marth = { 0 };
// How far in front of Marth the tipper is on its first frame. A guess until
// the first swing measures it; later resets place him with the measured value.
static float tipper_reach = 24;

// This session's totals and the last counted attempt.
typedef struct Stats {
    int total, successes;
    int last, last_frames;
} Stats;
static Stats stats = { 0 };

typedef struct UpBCarry {
    int side;
    float tipper_reach;
    Stats stats;
} UpBCarry;

static float Outward(void) { return side_idx * 2 - 1; }

static void CountAttempt(int result) {
    if (attempt.counted)
        return;
    attempt.counted = true;
    stats.last = result;
    stats.last_frames = upb.frames;
    if (stats.total < 999999) {
        stats.total++;
        if (result == RESULT_SWEETSPOT)
            stats.successes++;
    }
}

/* The ledge-grab box (mpColl_80044164) reaches cliffgrab_width past the ECB's
 * side toward the ledge (the side it faces), and is cliffgrab_height tall,
 * centered cliffgrab_y_offset above the feet. So it holds the ledge while your
 * position is from the ledge out to that reach, and from cliffgrab_y_offset ±
 * cliffgrab_height / 2 below it.
 * - The X (target) is the middle of that: the most room for an up-B that ends
 *   there, level or rising, and then falls into the grab.
 * - The game checks the box only while you move down, and then grabs as soon as
 *   you're in it. So coming down, the travel grabs at the top of the box, on its
 *   way to the X: snap is that spot, above the X. */
static void SetTargets(FighterData *ft) {
    Vec2 ledge = ledge_positions[side_idx];
    float front = ft->facing_direction > 0 ? ft->coll_data.ecbCurr_right.X : -ft->coll_data.ecbCurr_left.X;
    CollData *coll = &ft->coll_data;
    attempt.reach = front + coll->cliffgrab_width;
    attempt.target = (Vec2) { ledge.X + Outward() * attempt.reach * 0.5f, ledge.Y - coll->cliffgrab_y_offset };
    attempt.snap = (Vec2) { attempt.target.X, attempt.target.Y + coll->cliffgrab_height * 0.5f };
    attempt.has_target = true;
}

/* Whether a travel from `from` to `to` goes through the grab box's positions
 * (SetTargets). It only counts moving down, the only time the box is checked. */
static bool CrossesGrabBox(FighterData *ft, Vec2 from, Vec2 to) {
    Vec2 ledge = ledge_positions[side_idx];
    float outward = Outward(), half = ft->coll_data.cliffgrab_height * 0.5f;
    float bottom = attempt.target.Y - half, top = attempt.target.Y + half;
    // Liang-Barsky, out from the ledge and up: where the segment is inside.
    float x = (from.X - ledge.X) * outward, dx = (to.X - from.X) * outward, dy = to.Y - from.Y;
    float p[4] = { -dx, dx, -dy, dy };
    float q[4] = { x, attempt.reach - x, from.Y - bottom, top - from.Y };
    float enter = 0, leave = 1;
    if (dy >= 0)
        return false;
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) {
            if (q[i] < 0)
                return false;
            continue;
        }
        float t = q[i] / p[i];
        if (p[i] < 0 && t > enter)
            enter = t;
        else if (p[i] > 0 && t < leave)
            leave = t;
    }
    return enter <= leave;
}

// Marth -----------------------------------------------------------------

static void PlaceMarth(GOBJ *cpu) {
    FighterData *cpu_data = cpu->userdata;
    Vec2 ledge = ledge_positions[side_idx];
    float outward = Outward();
    cpu_data->facing_direction = outward;
    ClearVelocity(cpu_data);
    int dtilt = Options_Main[OPT_MARTH].val == TM_UPB_MARTH_DTILT;
    cpu_data->phys.pos.X = ledge.X - outward * (dtilt ? MARTH_DTILT_INLAND : MARTH_TIPPER_INLAND + tipper_reach);
    cpu_data->phys.pos.Y = ledge.Y;
    UpdatePosition(cpu);
    PlacePlayerOnGround(cpu);
    Fighter_EnterWait(cpu);
    UpdateCameraBox(cpu);
}

// Swing as the attempt starts: nothing about hitting you depends on your timing.
static void StartMarth(GOBJ *cpu) {
    FighterData *cpu_data = cpu->userdata;
    int attack = Options_Main[OPT_MARTH].val;
    if (attack == TM_UPB_MARTH_OFF || marth.swinging || cpu_data->state_id != ASID_WAIT)
        return;
    marth.dtilt = attack == TM_UPB_MARTH_DTILT;
    if (marth.dtilt) {
        ActionStateChange(0, 1, 0, cpu, ASID_ATTACKLW3, 0, 0);
        Fighter_AdvanceScript(cpu);
    } else {
        FSmash_GetASForAnalogAngle(cpu);
    }
    marth.swinging = true;
}

static bool MarthIsAttacking(FighterData *cpu_data) {
    int state = cpu_data->state_id;
    return marth.dtilt ? state == ASID_ATTACKLW3 : state >= ASID_ATTACKS4HI && state <= ASID_ATTACKS4LW;
}

/* Hold the attack so its hitboxes linger, setting the freeze every frame the way
 * Side-B does. The forward smash's first freeze measures how far in front of
 * Marth the tipper is and moves him so it sits MARTH_TIPPER_INLAND in. */
static void MarthThink(GOBJ *cpu) {
    FighterData *cpu_data = cpu->userdata;
    int freeze_frame = marth.dtilt ? MARTH_DTILT_FRAME : MARTH_FSMASH_FRAME;
    // After the result, let the swing finish instead.
    if (!marth.swinging || !MarthIsAttacking(cpu_data) || attempt.done
        || cpu_data->state.frame < freeze_frame)
        return;
    FrameSpeedChange(cpu, 0);
    marth.frozen = true;
    ftHit *tipper = &cpu_data->hitbox[MARTH_TIPPER_HITBOX];
    if (marth.dtilt || marth.measured || !tipper->active)
        return;
    marth.measured = true;
    float reach = (tipper->pos.X - cpu_data->phys.pos.X) * cpu_data->facing_direction;
    if (reach < 5 || reach > 60) // Not a believable sword; keep the last placement.
        return;
    tipper_reach = reach;
    float wanted = ledge_positions[side_idx].X - Outward() * MARTH_TIPPER_INLAND;
    float shift = wanted - tipper->pos.X;
    if (fabs(shift) > 0.5f) {
        cpu_data->phys.pos.X += shift;
        UpdatePosition(cpu);
        // Move the hitboxes too, so they don't sweep across the gap next frame.
        for (int i = 0; i < (int)countof(cpu_data->hitbox); i++) {
            cpu_data->hitbox[i].pos.X += shift;
            cpu_data->hitbox[i].pos_prev.X += shift;
        }
    }
}

// Ends the attempt: a reset follows after `delay` frames.
static void Finish(GOBJ *cpu, int result, int delay) {
    CountAttempt(result);
    SFX_Play(result == RESULT_SWEETSPOT ? 0xAD : 0xAF);
    if (marth.frozen)
        FrameSpeedChange(cpu, 1.0);
    attempt.done = true;
    reset_timer = delay;
}

// Aim -------------------------------------------------------------------

/* Fox and Falco can't drift while charging, so where the travel starts is
 * known: Y speed starts at 0, gravity (x60) waits x54 frames, and X speed
 * slows by x5C a frame. The others start where they are. */
static Vec2 PredictLaunch(FighterData *ft) {
    Vec2 pos = { ft->phys.pos.X, ft->phys.pos.Y };
    if (!IsSpacie(ft))
        return pos;
    float delay = ATTR_F(ft, 0x54), slow = ATTR_F(ft, 0x5C), fall = ATTR_F(ft, 0x60);
    float vx = ft->phys.self_vel.X, vy = ft->phys.self_vel.Y;
    for (int f = ft->TM.state_frame + 1; f < SPACIES_CHARGE_FRAMES; f++) {
        if (f > delay) {
            vy -= fall;
            if (vy < -ft->attr.terminal_velocity)
                vy = -ft->attr.terminal_velocity;
        }
        vx = vx > slow ? vx - slow : vx < -slow ? vx + slow : 0;
        pos.X += vx;
        pos.Y += vy;
    }
    return pos;
}

/* Where the stick would send the up-B if it locked now, by the move's own
 * rule: along the stick, or straight up at full distance below the threshold. */
static void AimFromStick(FighterData *ft) {
    float x = ft->input.lstick.X, y = ft->input.lstick.Y;
    float length = sqrtf(x * x + y * y);
    float tilt = length > 1 ? 1 : length;
    bool aimed = IsSpacie(ft) ? fabs(x) + fabs(y) >= AimedThreshold(ft) : tilt > AimedThreshold(ft);
    if (aimed && length > 0) {
        upb.aim = (Vec2) { x / length, y / length };
        upb.distance = AimedDistance(ft, tilt);
    } else {
        upb.aim = (Vec2) { 0, 1 };
        upb.distance = AimedDistance(ft, 1);
    }
}

static void TrackAim(FighterData *ft, const UpBInfo *info) {
    if (info->guide != GUIDE_AIMED || upb.locked)
        return;
    int state = ft->state_id;
    if (state == info->charge) {
        upb.aiming = true;
        upb.launch = PredictLaunch(ft);
        AimFromStick(ft);
        return;
    }
    // The charge has ended: the travel's velocity is the direction it took.
    Vec3 vel = ft->phys.self_vel;
    float speed = sqrtf(vel.X * vel.X + vel.Y * vel.Y);
    if (!upb.aiming || state < info->first || state > info->last || speed < 0.01f)
        return;
    upb.locked = true;
    upb.launch = (Vec2) { ft->phys.pos_prev.X, ft->phys.pos_prev.Y };
    upb.aim = (Vec2) { vel.X / speed, vel.Y / speed };
    upb.distance = IsSpacie(ft) ? AimedDistance(ft, 1) : TravelFrames(ft) * speed;
}

/* The angle the up-B set out at, taken once from where it started, and then left
 * alone however the rest of the path curves or drifts. It's measured because a
 * stick tilt locks early in lstick_angle, the air up-Bs don't follow UpB_Info's
 * numbers exactly (see the notes), and drift, all that steers Falcon and
 * Ganondorf, can't be known ahead of time. Mario and Peach drop before they rise,
 * so only rising counts. */
static void TrackPathAngle(FighterData *ft, const UpBInfo *info) {
    int state = ft->state_id;
    if (!info || info->guide != GUIDE_PATH || upb.path_set || state < info->first || state > info->last)
        return;
    float x = ft->phys.pos.X - upb.start.X, y = ft->phys.pos.Y - upb.start.Y;
    if (y < PATH_ANGLE_RISE)
        return;
    float travelled = sqrtf(x * x + y * y);
    float length = sqrtf(info->rise * info->rise + info->forward * info->forward) * ft->scale.Y;
    upb.path_set = true;
    upb.path_end = (Vec2) { upb.start.X + x / travelled * length, upb.start.Y + y / travelled * length };
}

// Degrees above level, toward the stage.
static int Degrees(Vec2 v) {
    float degrees = atan2(v.Y, -v.X * Outward()) / M_1DEGREE;
    return (int)(degrees + (degrees < 0 ? -0.5f : 0.5f));
}

// Drawing ---------------------------------------------------------------

static const GXColor White = { 0xff, 0xff, 0xff, 0xff };
static const GXColor Blue = { 0x30, 0xc8, 0xff, 0xff };
static const GXColor DimBlue = { 0x1c, 0x6c, 0x94, 0xff };
static const GXColor Green = { 0x00, 0xff, 0x00, 0xff };
static const GXColor Yellow = { 0xff, 0xe0, 0x00, 0xff };
static const GXColor Red = { 0xff, 0x00, 0x00, 0xff };
static const GXColor Pink = { 0xff, 0x60, 0xe0, 0xff };

/* on_top draws over the stage, which is what Fox's and Falco's marks need to be
 * seen from under it. */
static void DrawLine(Vec2 a, Vec2 b, GXColor color, int width, bool on_top) {
    event_vars->GFX_Start(2, (GFX_Params) { .shape = GX_LINES, .size = width, .no_depth = on_top });
    GFX_AddVtx(a.X, a.Y, 0, color);
    GFX_AddVtx(b.X, b.Y, 0, color);
}

static void DrawDot(Vec2 at, GXColor color, bool on_top) {
    event_vars->GFX_Start(1, (GFX_Params) { .shape = GX_POINTS, .size = 42, .no_depth = on_top });
    GFX_AddVtx(at.X, at.Y, 0, color);
}

static void DrawCross(Vec2 at, float size, GXColor color) {
    event_vars->GFX_Start(4, (GFX_Params) { .shape = GX_LINES, .size = 12 });
    GFX_AddVtx(at.X - size, at.Y - size, 0, color);
    GFX_AddVtx(at.X + size, at.Y + size, 0, color);
    GFX_AddVtx(at.X - size, at.Y + size, 0, color);
    GFX_AddVtx(at.X + size, at.Y - size, 0, color);
}

/* The aimed up-Bs all take their angle from the stick the game gives fighters
 * (atan2 of input.lstick). It reads in 80ths of a full tilt, inside a circle of
 * radius 80 (gmmain.c's pad clamp), and Fighter_procInput reads an axis at or
 * under its deadzone as 0 (PlCo.dat: 0.28 across and up). So a tilted axis
 * reads at least 23/80, and the shallowest angle off straight is atan(23 / 76),
 * 16.8 degrees, 76 being the most the other axis can then read inside the
 * circle. Nothing between can be aimed: near straight reads straight. */
#define STICK_UNITS 80

typedef struct Stick {
    float least_x, least_y;       // The least a tilted axis reads, in 80ths.
    float gap_level, gap_upright; // Degrees from straight across and straight
                                  // up to the shallowest angle off them.
} Stick;

// The step past the deadzone, in 80ths.
static float LeastAxis(float deadzone) { return (int)(deadzone * STICK_UNITS + 1); }

/* Degrees from straight to the shallowest angle, when the axis across straight
 * reads `least` and the other as much as the circle lets it. */
static float Gap(float least) {
    return atan2(least, (int)sqrtf(STICK_UNITS * STICK_UNITS - least * least)) / M_1DEGREE;
}

static Stick GetStick(void) {
    ftCommonData *common = *stc_ftcommon;
    float x = LeastAxis(common->x0), y = LeastAxis(common->x4);
    return (Stick) { x, y, Gap(y), Gap(x) };
}

/* Ring angles are degrees around the X: 90 straight above it, 0 straight out
 * past the ledge, -90 straight below. Up-B from there, aimed at the X, holds
 * the stick the other way. This is the least tilt that aims along `angle`:
 * past the up-B's threshold, with both axes reading at least their least. */
static float LeastTilt(FighterData *ft, const Stick *stick, float angle) {
    float a = angle * M_1DEGREE, tilt = AimedThreshold(ft);
    float up = stick->least_y / STICK_UNITS / fabs(sin(a));
    float across = stick->least_x / STICK_UNITS / fabs(cos(a));
    if (up > tilt)
        tilt = up;
    if (across > tilt)
        tilt = across;
    return tilt > 1 ? 1 : tilt;
}

/* The stretch of the quarter below straight direction `top` (90, 0 or -90)
 * the stick can aim, down to `bottom`: a gap off each end. */
static bool Quarter(const Stick *stick, int top, float bottom, float *from, float *to) {
    bool upright = top != 0;
    *from = top - (upright ? stick->gap_upright : stick->gap_level);
    *to = top - 90 + (upright ? stick->gap_level : stick->gap_upright);
    if (*to < bottom)
        *to = bottom;
    return *from > *to;
}

/* A stretch of ring around center, from ring angle `from` down to `to`, at
 * radius, or with `least` at the least tilt that aims each way. */
static void DrawRingArc(FighterData *ft, const Stick *stick, Vec2 center, float from, float to,
                        float radius, bool least, GXColor color, bool on_top) {
    int steps = (int)((from - to) / ARC_STEP) + 1;
    if (least)
        steps *= 2; // It curves fastest at the ends.
    event_vars->GFX_Start(steps + 1, (GFX_Params) { .shape = GX_LINESTRIP, .size = 10, .no_depth = on_top });
    float outward = Outward();
    for (int i = 0; i <= steps; i++) {
        float angle = from - (from - to) * i / steps;
        if (least)
            radius = AimedDistance(ft, LeastTilt(ft, stick, angle));
        float a = angle * M_1DEGREE;
        GFX_AddVtx(center.X + outward * cos(a) * radius, center.Y + sin(a) * radius, 0, color);
    }
}

/* The ring at the travel around the X, sweep degrees from straight above it
 * out past the ledge and down, less the angles the stick can't aim: a gap
 * either side of straight across, up and down. A white mark on each straight
 * direction is where holding straight sweetspots, with a bit of ring through
 * it at its distance. The teleports and Quick Attack go farther with more
 * tilt, so a dim edge inside follows the least tilt that aims each way, and
 * their white marks run between the two, with a bit of each at its ends.
 * Above the X the up-B comes down, and grabs at the snap on its way to the X,
 * so the quarter above it, and the mark straight above, go around the snap. */
static void DrawAimedRing(FighterData *ft, Vec2 target, Vec2 snap, float sweep, bool on_top) {
    Stick stick = GetStick();
    bool spacie = IsSpacie(ft);
    float bottom = 90 - sweep, from, to;
    float travel = AimedDistance(ft, 1), least = spacie ? travel : AimedDistance(ft, AimedThreshold(ft));
    for (int full = spacie; full < 2; full++) {
        GXColor color = full ? Blue : DimBlue;
        for (int top = 90; top > bottom; top -= 90)
            if (Quarter(&stick, top, bottom, &from, &to))
                DrawRingArc(ft, &stick, top == 90 ? snap : target, from, to, travel, !full, color, on_top);
        for (int straight = 90; straight >= bottom; straight -= 90)
            DrawRingArc(ft, &stick, straight == 90 ? snap : target, straight + STRAIGHT_ARC,
                        straight - STRAIGHT_ARC, full ? travel : least, false, color, on_top);
    }
    float inner = spacie ? travel - LEVEL_TICK : least;
    float outer = spacie ? travel + LEVEL_TICK : travel;
    int marks = 0;
    for (int straight = 90; straight >= bottom; straight -= 90)
        marks++;
    event_vars->GFX_Start(marks * 2, (GFX_Params) { .shape = GX_LINES, .size = 12, .no_depth = on_top });
    float outward = Outward();
    for (int straight = 90; straight >= bottom; straight -= 90) {
        float a = straight * M_1DEGREE, x = outward * cos(a), y = sin(a);
        Vec2 center = straight == 90 ? snap : target;
        GFX_AddVtx(center.X + x * inner, center.Y + y * inner, 0, White);
        GFX_AddVtx(center.X + x * outer, center.Y + y * outer, 0, White);
    }
}

/* The B0XX's sticks for the up-B angles in one quadrant, in 80ths toward the
 * stage and up, as HayBox's Melee mode gives them, "up to date with B0XX V3
 * specifications" (src/modes/Melee20Button.cpp). Mod X and Mod Y on a diagonal,
 * alone or with C-Down, C-Left, C-Up or C-Right, step from 23 degrees to 67,
 * 4.4 apart, and B held too gives the same angles longer: the extended up-B
 * angles, for the up-Bs that go farther with more tilt. Mod Y with B across
 * is a full tilt. scripts/upb_overlay_numbers.py has the same tables. */
static const s8 B0XX_Plain[][2] = { {80, 0}, {0, 80}, {56, 56} };
static const s8 B0XX_ModB[][2] = {
    {53, 0}, {0, 43}, {73, 31}, {70, 36}, {68, 42}, {59, 43}, {51, 43}, // Mod X
    {80, 0}, {0, 59}, {31, 73}, {36, 70}, {42, 68}, {47, 64}, {47, 57}, // Mod Y
};
static const s8 B0XX_Mod[][2] = {
    {53, 0}, {0, 43}, {59, 25}, {56, 29}, {63, 39}, {56, 41}, {49, 42}, // Mod X
    {27, 0}, {0, 59}, {25, 59}, {29, 56}, {39, 63}, {41, 56}, {51, 61}, // Mod Y
};
#define B0XX_SPOTS ((countof(B0XX_Plain) + countof(B0XX_ModB) + countof(B0XX_Mod)) * 4)

/* Where up-B with the stick at (x, y) 80ths, toward the stage and up, has to
 * start to end at the X, or coming down, at the snap, by the move's own rule
 * (as AimFromStick has it). False if that's outside the ring's sweep. */
static bool B0XXSpot(FighterData *ft, const Stick *stick, Vec2 target, Vec2 snap, float x, float y,
                     float sweep, Vec2 *spot) {
    if (fabs(x) < stick->least_x)
        x = 0;
    if (fabs(y) < stick->least_y)
        y = 0;
    float length = sqrtf(x * x + y * y), tilt = length / STICK_UNITS;
    if (tilt > 1)
        tilt = 1;
    bool aimed = IsSpacie(ft) ? (fabs(x) + fabs(y)) / STICK_UNITS >= AimedThreshold(ft)
                              : tilt > AimedThreshold(ft);
    if (!aimed) {
        x = 0, y = 1, length = 1, tilt = 1;
    }
    float out = x / length, up = -y / length; // From center back to the start.
    float angle = atan2(up, out) / M_1DEGREE;
    if (angle > 90.5f || angle < 89.5f - sweep)
        return false;
    float distance = AimedDistance(ft, tilt);
    Vec2 center = up > 0 ? snap : target;
    *spot = (Vec2) { center.X + Outward() * out * distance, center.Y + up * distance };
    return true;
}

static bool IsDrawn(Vec2 spot, const Vec2 *spots, int count) {
    for (int i = 0; i < count; i++) {
        float x = spot.X - spots[i].X, y = spot.Y - spots[i].Y;
        if (x * x + y * y < B0XX_SAME_SPOT * B0XX_SAME_SPOT)
            return true;
    }
    return false;
}

/* B0XX Angles: instead of the ring, a dot where each B0XX up-B angle has to
 * start: white with no modifier, blue with Mod X or Mod Y and B, and dim
 * without B where that's somewhere else (the teleports and Quick Attack go
 * less far). Spots that close together are one dot. */
static void DrawB0XXGuide(FighterData *ft, Vec2 target, Vec2 snap, float sweep, bool on_top) {
    static const struct { const s8 (*sticks)[2]; int count; } sets[] = {
        { B0XX_Plain, countof(B0XX_Plain) },
        { B0XX_ModB, countof(B0XX_ModB) },
        { B0XX_Mod, countof(B0XX_Mod) },
    };
    const GXColor colors[] = { White, Blue, DimBlue };
    static Vec2 spots[B0XX_SPOTS];
    static u8 kinds[B0XX_SPOTS];
    Stick stick = GetStick();
    int count = 0;
    for (int set = 0; set < (int)countof(sets); set++)
        for (int i = 0; i < sets[set].count; i++)
            for (int sx = 1; sx >= -1; sx -= 2)
                for (int sy = 1; sy >= -1; sy -= 2) {
                    Vec2 spot;
                    float x = sx * sets[set].sticks[i][0], y = sy * sets[set].sticks[i][1];
                    if (!B0XXSpot(ft, &stick, target, snap, x, y, sweep, &spot) || IsDrawn(spot, spots, count))
                        continue;
                    spots[count] = spot;
                    kinds[count++] = set;
                }
    // Dim first, so the brighter dots draw over it.
    for (int set = countof(sets) - 1; set >= 0; set--) {
        int dots = 0;
        for (int i = 0; i < count; i++)
            dots += kinds[i] == set;
        if (!dots)
            continue;
        event_vars->GFX_Start(dots, (GFX_Params) { .shape = GX_POINTS, .size = B0XX_DOT_SIZE, .no_depth = on_top });
        for (int i = 0; i < count; i++)
            if (kinds[i] == set)
                GFX_AddVtx(spots[i].X, spots[i].Y, 0, colors[set]);
    }
}

/* What to aim at from the launch: the snap when coming down onto the ledge from
 * above it, else the X. */
static Vec2 AimTarget(void) {
    return upb.launch.Y > attempt.snap.Y ? attempt.snap : attempt.target;
}

/* Green when the travel crosses the grab box on its way down, or, level or
 * rising, ends with the ledge in reach; yellow when close (coming down, that
 * includes ending just above the box and falling into it); red otherwise. */
static GXColor AimColor(FighterData *ft) {
    Vec2 end = { upb.launch.X + upb.aim.X * upb.distance, upb.launch.Y + upb.aim.Y * upb.distance };
    bool down = upb.aim.Y < 0;
    if (down && CrossesGrabBox(ft, upb.launch, end))
        return Green;
    // Coming down, from what the white line aims at; else from the X.
    Vec2 target = down ? AimTarget() : attempt.target;
    Vec2 to_target = { target.X - upb.launch.X, target.Y - upb.launch.Y };
    float along = upb.distance;
    if (down) {
        along = to_target.X * upb.aim.X + to_target.Y * upb.aim.Y;
        if (along < 0) along = 0;
        if (along > upb.distance) along = upb.distance;
    }
    float miss_x = upb.launch.X + upb.aim.X * along - target.X;
    float miss_y = upb.launch.Y + upb.aim.Y * along - target.Y;
    float miss = sqrtf(miss_x * miss_x + miss_y * miss_y);
    float tolerance = ft->coll_data.cliffgrab_height * 0.5f;
    if (tolerance < 3) tolerance = 3;
    return !down && miss <= tolerance ? Green : miss <= tolerance * 3 ? Yellow : Red;
}

/* Fox's and Falco's ECB at the end of the Firefox and Firebird travel, from
 * posing SpecialHi pitched to the travel angle, as ftFox_SpecialHi_RotateModel
 * turns XRotN: its bottom, above their position, and the height of its sides,
 * which are at the game's 2-unit minimum from about 60 degrees up, where they
 * start to matter. The line they give is within 0.3 (Fox) and 0.6 (Falco) of
 * every angle's own ECB. See "The landing line" in
 * notes/UPB-SWEETSPOT-RESEARCH.md. */
#define ECB_MIN_SIDE 2
typedef struct LandingECB {
    float bottom, side_y;
} LandingECB;

static const LandingECB *GetLandingECB(FighterData *ft) {
    static const LandingECB fox = { 2.2f, 6.8f }, falco = { 2.5f, 7.6f };
    return ft->kind == FTKIND_FOX ? &fox : ft->kind == FTKIND_FALCO ? &falco : 0;
}

/* How high above the ledge your position has to be as the travel crosses the
 * ledge's x, `rise` degrees above level toward the stage, for it to carry you
 * over the corner without touching it: the ECB's bottom clears it, and on steep
 * angles its side, which reaches the wall first, has to as well. */
static float LandingClearance(const LandingECB *ecb, float rise) {
    float clearance = -ecb->bottom;
    if (rise > 0) {
        float a = rise * M_1DEGREE;
        float side = ECB_MIN_SIDE * sin(a) / cos(a) - ecb->side_y;
        if (side > clearance)
            clearance = side;
    }
    return clearance;
}

/* The landing line from ring angle `from` down to `to` (ring angles as for the
 * ring): where up-B has to start for the travel, aimed back the other way, to
 * cross the ledge's x just high enough to clear it. */
static void DrawLandingArc(FighterData *ft, const LandingECB *ecb, float from, float to) {
    Vec2 ledge = ledge_positions[side_idx];
    float travel = AimedDistance(ft, 1), outward = Outward();
    int steps = (int)((from - to) / ARC_STEP) + 1;
    event_vars->GFX_Start(steps + 1, (GFX_Params) { .shape = GX_LINESTRIP, .size = 10, .no_depth = true });
    for (int i = 0; i <= steps; i++) {
        float angle = from - (from - to) * i / steps, a = angle * M_1DEGREE;
        float y = ledge.Y + LandingClearance(ecb, -angle);
        GFX_AddVtx(ledge.X + outward * cos(a) * travel, y + sin(a) * travel, 0, Pink);
    }
}

/* Landing Line, for Fox and Falco: the lowest you can up-B from and still land
 * on the stage, for each angle the stick can aim up toward it, and straight
 * across, with a tick. Above it, aim just over the ledge and the travel itself
 * carries you over. That leaves out what's left of your speed after it and
 * wallriding up the stage's side, so it's the safe line. */
static void DrawLandingLine(FighterData *ft) {
    const LandingECB *ecb = GetLandingECB(ft);
    if (!ecb || !Options_Main[OPT_LANDING].val)
        return;
    Stick stick = GetStick();
    float from, to;
    if (Quarter(&stick, 0, -90, &from, &to))
        DrawLandingArc(ft, ecb, from, to);
    DrawLandingArc(ft, ecb, STRAIGHT_ARC, -STRAIGHT_ARC);
    Vec2 ledge = ledge_positions[side_idx];
    float travel = AimedDistance(ft, 1), outward = Outward(), y = ledge.Y + LandingClearance(ecb, 0);
    event_vars->GFX_Start(2, (GFX_Params) { .shape = GX_LINES, .size = 12, .no_depth = true });
    GFX_AddVtx(ledge.X + outward * (travel - LEVEL_TICK), y, 0, Pink);
    GFX_AddVtx(ledge.X + outward * (travel + LEVEL_TICK), y, 0, Pink);
}

static void DrawAimedGuide(FighterData *ft) {
    // Fox and Falco can recover from under the stage, so their marks carry on
    // under it, drawn over the stage. The rest get the offstage half.
    bool spacie = IsSpacie(ft);
    float sweep = spacie ? UNDER_STAGE_SWEEP : 180;
    Vec2 target = attempt.target, snap = attempt.snap;
    DrawLandingLine(ft);
    if (Options_Main[OPT_B0XX].val)
        DrawB0XXGuide(ft, target, snap, sweep, spacie);
    else
        DrawAimedRing(ft, target, snap, sweep, spacie);
    // A bar over the X where coming down snaps to the ledge.
    DrawLine((Vec2) { snap.X - TARGET_SIZE, snap.Y }, (Vec2) { snap.X + TARGET_SIZE, snap.Y }, White, 12, false);
    if (!upb.aiming)
        return;
    DrawLine(upb.launch, AimTarget(), White, 8, spacie);
    Vec2 end = { upb.launch.X + upb.aim.X * upb.distance, upb.launch.Y + upb.aim.Y * upb.distance };
    GXColor color = AimColor(ft);
    DrawLine(upb.launch, end, color, 12, spacie);
    DrawDot(end, color, spacie);
}

// Where to start, with the path turned `tilt` degrees toward the stage.
static void DrawPathStart(const UpBInfo *info, Vec2 target, float scale, float tilt, GXColor color) {
    float a = tilt * M_1DEGREE, c = cos(a), s = sin(a), out = Outward() * scale;
    Vec2 fast = { target.X + out * (info->forward * c + info->rise * s),
                  target.Y - scale * (info->rise * c - info->forward * s) };
    Vec2 slow = { target.X + out * (info->end_forward * c + info->end_rise * s),
                  target.Y - scale * (info->end_rise * c - info->end_forward * s) };
    DrawLine(fast, slow, color, 12, false);
    DrawDot(fast, color, false);
}

static void DrawPathGuide(FighterData *ft, const UpBInfo *info, Vec2 target) {
    float scale = ft->scale.Y, tilt = FullTilt(ft, info);
    if (tilt)
        DrawPathStart(info, target, scale, tilt, DimBlue);
    DrawPathStart(info, target, scale, 0, Blue);
    if (!upb.used)
        return;
    if (upb.path_set) {
        DrawLine(upb.start, upb.path_end, Yellow, 12, false);
        DrawDot(upb.path_end, Yellow, false);
    }
    DrawCross(upb.start, TARGET_SIZE, Yellow);
}

static void Draw_Guides(void) {
    if (!Options_Main[OPT_GUIDES].val || !attempt.has_target)
        return;
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *ft = hmn->userdata;
    const UpBInfo *info = GetUpBInfo(ft);

    COBJ *cur_cam = COBJ_GetCurrent();
    CObj_SetCurrent(*stc_matchcam_cobj);

    // A line out from each ledge, like Side-B's.
    event_vars->GFX_Start(4, (GFX_Params) { .shape = GX_LINES, .size = 16 });
    GFX_AddVtx(ledge_positions[0].X, ledge_positions[0].Y, 0, White);
    GFX_AddVtx(ledge_positions[0].X - 100, ledge_positions[0].Y, 0, White);
    GFX_AddVtx(ledge_positions[1].X, ledge_positions[1].Y, 0, White);
    GFX_AddVtx(ledge_positions[1].X + 100, ledge_positions[1].Y, 0, White);

    if (info && info->guide == GUIDE_AIMED)
        DrawAimedGuide(ft);
    else if (info && info->guide == GUIDE_PATH)
        DrawPathGuide(ft, info, attempt.target);
    DrawCross(attempt.target, TARGET_SIZE, White);

    CObj_SetCurrent(cur_cam);
}

static void DrawHUD(GOBJ *gobj, int pass) {
    if (pass != 2)
        return;
    static char last_text[24], rate_text[24], angle_text[24];
    if (stats.last == RESULT_SWEETSPOT)
        sprintf(last_text, "Sweetspot %df", stats.last_frames);
    else
        sprintf(last_text, "%s", Result_Text[stats.last]);
    if (stats.total)
        sprintf(rate_text, "%d/%d (%.1f%%)", stats.successes, stats.total,
                100.f * stats.successes / stats.total);
    else
        sprintf(rate_text, "-");
    static const char *labels[] = {"Last Attempt", "Success Rate", "Held / Need"};
    const char *info[] = { last_text, rate_text, angle_text };

    // Aimed up-Bs add the angle you're holding and the one the ledge needs.
    FighterData *ft = Fighter_GetGObj(0)->userdata;
    const UpBInfo *upb_info = GetUpBInfo(ft);
    int rows = upb_info && upb_info->guide == GUIDE_AIMED ? 3 : 2;
    if (upb.aiming && attempt.has_target) {
        Vec2 at = AimTarget();
        Vec2 need = { at.X - upb.launch.X, at.Y - upb.launch.Y };
        sprintf(angle_text, "%d / %d", Degrees(upb.aim), Degrees(need));
    } else {
        sprintf(angle_text, "-");
    }
    event_vars->HUD_DrawInfoPanel(labels, info, rows);
}

// Reset -----------------------------------------------------------------

static void Reset(int new_side) {
    side_idx = new_side;
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    attempt = (struct attempt) { 0 };
    upb = (struct upb) { 0 };
    marth = (struct marth) { 0 };
    reset_timer = 0;

    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    Options_Main[OPT_LANDING].disable = !GetLandingECB(hmn_data); // Fox and Falco only.

    float outward = Outward();
    Vec2 spot = PlatformSpot(hmn_data, ledge_positions[side_idx], outward);
    float x = spot.X, y = spot.Y;
    PlaceOnPlatform(hmn, x, y, -outward);
    GOBJ *hmn_sub = Fighter_GetSubcharGObj(0, 1);
    if (hmn_sub && hmn_data->flags.has_follower) {
        PlaceOnPlatform(hmn_sub, x - outward * NANA_PLATFORM_GAP, y, -outward);
        SeedFollower(hmn_sub, hmn);
    }

    float dmg = 50 * HSD_Randf() + 50;
    hmn_data->dmg.percent = dmg;
    Fighter_SetHUDDamage(hmn_data->ply, dmg);

    GOBJ *cpu = Fighter_GetGObj(1);
    PlaceMarth(cpu);
    StartMarth(cpu);
}

// Event -----------------------------------------------------------------

static void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

static void StageSwap_BeforeRestart(void) {
    UpBCarry *carry = event_vars->carry_store(UPB_CARRY_TAG, sizeof(UpBCarry));
    if (!carry)
        return;
    carry->side = side_idx;
    carry->tipper_reach = tipper_reach;
    carry->stats = stats;
}

void Event_Init(GOBJ *gobj) {
    restarting = false;
    Options_Main[OPT_GUIDES].val = Options_Main[OPT_GUIDES].val_prev =
        TM_GetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_GUIDES);
    Options_Main[OPT_B0XX].val = Options_Main[OPT_B0XX].val_prev =
        TM_GetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_B0XX);
    Options_Main[OPT_LANDING].val = Options_Main[OPT_LANDING].val_prev =
        TM_GetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_LANDING);
    Options_Main[OPT_MARTH].val = Options_Main[OPT_MARTH].val_prev =
        TM_GetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_MARTH);
    Options_Main[OPT_CAMERA].val = Options_Main[OPT_CAMERA].val_prev =
        TM_GetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_CAMERA);
    const UpBCarry *carry = event_vars->carry_take(UPB_CARRY_TAG, sizeof(UpBCarry));
    if (carry) {
        side_idx = carry->side;
        tipper_reach = carry->tipper_reach;
        stats = carry->stats;
    }
    GetLedgePositions(ledge_positions);
    StageSwap_Init();
    // Fighters fall far below the stage here, so Zoomed Out also keeps the
    // bottom blast zone in view, as in Edgeguard. The draw GObj's cleanup
    // restores the stage's camera when the match ends.
    ZoomCamera_Init(Options_Main[OPT_CAMERA].val, 1);
    GObj_AddGXLink(gobj, DrawHUD, GXLINK_HUD, 80);
    GOBJ *draw_gobj = GObj_Create(0, 0, 0);
    GObj_AddUserData(draw_gobj, 0, ZoomCamera_Restore, &ZoomCamera_Normal);
    // After the stage and fighters, or they'd paint over what's drawn on top.
    GObj_AddGXLink(draw_gobj, Draw_Guides, MATCHGX_EFFECT, 255);
}

void Event_Think(GOBJ *menu) {
    if (restarting)
        return;
    if (event_vars->game_timer == 1) {
        // Only the six legal stages: anything else restarts on Battlefield.
        if (!IsLegalStage()) {
            restarting = true;
            event_vars->restart_on_stage(GRKINDEXT_BATTLE);
            return;
        }
        event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
        Reset(side_idx);
    }

    if (reset_timer == 1)
        Reset(side_idx);
    else if (reset_timer > 1)
        reset_timer--;

    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;

    // D-pad left/right resets on that ledge, without counting the attempt.
    if (hmn_data->input.down & HSD_BUTTON_DPAD_LEFT)
        Reset(0);
    else if (hmn_data->input.down & HSD_BUTTON_DPAD_RIGHT)
        Reset(1);

    if (hmn_data->flags.dead) {
        CountAttempt(RESULT_FELL);
        Reset(side_idx);
    }

    Fighter_ZeroCPUInputs(cpu_data);
    ZoomCamera_Follow((hmn_data->phys.pos.X + cpu_data->phys.pos.X) * 0.5f);
    MarthThink(cpu);

    // The target settles while you stand on the platform, then stays put.
    int state = hmn_data->state_id;
    if (state == ASID_REBIRTH || state == ASID_REBIRTHWAIT || !attempt.has_target)
        SetTargets(hmn_data);
    if (state == ASID_REBIRTH || state == ASID_REBIRTHWAIT)
        return;
    EndRespawnInvincibility(hmn_data);
    GOBJ *hmn_sub = Fighter_GetSubcharGObj(0, 1);
    if (hmn_sub && hmn_data->flags.has_follower)
        EndRespawnInvincibility(hmn_sub->userdata);

    const UpBInfo *info = GetUpBInfo(hmn_data);
    if (info && !upb.used && state >= info->first && state <= info->last) {
        upb.used = true;
        upb.start = (Vec2) { hmn_data->phys.pos.X, hmn_data->phys.pos.Y };
    }
    if (upb.used) {
        upb.frames++;
        if (info)
            TrackAim(hmn_data, info);
    }

    if (attempt.done)
        return;
    if (upb.used)
        TrackPathAngle(hmn_data, info);
    if (state == ASID_CLIFFCATCH) {
        Finish(cpu, upb.used ? RESULT_SWEETSPOT : RESULT_NO_UPB,
               upb.used ? RESET_SUCCESS : RESET_FAILURE);
    } else if (state >= ASID_DAMAGEHI1 && state <= ASID_DAMAGEFLYROLL) {
        Finish(cpu, RESULT_HIT, RESET_FAILURE);
    } else if (hmn_data->phys.air_state == 0) {
        Finish(cpu, upb.used ? RESULT_LANDED : RESULT_NO_UPB, RESET_FAILURE);
    }
}

EventMenu *Event_Menu = &Menu_Main;
