#include "events.h"
#include "stage_points.h"
#include "stage_swap.h"
#include "fighter_swap.h"

/* Grab-ercise (Minigames): Eggs-ercise with a CPU to grab instead of eggs to
 * break. The CPU you picked appears somewhere on the stage you picked: on a
 * platform, on the ground under one, or anywhere along the ground. Half the
 * time it stands still; otherwise it walks, runs or rolls at full speed, or
 * dash dances, back and forth across the floor it's on, or dash dances in
 * place, with full or half dashes. Grab it,
 * throw it, and it
 * disappears and comes back somewhere else. A timed minute counts grabs, with
 * the best saved; Free Practice has no clock and lets you pick the movements.
 *
 * The CPU moves through its own inputs (cpu.ai 15), set in the think, which
 * runs before the fighters' input proc. The rules below are the native
 * ones, from the decomp (ftCo_Walk/Dash/Run/TurnRun/Turn/Guard/Escape.c), with
 * PlCo.dat's ftLoadCommonData values:
 *
 * - An axis's tilt timer (input.timer_lstick_tilt_x) goes to 0 when the stick
 *   crosses PlCo 0x8 (0.25) from inside, and counts up while it stays past.
 * - A dash (ftCo_Dash_CheckInput) is a stick at least 0x3C (0.8) out with the
 *   timer under 0x40 (2), from Wait, Walk or a dash: a fresh smash. So a full
 *   speed walk tilts halfway first and goes all the way only once the timer
 *   can't be under 2.
 * - The dash script sets cmd_vars[0] (ftcmd_var.flag0) partway through (frame
 *   8-19 by character), and from then a stick 0x58 (0.625) forward turns the
 *   dash into Run (fn_800CA5F0). A reversed fresh smash turns a dash around
 *   (Turn_Enter_Smash, then Dash on the next frame if the stick is still out),
 *   from its fifth frame (0x44) or at once if the dash came from a turn. The
 *   turn brakes the dash to a quarter of its speed (0x54, 0.75 off), and the
 *   new dash adds its initial speed the other way (ftCo_Dash_Enter).
 * - From Run, a stick 0x38 (-0.375) back turns around (TurnRun), which brakes
 *   at the dash acceleration until the velocity reverses, then runs.
 * - From GuardOn or Guard, a stick 0x31C (0.7) out with the timer under 0x320
 *   (4) rolls. Rolls and run turnarounds stop at the edge of a floor; walks,
 *   dashes and runs go off it. */

// The movement rows, in menu order, as the saved preferences number them.
enum move { MOVE_STAND, MOVE_WALK, MOVE_RUN, MOVE_DASH, MOVE_DASH_FULL, MOVE_DASH_HALF, MOVE_ROLL };

// PlCo values the inputs below rely on (see the comment above).
#define TILT .25f          // 0x8: the tilt timers' crossing.
// CPU stick values (value / 127 in the game).
#define STICK_FULL 127
#define STICK_WALK 64      // .50: a walk, not a dash.
#define SAFE_TILT_FRAMES 2 // A walk goes to full tilt once the timer reads this.

/* A moving CPU turns back once it would otherwise stop closer than this to
 * the end of its floor, and only toward the side with more room. */
#define EDGE_ROOM 6.f
#define ROLL_ROOM 20.f     // Under a roll: the last one runs out of floor and stops at the edge.

// Spawns: this far in from a floor's ends, from you, and from the last spot.
#define SPAWN_INSET 8.f
#define SPAWN_FROM_YOU 30.f
#define SPAWN_FROM_LAST 25.f
#define SPAWN_TRIES 40
// An airborne CPU this long, or this far off the stage, comes back.
#define AIR_FRAMES 120
#define OFF_STAGE 10.f

#define TIMED_FRAMES 3600  // The timed run's minute.

enum options_main {
    OPT_RETRY, OPT_STAGE, OPT_CHARACTERS, OPT_FREEPRACTICE,
    OPT_WALK, OPT_RUN, OPT_DASH, OPT_DASH_FULL, OPT_DASH_HALF, OPT_ROLL, OPT_COLLISION,
    OPT_COUNTER, OPT_DEFAULTS, OPT_EXIT, OPT_COUNT
};
// The movement rows' order is the saved preferences' and the moves'.
#define SAME_ORDER(opt, pref, move) ((opt) - OPT_WALK == (pref) - TM_GRABS_WALK && (move) - MOVE_WALK == (opt) - OPT_WALK)
typedef char grabercise_rows[(SAME_ORDER(OPT_RUN, TM_GRABS_RUN, MOVE_RUN) &&
    SAME_ORDER(OPT_DASH, TM_GRABS_DASH, MOVE_DASH) && SAME_ORDER(OPT_DASH_FULL, TM_GRABS_DASH_FULL, MOVE_DASH_FULL) &&
    SAME_ORDER(OPT_DASH_HALF, TM_GRABS_DASH_HALF, MOVE_DASH_HALF) && SAME_ORDER(OPT_ROLL, TM_GRABS_ROLL, MOVE_ROLL) &&
    OPT_ROLL - OPT_WALK + 1 == TM_GRABS_MOVES) ? 1 : -1];
