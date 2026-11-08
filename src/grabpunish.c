#include "events.h"

/* 2v1 Grab Punish Practice: your teammate CPU grabs the enemy CPU, and you
 * punish the enemy (say with a JC up-smash) before they mash out, without
 * hitting your teammate. The VS character select picks all three fighters
 * and the team colors. grabpunish_css.c keeps exactly one CPU on your team
 * there, and EventInit (GrabPunish_PrepareMatch in events.c) makes the match
 * the same way, so the CPU on your team is your teammate and the other is
 * the enemy. The teams are saved for this event alone, and nothing picked
 * there changes your normal character or costume.
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
 * Melee caps neither the percent nor the hold.
 * CPU Mashing (frames until the enemy breaks free):
 *                 0%  20%  40%  60%  80% 100% 120% 140% 160% 180% 200%
 *   No Mash       75  107  139  171  203  235  267  299  331  363  395
 *   Light         38   54   70   86  102  118  134  150  166  182  198
 *   Medium        25   36   47   57   68   79   89  100  111  121  132
 *   Heavy         19   27   35   43   51   59   67   75   83   91   99
 *   Very Heavy    11   16   20   25   29   34   39   43   48   52   57
 *   Inhuman        6    9   11   14   16   19   21   23   26   28   31
 * What each takes off the timer a frame:
 *   No Mash        1  nothing but the frame
 *   Light          2  a button every 6 frames (10 inputs a second)
 *   Medium         3  a button every 3 frames (20 a second)
 *   Heavy          4  a button and a new stick direction, each every 4
 *                     frames (30 a second)
 *   Very Heavy     7  a button and a new stick direction, each every 2
 *                     frames (60 a second)
 *   Inhuman       13  a new button and stick direction every frame, the
 *                     fastest possible
 *
 * The grab ending isn't the end of the punish. The enemy lands in CaptureCut,
 * the ground release: 30 frames (20 for Donkey Kong), which nothing can cut
 * short, then Wait. Holding up as it ends, or pressing X or Y with under 16
 * left on the timer, makes it CaptureJump, the jump release, instead. This
 * event's mash does neither. */

#define FD_EDGE 85.5656967f      // FD's main-stage edge; ground spots scale to each stage's.
#define PLAYER_CLEARANCE 22.f    // Least room between you and the nearer of the pair.
#define EDGE_MARGIN 6.f          // Keep fighters this far in from a surface's ends.
#define CORNER_INSET 14.f        // A corner spot's distance in from the ledge.
#define SETUP_FRAMES 40          // A grab that hasn't connected by then is tried again.
#define GRACE_FRAMES 12          // After your hit, the same attack can still land.
#define FOLLOWUP_IDLE 45         // Reset once the enemy is out of hitstun this long,
#define FOLLOWUP_MIN 60          // but never sooner than this after the result,
#define FOLLOWUP_MAX (8 * 60)    // and never later than this.
#define KO_WAIT 60
#define GRAB_IMMUNE 0x1FF        // grab.vuln: no grab type can catch this fighter.
// The big countdown's HUD rect, just under the match timer, with its text's
// size and black border. Bigger text sits lower in the rect: from this Y its
// digits are 77 to 95 down the 640x480 screen, and the timer's end at 74.
#define BIG_X -5.f
#define BIG_Y 12.9f
#define BIG_WIDTH 10.f
#define BIG_HEIGHT 2.5f
#define BIG_SIZE 1.f
#define BIG_BORDER 2.f

// Teammate-to-enemy distance (center to center) each time the grab is tried.
static const float Grab_Gaps[] = { 8, 5, 11, 14 };
// Frames taken off the grab timer a frame by each mash setting, on average.
static const int Mash_Rate[] = { 1, 2, 3, 4, 7, 13 };

void Exit(GOBJ *menu);
static void Reset(int same_attempt);

