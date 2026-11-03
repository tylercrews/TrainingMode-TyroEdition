#include "events.h"

/* 2v1 Grab Punish Practice: your teammate CPU grabs the enemy CPU, and you
 * punish the enemy (say with a JC up-smash) before they mash out, without
 * hitting your teammate. The VS character select picks all three fighters
 * and the team colors. grabpunish_css.c keeps exactly one CPU on your team
 * there, and EventInit (GrabPunish_PrepareMatch in events.c) makes the match
 * the same way, so the CPU on your team is your teammate and the other is
 * the enemy.
 *
 * How long a grab holds. NTSC 1.02 sets the grabbed player's grab timer when
 * the grab connects (800DA944), with these PlCo.dat ftLoadCommonData values:
 *     hold = 30 + 1.6 x percent + 8 x (9 - handicap) + 15 x (3 - placing)
 * Handicap is 9 when it's off, which adds nothing. Placing is the grabbed
 * player's current place, 0 for first, so Melee holds whoever is ahead
 * longer. Every frame of CapturePulled and CaptureWait (800DB8A4, 800DB908)
 * takes 1 off the timer, and Fighter_CheckGrabBreakout (8007DC08) takes 6
 * more for a fresh press of A, B, X or Y, and 6 more when the stick moves
 * into a new direction (past half tilt, either axis, at most once a frame).
 * At 0 the enemy breaks free, so mashing can take at most 13 a frame.
 *
 * This event holds every grab as if the match were tied with handicap off,
 * so the KOs you score don't shorten later grabs:
 *     hold = 75 + 1.6 x percent frames, without mashing.
 * CPU Mashing (frames until the enemy breaks free):
 *                                              0%   20%  40%  60%  80%  100%
 *   No Mash       1 a frame                    75   107  139  171  203  235
 *   Light Mash    a button every 6 frames      38    54   70   86  102  118
 *                 (10 a second): 2 a frame, half the hold
 *   Heavy Mash    a button and a new stick     19    27   35   43   51   59
 *                 direction, each every 4 frames (30 inputs a second):
 *                 4 a frame, a quarter
 *   Inhuman Mash  a new button and stick        6     9   11   14   16   19
 *                 direction every frame: 13 a frame, the fastest possible */

#define FD_EDGE 85.5656967f      // FD's main-stage edge; ground spots scale to each stage's.
#define PLAYER_CLEARANCE 22.f    // Least room between you and the nearer of the pair.
#define EDGE_MARGIN 6.f          // Keep fighters this far in from a surface's ends.
#define CORNER_INSET 14.f        // A corner spot's distance in from the ledge.
#define SETUP_FRAMES 40          // A grab that hasn't connected by then is tried again.
#define GRACE_FRAMES 12          // After the grab ends, the same attack can still land.
#define FOLLOWUP_IDLE 45         // Reset once the enemy is out of hitstun this long,
#define FOLLOWUP_MIN 60          // but never sooner than this after the result,
#define FOLLOWUP_MAX (8 * 60)    // and never later than this.
#define KO_WAIT 60
#define GRAB_IMMUNE 0x1FF        // grab.vuln: no grab type can catch this fighter.

// Teammate-to-enemy distance (center to center) each time the grab is tried.
static const float Grab_Gaps[] = { 8, 5, 11, 14 };
// Frames taken off the grab timer a frame by each mash setting, on average.
static const int Mash_Rate[] = { 1, 2, 4, 13 };

void Exit(GOBJ *menu);
static void Reset(int same_attempt);

enum menu_options {
    OPT_GRAB_SPOT,
    OPT_YOUR_SPOT,
    OPT_PERCENT,
    OPT_MASH,
    OPT_FACING,
};