#undef SAME_ORDER

static void Retry(GOBJ *menu);
static void Exit(GOBJ *menu);
static void StartFreePractice(GOBJ *menu);
static void ChangeWalk(GOBJ *menu, int value);
static void ChangeRun(GOBJ *menu, int value);
static void ChangeDash(GOBJ *menu, int value);
static void ChangeDashFull(GOBJ *menu, int value);
static void ChangeDashHalf(GOBJ *menu, int value);
static void ChangeRoll(GOBJ *menu, int value);
static void ChangeCollision(GOBJ *menu, int value);
static void ChangeCounterPosition(GOBJ *menu, int value);
static void ResetSettings(GOBJ *menu);

static const char *CounterPositionText[TM_EGGS_COUNTER_COUNT] = {
    "Top Left", "Top Right", "Bottom Left", "Bottom Right", "Under Timer"
};

static EventMenu Menu_Characters = {
    .name = "Change Characters",
    .option_num = countof(FighterSwap_Options),
    .options = FighterSwap_Options,
    .row_hooks = &FighterSwap_RowHooks,
};

static EventOption Options_Main[OPT_COUNT] = {
    {
        .kind = OPTKIND_FUNC,
        .name = "Retry",
        .desc = {"Retry this event. Pressing Z while paused also does this."},
        .OnSelect = Retry,
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Change Stage",
        .desc = {"Restart on another tournament-legal stage.",
                 "A timed run starts over; Free Practice stays on",
                 "with its saved settings."},
        .menu = &StageSwap_Menu,
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Change Characters",
        .desc = {"Pick your character and the CPU's, then restart.",
                 "A timed run starts over; Free Practice stays on",
                 "with its saved settings."},
        .menu = &Menu_Characters,
    },
    {
        .kind = OPTKIND_FUNC,
        .name = "Enable Free Practice",
        .desc = {"Practice with no time limit, and choose how the",
                 "CPU moves. Timed runs mix all four movements.",
                 "Reset Event Settings goes back to timed runs."},
        .OnSelect = StartFreePractice,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Full Speed Walk",
        .val = 1,
        .desc = {"Half the CPUs stand still. The rest pick one of",
                 "the movements that are on and go back and forth",
                 "on their floor. This one walks at full speed.",
                 "One movement always stays on. Free Practice only."},
        .disable = 1,
        .OnChange = ChangeWalk,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Full Speed Run",
        .val = 1,
        .desc = {"Moving CPUs can dash into a full speed run, and",
                 "run-turnaround at each end of their floor.",
                 "One movement always stays on. Free Practice only."},
        .disable = 1,
        .OnChange = ChangeRun,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Traveling Dash Dance",
        .val = 1,
        .desc = {"Moving CPUs can dash dance across: a full dash, a",
                 "quick dash back for a frame or two, then the next",
                 "dash, and the same the other way at each end.",
                 "One movement always stays on. Free Practice only."},
        .disable = 1,
        .OnChange = ChangeDash,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Full Dash Dance",
        .val = 1,
        .desc = {"Moving CPUs can dash dance in place: a full dash",
                 "one way, then a full dash back, over and over. A",
                 "full dash goes until it would turn into a run.",
                 "One movement always stays on. Free Practice only."},
        .disable = 1,
        .OnChange = ChangeDashFull,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Half Dash Dance",
        .val = 1,
        .desc = {"Moving CPUs can dash dance in place with dashes",
                 "half as long: half a dash one way, then half back.",
                 "The first dash of a match is a full one.",
                 "One movement always stays on. Free Practice only."},
        .disable = 1,
        .OnChange = ChangeDashHalf,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Rolling",
        .val = 1,
        .desc = {"Moving CPUs can shield and roll, roll after",
                 "roll, and roll back at each end.",
                 "One movement always stays on. Free Practice only."},
        .disable = 1,
        .OnChange = ChangeRoll,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fighter Collision",
        .desc = {"Toggle hitbox and hurtbox visualization.",
                 "Hurtboxes: yellow=hurt, purple=ungrabbable, blue=shield.",
                 "Hitboxes: (by priority) red, green, blue, purple.",
                 "Free Practice only."},
        .disable = 1,
        .OnChange = ChangeCollision,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = countof(CounterPositionText),
        .name = "Counter Position",
        .desc = {"Move the grab count and the text under it to a",
                 "corner of the screen or under the timer.",
                 "Shared with Eggs-ercise's Egg Counter Position."},
        .values = CounterPositionText,
        .OnChange = ChangeCounterPosition,
    },
    {
        .kind = OPTKIND_FUNC,
        .name = "Reset Event Settings",
        .desc = {"Restore this event's defaults and restart in",
                 "timed mode. Your best, the counter position and",
                 "other events are unchanged."},
        .OnSelect = ResetSettings,
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
    .name = "Grab-ercise",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};
EventMenu *Event_Menu = &Menu_Main;

// Session state. This .dat is read again for every match, statics and all.
static int grab_count, best;
static bool reset_clock;
static int canvas;
static Text *hud_score_text, *hud_best_text;
static GXColor text_gold = {255, 211, 0, 255};
static GXColor text_white = {255, 255, 255, 255};

// The CPU between spawns.
static struct target {
    bool placed;       // It has spawned this match.
    int move;          // MOVE_*
    int dir;           // Its heading, -1 or 1.
    int leg;           // Dash Dance: the way this dash, or the next, goes.
    int last_state;    // Last frame's state, and whether this dash came from a
    bool turn_dash;    // turn, which can turn back at once (dash.x4 is 0).
    bool last_flag;    // Last frame's dash had its run flag set.
    bool grabbed;      // Counted: waiting for the grab or throw to end.
    int air_frames;    // Airborne, not held.
    Vec3 spot;         // Where it last appeared.
    int ends_line;     // The floor FloorEnds last measured, and its ends.
    float left, right;
} target = { .ends_line = -1 };

// Settings ----------------------------------------------------------------

static bool FreePractice(void) { return stc_match->match.timer == MATCH_TIMER_COUNTUP; }

static const u8 saved_rows[] = {OPT_WALK, OPT_RUN, OPT_DASH, OPT_DASH_FULL, OPT_DASH_HALF, OPT_ROLL, OPT_COLLISION};
static const u8 saved_ids[] = {TM_GRABS_WALK, TM_GRABS_RUN, TM_GRABS_DASH, TM_GRABS_DASH_FULL, TM_GRABS_DASH_HALF,
    TM_GRABS_ROLL, TM_GRABS_COLLISION};
static void LoadSettings(void) {
    for (unsigned i = 0; i < countof(saved_rows); ++i) {
        EventOption *option = &Options_Main[saved_rows[i]];
        option->val = option->val_prev = TM_GetSetting(TM_SETTING_GRABERCISE, saved_ids[i]);
    }
    // One movement always stays on, whatever a save says.
    int on = 0;
    for (int i = 0; i < TM_GRABS_MOVES; ++i) on += Options_Main[OPT_WALK + i].val;
    if (!on)
        for (int i = 0; i < TM_GRABS_MOVES; ++i) Options_Main[OPT_WALK + i].val = Options_Main[OPT_WALK + i].val_prev = 1;
    Options_Main[OPT_COUNTER].val = Options_Main[OPT_COUNTER].val_prev = TM_GetSetting(TM_SETTING_EGGS_COUNTER, 0);
    best = TM_GetSetting(TM_SETTING_GRABERCISE, TM_GRABS_BEST);
}

/* Turning off the last movement turns it back on. */
static void ChangeMove(int move, int value) {
    EventOption *row = &Options_Main[OPT_WALK + move];
    int on = 0;
    for (int i = 0; i < TM_GRABS_MOVES; ++i) on += Options_Main[OPT_WALK + i].val;
    if (!on) {
        row->val = 1;
        SFX_PlayCommon(3);
    }
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_WALK + move, row->val);
}
static void ChangeWalk(GOBJ *menu, int value) { ChangeMove(0, value); }
static void ChangeRun(GOBJ *menu, int value) { ChangeMove(1, value); }
static void ChangeDash(GOBJ *menu, int value) { ChangeMove(2, value); }
static void ChangeDashFull(GOBJ *menu, int value) { ChangeMove(3, value); }
static void ChangeDashHalf(GOBJ *menu, int value) { ChangeMove(4, value); }
static void ChangeRoll(GOBJ *menu, int value) { ChangeMove(5, value); }