enum menu_options {
    OPT_GRAB_SPOT,
    OPT_YOUR_SPOT,
    OPT_PERCENT,
    OPT_MATE_PERCENT,
    OPT_MASH,
    OPT_FACING,
    OPT_RELEASE_HITS,
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

enum mash { MASH_NONE, MASH_LIGHT, MASH_MEDIUM, MASH_HEAVY, MASH_VERY_HEAVY, MASH_INHUMAN };
static const char *Values_Mash[] = {
    "No Mash", "Light Mash", "Medium Mash", "Heavy Mash", "Very Heavy Mash", "Inhuman Mash",
};
typedef char grabpunish_mash_rates[(countof(Mash_Rate) == countof(Values_Mash)) ? 1 : -1];

enum facing { FACING_RANDOM, FACING_TOWARD, FACING_AWAY };
static const char *Values_Facing[] = { "Random", "Toward You", "Away From You" };

static const char *Values_Percent[] = {
    "0%", "20%", "40%", "60%", "80%", "100%", "120%", "140%", "160%", "180%", "200%",
};

// Each saved row's preference ID is its row number.
typedef char grabpunish_saved_rows[((int)OPT_GRAB_SPOT == TM_GP_GRAB_SPOT && (int)OPT_YOUR_SPOT == TM_GP_YOUR_SPOT &&
    (int)OPT_PERCENT == TM_GP_PERCENT && (int)OPT_MATE_PERCENT == TM_GP_MATE_PERCENT &&
    (int)OPT_MASH == TM_GP_MASH && (int)OPT_FACING == TM_GP_FACING &&
    (int)OPT_RELEASE_HITS == TM_GP_RELEASE_HITS &&
    countof(Values_Location) == TM_GP_LOCATION_COUNT && countof(Values_Percent) == TM_GP_PERCENT_COUNT &&
    countof(Values_Mash) == TM_GP_MASH_COUNT && countof(Values_Facing) == TM_GP_FACING_COUNT) ? 1 : -1];
static void SaveSettings(GOBJ *menu, int value);
static void ChangeReleaseHits(GOBJ *menu, int value);

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
                 "before mashing (0%: 75f, 60%: 171f, 200%: 395f).",
                 "Takes effect on the next reset."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Teammate Percent",
        .values = Values_Percent,
        .value_num = countof(Values_Percent),
        .OnChange = SaveSettings,
        .desc = {"Your teammate's percent at the start of each grab.",
                 "It doesn't change how long the enemy is held.",
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
                 "Light holds 1/2 as long, Medium 1/3, Heavy 1/4,",
                 "Very Heavy 1/7, Inhuman 1/13. See Mashing Info."},
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
        .kind = OPTKIND_TOGGLE,
        .name = "Release Hit = Success",
        .val = 1,
        .val_prev = 1,
        .OnChange = ChangeReleaseHits,
        .desc = {"The enemy can't act for 30 frames after the grab",
                 "ends (20 for DK). On counts a hit in that window",
                 "as an Enemy Hit. It always shows as a Release Hit.",
                 "Changing this resets your totals."},
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Mashing Info",
        .desc = {"Holds 75 + 1.6 x percent frames, minus 6 for each",
                 "new A/B/X/Y press or stick direction. At 60%, 200%:",
                 "No Mash 171, 395. Light 86, 198. Medium 57, 132.",
                 "Heavy 43, 99. Very Heavy 25, 57. Inhuman 14, 31."},
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Help",
        .desc = {"Punish the grabbed enemy before they mash out.",
                 "Enemy Hits and Ally Hits count the attempts you hit",
                 "each one. Too late, they turn red and frames count",
                 "up in red. D-Pad left/right resets, uncounted."},
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
    for (int i = 0; i < TM_GP_ROW_COUNT; i++)
        TM_SetSetting(TM_SETTING_GRAB_PUNISH, i, Options_Main[i].val);
}
static void LoadSettings(void) {
    for (int i = 0; i < TM_GP_ROW_COUNT; i++)
        Options_Main[i].val = Options_Main[i].val_prev = TM_GetSetting(TM_SETTING_GRAB_PUNISH, i);
}