enum location {
    LOC_RANDOM,
    LOC_LEFT_PLAT,
    LOC_RIGHT_PLAT,
    LOC_CENTER,
    LOC_UNDER_LEFT,
    LOC_UNDER_RIGHT,
    LOC_LEFT_CORNER,
    LOC_RIGHT_CORNER,
    LOC_COUNT,
};
static const char *Values_Location[] = {
    "Random", "Side Plat Left", "Side Plat Right", "Center Stage",
    "Under Left Plat", "Under Right Plat", "Left Corner", "Right Corner",
};

enum mash { MASH_NONE, MASH_LIGHT, MASH_HEAVY, MASH_INHUMAN };
static const char *Values_Mash[] = { "No Mash", "Light Mash", "Heavy Mash", "Inhuman Mash" };
typedef char grabpunish_mash_rates[(countof(Mash_Rate) == countof(Values_Mash)) ? 1 : -1];

enum facing { FACING_RANDOM, FACING_TOWARD, FACING_AWAY };
static const char *Values_Facing[] = { "Random", "Toward You", "Away From You" };

static const char *Values_Percent[] = { "0%", "20%", "40%", "60%", "80%", "100%" };

// Each saved row's preference ID is its row number.
typedef char grabpunish_saved_rows[((int)OPT_GRAB_SPOT == TM_GP_GRAB_SPOT && (int)OPT_YOUR_SPOT == TM_GP_YOUR_SPOT &&
    (int)OPT_PERCENT == TM_GP_PERCENT && (int)OPT_MASH == TM_GP_MASH && (int)OPT_FACING == TM_GP_FACING &&
    countof(Values_Location) == TM_GP_LOCATION_COUNT && countof(Values_Percent) == TM_GP_PERCENT_COUNT &&
    countof(Values_Mash) == TM_GP_MASH_COUNT && countof(Values_Facing) == TM_GP_FACING_COUNT) ? 1 : -1];