static void ShowCollision(int value) {
    for (int ply = 0; ply < 2; ++ply)
        for (int ms = 0; ms < 2; ++ms) {
            GOBJ *fighter = Fighter_GetSubcharGObj(ply, ms);
            if (fighter) ((FighterData *)fighter->userdata)->show_hit = value;
        }
}
static void ChangeCollision(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_COLLISION, value);
    ShowCollision(value);
}

/* Where the count goes for each Counter Position: Eggs-ercise's spots. The
 * label sits COUNTER_LABEL_Y under the count. */
#define COUNTER_LABEL_Y 15
static const struct { float x, y; u8 align; } Counter_Spots[TM_EGGS_COUNTER_COUNT] = {
    [TM_EGGS_COUNTER_TOP_LEFT] = {50, 25, 0},
    [TM_EGGS_COUNTER_TOP_RIGHT] = {590, 25, 2},
    [TM_EGGS_COUNTER_BOTTOM_LEFT] = {50, 408, 0},
    [TM_EGGS_COUNTER_BOTTOM_RIGHT] = {590, 408, 2},
    [TM_EGGS_COUNTER_UNDER_TIMER] = {320, 75, 1},
};
static void PlaceCounter(int position) {
    if (position < 0 || position >= TM_EGGS_COUNTER_COUNT) position = TM_EGGS_COUNTER_TOP_LEFT;
    hud_score_text->trans.X = hud_best_text->trans.X = Counter_Spots[position].x;
    hud_score_text->trans.Y = Counter_Spots[position].y;
    hud_best_text->trans.Y = Counter_Spots[position].y + COUNTER_LABEL_Y;
    hud_score_text->align = hud_best_text->align = Counter_Spots[position].align;
}
static void ChangeCounterPosition(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_EGGS_COUNTER, 0, value);
    PlaceCounter(value);
}

