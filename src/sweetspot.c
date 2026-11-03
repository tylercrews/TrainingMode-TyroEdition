#include "events.h"
#include "ledgedash_logic.h"
#include "stage_swap.h"
#include "zoom_camera.h"

static Vec2 ledge_positions[2];

static void ChangeCamera(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_SWEETSPOT_CAMERA, 0, value);
    ZoomCamera_Apply(value);
}

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
    float y1 = data->phys.pos.Y + 10;
    float y2 = data->phys.pos.Y - 100;

    Vec3 _line_unk;
    int _line_kind;

    return GrColl_RaycastGround(pos, line_idx, &_line_kind, &_line_unk,
            -1, -1, -1, 0, x, y1, x, y2, 0);
}

static void PlacePlayerOnGround(GOBJ *fighter) {
    FighterData *data = fighter->userdata;

    Vec3 pos;
    int line_idx;
    if (FindGroundNearPlayer(fighter, &pos, &line_idx)) {
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

void Exit(GOBJ *menu);
void Reset(int side_idx);
static void ChangeLineGuides(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_SWEETSPOT_LINES, 0, value);
}
static void ChangeDoubleJumpIndicator(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_SWEETSPOT_DJ, 0, value);
}
static void ChangeKnockback(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_SWEETSPOT, TM_SWEETSPOT_KNOCKBACK, value);
}
static void ChangeSuccess(GOBJ *menu, int value);

enum menu_options {
    OPT_KNOCKBACK,
    OPT_SUCCESS,
    OPT_VERTICAL_LINES,
    OPT_CAMERA,
    OPT_DJ_HEIGHT,
    // OPT_EXTRA_UNDERSIDE_LINES,
};

enum ledge_direction {
    DIRECTION_RANDOM,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
};

enum success_criteria {
    SUCCESS_SWEETSPOT,
    SUCCESS_LEDGEDASH,
};

/* Each preset is named for where you can act again when hitstun ends, with
 * no stick input: above, level with or below the ledge, and close to it
 * (about 45 out) or far out (75-80, the end of a full side-B). Random is the
 * original mix, which always ends 6-36 below the ledge and 55-113 out. See
 * scripts/sweetspot_knockback_presets.py for every preset's numbers. */
enum knockback {
    KB_RANDOM,
    KB_ABOVE_CLOSE,
    KB_ABOVE_FAR,
    KB_LEVEL_CLOSE,
    KB_LEVEL_FAR,
    KB_BELOW_CLOSE,
    KB_BELOW_FAR,
    KB_RANDOM_PRESET, // One of the six presets, picked each reset.
};
static const char *Values_Knockback[] = {
    "Random", "Above, Close", "Above, Far", "Level, Close", "Level, Far",
    "Below, Close", "Below, Far", "Random Preset",
};
typedef char sweetspot_knockback_names[(countof(Values_Knockback) == TM_SWEETSPOT_KNOCKBACK_COUNT) ? 1 : -1];
static const char *Values_Success[] = {"Sweetspot", "Sweetspot + Ledgedash"};
typedef char sweetspot_success_names[(countof(Values_Success) == TM_SWEETSPOT_SUCCESS_COUNT) ? 1 : -1];