static void SaveSettings(GOBJ *menu, int value);

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_STRING,
        .name = "Grab Location",
        .values = Values_Location,
        .value_num = countof(Values_Location),
        .OnChange = SaveSettings,
        .desc = {"Where your teammate grabs the enemy.",
                 "Without that platform (like on Final Destination),",
                 "the ground where it usually is gets used.",
                 "Takes effect on the next reset."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Your Location",
        .values = Values_Location,
        .value_num = countof(Values_Location),
        .OnChange = SaveSettings,
        .desc = {"Where you start. The grab's spot, or one too close",
                 "to it, puts you beside the pair instead, so your",
                 "teammate never grabs you instead of the enemy.",
                 "Takes effect on the next reset."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Enemy Percent",
        .values = Values_Percent,
        .value_num = countof(Values_Percent),
        .val = 3,
        .val_prev = 3,
        .OnChange = SaveSettings,
        .desc = {"The enemy's percent when grabbed. Higher percents",
                 "are held longer: 75 + 1.6 frames per percent",
                 "before mashing (0%: 75f, 60%: 171f, 100%: 235f).",
                 "Takes effect on the next reset."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "CPU Mashing",
        .values = Values_Mash,
        .value_num = countof(Values_Mash),
        .val = MASH_LIGHT,
        .val_prev = MASH_LIGHT,
        .OnChange = SaveSettings,
        .desc = {"How hard the enemy mashes. Each mash input takes",
                 "6 frames off the hold, on top of 1 a frame.",
                 "Light holds 1/2 as long, Heavy 1/4, Inhuman 1/13.",
                 "See Mashing Info for the frames."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Teammate Facing",
        .values = Values_Facing,
        .value_num = countof(Values_Facing),
        .OnChange = SaveSettings,
        .desc = {"Toward You: the enemy is between you and your",
                 "teammate. Away From You: your teammate is in",
                 "front, so it's easier to hit them too.",
                 "Random picks each reset."},
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Mashing Info",
        .desc = {"A grab holds 75 + 1.6 x percent frames, minus 6",
                 "for each new A/B/X/Y press or stick direction.",
                 "60%: none 171f, light 86f, heavy 43f, inhuman 14f.",
                 "100%: none 235f, light 118f, heavy 59f, inhuman 19f."},
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Help",
        .desc = {"Punish the grabbed enemy before they mash out.",
                 "Success: hit them. Partial Success: hit them and",
                 "your teammate. Failure: no hit. Mega Failure: only",
                 "your teammate. D-Pad left/right resets, uncounted."},
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
    .name = "2v1 Grab Punish",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};
EventMenu *Event_Menu = &Menu_Main;

// The menu is saved to the memory card as it changes.
static void SaveSettings(GOBJ *menu, int value) {
    for (int i = 0; i < TM_GP_PREF_COUNT; i++)
        TM_SetSetting(TM_SETTING_GRAB_PUNISH, i, Options_Main[i].val);
}
static void LoadSettings(void) {
    for (int i = 0; i < TM_GP_PREF_COUNT; i++)
        Options_Main[i].val = Options_Main[i].val_prev = TM_GetSetting(TM_SETTING_GRAB_PUNISH, i);
}

enum phase {
    PHASE_SETUP,    // Your teammate is grabbing the enemy.
    PHASE_GRAB,     // The enemy is held and mashing.
    PHASE_RESOLVE,  // The grab ended; the same attack can still land.
    PHASE_FOLLOWUP, // Counted; keep punishing until the enemy settles.
};

enum result { RESULT_NONE, RESULT_SUCCESS, RESULT_PARTIAL, RESULT_FAILURE, RESULT_MEGA };
static const char *Result_Text[] = { "-", "Success", "Partial Success", "Failure", "Mega Failure" };

// Ports of the three fighters; -1 when the CSS didn't give us one.
static int hmn_ply = -1, mate_ply = -1, enemy_ply = -1;
static bool roles_found;
static int phase = PHASE_SETUP, phase_timer = 0;

typedef struct Spot {
    float x;
    int platform; // 1: on a one-way platform line; 0: the main stage's ground.
    int line;     // The platform's line.
    float left, right;
} Spot;

// This attempt. A D-pad reset starts a new one without counting it.
static struct attempt {
    Spot grab, you;
    float mid, you_x;
    int dir;          // Your teammate faces this way, toward the enemy.
    int tries;        // Grabs tried this attempt; picks the gap.
    bool hit_enemy, hit_mate;
    int result;
    int frames_left;  // Grab Time Left while the enemy is held.
    int time_left;    // Grab Time Left once it's over: frames at your hit, or a TIME_*.
    int idle, dead;   // Follow-up: frames the enemy has been settled, or KO'd.
    bool ko;
    float enemy_pct, mate_pct;
} attempt;
enum { TIME_NONE = -1, TIME_ESCAPED = -2, TIME_BROKEN = -3 };

// This session's totals and the last counted attempt.
static struct stats {
    int total, counts[5];
    int last;
    bool last_ko;
} stats;

/* Live collision geometry, as in Low Laser Puff Practice: current vertices
 * follow Fountain's moving platforms, and only enabled lines count. */
static int IsPlatformLine(const CollLineDesc *line) {
    /* Native MapLine.lo_flags LINE_FLAG_PLATFORM is 0x0100. */
    return ((const u8 *)line)[0xE] & 1;
}
static int FloorLine(int i, Vec2 *a, Vec2 *b, int *platform) {
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts || i < 0 || i >= stage->line_num) return 0;
    CollLineDesc *line = lines[i].desc;
    if (!line || !line->is_floor || line->disabled || !GrColl_CheckIfLineEnabled(i) ||
        line->vert_prev < 0 || line->vert_next < 0 ||
        line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num) return 0;
    *a = verts[line->vert_prev].pos_curr;
    *b = verts[line->vert_next].pos_curr;
    if (a->X > b->X) { Vec2 swap = *a; *a = *b; *b = swap; }
    *platform = IsPlatformLine(line);
    return b->X - a->X >= 0.01f;
}
static int LineCount(void) { return *stc_colldata ? (*stc_colldata)->line_num : 0; }
static int LineY(int i, float x, float *y) {
    Vec2 a, b;
    int platform;
    if (!FloorLine(i, &a, &b, &platform) || x < a.X || x > b.X) return 0;
    *y = a.Y + (b.Y - a.Y) * (x - a.X) / (b.X - a.X);
    return 1;
}
static int FindSolidGround(float x, Vec3 *position, int *index) {
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        float y;
        if (!FloorLine(i, &a, &b, &platform) || platform || !LineY(i, x, &y)) continue;
        if (!found || y > position->Y) { *position = (Vec3){x, y, 0}; *index = i; found = 1; }
    }
    return found;
}
static void GroundEdges(float *left, float *right) {
    *left = -FD_EDGE; *right = FD_EDGE;
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        if (!FloorLine(i, &a, &b, &platform) || platform) continue;
        if (!found || a.X < *left) *left = a.X;
        if (!found || b.X > *right) *right = b.X;
        found = 1;
    }
}
/* Solid ground at x, moving toward center if a non-legal stage has a gap. */
static void GroundPoint(float x, Vec3 *position, int *index) {
    float inward = x;
    for (int i = 0; i < 4; ++i, inward *= .5f)
        if (FindSolidGround(inward, position, index)) return;
    Vec3 slope;
    int kind;
    *position = (Vec3){x, 0, 0};
    *index = -1;
    GrColl_RaycastGround(position, index, &kind, &slope, -1, -1, -1, 0, x, 10, x, -20, 0);
}
static int LegalStage(void) {
    int stage = Stage_GetExternalID();
    return stage == GRKINDEXT_STORY || stage == GRKINDEXT_IZUMI || stage == GRKINDEXT_FD ||
        stage == GRKINDEXT_PSTAD || stage == GRKINDEXT_BATTLE || stage == GRKINDEXT_OLDPU;
}
/* side -1/1: the left or right one-way platform nearest the middle. */
static int FindPlatform(int side, Vec3 *position, int *index) {
    if (!LegalStage()) return 0;
    int found = 0;
    float best = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        float y;
        if (!FloorLine(i, &a, &b, &platform) || !platform || b.X - a.X < 10) continue;
        float x = (a.X + b.X) * .5f;
        if (side < 0 ? x >= -10 : x <= 10) continue;
        if (!LineY(i, x, &y)) continue;
        if (!found || -fabs(x) > best) { *position = (Vec3){x, y, 0}; *index = i; best = -fabs(x); found = 1; }
    }
    return found;
}