static void Retry(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->end_kind = MATCHENDKIND_RETRY;
    stc_match->state = 3;
    Match_EndVS();
}
static void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}
// Free Practice and the menu are saved, so a restart keeps them.
static void StageSwap_BeforeRestart(void) { }
static void FighterSwap_BeforeRestart(void) { }

static void StartFreePractice(GOBJ *menu) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_FREE_PRACTICE, 1);
    // A timed run restarts, so the clock drops its countdown.
    if (menu) {
        Retry(menu);
        return;
    }
    reset_clock = true;
    Options_Main[OPT_FREEPRACTICE].disable = 1;
    for (unsigned i = 0; i < countof(saved_rows); ++i) Options_Main[saved_rows[i]].disable = 0;
    stc_match->match.timer = MATCH_TIMER_COUNTUP;
    ShowCollision(Options_Main[OPT_COLLISION].val);
}

static void ResetSettings(GOBJ *menu) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_FREE_PRACTICE, 0);
    for (unsigned i = 0; i < countof(saved_rows); ++i)
        TM_SetSetting(TM_SETTING_GRABERCISE, saved_ids[i], saved_rows[i] != OPT_COLLISION);
    LoadSettings();
    Retry(menu);
}

// The stage -----------------------------------------------------------------

static float RandomRange(float low, float high) { return low + (high - low) * HSD_Randf(); }
static float Distance(Vec3 a, Vec3 b) {
    float x = a.X - b.X, y = a.Y - b.Y;
    return sqrtf(x * x + y * y);
}

/* A floor's ends: the line, and the floor lines joined to it on either side,
 * as a slope's pieces or a stage's halves are. Platforms are separate floors. */
static int FloorEnds(int line, float *left, float *right) {
    Vec2 a, b;
    int platform;
    if (!FloorLine(line, &a, &b, &platform)) return 0;
    for (int side = 0; side < 2; ++side) {
        Vec2 *end = side ? &b : &a;
        for (int steps = 0; steps < 16; ++steps) {
            int found = 0;
            for (int i = 0; i < LineCount() && !found; ++i) {
                Vec2 c, d;
                int p;
                if (i == line || !FloorLine(i, &c, &d, &p)) continue;
                Vec2 near = side ? c : d, far = side ? d : c;
                if (fabs(near.X - end->X) < .01f && fabs(near.Y - end->Y) < .01f &&
                    (side ? far.X > end->X : far.X < end->X)) {
                    *end = far;
                    found = 1;
                }
            }
            if (!found) break;
        }
    }
    *left = a.X;
    *right = b.X;
    return 1;
}
/* Every floor's span, and the lowest floor. */
static int StageBounds(float *left, float *right, float *bottom) {
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        if (!FloorLine(i, &a, &b, &platform)) continue;
        float low = a.Y < b.Y ? a.Y : b.Y;
        if (!found || a.X < *left) *left = a.X;
        if (!found || b.X > *right) *right = b.X;
        if (!found || low < *bottom) *bottom = low;
        found = 1;
    }
    return found;
}

/* A spot anywhere across the stage, on whichever floor at that x the draw
 * picks: a platform, the ground under it, or open ground. Far from you and
 * the last spot when there's room. */
