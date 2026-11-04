#include "events.h"
#include "stage_swap.h"
#include "zoom_camera.h"

/* Up-B Sweetspot: start on an angel platform off one ledge and grab the ledge
 * with your up-B. Marth swings his forward smash as each attempt starts and
 * freezes with the tipper out, so its hitboxes are simply there whenever you
 * arrive and overshooting onto the stage gets you hit. The guides and every
 * number behind them are in notes/UPB-SWEETSPOT-RESEARCH.md. */

// Where the angel platform sits, from the ledge: far enough out that you have
// to recover, close enough that every character here can.
#define PLATFORM_OUT 40
#define PLATFORM_UP 15
// Nana's platform, beside Popo's on the stage side.
#define NANA_PLATFORM_GAP 8

// Marth's forward smash comes out on frame 10, the tipper as hitbox 3 (20%).
// He freezes there with the whole swing out, like Side-B's down-tilt, and the
// tipper centered this far inland of the ledge: 0 is right over it.
#define MARTH_TIPPER_HITBOX 3
#define MARTH_TIPPER_FRAME 10
#define MARTH_TIPPER_INLAND 0

#define RESET_SUCCESS 60
#define RESET_FAILURE 30

// Fox and Falco's charge animation (SpecialHiHoldAir) is 43 frames; the stick
// is read as it ends.
#define SPACIES_CHARGE_FRAMES 43

#define ARC_STEPS 40

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
 * the stage turns the move before it locks. */
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
    [FTKIND_GAW]       = { 373, 374, GUIDE_NONE },
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
static void ChangeMarth(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_MARTH, value);
}
static void ChangeCamera(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_UPB_SWEETSPOT, TM_UPB_CAMERA, value);
    ZoomCamera_Apply(value);
}

enum menu_options {
    OPT_GUIDES,
    OPT_MARTH,
    OPT_CAMERA,
};