static void ResolveSpot(int choice, Spot *spot) {
    float left, right;
    GroundEdges(&left, &right);
    *spot = (Spot) { .line = -1, .left = left, .right = right };
    if (choice == LOC_CENTER) return;
    if (choice == LOC_LEFT_CORNER || choice == LOC_RIGHT_CORNER) {
        spot->x = choice == LOC_LEFT_CORNER ? left + CORNER_INSET : right - CORNER_INSET;
        return;
    }
    int side = choice == LOC_LEFT_PLAT || choice == LOC_UNDER_LEFT ? -1 : 1;
    Vec3 platform;
    int index;
    if (!FindPlatform(side, &platform, &index)) {
        // No platform there: the ground where FD's would be, scaled to this stage.
        spot->x = 35 * (side < 0 ? left : right) / FD_EDGE;
        return;
    }
    spot->x = platform.X;
    if (choice == LOC_LEFT_PLAT || choice == LOC_RIGHT_PLAT) {
        Vec2 a, b;
        int is_platform;
        FloorLine(index, &a, &b, &is_platform);
        spot->platform = 1;
        spot->line = index;
        spot->left = a.X;
        spot->right = b.X;
    }
}

static float Clamp(float x, float lo, float hi) {
    if (lo > hi) return (lo + hi) * .5f;
    return x < lo ? lo : x > hi ? hi : x;
}
static int RandomSign(void) { return HSD_Randi(2) ? 1 : -1; }