static int PickSpot(Vec3 you, Vec3 *spot, int *line) {
    float left, right, bottom;
    if (StageBounds(&left, &right, &bottom)) {
        for (int tries = 0; tries < SPAWN_TRIES; ++tries) {
            float x = RandomRange(left, right);
            int lines[8], count = 0;
            float heights[8];
            for (int i = 0; i < LineCount() && count < 8; ++i) {
                float y, l, r;
                if (!LineY(i, x, &y) || !FloorEnds(i, &l, &r)) continue;
                if (x < l + SPAWN_INSET || x > r - SPAWN_INSET) continue;
                lines[count] = i;
                heights[count++] = y;
            }
            if (!count) continue;
            int pick = HSD_Randi(count);
            Vec3 at = {x, heights[pick], 0};
            // The last tries take any floor.
            if (tries < SPAWN_TRIES - 5 &&
                (Distance(at, you) < SPAWN_FROM_YOU || (target.placed && Distance(at, target.spot) < SPAWN_FROM_LAST)))
                continue;
            *spot = at;
            *line = lines[pick];
            return 1;
        }
    }
    GroundPoint(0, spot, line);
    return 0;
}

static int ChooseMove(void) {
    if (HSD_Randi(2) == 0) return MOVE_STAND;
    int on[TM_GRABS_MOVES], count = 0;
    for (int i = 0; i < TM_GRABS_MOVES; ++i)
        if (!FreePractice() || Options_Main[OPT_WALK + i].val) on[count++] = MOVE_WALK + i;
    return count ? on[HSD_Randi(count)] : MOVE_WALK;
}

// The CPU -------------------------------------------------------------------

static GOBJ *Follower(int ply) {
    GOBJ *leader = Fighter_GetGObj(ply);
    // Zelda and Sheik's second GOBJ is a sleeping form; only Nana follows along.
    if (!leader || !((FighterData *)leader->userdata)->flags.has_follower) return 0;
    return Fighter_GetSubcharGObj(ply, 1);
}
// A CPU Nana sits out, as in Passing Practice: only Popo is there to grab.
static void BenchFollower(void) {
    GOBJ *nana = Follower(1);
    if (!nana) return;
    FighterData *ft = nana->userdata;
    if (ft->state_id != ASID_SLEEP) Fighter_EnterSleep(nana, 1);
    ft->phys.pos.X = 999.f;
    ft->phys.pos.Y = -999.f;
}

static void SetPercent(FighterData *ft, float percent) {
    ft->dmg.percent = percent;
    ft->dmg.percent_temp = 0; // Pending damage, not a copy of the percent.
    Fighter_SetHUDDamage(ft->ply, percent);
}

/* The CPU disappears from wherever it was and comes back on a new spot, facing
 * its heading, at 0%. Sleep and Rebirth wipe a throw's or hit's leftovers
 * (hitstun, knockback), as Ledgechase and Ledgedash reset their fighters. */
static void Respawn(void) {
    GOBJ *cpu = Fighter_GetGObj(1), *hmn = Fighter_GetGObj(0);
    FighterData *ft = cpu->userdata;
    Vec3 spot;
    int line;
    PickSpot(((FighterData *)hmn->userdata)->phys.pos, &spot, &line);
    Fighter_EnterSleep(cpu, ft->flags.ms);
    Fighter_EnterRebirth(cpu);
    ft->flags.hitstun = 0;
    ft->dmg.hitlag_frames = 0;
    SetPercent(ft, 0);
    GroundAt(cpu, spot, line);
    target.move = ChooseMove();
    target.dir = target.leg = HSD_Randi(2) ? 1 : -1;
    ft->facing_direction = ft->facing_direction_prev = target.dir;
    Fighter_EnterWait(cpu);
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(cpu, 0);
    ft->cpu.ai = 15;
    Fighter_UpdateCameraBox(cpu);
    target.placed = true;
    target.last_state = ASID_WAIT;
    target.last_flag = false;
    target.grabbed = false;
    target.air_frames = 0;
    target.spot = spot;
}

/* A standard grab: every one starts with the victim pulled in. */
static bool JustGrabbed(int state) { return state == ASID_CAPTUREPULLEDHI || state == ASID_CAPTUREPULLEDLW; }
/* Held, thrown or carried, by any grab or command grab. */
static bool Held(int state) {
    return (state >= ASID_CAPTUREPULLEDHI && state <= ASID_CAPTUREFOOT) ||
        (state >= ASID_THROWNF && state <= ASID_THROWNLWWOMEN) ||
        (state >= ASID_SHOULDEREDWAIT && state <= ASID_SHOULDEREDTURN) ||
        (state >= ASID_THROWNFF && state <= ASID_THROWNFLW) ||
        (state >= ASID_CAPTURECAPTAIN && state <= ASID_THROWNKIRBY) ||
        (state >= ASID_CAPTUREMEWTWO && state <= ASID_THROWNMEWTWOAIR);
}
/* The states the CPU's own movement puts it in. Anything else (a hit, a
 * shield break) sends it back to a new spot. */