enum phase {
    PHASE_SETUP,    // Your teammate is grabbing the enemy.
    PHASE_GRAB,     // The enemy is held and mashing.
    PHASE_RELEASE,  // Let go without a hit, and can't act yet.
    PHASE_RESOLVE,  // You hit them; the same attack can still land.
    PHASE_FOLLOWUP, // Counted; keep punishing until the enemy settles.
};

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
    bool hit_enemy;   // In the grab; hit_release is in the release after it.
    bool hit_release, hit_mate;
    bool free;        // Got away without a hit, and can act: shown red.
    int frames_left;  // Grab Time Left while the enemy is held or in the release.
    int let_go_left;  // The frames left when your hit on your teammate let go of the enemy.
    int time_left;    // Grab Time Left once it's over: frames at your hit, or a TIME_*.
    int late;         // Once they got away: frames since they could act, or until your hit.
    bool late_hit;    // That count has stopped at your hit.
    int idle, dead;   // Follow-up: frames the enemy has been settled, or KO'd.
    float enemy_pct, mate_pct;
} attempt;
enum { TIME_NONE = -1, TIME_ESCAPED = -2, TIME_BROKEN = -3 };

// This session's totals. An attack that hits both counts for both.
static struct stats {
    int attempts, enemy_hits, ally_hits;
} stats;

// What counts as an Enemy Hit changed, so the totals start over.
static void ChangeReleaseHits(GOBJ *menu, int value) {
    SaveSettings(menu, value);
    stats = (struct stats) { 0 };
}

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

static void SetPercent(int ply, float percent) {
    for (int ms = 0; ms < 2; ms++) {
        GOBJ *fighter = Body(ply, ms);
        if (!fighter) continue;
        FighterData *ft = fighter->userdata;
        ft->dmg.percent = percent;
        // This field is pending damage to add, not a copy of the current percent.
        ft->dmg.percent_temp = 0;
    }
    Fighter_SetHUDDamage(ply, percent);
}

/* The pair, your teammate facing the enemy, a gap apart that changes with
 * each try. Each try starts them both at their menu percents. */
static void PlaceCPUs(void) {
    float half = Grab_Gaps[attempt.tries % countof(Grab_Gaps)] * .5f;
    float mid = attempt.mid;
    int dir = attempt.dir;
    PlacePlayer(mate_ply, &attempt.grab, mid - dir * half, dir);
    PlacePlayer(enemy_ply, &attempt.grab, mid + dir * half, -dir);
    Match_CorrectCamera();

    float percent = Options_Main[OPT_PERCENT].val * 20;
    float mate_percent = Options_Main[OPT_MATE_PERCENT].val * 20;
    SetPercent(enemy_ply, percent);
    SetPercent(mate_ply, mate_percent);
    attempt.enemy_pct = percent;
    attempt.mate_pct = mate_percent;
}

static void CountAttempt(void) {
    bool hit_enemy = attempt.hit_enemy || (attempt.hit_release && Options_Main[OPT_RELEASE_HITS].val);
    if (stats.attempts < 999999) {
        stats.attempts++;
        stats.enemy_hits += hit_enemy;
        stats.ally_hits += attempt.hit_mate;
    }
    SFX_Play(!hit_enemy ? 0xAF : attempt.hit_mate ? 0xAD : 303);
    phase = PHASE_FOLLOWUP;
    phase_timer = 0;
}

static bool IsCaptured(FighterData *ft) {
    return ft->state_id >= ASID_CAPTUREPULLEDHI && ft->state_id <= ASID_CAPTUREDAMAGELW;
}
static bool IsGrabbing(FighterData *ft) {
    return ft->state_id >= ASID_CATCH && ft->state_id <= ASID_CATCHWAIT;
}