static void UpdatePosition(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    Vec3 pos = ft->phys.pos;
    ft->phys.pos_prev = pos;
    ft->coll_data.topN_Curr = ft->coll_data.topN_CurrCorrect = pos;
    ft->coll_data.topN_Prev = ft->coll_data.topN_Proj = pos;
    ft->coll_data.coll_test = *stc_colltest;
    JOBJ *joint = fighter->hsd_object;
    joint->trans = pos;
    JOBJ_SetMtxDirtySub(joint);
    Fighter_SetPosition(ft->ply, ft->flags.ms, &pos);
}
static void GroundAt(GOBJ *fighter, Vec3 ground, int line) {
    FighterData *ft = fighter->userdata;
    if (line >= 0) ft->coll_data.ground_index = line;
    ft->phys.pos = ground;
    ft->phys.self_vel = ft->phys.kb_vel = ft->phys.self_vel_ground = (Vec3){0};
    ft->phys.atk_shield_kb_vel = (Vec3){0};
    ft->phys.nudge_vel = (Vec2){0};
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(ft);
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
}
static void PlaceAt(GOBJ *fighter, const Spot *spot, float x, int facing) {
    if (!fighter) return;
    FighterData *ft = fighter->userdata;
    Vec3 pos = { x, 0, 0 };
    int line = spot->line;
    if (!spot->platform || !LineY(line, x, &pos.Y))
        GroundPoint(x, &pos, &line);
    GroundAt(fighter, pos, line);
    ft->facing_direction = ft->facing_direction_prev = facing;
    Fighter_EnterWait(fighter);
    Fighter_UpdateCameraBox(fighter);
}
static GOBJ *Body(int ply, int ms) {
    GOBJ *leader = Fighter_GetGObj(ply);
    if (!ms) return leader;
    // Zelda/Sheik's second GOBJ is a sleeping alternate form; only the Ice
    // Climbers' Nana follows along.
    if (!leader || !((FighterData *)leader->userdata)->flags.has_follower) return 0;
    return Fighter_GetSubcharGObj(ply, ms);
}
// The fighter, and Nana a little behind Popo.
static void PlacePlayer(int ply, const Spot *spot, float x, int facing) {
    PlaceAt(Body(ply, 0), spot, x, facing);
    PlaceAt(Body(ply, 1), spot, x - facing * 6, facing);
}

static FighterData *Data(int ply) { return Fighter_GetGObj(ply)->userdata; }

static void FindRoles(void) {
    roles_found = true;
    hmn_ply = mate_ply = enemy_ply = -1;
    for (int i = 0; i < 6 && hmn_ply < 0; i++) {
        Playerblock *player = Fighter_GetPlayerblock(i);
        if (player->state && player->p_kind == PKIND_HMN && Fighter_GetGObj(i)) hmn_ply = i;
    }
    if (hmn_ply < 0) return;
    int team = Fighter_GetPlayerblock(hmn_ply)->team;
    for (int i = 0; i < 6; i++) {
        Playerblock *player = Fighter_GetPlayerblock(i);
        if (!player->state || player->p_kind != PKIND_CPU || !Fighter_GetGObj(i)) continue;
        if (player->team == team) { if (mate_ply < 0) mate_ply = i; }
        else if (enemy_ply < 0) enemy_ply = i;
    }
}
static bool RolesReady(void) { return hmn_ply >= 0 && mate_ply >= 0 && enemy_ply >= 0; }