static bool Moving(int state) {
    return (state >= ASID_WAIT && state <= ASID_RUNBRAKE) || state == ASID_FALL ||
        state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
        state == ASID_OTTOTTO || state == ASID_OTTOTTOWAIT ||
        (state >= ASID_GUARDON && state <= ASID_GUARDREFLECT) ||
        state == ASID_ESCAPEF || state == ASID_ESCAPEB;
}
static bool Frozen(FighterData *ft) {
    return ft->flags.freeze || ft->flags.hitlag || ft->dmg.hitlag_frames > 0;
}

/* How close to the end of its floor the CPU can get before it turns back:
 * about where its turn would stop it, and a little more. */
static float TurnRoom(FighterData *ft) {
    float speed = fabs(ft->phys.self_vel_ground.X);
    float friction = ft->attr.ground_friction > .01f ? ft->attr.ground_friction : .01f;
    // A dash commits to its first frames at dash speed, even from standing.
    if (target.move >= MOVE_RUN && target.move <= MOVE_DASH_HALF && speed < ft->attr.dash_initial_velocity)
        speed = ft->attr.dash_initial_velocity;
    int state = ft->state_id;
    switch (target.move) {
        case MOVE_WALK: // Turn brakes with ground friction.
            return speed * speed / (2 * friction) + speed * 3 + EDGE_ROOM;
        case MOVE_RUN:
            /* TurnRun brakes at the dash acceleration. Until the run starts,
             * the CPU is in its dash, which turns as a dash does, so a short
             * platform still gets a run. */
            if (state == ASID_RUN || state == ASID_RUNDIRECT || state == ASID_TURNRUN || state == ASID_RUNBRAKE) {
                float brake = ft->attr.dashrun_acceleration_a + ft->attr.dashrun_acceleration_b;
                if (brake < .01f) brake = .01f;
                return speed * speed / (2 * brake) + speed * 2 + EDGE_ROOM;
            }
            // Fall through.
        case MOVE_DASH:
        case MOVE_DASH_FULL:
        case MOVE_DASH_HALF: {
            /* A dash turns back almost at once (Turn_Enter_Smash, then a dash
             * the other way), after a neutral frame. A dash out of a turn can
             * from its first frame, but one from standing only from its fifth
             * (PlCo 0x44). */
            float frame = ft->state.frame, frames = 8;
            if (state == ASID_DASH)
                frames = target.turn_dash || frame > 4 ? 3 : 8 - frame;
            else if (state == ASID_TURN)
                frames = 4;
            return speed * frames + EDGE_ROOM;
        }
        default:
            return ROLL_ROOM;
    }
}
// The ends of the floor the CPU stands on, measured again when it changes.
static void UpdateEnds(FighterData *ft) {
    int line = ft->coll_data.ground_index;
    if (line == target.ends_line) return;
    target.ends_line = line;
    if (!FloorEnds(line, &target.left, &target.right)) target.ends_line = -1;
}
// The floor left in a direction; plenty if the floor isn't known.
static float RoomAhead(FighterData *ft, int dir) {
    if (target.ends_line < 0) return 999.f;
    float x = ft->phys.pos.X;
    return dir > 0 ? target.right - x : x - target.left;
}
static void Steer(FighterData *ft) {
    UpdateEnds(ft);
    float ahead = RoomAhead(ft, target.dir), behind = RoomAhead(ft, -target.dir);
    if (ahead < TurnRoom(ft) && behind > ahead) target.dir = target.leg = -target.dir;
}

// Last frame's stick: whether a full one now would be a fresh smash.
static bool StickInside(FighterData *ft) { return fabs(ft->input.lstick.X) < TILT; }
static int Smash(FighterData *ft, int dir) { return StickInside(ft) ? dir * STICK_FULL : 0; }
static bool Facing(FighterData *ft, int dir) { return ft->facing_direction * dir > 0; }