enum marth_option {
    MARTH_TIPPER,
    MARTH_OFF,
};
static const char *Values_Marth[] = {"Tipper", "Off"};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Enable Line Guides",
        .val = 1,
        .val_prev = 1,
        .OnChange = ChangeGuides,
        .desc = {"Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu, Pichu:",
                 "up-B on the blue ring, then hold the white line.",
                 "Mario, Luigi, Marth, Falcon and more: up-B at the",
                 "blue dot (the dim one is the stick toward the stage)."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Marth",
        .values = Values_Marth,
        .value_num = countof(Values_Marth),
        .OnChange = ChangeMarth,
        .desc = {"Tipper: Marth freezes his forward smash with",
                 "the tipper right over the ledge, so",
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
} attempt = { 0 };

static struct upb {
    bool used;
    int frames;              // Since it started.
    Vec2 start;
    // GUIDE_AIMED: where the travel starts and where the stick sends it.
    bool aiming, locked;
    Vec2 launch, aim;
    float distance;
} upb = { 0 };

static struct marth {
    bool swinging, frozen, measured;
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

/* The fighter's position when the ledge sits in the middle of its ledge-grab
 * box: the box reaches cliffgrab_width past the ECB's side toward the ledge
 * (the side it faces), and is centered cliffgrab_y_offset above the feet. */
static Vec2 LedgeTarget(FighterData *ft) {
    Vec2 ledge = ledge_positions[side_idx];
    float front = ft->facing_direction > 0 ? ft->coll_data.ecbCurr_right.X : -ft->coll_data.ecbCurr_left.X;
    float reach = front + ft->coll_data.cliffgrab_width;
    return (Vec2) { ledge.X + Outward() * reach * 0.5f, ledge.Y - ft->coll_data.cliffgrab_y_offset };
}

// Marth -----------------------------------------------------------------

static void PlaceMarth(GOBJ *cpu) {
    FighterData *cpu_data = cpu->userdata;
    Vec2 ledge = ledge_positions[side_idx];
    float outward = Outward();
    cpu_data->facing_direction = outward;
    ClearVelocity(cpu_data);
    cpu_data->phys.pos.X = ledge.X - outward * (MARTH_TIPPER_INLAND + tipper_reach);
    cpu_data->phys.pos.Y = ledge.Y;
    UpdatePosition(cpu);
    PlacePlayerOnGround(cpu);
    Fighter_EnterWait(cpu);
    UpdateCameraBox(cpu);
}

// Swing as the attempt starts: nothing about hitting you depends on your timing.
static void StartMarth(GOBJ *cpu) {
    FighterData *cpu_data = cpu->userdata;
    if (Options_Main[OPT_MARTH].val != MARTH_TIPPER || marth.swinging
        || cpu_data->state_id != ASID_WAIT)
        return;
    FSmash_GetASForAnalogAngle(cpu);
    marth.swinging = true;
}

/* Freeze on the first active frame so the whole forward smash lingers, as
 * Side-B does with the down-tilt. The first freeze measures how far in front
 * of Marth the tipper is and moves him so it sits MARTH_TIPPER_INLAND in. */
static void MarthThink(GOBJ *cpu) {
    FighterData *cpu_data = cpu->userdata;
    int state = cpu_data->state_id;
    if (!marth.swinging || marth.measured || state < ASID_ATTACKS4HI || state > ASID_ATTACKS4LW)
        return;
    if (!marth.frozen) {
        // After the result, let the swing finish instead.
        if (attempt.done || cpu_data->state.frame < MARTH_TIPPER_FRAME)
            return;
        FrameSpeedChange(cpu, 0);
        marth.frozen = true;
    }
    ftHit *tipper = &cpu_data->hitbox[MARTH_TIPPER_HITBOX];
    if (!tipper->active)
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

static void DrawLine(Vec2 a, Vec2 b, GXColor color, int width) {
    event_vars->GFX_Start(2, (GFX_Params) { .shape = GX_LINES, .size = width });
    GFX_AddVtx(a.X, a.Y, 0, color);
    GFX_AddVtx(b.X, b.Y, 0, color);
}

static void DrawDot(Vec2 at, GXColor color) {
    event_vars->GFX_Start(1, (GFX_Params) { .shape = GX_POINTS, .size = 42 });
    GFX_AddVtx(at.X, at.Y, 0, color);
}

static void DrawCross(Vec2 at, float size, GXColor color) {
    event_vars->GFX_Start(4, (GFX_Params) { .shape = GX_LINES, .size = 12 });
    GFX_AddVtx(at.X - size, at.Y - size, 0, color);
    GFX_AddVtx(at.X + size, at.Y + size, 0, color);
    GFX_AddVtx(at.X - size, at.Y + size, 0, color);
    GFX_AddVtx(at.X + size, at.Y - size, 0, color);
}

// The offstage half of a circle around center: from straight above, out
// past the ledge, to straight below.
static void DrawOffstageArc(Vec2 center, float radius, GXColor color) {
    event_vars->GFX_Start(ARC_STEPS + 1, (GFX_Params) { .shape = GX_LINESTRIP, .size = 10 });
    float outward = Outward();
    for (int i = 0; i <= ARC_STEPS; i++) {
        float angle = (90 - 180.f * i / ARC_STEPS) * M_1DEGREE;
        GFX_AddVtx(center.X + outward * cos(angle) * radius, center.Y + sin(angle) * radius, 0, color);
    }
}

/* Green when the travel ends with the ledge in reach (or passes it on the way
 * down, when the travel itself can grab), yellow when close, red otherwise. */
static GXColor AimColor(FighterData *ft, Vec2 target) {
    Vec2 to_target = { target.X - upb.launch.X, target.Y - upb.launch.Y };
    float along = upb.distance;
    if (upb.aim.Y < 0) {
        along = to_target.X * upb.aim.X + to_target.Y * upb.aim.Y;
        if (along < 0) along = 0;
        if (along > upb.distance) along = upb.distance;
    }
    float miss_x = upb.launch.X + upb.aim.X * along - target.X;
    float miss_y = upb.launch.Y + upb.aim.Y * along - target.Y;
    float miss = sqrtf(miss_x * miss_x + miss_y * miss_y);
    float tolerance = ft->coll_data.cliffgrab_height * 0.5f;
    if (tolerance < 3) tolerance = 3;
    return miss <= tolerance ? Green : miss <= tolerance * 3 ? Yellow : Red;
}

static void DrawAimedGuide(FighterData *ft, Vec2 target) {
    DrawOffstageArc(target, AimedDistance(ft, 1), Blue);
    if (!IsSpacie(ft)) // Their distance grows with tilt: the inner ring is the least.
        DrawOffstageArc(target, AimedDistance(ft, AimedThreshold(ft)), DimBlue);
    if (!upb.aiming)
        return;
    DrawLine(upb.launch, target, White, 8);
    Vec2 end = { upb.launch.X + upb.aim.X * upb.distance, upb.launch.Y + upb.aim.Y * upb.distance };
    GXColor color = AimColor(ft, target);
    DrawLine(upb.launch, end, color, 12);
    DrawDot(end, color);
}

// Where to start, with the path turned `tilt` degrees toward the stage.
static void DrawPathStart(const UpBInfo *info, Vec2 target, float scale, float tilt, GXColor color) {
    float a = tilt * M_1DEGREE, c = cos(a), s = sin(a), out = Outward() * scale;
    Vec2 fast = { target.X + out * (info->forward * c + info->rise * s),
                  target.Y - scale * (info->rise * c - info->forward * s) };
    Vec2 slow = { target.X + out * (info->end_forward * c + info->end_rise * s),
                  target.Y - scale * (info->end_rise * c - info->end_forward * s) };
    DrawLine(fast, slow, color, 12);
    DrawDot(fast, color);
}

static void DrawPathGuide(FighterData *ft, const UpBInfo *info, Vec2 target) {
    float scale = ft->scale.Y;
    if (info->tilt_attr)
        DrawPathStart(info, target, scale, ATTR_F(ft, info->tilt_attr), DimBlue);
    DrawPathStart(info, target, scale, 0, Blue);
    if (upb.used)
        DrawCross(upb.start, 2.5f, Yellow);
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
        DrawAimedGuide(ft, attempt.target);
    else if (info && info->guide == GUIDE_PATH)
        DrawPathGuide(ft, info, attempt.target);
    DrawCross(attempt.target, 2.5f, White);

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
        Vec2 need = { attempt.target.X - upb.launch.X, attempt.target.Y - upb.launch.Y };
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

    float outward = Outward();
    Vec2 ledge = ledge_positions[side_idx];
    float x = ledge.X + outward * PLATFORM_OUT, y = ledge.Y + PLATFORM_UP;
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
    GObj_AddGXLink(draw_gobj, Draw_Guides, 3, 0);
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
    if (state == ASID_REBIRTH || state == ASID_REBIRTHWAIT || !attempt.has_target) {
        attempt.target = LedgeTarget(hmn_data);
        attempt.has_target = true;
    }
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