/* Where everyone stands this attempt. Random choices hold for its retries. */
static void ChooseSpots(void) {
    int grab_choice = Options_Main[OPT_GRAB_SPOT].val, your_choice = Options_Main[OPT_YOUR_SPOT].val;
    if (grab_choice == LOC_RANDOM) grab_choice = 1 + HSD_Randi(LOC_COUNT - 1);
    if (your_choice == LOC_RANDOM) your_choice = 1 + HSD_Randi(LOC_COUNT - 1);
    ResolveSpot(grab_choice, &attempt.grab);
    ResolveSpot(your_choice, &attempt.you);

    // The widest gap the retries use, so the pair fits on its surface.
    float half = Grab_Gaps[countof(Grab_Gaps) - 1] * .5f;
    Spot *grab = &attempt.grab, *you = &attempt.you;
    float mid = Clamp(grab->x, grab->left + EDGE_MARGIN + half, grab->right - EDGE_MARGIN - half);
    float x = you->x;
    int same_surface = you->platform == grab->platform && (!you->platform || you->line == grab->line);
    float room = half + PLAYER_CLEARANCE;
    if (same_surface && fabs(x - mid) < room) {
        // Too close, or the same spot: beside the pair, on the side you were
        // already on, else the other side, else the end with more room.
        float lo = you->left + EDGE_MARGIN, hi = you->right - EDGE_MARGIN;
        int side = fabs(x - mid) < .5f ? RandomSign() : x > mid ? 1 : -1;
        x = mid + side * room;
        if (x < lo || x > hi) {
            x = mid - side * room;
            if (x < lo || x > hi) x = mid - lo > hi - mid ? lo : hi;
        }
    }
    attempt.mid = mid;
    attempt.you_x = x;

    int toward = x > mid + 1 ? 1 : x < mid - 1 ? -1 : RandomSign();
    int facing = Options_Main[OPT_FACING].val;
    attempt.dir = facing == FACING_TOWARD ? toward : facing == FACING_AWAY ? -toward : RandomSign();
}

static void PlaceFighters(void) {
    float half = Grab_Gaps[attempt.tries % countof(Grab_Gaps)] * .5f;
    float mid = attempt.mid;
    int dir = attempt.dir;
    PlacePlayer(hmn_ply, &attempt.you, attempt.you_x, mid > attempt.you_x ? 1 : -1);
    PlacePlayer(mate_ply, &attempt.grab, mid - dir * half, dir);
    PlacePlayer(enemy_ply, &attempt.grab, mid + dir * half, -dir);
    Match_CorrectCamera();

    FighterData *enemy = Data(enemy_ply);
    float percent = Options_Main[OPT_PERCENT].val * 20;
    enemy->dmg.percent = enemy->dmg.percent_temp = percent;
    Fighter_SetHUDDamage(enemy_ply, percent);
    attempt.enemy_pct = percent;
    attempt.mate_pct = Data(mate_ply)->dmg.percent;
}

static void CountAttempt(int result) {
    attempt.result = result;
    stats.last = result;
    stats.last_ko = false;
    if (stats.total < 999999) {
        stats.total++;
        stats.counts[result]++;
    }
    SFX_Play(result == RESULT_SUCCESS ? 303 : result == RESULT_PARTIAL ? 0xAD : 0xAF);
}

static bool IsCaptured(FighterData *ft) {
    return ft->state_id >= ASID_CAPTUREPULLEDHI && ft->state_id <= ASID_CAPTUREDAMAGELW;
}
static bool IsGrabbing(FighterData *ft) {
    return ft->state_id >= ASID_CATCH && ft->state_id <= ASID_CATCHWAIT;
}

static int FramesLeft(FighterData *enemy) {
    float timer = enemy->grab.grab_timer;
    int rate = Mash_Rate[Options_Main[OPT_MASH].val];
    if (timer <= 0) return 0;
    int frames = (int)(timer / rate);
    return frames * rate < timer ? frames + 1 : frames;
}

/* Inputs a frame for each mash setting; frame counts from the grab. */
static void Mash(FighterData *cpu, int frame) {
    switch (Options_Main[OPT_MASH].val) {
        case MASH_LIGHT:
            if (frame % 6 < 3) cpu->cpu.held = PAD_BUTTON_A;
            break;
        case MASH_HEAVY:
            if (frame % 4 < 2) cpu->cpu.held = frame / 4 % 2 ? PAD_BUTTON_B : PAD_BUTTON_A;
            cpu->cpu.lstickX = (frame + 2) / 4 % 2 ? 127 : -127;
            break;
        case MASH_INHUMAN:
            // Alternating buttons, so each frame is a fresh press.
            cpu->cpu.held = frame % 2 ? PAD_BUTTON_B : PAD_BUTTON_A;
            cpu->cpu.lstickX = frame % 2 ? 127 : -127;
            break;
    }
}