static int WalkStick(FighterData *ft, int dir) {
    int state = ft->state_id;
    bool walking = state >= ASID_WALKSLOW && state <= ASID_WALKFAST && Facing(ft, dir);
    bool settled = ft->input.lstick.X * dir >= TILT && (u8)ft->input.timer_lstick_tilt_x >= SAFE_TILT_FRAMES;
    return dir * (walking && settled ? STICK_FULL : STICK_WALK);
}
static int RunStick(FighterData *ft, int dir) {
    switch (ft->state_id) {
        case ASID_RUN:
        case ASID_RUNDIRECT:
        case ASID_TURNRUN: // Run turnarounds take any stick past -0.375.
            return dir * STICK_FULL;
        case ASID_DASH:
            return Facing(ft, dir) ? dir * STICK_FULL : Smash(ft, dir);
        case ASID_TURN: // A smash turn dashes if the stick is still out.
            return dir * STICK_FULL;
        default:
            return Smash(ft, dir);
    }
}
/* A dash dance that travels: a full dash the way the CPU is heading, cut off
 * where it would become a run (its script's flag) by a quick dash back, then
 * as soon as that one can turn, the next dash forward. Each turn is a fresh
 * smash (a neutral frame, then full), and the stick stays out through the
 * Turn so it dashes. The dash back lasts two frames before turning again. */
static int DashStick(FighterData *ft, int dir) {
    int state = ft->state_id;
    if (state == ASID_TURN) return target.leg * STICK_FULL;
    if (state != ASID_DASH) {
        target.leg = dir;
        return Smash(ft, dir);
    }
    if (!Facing(ft, target.leg)) return Smash(ft, target.leg);
    if (target.leg == dir) {
        if (!ft->ftcmd_var.flag0) return dir * STICK_FULL;
        target.leg = -dir; // The end of the dash: the quick dash back.
    } else {
        target.leg = dir;  // The dash back is under way: forward again.
    }
    return Smash(ft, target.leg);
}
/* The dash frame a dash would become a run on (cmd_vars[0] set by its
 * script), learned from the first dash seen this match. The .dat reloads with
 * every match, and the CPU's character only changes with a new match. */
static int run_frame;
/* A dash dance in place: dash one way, then back, over and over, each dash
 * cut off where it would become a run (Full) or at half that many frames
 * (Half; until a dash has shown where that is, a full one). A dash that would
 * run out of floor turns early, so a dance near an end drifts away from it.
 * Turns are as in the traveling dance: a neutral frame, a fresh smash, and
 * the stick held out through the Turn. */
static int DanceStick(FighterData *ft) {
    int state = ft->state_id, leg = target.leg;
    if (state == ASID_TURN) return leg * STICK_FULL;
    if (state != ASID_DASH) {
        // A dash from standing can't turn for its first frames: start it away
        // from an end that's too close.
        if (RoomAhead(ft, leg) < TurnRoom(ft) && RoomAhead(ft, -leg) > RoomAhead(ft, leg)) target.leg = leg = -leg;
        return Smash(ft, leg);
    }
    if (!Facing(ft, leg)) return Smash(ft, leg);
    bool done = ft->ftcmd_var.flag0 || RoomAhead(ft, leg) < TurnRoom(ft);
    if (target.move == MOVE_DASH_HALF && run_frame && ft->state.frame >= run_frame / 2) done = true;
    if (!done) return leg * STICK_FULL;
    target.leg = -leg;
    return Smash(ft, -leg);
}
static int RollStick(FighterData *ft, int dir) {
    int state = ft->state_id;
    return state == ASID_GUARDON || state == ASID_GUARD ? Smash(ft, dir) : 0;
}

static void Move(GOBJ *cpu, FighterData *ft) {
    int state = ft->state_id;
    if (state == ASID_DASH && target.last_state != ASID_DASH) target.turn_dash = target.last_state == ASID_TURN;
    bool flag = state == ASID_DASH && ft->ftcmd_var.flag0;
    if (flag && !target.last_flag && target.last_state == ASID_DASH && !run_frame) run_frame = ft->state.frame;
    target.last_flag = flag;
    target.last_state = state;
    if (target.move == MOVE_STAND || ft->phys.air_state != 0) return;
    if (target.move == MOVE_DASH_FULL || target.move == MOVE_DASH_HALF) {
        UpdateEnds(ft);
        ft->cpu.lstickX = DanceStick(ft);
        return;
    }
    Steer(ft);
    int dir = target.dir;
    switch (target.move) {
        case MOVE_WALK: ft->cpu.lstickX = WalkStick(ft, dir); break;
        case MOVE_RUN: ft->cpu.lstickX = RunStick(ft, dir); break;
        case MOVE_DASH: ft->cpu.lstickX = DashStick(ft, dir); break;
        case MOVE_ROLL:
            // Shield held throughout, so each roll ends back in shield. The
            // shield stays full, as Infinite Shields keeps it.
            ft->cpu.held |= PAD_TRIGGER_R;
            ft->shield.health = (*stc_ftcommon)->x260;
            ft->cpu.lstickX = RollStick(ft, dir);
            break;
    }
}

/* Once a frame, before the fighters' input: count a new grab, bring the CPU
 * back when it's done, and otherwise set its inputs. */