static int WholeFrames(float frames) {
    int whole = (int)frames;
    return whole < frames ? whole + 1 : whole;
}
static int FramesLeft(FighterData *enemy) {
    float timer = enemy->grab.grab_timer;
    if (timer <= 0) return 0;
    return WholeFrames(timer / Mash_Rate[Options_Main[OPT_MASH].val]);
}
// Frames until the enemy can act after the grab, 0 once they can.
static int ReleaseFrames(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    if (ft->state_id != ASID_CAPTURECUT) return 0;
    int frames = WholeFrames((Fighter_GetCurrentAnimLength(fighter) - ft->state.frame) / ft->state.rate);
    return frames > 0 ? frames : 1;
}

/* Inputs a frame for each mash setting; frame counts from the grab. */
static void Mash(FighterData *cpu, int frame) {
    switch (Options_Main[OPT_MASH].val) {
        case MASH_LIGHT:
            if (frame % 6 < 3) cpu->cpu.held = PAD_BUTTON_A;
            break;
        case MASH_MEDIUM:
            if (frame % 3 == 0) cpu->cpu.held = PAD_BUTTON_A;
            break;
        case MASH_HEAVY:
            if (frame % 4 < 2) cpu->cpu.held = frame / 4 % 2 ? PAD_BUTTON_B : PAD_BUTTON_A;
            cpu->cpu.lstickX = (frame + 2) / 4 % 2 ? 127 : -127;
            break;
        case MASH_VERY_HEAVY:
            if (frame % 2 == 0) cpu->cpu.held = PAD_BUTTON_A;
            cpu->cpu.lstickX = (frame + 1) / 2 % 2 ? 127 : -127;
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

/* The timing overlay, so this red goes over every other overlay. */
static void SetEnemyOverlay(const GXColor *color) {
    for (int ms = 0; ms < 2; ms++) {
        GOBJ *fighter = Body(enemy_ply, ms);
        if (fighter) event_vars->set_timing_overlay(fighter, color);
    }
}
// The pulse of the Missed L Cancel and Run Turnaround overlays.
static void PulseEnemyRed(void) {
    static const u8 pulse[TM_CUE_PULSE_FRAMES] = TM_CUE_RED_PULSE;
    GXColor color = TM_CUE_RED_COLOR;
    color.a = pulse[phase_timer % TM_CUE_PULSE_FRAMES];
    SetEnemyOverlay(&color);
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
    static char time_text[32], hit_text[2][32], big_text[12];
    if (phase == PHASE_GRAB)
        sprintf(time_text, "%df", attempt.frames_left);
    else if (phase == PHASE_RELEASE)
        sprintf(time_text, "Release %df", attempt.frames_left);
    else if (attempt.time_left >= 0)
        sprintf(time_text, "%s Hit %df", attempt.hit_release ? "Release" : "Grab", attempt.time_left);
    else if (attempt.free)
        sprintf(time_text, "%s %df", attempt.late_hit ? "Late Hit"
            : attempt.time_left == TIME_ESCAPED ? "Escaped" : "Let Go", attempt.late);
    else
        sprintf(time_text, "-");
    const int hits[] = { stats.enemy_hits, stats.ally_hits };
    for (int i = 0; i < 2; i++) {
        if (stats.attempts)
            sprintf(hit_text[i], "%d/%d (%.1f%%)", hits[i], stats.attempts, 100.f * hits[i] / stats.attempts);
        else
            sprintf(hit_text[i], "-");
    }
    static const char *labels[] = { "Grab Time Left", "Enemy Hits", "Ally Hits" };
    const char *info[] = { time_text, hit_text[0], hit_text[1] };
    event_vars->HUD_DrawInfoPanel(labels, info, countof(labels));

    // The same frames, big, under the match timer. Yellow is the release, and
    // red is how late you are once it's over.
    int big = phase == PHASE_GRAB || phase == PHASE_RELEASE ? attempt.frames_left
        : attempt.free ? attempt.late : attempt.time_left;
    if (big >= 0) {
        Rect rect = { BIG_X, BIG_Y, BIG_WIDTH, BIG_HEIGHT };
        bool release = phase == PHASE_RELEASE || attempt.hit_release;
        GXColor color = attempt.free ? (GXColor) TM_CUE_RED_COLOR
            : release ? (GXColor) {255, 240, 0, 255} : (GXColor) {255, 255, 255, 255};
        color.a = 255;
        sprintf(big_text, "%d", big);
        event_vars->HUD_DrawTextEx(big_text, &rect, BIG_SIZE, color, (GXColor) {0, 0, 0, 255}, BIG_BORDER, 1);
    }
}

void Event_Init(GOBJ *gobj) {
    LoadSettings();
    stats = (struct stats) { 0 };
    attempt = (struct attempt) { .time_left = TIME_NONE };
    GObj_AddGXLink(gobj, DrawHUD, GXLINK_HUD, 80);
}

/* A new attempt starts everyone over. Trying the grab again only moves the
 * pair: a savestate would put you back too and rewind the event timer, which
 * keeps the pause menu shut for a few frames. */
static void Reset(int same_attempt) {
    if (!same_attempt) {
        event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
        SetEnemyOverlay(0);
        attempt = (struct attempt) { .time_left = TIME_NONE };
        ChooseSpots();
        PlacePlayer(hmn_ply, &attempt.you, attempt.you_x, attempt.mid > attempt.you_x ? 1 : -1);
    }
    PlaceCPUs();
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
            if (attempt.hit_enemy) {
                phase = PHASE_RESOLVE;
            } else {
                attempt.let_go_left = attempt.hit_mate ? attempt.frames_left : TIME_NONE;
                attempt.frames_left = ReleaseFrames(Fighter_GetGObj(enemy_ply));
                phase = PHASE_RELEASE;
            }
            phase_timer = 0;
            break;
        case PHASE_RELEASE:
            attempt.hit_mate |= mate_hit;
            if (enemy_hit) {
                // Your hit on your teammate let go of the enemy, and the rest
                // of that attack can still land: that hit is in the grab.
                attempt.hit_enemy = attempt.let_go_left >= 0 && phase_timer <= GRACE_FRAMES;
                attempt.hit_release = !attempt.hit_enemy;
                attempt.time_left = attempt.hit_enemy ? attempt.let_go_left : attempt.frames_left;
                phase = PHASE_RESOLVE;
                phase_timer = 0;
                break;
            }
            attempt.frames_left = ReleaseFrames(Fighter_GetGObj(enemy_ply));
            if (attempt.frames_left)
                break;
            attempt.free = true;
            attempt.time_left = attempt.let_go_left >= 0 ? TIME_BROKEN : TIME_ESCAPED;
            attempt.late = 1;
            CountAttempt();
            break;
        case PHASE_RESOLVE:
            // The rest of the attack that hit can still reach your teammate.
            attempt.hit_mate |= mate_hit;
            if (phase_timer >= GRACE_FRAMES)
                CountAttempt();
            break;
        case PHASE_FOLLOWUP:
            // A hit stops the count on last frame's, as with the time left.
            if (attempt.free && !attempt.late_hit) {
                if (enemy_hit) attempt.late_hit = true;
                else attempt.late++;
            }
            if (enemy->flags.dead) {
                if (++attempt.dead >= KO_WAIT)
                    Reset(0);
                break;
            }
            attempt.idle = enemy_hit || enemy->flags.hitstun || enemy->flags.hitlag ? 0 : attempt.idle + 1;
            if ((attempt.idle >= FOLLOWUP_IDLE && phase_timer >= FOLLOWUP_MIN) || phase_timer >= FOLLOWUP_MAX)
                Reset(0);
            break;
    }
    if (attempt.free)
        PulseEnemyRed();
}

void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}