static void HoldStill(int ply) {
    for (int ms = 0; ms < 2; ms++) {
        GOBJ *fighter = Body(ply, ms);
        if (!fighter) continue;
        FighterData *ft = fighter->userdata;
        ft->cpu.ai = 15;
        Fighter_ZeroCPUInputs(ft);
    }
}
static void SetGrabImmune(int ply, bool immune) {
    for (int ms = 0; ms < 2; ms++) {
        GOBJ *fighter = Body(ply, ms);
        if (!fighter) continue;
        FighterData *ft = fighter->userdata;
        if (immune) ft->grab.vuln = GRAB_IMMUNE;
        // A state you entered since keeps its own value.
        else if (ft->grab.vuln == GRAB_IMMUNE) ft->grab.vuln = 0;
    }
}

static void StartGrab(FighterData *enemy) {
    // As if the match were tied with handicap off (see the top of this file).
    ftCommonData *common = *stc_ftcommon;
    enemy->grab.grab_timer += common->grab_placing_mult * Fighter_GetCurrentPlacing(enemy_ply)
        - common->grab_mash_per_handicap * (common->grab_max_handicap - Fighter_GetHandicap(enemy_ply));
    SetGrabImmune(hmn_ply, false);
    attempt.frames_left = FramesLeft(enemy);
    phase = PHASE_GRAB;
    phase_timer = 0;
}

// New damage since last frame; neither CPU attacks, so it's yours.
static bool TookHit(int ply, float *last) {
    float percent = Data(ply)->dmg.percent;
    bool hit = percent > *last;
    *last = percent;
    return hit;
}

static void DrawHUD(GOBJ *gobj, int pass) {
    if (pass != 2 || !roles_found)
        return;
    if (!RolesReady()) {
        static const char *labels[] = {"Teammate CPU", "Enemy CPU"};
        const char *info[] = {
            mate_ply >= 0 ? "Ready" : "Missing",
            enemy_ply >= 0 ? "Ready" : "Missing",
        };
        event_vars->HUD_DrawInfoPanel(labels, info, countof(labels));
        return;
    }
    static char time_text[16], last_text[24], count_text[4][24];
    if (phase == PHASE_GRAB)
        sprintf(time_text, "%df", attempt.frames_left);
    else if (attempt.time_left >= 0)
        sprintf(time_text, "Hit, %df", attempt.time_left);
    else
        sprintf(time_text, "%s", attempt.time_left == TIME_ESCAPED ? "Escaped"
            : attempt.time_left == TIME_BROKEN ? "Let Go" : "-");
    sprintf(last_text, "%s%s", Result_Text[stats.last], stats.last_ko ? " + KO" : "");
    for (int i = 0; i < 4; i++) {
        int count = stats.counts[RESULT_SUCCESS + i];
        if (stats.total)
            sprintf(count_text[i], "%d (%.0f%%)", count, 100.f * count / stats.total);
        else
            sprintf(count_text[i], "0");
    }
    static const char *labels[] = {
        "Grab Time Left", "Last Attempt", "Success", "Partial Success", "Failure", "Mega Failure",
    };
    const char *info[] = {
        time_text, last_text, count_text[0], count_text[1], count_text[2], count_text[3],
    };
    event_vars->HUD_DrawInfoPanel(labels, info, countof(labels));
}

void Event_Init(GOBJ *gobj) {
    LoadSettings();
    stats = (struct stats) { 0 };
    attempt = (struct attempt) { .time_left = TIME_NONE };
    GObj_AddGXLink(gobj, DrawHUD, GXLINK_HUD, 80);
}