static void UpdateTarget(void) {
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *ft = cpu->userdata;
    BenchFollower();
    ft->cpu.ai = 15;
    Fighter_ZeroCPUInputs(ft);
    if (!target.placed) {
        Respawn();
        return;
    }
    int state = ft->state_id;
    if (Held(state)) {
        if (JustGrabbed(state) && !target.grabbed) {
            target.grabbed = true;
            if (grab_count < 999) grab_count++;
        }
        return;
    }
    if (ft->flags.dead) return;
    bool away = target.grabbed || !Moving(state);
    if (ft->phys.air_state != 0) {
        float left, right, bottom;
        target.air_frames++;
        if (StageBounds(&left, &right, &bottom) &&
            (ft->phys.pos.X < left - OFF_STAGE || ft->phys.pos.X > right + OFF_STAGE || ft->phys.pos.Y < bottom - OFF_STAGE))
            away = true;
        if (target.air_frames > AIR_FRAMES) away = true;
    } else {
        target.air_frames = 0;
    }
    // A throw, hit or fall: after the hitlag, it's gone and back somewhere else.
    if (away) {
        if (!Frozen(ft)) Respawn();
        return;
    }
    Move(cpu, ft);
}

// Event ---------------------------------------------------------------------

static void FinishTimedRun(void) {
    Match_SetEndGraphic(MATCHENDKIND_VSGAME);
    if (grab_count > best) {
        MatchInfo_0x0010_store(0x9c40); // The "new record" voice, as Eggs-ercise does.
        Match_SetPostMatchSFX(325);
        TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_BEST,
            grab_count < TM_GRABS_BEST_MAX ? grab_count : TM_GRABS_BEST_MAX);
    } else {
        Match_SetPostMatchSFX(324);
    }
    // The setting is saved to the card with the rest, back at the event select.
    Match_EndImmediate();
}

static void UpdateHUD(void) {
    if (Pause_CheckStatus(1) == 2) {
        Text_SetText(hud_score_text, 0, "");
        Text_SetText(hud_best_text, 0, "");
        return;
    }
    Text_SetText(hud_score_text, 0, "%d", grab_count);
    if (FreePractice()) {
        Text_SetText(hud_best_text, 0, "Free Practice");
        Text_SetColor(hud_best_text, 0, &text_gold);
    } else if (best > 0) {
        Text_SetText(hud_best_text, 0, "Best: %d", best);
        Text_SetColor(hud_best_text, 0, &text_white);
    } else {
        Text_SetText(hud_best_text, 0, "");
    }
}

void Event_Init(GOBJ *gobj) {
    LoadSettings();
    // A retry may reuse the module: rebuild the gating from the saved mode.
    Options_Main[OPT_FREEPRACTICE].disable = 0;
    for (unsigned i = 0; i < countof(saved_rows); ++i) Options_Main[saved_rows[i]].disable = 1;
    if (TM_GetSetting(TM_SETTING_GRABERCISE, TM_GRABS_FREE_PRACTICE)) StartFreePractice(0);
    else ShowCollision(0);
    StageSwap_Init();
    FighterSwap_Init();

    canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 81, 19);
    hud_score_text = Text_CreateText(2, canvas);
    hud_score_text->kerning = 1;
    hud_score_text->use_aspect = 1;
    hud_score_text->viewport_scale.X = 1;
    hud_score_text->viewport_scale.Y = 1;
    hud_best_text = Text_CreateText(2, canvas);
    hud_best_text->kerning = 1;
    hud_best_text->use_aspect = 1;
    hud_best_text->scale.X = 0.35;
    hud_best_text->scale.Y = 0.35;
    hud_best_text->viewport_scale.X = 1;
    hud_best_text->viewport_scale.Y = 1;
    PlaceCounter(Options_Main[OPT_COUNTER].val);
    Text_AddSubtext(hud_score_text, 0, 0, "-");
    Text_AddSubtext(hud_best_text, 0, 0, "-");

    stc_match->end_kind = MATCHENDKIND_NONE;
}

void Event_Think(GOBJ *event) {
    // Fresh moves: a throw's knockback doesn't shrink as it's repeated.
    for (int i = 0; i < 6; i++)
        if (Fighter_GetGObj(i)) memset(Fighter_GetStaleMoveTable(i), 0, 0x2C);

    // The match starts with the timed minute loaded; Free Practice counts up from 0:00.
    if (reset_clock) {
        stc_match->time_seconds = 0;
        stc_match->time_ms = 0;
        reset_clock = false;
    }

    if (Fighter_GetGObj(0) && Fighter_GetGObj(1) && Pause_CheckStatus(1) != 2)
        UpdateTarget();
    UpdateHUD();

    if (stc_match->time_frames == TIMED_FRAMES && !FreePractice())
        FinishTimedRun();
}