// Knockback and launch angle (degrees up from the ground, away from the
// stage) ranges. The HUD's Knockback row shows the preset of each attempt.
static const struct { float mag_min, mag_max, ang_min, ang_max; } Knockbacks[KB_RANDOM_PRESET] = {
    { 105, 135, 45,   60   }, // Random
    { 149, 153, 78,   81   }, // Above, Close: 19-32 above, 39-50 out
    { 154, 158, 69.5, 72.5 }, // Above, Far: 17-32 above, 68-81 out
    { 126, 130, 73.5, 76.5 }, // Level, Close: 3 below to 5 above, 41-50 out
    { 138, 142, 62.5, 65.5 }, // Level, Far: 5 below to 8 above, 74-85 out
    {  85,  89, 54,   57   }, // Below, Close: 24-28 below, 43-48 out
    { 114, 118, 48.5, 51.5 }, // Below, Far: 25-31 below, 75-84 out
};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_STRING,
        .name = "Knockback",
        .values = Values_Knockback,
        .value_num = countof(Values_Knockback),
        .OnChange = ChangeKnockback,
        .desc = {"Where you can act again after hitstun: above, level",
                 "with or below the ledge, close to it or far out.",
                 "Random is the original mix, always below the ledge.",
                 "Random Preset picks one of the six each reset."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Success Criteria",
        .values = Values_Success,
        .value_num = countof(Values_Success),
        .OnChange = ChangeSuccess,
        .desc = {"Sweetspot: grab the ledge with side-B.",
                 "Sweetspot + Ledgedash: then ledgedash onto the stage",
                 "with ledge intangibility left (GALINT).",
                 "Changing this resets the success rate."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Enable Line Guides",
        .val = 1,
        .val_prev = 1,
        .OnChange = ChangeLineGuides,
        .desc = {"Side-B just above the line guide",
                 "to sweetspot the ledge."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Camera",
        .value_num = 2,
        .values = (const char *[]) { ZOOM_CAMERA_VALUES },
        .OnChange = ChangeCamera,
        .desc = {"Zoom out to see farther offstage",
                 "and line up with the ledge."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double-Jump Height Indicator",
        .OnChange = ChangeDoubleJumpIndicator,
        .desc = {"Double jump before your feet fall below the line,",
                 "then drift to the X and side-B at the peak."},
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Change Stage",
        .desc = {"Restart on another tournament-legal stage.",
                 "Your saved settings are kept."},
        .menu = &StageSwap_Menu,
    },
    /* Extra underside guide retained for reference; excluded from the menu.
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Extra Underside Distance Markings, Totally Unnecessary",
        .desc = {"B timing: 1/2 shortest, 3/4 longer.",
                 "0 = full range (no shorten)."},
    },
    */
    {
        .kind = OPTKIND_INFO,
        .name = "Help",
        .desc = {"Use D-Pad left/right to reset.",
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
    .name = "Sweetspot Training",
    .option_num = sizeof(Options_Main) / sizeof(EventOption),
    .options = Options_Main,
};

enum event_state {
    STATE_ENDLAG,
    STATE_WAIT_ATTACK,
    STATE_ATTACK_THINK,
    STATE_RESET,
    STATE_LEDGEDASH, // Sweetspot + Ledgedash: on the ledge, waiting for the ledgedash.
};

static struct sideb {
    bool attempted;
    Vec3 pos;
    Vec3 range_origin;
    bool range_locked;
    int dir;
} sideb = { 0 };
static int reset_timer = 0;
static int state = STATE_ENDLAG;

enum attempt_result {
    RESULT_NONE,
    RESULT_SWEETSPOT,
    RESULT_LEDGEDASH,
    RESULT_HIT,
    RESULT_FELL,
    RESULT_LANDED,
    RESULT_UP_B,
    RESULT_AIR_DODGE,
    RESULT_LEDGE_OPTION,
    RESULT_NO_GALINT,
    RESULT_REGRAB,
    RESULT_TOO_SLOW,
};
static const char *Result_Text[] = {
    "-", "Sweetspot", "Ledgedash", "Got Hit", "Fell", "Landed", "Up-B",
    "Air Dodge", "Ledge Option", "No GALINT", "Regrabbed", "Too Slow",
};

// This attempt. A D-pad reset starts a new one without counting it.
static struct attempt {
    bool counted;
    int knockback;          // The preset used, never KB_RANDOM_PRESET.
    bool grabbed, released; // Sweetspot + Ledgedash: hung on the ledge, then let go.
    bool airdodged;
    LdshAttempt ledgedash;
} attempt = { 0 };
// This session's totals and the last counted attempt.
static struct stats {
    int total, successes;
    int last, galint;
} stats = { 0 };

static void CountAttempt(int result) {
    if (attempt.counted)
        return;
    attempt.counted = true;
    stats.last = result;
    if (stats.total < 999999) {
        stats.total++;
        if (result == RESULT_SWEETSPOT || result == RESULT_LEDGEDASH)
            stats.successes++;
    }
}

static void ChangeSuccess(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_SWEETSPOT, TM_SWEETSPOT_SUCCESS, value);
    // Rates under different criteria don't mix, and neither does this attempt.
    stats = (struct stats) { 0 };
    attempt.counted = true;
}

/* After the sweetspot, judge the ledgedash like the Ledgedash event's Any
 * GALINT criterion: let go, and be actionable on the stage with ledge
 * intangibility left. */
static void LedgedashThink(FighterData *ft) {
    int state_id = ft->state_id;
    if (state_id == ASID_CLIFFWAIT)
        attempt.grabbed = true;
    else if (attempt.grabbed && state_id != ASID_CLIFFCATCH)
        attempt.released = true;
    if (state_id == ASID_ESCAPEAIR)
        attempt.airdodged = true;

    int ledge_option = state_id >= ASID_CLIFFCLIMBSLOW && state_id <= ASID_CLIFFJUMPQUICK2;
    bool regrab = attempt.released && state_id == ASID_CLIFFCATCH;
    int result = LDSH_FAILURE;
    if (!regrab) {
        int landing = state_id == ASID_LANDING || state_id == ASID_LANDINGFALLSPECIAL ||
            (state_id >= ASID_LANDINGAIRN && state_id <= ASID_LANDINGAIRLW);
        int attack_dash = Ldsh_IsAttackDash(ft->atk_kind, state_id);
        LdshSample sample = {
            .on_ledge = state_id == ASID_CLIFFWAIT,
            .released = attempt.released,
            .airdodge = attempt.airdodged,
            .grounded = !ft->phys.air_state && (ft->coll_data.envFlags & ECB_GROUND),
            .actionable = state_id == ASID_LANDING ? ft->TM.state_frame >= ft->attr.normal_landing_lag :
                !landing && (state_id == ASID_WAIT || (state_id >= ASID_WALKSLOW && state_id <= ASID_RUN) ||
                    ft->TM.state_prev[0] == ASID_LANDING || ft->TM.state_prev[0] == ASID_LANDINGFALLSPECIAL ||
                    ft->TM.state_prev[1] == ASID_LANDING || ft->TM.state_prev[1] == ASID_LANDINGFALLSPECIAL ||
                    attack_dash),
            .galint = ft->hurt.intang_frames.ledge,
            .dead = ft->flags.dead,
            .ledge_option = ledge_option,
            .frozen = ft->flags.hitlag || ft->flags.freeze,
        };
        result = LdshAttempt_Step(&attempt.ledgedash, &sample, LDSH_GALINT);
        if (result == LDSH_PENDING)
            return;
    }

    if (result == LDSH_SUCCESS) {
        CountAttempt(RESULT_LEDGEDASH);
        stats.galint = attempt.ledgedash.galint_at_landing;
        SFX_Play(303);
        reset_timer = 60;
    } else {
        CountAttempt(regrab ? RESULT_REGRAB
            : ledge_option ? RESULT_LEDGE_OPTION
            : attempt.ledgedash.landing_complete ? RESULT_NO_GALINT
            : RESULT_TOO_SLOW);
        SFX_Play(0xAF);
        reset_timer = 30;
    }
    state = STATE_RESET;
}

static void DrawHUD(GOBJ *gobj, int pass) {
    if (pass != 2)
        return;
    static char last_text[24], rate_text[24];
    if (stats.last == RESULT_LEDGEDASH)
        sprintf(last_text, "Ledgedash %df", stats.galint);
    else
        sprintf(last_text, "%s", Result_Text[stats.last]);
    if (stats.total)
        sprintf(rate_text, "%d/%d (%.1f%%)", stats.successes, stats.total,
                100.f * stats.successes / stats.total);
    else
        sprintf(rate_text, "-");
    static const char *labels[] = {"Last Attempt", "Success Rate", "Criterion", "Knockback"};
    const char *info[] = {
        last_text, rate_text,
        Options_Main[OPT_SUCCESS].val == SUCCESS_LEDGEDASH ? "+ Ledgedash" : "Sweetspot",
        Values_Knockback[attempt.knockback],
    };
    event_vars->HUD_DrawInfoPanel(labels, info, countof(labels));
}

// NTSC 1.02 PlFxAJ/PlFcAJ SpecialAirS TransN-Z tracks are linear:
// Fox 0 -> 78, Falco 0 -> 60 over animation frames 0..4. The animation
// callback enters endlag at frame 4, before dash physics, so only three
// moving physics frames run. Normal model scales are Fox .96, Falco 1.10:
// dash step = 18.72 / 16.50 units; full dash = 56.16 / 49.50 units.
// B is checked before physics: shorten-window frames 1 and 2 both skip all
// dash movement, frame 3 permits one step, frame 4 permits two steps.
// Aerial endlag starts at speed 2 and subtracts .07 before each movement;
// its 28 positive-speed frames add 27.58 units to EVERY endpoint.
// From rest, unobstructed aerial total ranges (world units):
//                 1/2      3       4       0 (no second B)
// Fox             27.58    46.30   65.02   83.74
// Falco           27.58    44.08   60.58   77.08
// Archived dash-only distances WITHOUT drift (not used by the sweetspot guide):
//                 1/2      3       4       0
// Fox              0.00    18.72   37.44   56.16
// Falco            0.00    16.50   33.00   49.50
// const float fox_dash_only[4] = { 0.00f, 18.72f, 37.44f, 56.16f };
// const float falco_dash_only[4] = { 0.00f, 16.50f, 33.00f, 49.50f };
// Startup drift, knockback, walls, landing and ledge catches can alter actual
// travel. Guides describe aerial recovery from the dash origin, not hitboxes.
// Physics/order reference:
// https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftFox/ftfoxspecials.c
static void GetHorizontalRanges(FighterData *fighter, float ranges[4], bool with_drift) {
    float step = (fighter->kind == FTKIND_FOX ? 78.0f : 60.0f) / 4.0f;
    step *= fighter->attr.model_scaling * fighter->scale.Y;

    // Read the selected fighter's live aerial end speed and friction (0x3C,
    // 0x40 in the shared Fox/Falco special attributes), rather than assuming
    // both characters always use vanilla values.
    float *attributes = fighter->special_attributes;
    float speed = attributes[0x3C / sizeof(float)];
    float friction = attributes[0x40 / sizeof(float)];
    float drift = 0;
    // Aerial endlag lasts 40 frames; after that, freefall is steerable.
    for (int i = 0; with_drift && i < 40; i++) {
        speed -= friction;
        if (speed <= 0)
            break;
        drift += speed;
    }
    for (int i = 0; i < 4; i++)
        ranges[i] = drift + step * i;
}

// Small world-space numerals keep the labels attached to the points as the
// match camera pans/zooms. Seven-segment strokes need no extra text camera.
static void DrawRangeNumber(int number, float x, float y, GXColor color) {
    static const u8 masks[5] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66 };
    static const float segments[7][4] = {
        { 0, 3, 1.5, 3 }, { 1.5, 3, 1.5, 1.5 },
        { 1.5, 1.5, 1.5, 0 }, { 1.5, 0, 0, 0 },
        { 0, 0, 0, 1.5 }, { 0, 1.5, 0, 3 },
        { 0, 1.5, 1.5, 1.5 },
    };
    int count = 0;
    for (int i = 0; i < 7; i++)
        if (masks[number] & (1 << i))
            count += 2;
    const float scale = 0.65f; // About 19% smaller again (was 0.8).
    event_vars->GFX_Start(count, (GFX_Params) { .shape = GX_LINES, .size = 8 });
    for (int i = 0; i < 7; i++) {
        if (!(masks[number] & (1 << i)))
            continue;
        GFX_AddVtx(x + (segments[i][0] - 0.75f) * scale, y + segments[i][1] * scale, 0, color);
        GFX_AddVtx(x + (segments[i][2] - 0.75f) * scale, y + segments[i][3] * scale, 0, color);
    }
}

static void DrawRangeGuide(Vec3 origin, float direction, float ranges[4], GXColor color) {
    GXColor white = { 0xff, 0xff, 0xff, 0xff };

    event_vars->GFX_Start(2, (GFX_Params) { .shape = GX_LINES, .size = 12 });
    GFX_AddVtx(origin.X, origin.Y, 0, color);
    GFX_AddVtx(origin.X + direction * ranges[3], origin.Y, 0, color);
    event_vars->GFX_Start(4, (GFX_Params) { .shape = GX_POINTS, .size = 42 });
    for (int i = 0; i < 4; i++)
        GFX_AddVtx(origin.X + direction * ranges[i], origin.Y, 0,
                   i == 3 ? white : color);

    float shortest_x = origin.X + direction * ranges[0];
    // Same endpoint, two input timings: label above/below the shared dot.
    DrawRangeNumber(1, shortest_x, origin.Y + 2, color);
    DrawRangeNumber(2, shortest_x, origin.Y - 3.95f, color);
    DrawRangeNumber(3, origin.X + direction * ranges[1], origin.Y + 2, color);
    DrawRangeNumber(4, origin.X + direction * ranges[2], origin.Y + 2, color);
    DrawRangeNumber(0, origin.X + direction * ranges[3], origin.Y + 2, white);
}

// Double jump rise to its peak and the farthest it can drift on the way up,
// from the fighter's live attributes. The jump sets Y speed to jump V *
// air jump V multiplier and X speed to stick X * air jump H multiplier, and
// gravity applies on its first physics frame (ftCo_JumpAerial_Enter_Basic,
// ft_80084DB0). Full-stick drift follows ftCommon_CalcSelfAccel_DriftFrom:
// above aerial_drift_max, X speed slows by aerial_friction a frame.
// NTSC 1.02 values: Fox rises 40.20 in 19 frames and drifts up to 15.86;
// Falco rises 41.78 in 22 frames and drifts up to 18.51.
static void GetDoubleJumpReach(FighterData *fighter, float *rise, float *drift) {
    float gravity = fighter->attr.gravity;
    float speed_y = fighter->attr.jump_v_initial_velocity * fighter->attr.air_jump_v_multiplier;
    float speed_x = fighter->attr.air_jump_h_multiplier;
    float accel = fighter->attr.aerial_drift_stick_mult + fighter->attr.aerial_drift_base;
    float target = fighter->attr.aerial_drift_max;
    float limit = fighter->attr.horizontal_air_mobility_constant; // Max air X speed.
    *rise = *drift = 0;
    for (int i = 0; gravity > 0 && i < 180; i++) {
        speed_y -= gravity;
        if (speed_y <= 0)
            break;
        float step = accel;
        if (speed_x + step > target) {
            step = -fighter->attr.aerial_friction;
            if (speed_x + step < target)
                step = target - speed_x;
            if (speed_x + step > limit)
                step = limit - speed_x;
        }
        speed_x += step;
        *rise += speed_y;
        *drift += speed_x;
    }
}

// The platform spans two thirds of the farthest drift either way, so reaching
// the middle never takes the stick held all the way for the whole rise.
#define DJ_WIDTH_SHARE (2.0f / 3.0f)
#define DJ_TARGET_SIZE 2.5f // Half the target X's width and height.

// Melee calculation for the top of the ledgegrab box, above the feet
static float LedgeGrabTop(FighterData *fighter) {
    CollData *coll_data = &fighter->coll_data;
    float offset = coll_data->cliffgrab_y_offset + 0.5 * coll_data->cliffgrab_height;
    if (fighter->kind == FTKIND_FOX) {
        // Only Fox drops a small amount during sideb before moving
        offset -= 1.0 / 6.0;
    }
    return offset;
}

// A platform under each ledge at the lowest your feet can be when you double
// jump and still sweetspot a side-B at the peak: there, the top of the
// ledge-grab box (the blue guide line) peaks level with the ledge. Fall below
// it and the double jump can't get you back up. An X on the white ledge line
// marks where to side-B. Both are centered where the full side-B (the white
// 0 dot) just reaches the ledge.
static void DrawDoubleJumpIndicator(FighterData *fighter, float full_range) {
    float rise, drift;
    GetDoubleJumpReach(fighter, &rise, &drift);
    float width = drift * DJ_WIDTH_SHARE;
    float depth = LedgeGrabTop(fighter) + rise;

    GXColor color = fighter->kind == FTKIND_FOX
        ? (GXColor) { 0xff, 0x8c, 0x00, 0xff }  // Orange
        : (GXColor) { 0x30, 0xc8, 0xff, 0xff }; // Sky blue
    const float s = DJ_TARGET_SIZE;
    event_vars->GFX_Start(12, (GFX_Params) { .shape = GX_LINES, .size = 16 });
    for (int i = 0; i < 2; i++) {
        float offstage = i == 0 ? -1 : 1;
        float x = ledge_positions[i].X + offstage * full_range;
        float y = ledge_positions[i].Y;
        GFX_AddVtx(x - width, y - depth, 0, color);
        GFX_AddVtx(x + width, y - depth, 0, color);
        GFX_AddVtx(x - s, y - s, 0, color);
        GFX_AddVtx(x + s, y + s, 0, color);
        GFX_AddVtx(x - s, y + s, 0, color);
        GFX_AddVtx(x + s, y - s, 0, color);
    }
}

/* Extra Underside Distance Markings, Totally Unnecessary
static void DrawExtraUndersideDistanceMarkings(FighterData *fighter) {
    float ranges[4];
    GetHorizontalRanges(fighter, ranges, true);
    Vec3 origin = sideb.attempted ? sideb.range_origin : fighter->phys.pos;
    float direction = sideb.attempted ? sideb.dir : fighter->facing_direction;
    DrawRangeGuide(origin, direction, ranges, (GXColor) { 0xff, 0xe0, 0x00, 0xff });
}
*/

static void DrawLineGuides(FighterData *hmn_data, float ranges[4]) {
    float ledgegrab_offset = LedgeGrabTop(hmn_data);

    float y = hmn_data->phys.pos.Y + ledgegrab_offset; 
    float x = hmn_data->phys.pos.X; 
    GXColor color = { 0x00, 0x00, 0xff, 0xff };
    if (sideb.attempted) {
        x = sideb.range_origin.X;
        y = sideb.pos.Y + ledgegrab_offset;

        Vec2 target_ledge;
        if (sideb.dir == 1)
            target_ledge = ledge_positions[0];
        else
            target_ledge = ledge_positions[1];

        float delta = y - target_ledge.Y;

        if (delta <= 0) { // failure (SD)
            color = (GXColor) { 0xff, 0x00, 0x00, 0xff };
        } else if (hmn_data->state_id == ASID_CLIFFCATCH
            || hmn_data->state_id == ASID_CLIFFWAIT) { // success
            color = (GXColor) { 0x00, 0xff, 0x00, 0xff };
        }
    }

    // Sweetspot targets use the full aerial distances INCLUDING endlag drift,
    // at the original ledge-grab height with the original result colors.
    float direction = sideb.attempted ? sideb.dir : hmn_data->facing_direction;
    DrawRangeGuide((Vec3) { x, y, 0 }, direction, ranges, color);

    // draw line from left edge
    GXColor white = { 0xff, 0xff, 0xff, 0xff };
    event_vars->GFX_Start(4, (GFX_Params) { .shape = GX_LINES, .size = 16 });
    x = ledge_positions[0].X;
    y = ledge_positions[0].Y;
    GFX_AddVtx(x, y, 0, white);
    GFX_AddVtx(x - 100, y, 0, white);

    // draw line from right edge
    x = ledge_positions[1].X;
    y = ledge_positions[1].Y;
    GFX_AddVtx(x, y, 0, white);
    GFX_AddVtx(x + 100, y, 0, white);
}

void Draw_Lines(void) {
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;

    if (!Options_Main[OPT_VERTICAL_LINES].val && !Options_Main[OPT_DJ_HEIGHT].val)
        return;

    COBJ *cur_cam = COBJ_GetCurrent();
    CObj_SetCurrent(*stc_matchcam_cobj);

    // if (Options_Main[OPT_EXTRA_UNDERSIDE_LINES].val)
    //     DrawExtraUndersideDistanceMarkings(hmn_data);

    float ranges[4];
    // GetHorizontalRanges(hmn_data, ranges, false); // Archived dash-only guide.
    GetHorizontalRanges(hmn_data, ranges, true);
    if (Options_Main[OPT_VERTICAL_LINES].val)
        DrawLineGuides(hmn_data, ranges);
    // After the guides, so the X sits on top of the white ledge line.
    if (Options_Main[OPT_DJ_HEIGHT].val)
        DrawDoubleJumpIndicator(hmn_data, ranges[3]);

    CObj_SetCurrent(cur_cam);
}

void Event_Init(GOBJ* gobj) {
    Options_Main[OPT_KNOCKBACK].val = Options_Main[OPT_KNOCKBACK].val_prev =
        TM_GetSetting(TM_SETTING_SWEETSPOT, TM_SWEETSPOT_KNOCKBACK);
    Options_Main[OPT_SUCCESS].val = Options_Main[OPT_SUCCESS].val_prev =
        TM_GetSetting(TM_SETTING_SWEETSPOT, TM_SWEETSPOT_SUCCESS);
    Options_Main[OPT_VERTICAL_LINES].val = Options_Main[OPT_VERTICAL_LINES].val_prev =
        TM_GetSetting(TM_SETTING_SWEETSPOT_LINES, 0);
    Options_Main[OPT_CAMERA].val = Options_Main[OPT_CAMERA].val_prev =
        TM_GetSetting(TM_SETTING_SWEETSPOT_CAMERA, 0);
    Options_Main[OPT_DJ_HEIGHT].val = Options_Main[OPT_DJ_HEIGHT].val_prev =
        TM_GetSetting(TM_SETTING_SWEETSPOT_DJ, 0);
    ZoomCamera_Init(Options_Main[OPT_CAMERA].val, 0);
    // Archived underside option was session-only, never memory-card backed.
    // Options_Main[OPT_EXTRA_UNDERSIDE_LINES].val = Options_Main[OPT_EXTRA_UNDERSIDE_LINES].val_prev = 0;
    GetLedgePositions(ledge_positions);
    StageSwap_Init();
    GObj_AddGXLink(gobj, DrawHUD, GXLINK_HUD, 80);
    GOBJ *draw_gobj = GObj_Create(0, 0, 0);
    GObj_AddUserData(draw_gobj, 0, ZoomCamera_Restore, &ZoomCamera_Normal);
    GObj_AddGXLink(draw_gobj, Draw_Lines, 3, 0);
}

void Event_Think(GOBJ *menu) {
    if (event_vars->game_timer == 1) {
        event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
        Reset(0);
    }

    // Reset timer is 0 when off, otherwise counts down until reset
    if (reset_timer == 1) {
        reset_timer = 0;
        Reset(2);
    }
    if (reset_timer > 1)
        reset_timer--;
    
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;

    // Reset left/right when user presses dpad manually
    if (hmn_data->input.down & HSD_BUTTON_DPAD_LEFT) {
        Reset(0);
    } else if (hmn_data->input.down & HSD_BUTTON_DPAD_RIGHT) {
        Reset(1);
    }

    // Reset when dead
    if (hmn_data->flags.dead) {
        CountAttempt(RESULT_FELL);
        Reset(2);
    }

    if (hmn_data->state_id == 350) { // Aerial sideb startup for fox/falco
        sideb.attempted = true;
        sideb.pos = hmn_data->phys.pos;
        sideb.range_origin = hmn_data->phys.pos;
        sideb.range_locked = false;
        sideb.dir = hmn_data->facing_direction;
    }
    if (sideb.attempted && !sideb.range_locked
        && (hmn_data->state_id == 351 || hmn_data->state_id == 352)) {
        // Capture before the first dash/endlag movement, including a B press
        // on the transition frame that skips the dash state entirely.
        sideb.range_origin = hmn_data->phys.pos_prev;
        sideb.range_locked = true;
    }

    /* Landing can occur before Marth reaches down-tilt or after he freezes.
     * During startup/dash, native Side-B collision switches directly to the
     * grounded special states; endlag lands in LandingFallSpecial instead.
     * After a sweetspot, landing is the ledgedash, judged on its own. */
    if (sideb.attempted && state != STATE_RESET && state != STATE_LEDGEDASH
        && (hmn_data->state_id == ASID_LANDING
            || hmn_data->state_id == ASID_LANDINGFALLSPECIAL
            || (hmn_data->phys.air_state == 0
                && hmn_data->state_id >= 347 && hmn_data->state_id <= 349))) {
        CountAttempt(RESULT_LANDED);
        FrameSpeedChange(cpu, 1.0);
        reset_timer = 30;
        state = STATE_RESET;
    }

    // Begin CPU AI logic
    Fighter_ZeroCPUInputs(cpu_data);

    // always hold crouch when state is higher than endlag
    if (state > STATE_ENDLAG)
        cpu_data->cpu.lstickY = -127;

    switch (state) {
        case STATE_ENDLAG:
            if (cpu_data->state_id == ASID_WAIT) {
                cpu_data->cpu.lstickY = -127;
                state = STATE_WAIT_ATTACK;
            }
            break;
        case STATE_WAIT_ATTACK:
            if (hmn_data->state_id == 0x15F) { // side b asid
                // input dtilt
                cpu_data->cpu.lstickY = -38;
                cpu_data->cpu.held = PAD_BUTTON_A;
                state = STATE_ATTACK_THINK;
            } else if (hmn_data->state_id == 0x162 // up b asid
                    || hmn_data->state_id == ASID_ESCAPEAIR) {
                CountAttempt(hmn_data->state_id == 0x162 ? RESULT_UP_B : RESULT_AIR_DODGE);
                SFX_Play(0xAF);
                reset_timer = 30;
                state = STATE_RESET;
            }
            break;
        case STATE_ATTACK_THINK:
            if (cpu_data->state_id != ASID_ATTACKLW3) break;
            if (cpu_data->state.frame == 7) {
                FrameSpeedChange(cpu, 0);       // freeze cpu
                if (hmn_data->victim) {         // if hit
                    CountAttempt(RESULT_HIT);
                    FrameSpeedChange(cpu, 1.0); // unfreeze
                    reset_timer = 30;
                    state = STATE_RESET;
                }
            }
            if (hmn_data->state_id == ASID_CLIFFCATCH) {
                SFX_Play(0xAD);
                FrameSpeedChange(cpu, 1.0);
                if (Options_Main[OPT_SUCCESS].val == SUCCESS_LEDGEDASH) {
                    state = STATE_LEDGEDASH;
                } else {
                    CountAttempt(RESULT_SWEETSPOT);
                    reset_timer = 60;
                    state = STATE_RESET;
                }
            }
            break;
        case STATE_RESET:
            break;
        case STATE_LEDGEDASH:
            LedgedashThink(hmn_data);
            break;
    }

}

void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

/* Every menu setting is saved, so the restart keeps them. The draw GObj's
 * cleanup restores the camera when the match ends, as it does on Exit. */
static void StageSwap_BeforeRestart(void) { }

void Reset(int side_idx) {
    if (side_idx == 2)
        side_idx = HSD_Randi(2);
    if (side_idx != 0 && side_idx != 1)
        assert("invalid side");

    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    sideb.attempted = false;
    sideb.range_locked = false;

    int knockback = Options_Main[OPT_KNOCKBACK].val;
    if (knockback == KB_RANDOM_PRESET)
        knockback = KB_ABOVE_CLOSE + HSD_Randi(KB_RANDOM_PRESET - KB_ABOVE_CLOSE);
    attempt = (struct attempt) { .knockback = knockback };

    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;

    int side = side_idx * 2 - 1;
    hmn_data->facing_direction = -side;
    cpu_data->facing_direction = side;

    hmn_data->phys.pos.X = side * 12 + ledge_positions[side_idx].X;
    // 6 above the ledge, so each preset ends at the same spot on every stage.
    hmn_data->phys.pos.Y = ledge_positions[side_idx].Y + 6;
    cpu_data->phys.pos.X = side * -15 + ledge_positions[side_idx].X;
    cpu_data->phys.pos.Y = 0;

    UpdatePosition(hmn);
    UpdatePosition(cpu);
    PlacePlayerOnGround(cpu);

    // cpu state
    cpu_data->gfx = 0;
    FSmash_GetASForAnalogAngle(cpu);
    Fighter_ApplyAnimation(cpu, 11, 1, 0);
    cpu_data->state.frame = 11;
    cpu_data->script.script_event_timer = 0;
    Fighter_SubactionFastForward(cpu);
    Fighter_UpdateStateFrameInfo(cpu);
    Fighter_HitboxDisableAll(cpu);
    cpu_data->script.script_current = 0;
    UpdateCameraBox(cpu);

    // hmn state
    ActionStateChange(0, 1, 0, hmn, ASID_DAMAGEFLYN, 0x40, 0);
    Fighter_UpdateStateFrameInfo(hmn);
    UpdateCameraBox(hmn);
    float dmg = 50 * HSD_Randf() + 50;
    hmn_data->dmg.percent = dmg;
    Fighter_SetHUDDamage(hmn_data->ply, dmg);
    
    // hmn knockback
    float angle = (Knockbacks[knockback].ang_min
        + (Knockbacks[knockback].ang_max - Knockbacks[knockback].ang_min) * HSD_Randf()) * M_1DEGREE;
    float mag = Knockbacks[knockback].mag_min
        + (Knockbacks[knockback].mag_max - Knockbacks[knockback].mag_min) * HSD_Randf();
    float vel = mag * (*stc_ftcommon)->force_applied_to_kb_mag_multiplier;
    hmn_data->phys.kb_vel.X = cos(angle) * vel * side;
    hmn_data->phys.kb_vel.Y = sin(angle) * vel;
    float kb_frames = (float)(int)((*stc_ftcommon)->x154 * mag);

    union IntToFloat {
        int i;
        float f;
    } itf;
    itf.f = kb_frames;
    hmn_data->state_var.state_var1 = itf.i;

    hmn_data->flags.hitstun = 1;
    Fighter_EnableCollUpdate(hmn_data);

    // give both hitlag
    hmn_data->dmg.hitlag_frames = 10;
    hmn_data->flags.hitlag = 1;
    hmn_data->flags.hitlag_unk = 1;
    cpu_data->dmg.hitlag_frames = 10;
    cpu_data->flags.hitlag = 1;
    cpu_data->flags.hitlag_unk = 1;

    reset_timer = 0;
    state = STATE_ENDLAG;
}

EventMenu *Event_Menu = &Menu_Main;