static void Reset(int same_attempt) {
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    if (!same_attempt) {
        attempt = (struct attempt) { .time_left = TIME_NONE };
        ChooseSpots();
    }
    PlaceFighters();
    phase = PHASE_SETUP;
    phase_timer = 0;
}

void Event_Think(GOBJ *gobj) {
    if (Pause_CheckStatus(1) == 2)
        return;
    if (event_vars->game_timer == 1) {
        FindRoles();
        if (RolesReady()) {
            event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
            Reset(0);
        }
    }
    if (!RolesReady())
        return;

    FighterData *hmn = Data(hmn_ply), *mate = Data(mate_ply), *enemy = Data(enemy_ply);
    if ((hmn->input.down & (HSD_BUTTON_DPAD_LEFT | HSD_BUTTON_DPAD_RIGHT)) || hmn->flags.dead) {
        Reset(0);
        return;
    }

    HoldStill(mate_ply);
    HoldStill(enemy_ply);
    phase_timer++;
    bool enemy_hit = TookHit(enemy_ply, &attempt.enemy_pct);
    bool mate_hit = TookHit(mate_ply, &attempt.mate_pct);

    switch (phase) {
        case PHASE_SETUP:
            if (IsCaptured(enemy)) {
                StartGrab(enemy);
                break;
            }
            SetGrabImmune(hmn_ply, true);
            if (enemy_hit || mate_hit) {
                // You hit one of them before the grab: set it up again.
                Reset(1);
            } else if (phase_timer == 2) {
                mate->cpu.held = PAD_TRIGGER_Z;
            } else if (phase_timer > SETUP_FRAMES || (phase_timer > 4 && !IsGrabbing(mate))) {
                // The grab missed: try again with another gap.
                attempt.tries++;
                Reset(1);
            }
            break;
        case PHASE_GRAB:
            // A hit lets go of the enemy, so the time left is last frame's.
            if (enemy_hit && !attempt.hit_enemy) {
                attempt.hit_enemy = true;
                attempt.time_left = attempt.frames_left;
            }
            attempt.hit_mate |= mate_hit;
            if (IsCaptured(enemy)) {
                attempt.frames_left = FramesLeft(enemy);
                Mash(enemy, phase_timer - 1);
                break;
            }
            if (!attempt.hit_enemy)
                attempt.time_left = attempt.hit_mate ? TIME_BROKEN : TIME_ESCAPED;
            phase = PHASE_RESOLVE;
            phase_timer = 0;
            break;
        case PHASE_RESOLVE:
            // The rest of the attack that hit can still land, but once the
            // enemy mashes out, hitting them is too late.
            attempt.hit_mate |= mate_hit;
            if (enemy_hit && attempt.time_left == TIME_BROKEN && !attempt.hit_enemy)
                attempt.hit_enemy = true;
            if (phase_timer < GRACE_FRAMES)
                break;
            CountAttempt(attempt.hit_enemy ? (attempt.hit_mate ? RESULT_PARTIAL : RESULT_SUCCESS)
                : (attempt.hit_mate ? RESULT_MEGA : RESULT_FAILURE));
            phase = PHASE_FOLLOWUP;
            phase_timer = 0;
            break;
        case PHASE_FOLLOWUP:
            if (enemy->flags.dead) {
                if (!attempt.ko && (attempt.result == RESULT_SUCCESS || attempt.result == RESULT_PARTIAL))
                    attempt.ko = stats.last_ko = true;
                if (++attempt.dead >= KO_WAIT)
                    Reset(0);
                break;
            }
            attempt.idle = enemy_hit || enemy->flags.hitstun || enemy->flags.hitlag ? 0 : attempt.idle + 1;
            if ((attempt.idle >= FOLLOWUP_IDLE && phase_timer >= FOLLOWUP_MIN) || phase_timer >= FOLLOWUP_MAX)
                Reset(0);
            break;
    }
}

void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}
