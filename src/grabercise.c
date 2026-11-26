#include "events.h"
#include "stage_points.h"
#include "stage_swap.h"
#include "fighter_swap.h"
#include "techchase_logic.h"

/* Grabs-ercise (Minigames): Eggs-ercise with a CPU to grab instead of eggs to
 * break. The CPU you picked appears somewhere on the stage you picked: on a
 * platform, on the ground under one, or anywhere along the ground. Half the
 * time it stands still (Free Practice's CPU Movement can make that every time,
 * or never); otherwise it walks, runs or rolls at full speed, or
 * dash dances, back and forth across the floor it's on, or dash dances in
 * place, with full or half dashes. Grab it, throw it, and it disappears and
 * comes back somewhere else. A hit leaves it where it is for half a second
 * after the hitlag, each new hit starting that over, so an attack can lead into
 * a grab; then, or at a knockdown, it comes back somewhere else. A shield it
 * holds never runs out, and a hit on it doesn't move it. A timed minute counts
 * grabs, with the best saved; Free Practice has no clock and lets you pick the
 * movements, whether they're used at all, and what counts: a grab, or (Hit)
 * a hit after the grab's throw,
 * while the CPU stays up to be juggled, with every hit counted apart, from a
 * percent you pick or a random one. A juggle that knocks it down is a tech
 * chase: it techs in place, away or toward you, or misses and gets up, with
 * Techchase's Tyro setting's chances, and comes back somewhere else if it's
 * back on its feet unhit. Hit mode's CPU DI has it DI the throw and every hit
 * a random way, or not at all. Knocked off the top, it comes back at once.
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
 *   at the dash acceleration until the velocity reverses, then runs. Grabs-ercise
 *   turns a run around by crouching instead (RunStick).
 * - From GuardOn or Guard, a stick 0x31C (0.7) out with the timer under 0x320
 *   (4) rolls. Rolls and run turnarounds stop at the edge of a floor; walks,
 *   dashes, runs and crouches go off it.
 * - Wait, Turn, Squat and SquatWait brake with ground friction, doubled above
 *   walk speed (ft_80084F3C, 0x6C).
 * - A landing in knockback (DamageFly, DamageFall) techs if L/R was pressed
 *   under 0x250 (20) frames ago (input.timer_LR) and at least 0x1C (40) after
 *   the press before it (since_rapid_lr), rolling with the stick 0x254 (0.2)
 *   out (ftCo_80090184, ftCo_800986B0, ftCo_80098928). Otherwise it's a
 *   knockdown (DownBound), which ends in a getup attack if A or B was pressed
 *   during it (0x24C, 60 frames), a getup roll with the stick 0x248 (0.2) out
 *   to the side, or DownWait, which takes a fresh A or B, the stick to the side,
 *   or the stick 0x244 (0.2) up (ftCo_DownBound_Anim, ftCo_DownWait_IASA).
 * - Past the top blast zone with upward knockback, a fighter enters a star or
 *   screen KO (DeadUpStar, DeadUpFall: ftCo_800D3158). */

// The movement rows, in menu order, as the saved preferences number them.
enum move { MOVE_STAND, MOVE_WALK, MOVE_RUN, MOVE_DASH, MOVE_DASH_FULL, MOVE_DASH_HALF, MOVE_ROLL };

// PlCo values the inputs below rely on (see the comment above).
#define TILT .25f          // 0x8: the tilt timers' crossing.
// CPU stick values (value / 127 in the game).
#define STICK_FULL 127
#define STICK_WALK 64      // .50: a walk, not a dash.
#define STICK_DASH_HOLD 78 // .61: keeps a dash going without it becoming a run (.625).
#define SAFE_TILT_FRAMES 2 // A walk goes to full tilt once the timer reads this.

/* A moving CPU turns back on the last frame it can and still stop at least
 * this far from the end of its floor (its feet: its body reaches past them),
 * and only toward the side with more room. */
#define TOUCH 1.5f
#define ROLL_ROOM 4.f      // Rolls stop at the end, so a CPU rolls until it's there.
// Run mode's crouch turnaround (RunStick).
#define SQUAT_FRAMES 8     // Every character's Squat, the crouch's start, which can't dash.
#define PASS_FRAMES 6      // PlCo 0x468: a down tilt this old can't drop through a platform.
#define STICK_HALF_DOWN -64 // -.50: past 0.25, so the down tilt ages, short of a crouch (0.6875).
#define CROUCH_EXTRA 3.f   // Frames of dash the crouch leaves beyond the least, to reach the end.
#define LONGEST_TURN 16    // Mewtwo's Turn; most are 12. Assumed until a Turn shows the CPU's.

// Spawns: this far in from a floor's ends, from you, and from the last spot.
#define SPAWN_INSET 8.f
#define SPAWN_FROM_YOU 30.f
#define SPAWN_FROM_LAST 25.f
#define SPAWN_TRIES 40
/* A hit CPU stays this long after the hitlag of the last hit, pressing nothing,
 * so an attack can lead into a grab, then comes back somewhere else. */
#define GRAB_WINDOW 30
// An airborne CPU this long, or this far off the stage, comes back.
#define AIR_FRAMES 120
#define OFF_STAGE 10.f
/* Hit mode's tech chase (ChaseInputs), with Techchase's Tyro setting's chances:
 * 60% in place, 15% away, 15% toward and 10% missed, and its default getups. */
#define TECH_PRESET TC_PRESET_TYRO
/* input.timer_LR: L/R pressed this long ago is inside the tech's 20 frames
 * (PlCo 0x250), and past the 2 (0x18C) in which a hit in the air would take 5%
 * less knockback (0x190). */
#define TECH_PRESS_AGO 10
// .31: past a tech roll's and a getup roll's 0.2 (0x254, 0x248) and the stick's
// 0.28 dead zone, as Techchase and the Training Lab press.
#define TECH_STICK 40
/* A tech roll's stick goes out this many frames of falling before the floor,
 * so it doesn't drift the CPU on the way down. In the air the CPU's ECB is
 * above its feet, which can pass the floor's height before it lands. */
#define LAND_FRAMES 4
#define LAND_BELOW 10.f

#define TIMED_FRAMES 3600  // The timed run's minute.

enum options_main {
    OPT_RETRY, OPT_STAGE, OPT_CHARACTERS, OPT_FREEPRACTICE, OPT_SUCCESS, OPT_PERCENT, OPT_DI, OPT_MOVEMENT, OPT_STANDING,
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
static void ChangeFreePractice(GOBJ *menu, int value);
static void ChangeWalk(GOBJ *menu, int value);
static void ChangeRun(GOBJ *menu, int value);
static void ChangeDash(GOBJ *menu, int value);
static void ChangeDashFull(GOBJ *menu, int value);
static void ChangeDashHalf(GOBJ *menu, int value);
static void ChangeRoll(GOBJ *menu, int value);
static void ChangeCollision(GOBJ *menu, int value);
static void ChangeMovement(GOBJ *menu, int value);
static void ChangeStanding(GOBJ *menu, int value);
static void ChangeSuccess(GOBJ *menu, int value);
static void ChangePercent(GOBJ *menu, int value);
static void ChangeDI(GOBJ *menu, int value);
static void ChangeCounterPosition(GOBJ *menu, int value);
static void ResetSettings(GOBJ *menu);

static const char *SuccessText[TM_GRABS_SUCCESS_COUNT] = { "Throw", "Hit" };
static const char *PercentText[TM_GRABS_PERCENT_COUNT] = {
    "Random", "0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%", "110%", "120%",
};
static const char *DIText[TM_GRABS_DI_COUNT] = { "None", "Random" };
static const char *MovementText[TM_GRABS_MOVEMENT_COUNT] = { "Random", "Still Only", "Moving Only" };
static const char *StandingText[TM_GRABS_STANDING_COUNT] = { "Stand", "Shield", "Random" };
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
        .kind = OPTKIND_TOGGLE,
        .name = "Free Practice",
        .desc = {"On: no time limit, and you choose what counts and",
                 "how the CPU moves. Off: the timed minute, which",
                 "counts grabs, mixes every movement and saves your",
                 "best. Saved. Changing it restarts the match."},
        .OnChange = ChangeFreePractice,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Success Criteria",
        .value_num = countof(SuccessText),
        .values = SuccessText,
        .desc = {"Throw: a grab counts. Hit: grab, throw and hit the",
                 "CPU. It stays up to juggle and techs when knocked",
                 "down. Every hit adds to Hits; a change resets both.",
                 "Free Practice only; timed runs are Throw."},
        .disable = 1,
        .OnChange = ChangeSuccess,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "CPU Percent",
        .value_num = countof(PercentText),
        .values = PercentText,
        .desc = {"The CPU's percent each time it appears, for Hit",
                 "mode's juggles. Random picks 0% to 120%, in",
                 "steps of 10, each time it appears.",
                 "Hit only: Throw and timed runs are always 0%."},
        .disable = 1,
        .OnChange = ChangePercent,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "CPU DI",
        .value_num = countof(DIText),
        .values = DIText,
        .desc = {"None: the CPU never DIs. Random: through each",
                 "throw and hit, it holds the stick all the way",
                 "one of its 8 ways, picked anew each time.",
                 "Hit only: Throw and timed runs never DI."},
        .disable = 1,
        .OnChange = ChangeDI,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "CPU Movement",
        .value_num = countof(MovementText),
        .values = MovementText,
        .desc = {"Random: half the CPUs stand still and half move.",
                 "Still Only: none move. Moving Only: all of them do.",
                 "The next CPU to appear follows it.",
                 "Free Practice only; timed runs are Random."},
        .disable = 1,
        .OnChange = ChangeMovement,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Standing CPU",
        .value_num = countof(StandingText),
        .values = StandingText,
        .val = TM_GRABS_STAND_RANDOM,
        .desc = {"What the CPUs that don't move do: stand, hold",
                 "shield, or either. The shield never runs out, and",
                 "hitting it doesn't send the CPU away. Free Practice",
                 "only, not with Moving Only; timed runs are Random."},
        .disable = 1,
        .OnChange = ChangeStanding,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Full Speed Walk",
        .val = 1,
        .desc = {"Moving CPUs pick one of the movements that are",
                 "on and go back and forth on their floor. This one",
                 "walks at full speed. One movement always stays on.",
                 "Free Practice only, not with Still Only."},
        .disable = 1,
        .OnChange = ChangeWalk,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Full Speed Run",
        .val = 1,
        .desc = {"Moving CPUs can dash into a full speed run. At",
                 "each end they crouch, dash forward out of the",
                 "crouch right to the edge, then dash back and run.",
                 "One stays on. Free Practice only, not Still Only."},
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
                 "One stays on. Free Practice only, not Still Only."},
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
                 "One stays on. Free Practice only, not Still Only."},
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
                 "One stays on. Free Practice only, not Still Only."},
        .disable = 1,
        .OnChange = ChangeDashHalf,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Rolling",
        .val = 1,
        .desc = {"Moving CPUs can shield and roll, roll after",
                 "roll, and roll back at each end.",
                 "One stays on. Free Practice only, not Still Only."},
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
    .name = "Grabs-ercise",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};
EventMenu *Event_Menu = &Menu_Main;

// Session state. This .dat is read again for every match, statics and all.
static int grab_count, best;
static int total_hits; // Hit mode: every hit after a throw, each juggle's included.
static bool reset_clock;
static int canvas;
static Text *hud_score_text, *hud_best_text, *hud_hits_text;
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
    int run_phase;     // Run mode: running, crouching, or dashing out of the crouch.
    bool shield;       // Standing: holding shield.
    bool grabbed;      // A standard grab caught it.
    bool held;         // Held by any grab: once it's let go, it's gone, but in Hit mode
    bool juggle;       // it stays up after a standard grab to be hit,
    bool scored;       // and the first hit counts.
    float last_lag;    // Last frame's hitlag: more now is a new hit.
    bool hit;          // Hit: left where it is, for a follow-up grab, until
    int hit_frames;    // this many frames after the last hit's hitlag pass GRAB_WINDOW.
    int di;            // Hit mode's CPU DI: the way (DI_Sticks) this throw or hit holds,
    float di_lag;      // -1 until picked, and last frame's hitlag: more now is a new hit.
    bool chase;        // Hit mode: knocked down or teching, until it's up again.
    int landing;       // Hit mode: how its next knockback landing goes (TC_TECH_*),
    int getup;         // and a missed tech's getup (TC_GETUP_*): -1 until picked.
    int air_frames;    // Airborne, not held.
    Vec3 spot;         // Where it last appeared.
    int ends_line;     // The floor FloorEnds last measured, and its ends.
    float left, right;
} target = { .ends_line = -1 };

// Settings ----------------------------------------------------------------

static bool FreePractice(void) { return stc_match->match.timer == MATCH_TIMER_COUNTUP; }

static const u8 saved_rows[] = {OPT_SUCCESS, OPT_PERCENT, OPT_DI, OPT_MOVEMENT, OPT_STANDING, OPT_WALK, OPT_RUN,
    OPT_DASH, OPT_DASH_FULL, OPT_DASH_HALF, OPT_ROLL, OPT_COLLISION};
static const u8 saved_ids[] = {TM_GRABS_SUCCESS, TM_GRABS_PERCENT, TM_GRABS_DI, TM_GRABS_MOVEMENT, TM_GRABS_STANDING,
    TM_GRABS_WALK, TM_GRABS_RUN, TM_GRABS_DASH, TM_GRABS_DASH_FULL, TM_GRABS_DASH_HALF, TM_GRABS_ROLL,
    TM_GRABS_COLLISION};
typedef char grabercise_saved_rows[countof(saved_rows) == countof(saved_ids) ? 1 : -1];
/* Each saved row's default: Throw, Random percents, no DI, Random movement and
 * standing, every movement on, Fighter Collision off. */
static int SavedDefault(unsigned row) {
    if (row == OPT_SUCCESS) return TM_GRABS_SUCCESS_THROW;
    if (row == OPT_PERCENT) return TM_GRABS_PERCENT_RANDOM;
    if (row == OPT_DI) return TM_GRABS_DI_NONE;
    if (row == OPT_MOVEMENT) return TM_GRABS_MOVEMENT_RANDOM;
    return row == OPT_STANDING ? TM_GRABS_STAND_RANDOM : row != OPT_COLLISION;
}
// Hit mode: a grab counts once a hit follows its throw. Free Practice only.
static bool HitMode(void) { return FreePractice() && Options_Main[OPT_SUCCESS].val == TM_GRABS_SUCCESS_HIT; }
// CPU Percent and CPU DI only do anything in Hit mode, so they're greyed out otherwise.
static void GateHitRows(void) { Options_Main[OPT_PERCENT].disable = Options_Main[OPT_DI].disable = !HitMode(); }
/* CPU Movement, Free Practice's: Random in timed runs. */
static int Movement(void) {
    int movement = FreePractice() ? Options_Main[OPT_MOVEMENT].val : TM_GRABS_MOVEMENT_RANDOM;
    return movement >= 0 && movement < TM_GRABS_MOVEMENT_COUNT ? movement : TM_GRABS_MOVEMENT_RANDOM;
}
/* Free Practice: Standing CPU does nothing when every CPU moves, and the
 * movement rows nothing when none do, so they're greyed out then. */
static void GateMovement(void) {
    int movement = Movement();
    Options_Main[OPT_STANDING].disable = movement == TM_GRABS_MOVEMENT_MOVING;
    for (int i = 0; i < TM_GRABS_MOVES; ++i) Options_Main[OPT_WALK + i].disable = movement == TM_GRABS_MOVEMENT_STILL;
}
// The next CPU to appear moves, or doesn't, as it says (ChooseMove).
static void ChangeMovement(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_MOVEMENT, value);
    GateMovement();
}
static void ApplyPercent(void);
// Changing what counts starts the counts over.
static void ChangeSuccess(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_SUCCESS, value);
    grab_count = total_hits = 0;
    GateHitRows();
    ApplyPercent();
}
static void ChangePercent(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_PERCENT, value);
    ApplyPercent();
}
// The next throw or hit DIs as it says (DIInputs).
static void ChangeDI(GOBJ *menu, int value) { TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_DI, value); }
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
    Options_Main[OPT_FREEPRACTICE].val = Options_Main[OPT_FREEPRACTICE].val_prev =
        TM_GetSetting(TM_SETTING_GRABERCISE, TM_GRABS_FREE_PRACTICE);
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
 * label sits COUNTER_LABEL_Y under the count, and Hit mode's hits HITS_LINE_Y
 * under the label. */
#define COUNTER_LABEL_Y 15
#define HITS_LINE_Y 12
static const struct { float x, y; u8 align; } Counter_Spots[TM_EGGS_COUNTER_COUNT] = {
    [TM_EGGS_COUNTER_TOP_LEFT] = {50, 25, 0},
    [TM_EGGS_COUNTER_TOP_RIGHT] = {590, 25, 2},
    [TM_EGGS_COUNTER_BOTTOM_LEFT] = {50, 408, 0},
    [TM_EGGS_COUNTER_BOTTOM_RIGHT] = {590, 408, 2},
    [TM_EGGS_COUNTER_UNDER_TIMER] = {320, 75, 1},
};
static void PlaceCounter(int position) {
    if (position < 0 || position >= TM_EGGS_COUNTER_COUNT) position = TM_EGGS_COUNTER_TOP_LEFT;
    hud_score_text->trans.X = hud_best_text->trans.X = hud_hits_text->trans.X = Counter_Spots[position].x;
    hud_score_text->trans.Y = Counter_Spots[position].y;
    hud_best_text->trans.Y = Counter_Spots[position].y + COUNTER_LABEL_Y;
    hud_hits_text->trans.Y = hud_best_text->trans.Y + HITS_LINE_Y;
    hud_score_text->align = hud_best_text->align = hud_hits_text->align = Counter_Spots[position].align;
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

/* The saved choice is the mode: Event_Init enters it as every match starts,
 * so a change restarts the match. That also gives a timed run its countdown
 * back, or takes it away. */
static void ChangeFreePractice(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_FREE_PRACTICE, value);
    Retry(menu);
}
static void EnterFreePractice(void) {
    reset_clock = true;
    stc_match->match.timer = MATCH_TIMER_COUNTUP;
    for (unsigned i = 0; i < countof(saved_rows); ++i) Options_Main[saved_rows[i]].disable = 0;
    GateHitRows();
    GateMovement();
    ShowCollision(Options_Main[OPT_COLLISION].val);
}

static void ResetSettings(GOBJ *menu) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_FREE_PRACTICE, 0);
    for (unsigned i = 0; i < countof(saved_rows); ++i)
        TM_SetSetting(TM_SETTING_GRABERCISE, saved_ids[i], SavedDefault(saved_rows[i]));
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

static void ChangeStanding(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_GRABERCISE, TM_GRABS_STANDING, value);
}
// A standing CPU holds shield, as Standing CPU says; timed runs mix both.
static bool ChooseShield(void) {
    int standing = FreePractice() ? Options_Main[OPT_STANDING].val : TM_GRABS_STAND_RANDOM;
    return standing == TM_GRABS_SHIELD || (standing == TM_GRABS_STAND_RANDOM && HSD_Randi(2));
}
/* Stand, or one of the movements that are on: half the time each (Random),
 * or always one or the other, as CPU Movement says. */
static int ChooseMove(void) {
    int movement = Movement();
    if (movement == TM_GRABS_MOVEMENT_STILL) return MOVE_STAND;
    if (movement == TM_GRABS_MOVEMENT_RANDOM && HSD_Randi(2) == 0) return MOVE_STAND;
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

/* The CPU's percent as it appears: CPU Percent's in Hit mode, a new pick each
 * time for Random, and 0% otherwise. */
static int SpawnPercent(void) {
    if (!HitMode()) return 0;
    int choice = Options_Main[OPT_PERCENT].val;
    if (choice <= TM_GRABS_PERCENT_RANDOM || choice >= TM_GRABS_PERCENT_COUNT)
        choice = 1 + HSD_Randi(TM_GRABS_PERCENT_COUNT - 1);
    return (choice - 1) * TM_GRABS_PERCENT_STEP;
}
/* A change to Success Criteria or CPU Percent reaches a CPU that's waiting to
 * be grabbed at once. One that's been hit, grabbed or juggled keeps its damage
 * until it comes back. */
static void ApplyPercent(void) {
    GOBJ *cpu = Fighter_GetGObj(1);
    if (!cpu || !target.placed || target.hit || target.held) return;
    SetPercent(cpu->userdata, SpawnPercent());
}

/* The CPU disappears from wherever it was and comes back on a new spot, facing
 * its heading, at its percent (SpawnPercent). Sleep and Rebirth wipe a throw's
 * or hit's leftovers (hitstun, knockback), as Ledgechase and Ledgedash reset
 * their fighters. */
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
    SetPercent(ft, SpawnPercent());
    GroundAt(cpu, spot, line);
    target.move = ChooseMove();
    target.shield = target.move == MOVE_STAND && ChooseShield();
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
    target.run_phase = 0;
    target.grabbed = target.held = target.juggle = target.scored = target.hit = target.chase = false;
    target.landing = target.getup = -1;
    target.hit_frames = 0;
    target.air_frames = 0;
    target.di = -1;
    target.di_lag = 0;
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
 * shield break) starts the grab window (UpdateTarget). */
static bool Moving(int state) {
    return (state >= ASID_WAIT && state <= ASID_RUNBRAKE) || state == ASID_FALL ||
        state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
        (state >= ASID_SQUAT && state <= ASID_SQUATRV) ||
        state == ASID_OTTOTTO || state == ASID_OTTOTTOWAIT ||
        (state >= ASID_GUARDON && state <= ASID_GUARDREFLECT) ||
        state == ASID_ESCAPEF || state == ASID_ESCAPEB;
}
static bool Frozen(FighterData *ft) {
    return ft->flags.freeze || ft->flags.hitlag || ft->dmg.hitlag_frames > 0;
}
/* Knocked down, lying on the floor, where no grab can catch it. */
static bool Down(int state) { return state >= ASID_DOWNBOUNDU && state <= ASID_DOWNSPOTD; }
/* What a tech chase chases: a knockdown and its getup, or a tech. */
static bool Chased(int state) { return state >= ASID_DOWNBOUNDU && state <= ASID_PASSIVESTANDB; }
/* Knockback, which a landing techs out of or is knocked down by. */
static bool KnockedBack(int state) {
    return (state >= ASID_DAMAGEHI1 && state <= ASID_DAMAGEFLYROLL) || state == ASID_DAMAGEFALL;
}
/* A star or screen KO off the top. The sides and bottom never come to a KO:
 * OFF_STAGE brings the CPU back first. */
static bool OffTheTop(int state) { return state >= ASID_DEADUP && state <= ASID_DEADUPFALLHITCAMERAICE; }

/* How far ground friction carries a fighter from speed v, frame by frame, for
 * up to `frames` frames: ft_80084F3C's friction, doubled above walk speed.
 * After `frames`, if `after` is more than 0, it brakes at that instead until
 * it stops. */
static float Slide(FighterData *ft, float v, int frames, float after) {
    float friction = ft->attr.ground_friction > .01f ? ft->attr.ground_friction : .01f, distance = 0;
    for (int i = 0; i < 400 && v > 0 && (i < frames || after > 0); ++i) {
        float brake = v > ft->attr.walk_maximum_velocity ? friction * 2 : friction;
        if (i >= frames && after > brake) brake = after;
        v -= brake;
        if (v > 0) distance += v;
    }
    return distance;
}
static float BrakeDistance(FighterData *ft, float v, int frames) { return Slide(ft, v, frames, 0); }
/* The CPU's Turn length, from the first Turn it shows this match (the .dat
 * reloads with each match, and the CPU's character with it). */
static int turn_frames;
/* The floor a dash needs to turn back: frames of travel before a smash back
 * can turn it (a dash from standing can't for its first five, PlCo 0x44), then
 * the turn's frame at a quarter of the speed (0x54). A dash that can't turn
 * yet is still speeding up, so it's counted at full dash speed. */
static float DashRoom(FighterData *ft, float speed, float frames) {
    float top = ft->attr.dashrun_terminal_velocity;
    if (frames > 1 && speed < top) speed = top;
    if (speed < ft->attr.dash_initial_velocity && frames > 1) speed = ft->attr.dash_initial_velocity;
    return speed * (frames + .25f);
}
/* The floor the CPU needs ahead of it, going `dir`, for its turnaround to stop
 * it no closer than TOUCH to that end. It's checked once a frame, so it counts
 * the frame it would travel before the next check, at the speed it will have
 * then: a walk or dash toward that end is still speeding up. Only speed
 * toward that end counts; just after a turn it's still sliding the other way. */
static float TurnRoom(FighterData *ft, int dir) {
    float speed = ft->phys.self_vel_ground.X * dir;
    if (speed < 0) speed = 0;
    int state = ft->state_id;
    bool toward = ft->facing_direction * dir > 0;
    switch (target.move) {
        case MOVE_WALK: {
            /* A walk with the stick back drops to Wait (ft_8008A244), then
             * turns and waits a frame more, all braking with ground friction.
             * Then it walks the other way, which brakes at the walk's
             * acceleration from the half tilt it starts with (ftWalkCommon_
             * 800E0060: the initial walk speed, plus the stick times the
             * acceleration), much harder than a slippery floor's friction. */
            if (state >= ASID_WALKSLOW && state <= ASID_WALKFAST && toward) {
                speed += ft->attr.walk_initial_velocity + ft->attr.walk_acceleration;
                if (speed > ft->attr.walk_maximum_velocity) speed = ft->attr.walk_maximum_velocity;
            }
            int frames = 2 + (turn_frames ? turn_frames : LONGEST_TURN);
            float back = ft->attr.walk_initial_velocity + ft->attr.walk_acceleration * STICK_WALK / 127.f;
            return speed + Slide(ft, speed, frames, back) + TOUCH;
        }
        case MOVE_RUN:
            /* A run that couldn't crouch in time turns around in TurnRun, which
             * brakes at the dash acceleration. Before the run starts, the CPU
             * is in its dash, which turns as a dash does. */
            if (state == ASID_RUN || state == ASID_RUNDIRECT || state == ASID_TURNRUN || state == ASID_RUNBRAKE) {
                float brake = ft->attr.dashrun_acceleration_a + ft->attr.dashrun_acceleration_b;
                if (brake < .01f) brake = .01f;
                return speed + speed * speed / (2 * brake) + TOUCH;
            }
            // Fall through.
        case MOVE_DASH:
        case MOVE_DASH_FULL:
        case MOVE_DASH_HALF: {
            /* A dash turns back after a neutral frame and a smash: at once if it
             * came out of a turn or is past its fourth frame, else on its fifth.
             * A Turn dashes the next frame; anything else would start a dash. */
            float frame = ft->state.frame, frames = 5;
            if (state == ASID_DASH)
                frames = target.turn_dash || frame > 4 ? 1 : 5 - frame;
            else if (state == ASID_TURN)
                frames = 2;
            if (state == ASID_DASH && toward) {
                speed += ft->attr.dashrun_acceleration_a + ft->attr.dashrun_acceleration_b;
                if (speed > ft->attr.dashrun_terminal_velocity) speed = ft->attr.dashrun_terminal_velocity;
            }
            return speed + DashRoom(ft, speed, frames) + TOUCH;
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
// Standing on a one-way platform, which a fresh down can drop through.
static bool OnPlatform(FighterData *ft) {
    Vec2 a, b;
    int platform;
    return FloorLine(ft->coll_data.ground_index, &a, &b, &platform) && platform;
}
static void Steer(FighterData *ft) {
    UpdateEnds(ft);
    float ahead = RoomAhead(ft, target.dir), behind = RoomAhead(ft, -target.dir);
    if (ahead < TurnRoom(ft, target.dir) && behind > ahead) target.dir = target.leg = -target.dir;
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
enum run_phase { RUN_GO, RUN_CROUCH, RUN_DASH_OUT };
/* How far the CPU goes from the frame it lets go of a run until the dash out
 * of its crouch can start: a frame of RunBrake (ground friction; PlCo 0x60 is
 * 1), then the crouch's Squat frames, which brake with ft_80084F3C. */
static float CrouchSlide(FighterData *ft) {
    float v = fabs(ft->phys.self_vel_ground.X), distance = 0;
    int brake_frames = 1;
    if (OnPlatform(ft) && (u8)ft->input.timer_lstick_tilt_y + 1 < PASS_FRAMES)
        brake_frames = PASS_FRAMES - (u8)ft->input.timer_lstick_tilt_y;
    for (int i = 0; i < brake_frames && v > 0; ++i) {
        v -= ft->attr.ground_friction;
        if (v > 0) distance += v;
    }
    if (v < 0) v = 0;
    return distance + BrakeDistance(ft, v, SQUAT_FRAMES);
}
// The floor a dash out of a crouch needs: it's a dash from standing.
static float DashOutRoom(FighterData *ft) { return DashRoom(ft, 0, 5) + TOUCH; }
/* All the way down crouches. Only a platform can be dropped through, so there
 * it waits until the down tilt is old enough. */
static int CrouchStickY(FighterData *ft) {
    if (OnPlatform(ft) && (u8)ft->input.timer_lstick_tilt_y + 1 < PASS_FRAMES) return STICK_HALF_DOWN;
    return -STICK_FULL;
}
/* Run mode turns a run around the way players do, without a run turnaround:
 * near the end it lets go into RunBrake and crouches (RunBrake takes a crouch,
 * ftCo_800D5FB0; Run doesn't), dashes forward out of the crouch (SquatWait
 * takes a dash; Squat, the crouch's first 8 frames, doesn't) right up to the
 * end, then dashes back and runs the other way. The crouch comes early enough
 * to leave that dash a few frames beyond the five it can't turn in. If the
 * crouch still ends too near the end, it dashes straight back out of it (a
 * reversed smash from SquatWait turns, then dashes). Running, the stick is
 * held half down, so by the crouch the down tilt is too old to drop through a
 * platform (ftCo_80099F1C: down past 0.66 within 6 frames of crossing 0.25; a
 * crouch needs 0.6875). A run that starts too near the end to crouch at all
 * turns around in TurnRun, which stops at the end. */
static void RunInputs(FighterData *ft) {
    int state = ft->state_id, dir = target.dir;
    ft->cpu.lstickY = 0;
    if (target.run_phase == RUN_CROUCH) {
        if (state == ASID_SQUATWAIT) {
            if (RoomAhead(ft, dir) >= DashOutRoom(ft)) {
                target.run_phase = RUN_DASH_OUT;
            } else {
                target.dir = target.leg = dir = -dir;
                target.run_phase = RUN_GO;
            }
            ft->cpu.lstickX = Smash(ft, dir); // The stick was neutral through the crouch.
            return;
        }
        if (state == ASID_RUN || state == ASID_RUNBRAKE || state == ASID_SQUAT) {
            ft->cpu.lstickX = 0;
            ft->cpu.lstickY = CrouchStickY(ft);
            return;
        }
        target.run_phase = RUN_GO;
    }
    if (target.run_phase == RUN_DASH_OUT) {
        if (state == ASID_DASH && Facing(ft, dir)) {
            if (!ft->ftcmd_var.flag0 && RoomAhead(ft, dir) >= TurnRoom(ft, dir)) {
                ft->cpu.lstickX = dir * STICK_FULL;
                return;
            }
            // At the end (or about to run): dash back the other way.
            target.dir = target.leg = dir = -dir;
            target.run_phase = RUN_GO;
            ft->cpu.lstickX = Smash(ft, dir);
            return;
        }
        if (state == ASID_SQUATWAIT) {
            ft->cpu.lstickX = Smash(ft, dir);
            return;
        }
        target.run_phase = RUN_GO;
    }
    if (state == ASID_RUN && Facing(ft, dir)) {
        float room = RoomAhead(ft, dir), slide = CrouchSlide(ft);
        float top = ft->attr.dashrun_terminal_velocity;
        if (room - slide < DashOutRoom(ft) + CROUCH_EXTRA * top) {
            if (room - slide >= TOUCH) {
                target.run_phase = RUN_CROUCH;
                ft->cpu.lstickX = 0;
                ft->cpu.lstickY = CrouchStickY(ft);
                return;
            }
            target.dir = target.leg = dir = -dir; // Too near to crouch: TurnRun.
        } else {
            ft->cpu.lstickY = STICK_HALF_DOWN;
        }
        ft->cpu.lstickX = dir * STICK_FULL;
        return;
    }
    Steer(ft);
    ft->cpu.lstickX = RunStick(ft, target.dir);
    /* A dash that would become a run (its script's flag is set) too near the
     * end to crouch in time stays a dash, with the stick short of a run, and
     * turns back as a dash does. */
    if (state == ASID_DASH && Facing(ft, target.dir) && ft->ftcmd_var.flag0 &&
        RoomAhead(ft, target.dir) - CrouchSlide(ft) < TOUCH + fabs(ft->phys.self_vel_ground.X))
        ft->cpu.lstickX = target.dir * STICK_DASH_HOLD;
    // The down tilt starts aging in the dash, for a crouch on a platform.
    if (state == ASID_DASH && Facing(ft, target.dir)) ft->cpu.lstickY = STICK_HALF_DOWN;
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
        if (RoomAhead(ft, leg) < TurnRoom(ft, leg) && RoomAhead(ft, -leg) > RoomAhead(ft, leg)) target.leg = leg = -leg;
        return Smash(ft, leg);
    }
    if (!Facing(ft, leg)) return Smash(ft, leg);
    bool done = ft->ftcmd_var.flag0 || RoomAhead(ft, leg) < TurnRoom(ft, leg);
    if (target.move == MOVE_DASH_HALF && run_frame && ft->state.frame >= run_frame / 2) done = true;
    if (!done) return leg * STICK_FULL;
    target.leg = -leg;
    return Smash(ft, -leg);
}
static int RollStick(FighterData *ft, int dir) {
    int state = ft->state_id;
    return state == ASID_GUARDON || state == ASID_GUARD ? Smash(ft, dir) : 0;
}

/* R held, and the shield kept full every frame, as Infinite Shields keeps it.
 * A hit on the shield puts the CPU in GuardSetOff, one of its own states
 * (Moving), so it stays put rather than coming back somewhere else; R still
 * held, it goes back to Guard. */
static void HoldShield(FighterData *ft) {
    ft->cpu.held |= PAD_TRIGGER_R;
    ft->shield.health = (*stc_ftcommon)->x260;
}
static void Move(GOBJ *cpu, FighterData *ft) {
    int state = ft->state_id;
    if (state == ASID_DASH && target.last_state != ASID_DASH) target.turn_dash = target.last_state == ASID_TURN;
    bool flag = state == ASID_DASH && ft->ftcmd_var.flag0;
    if (flag && !target.last_flag && target.last_state == ASID_DASH && !run_frame) run_frame = ft->state.frame;
    target.last_flag = flag;
    if (state == ASID_TURN && !turn_frames) turn_frames = Fighter_GetCurrentAnimLength(cpu);
    target.last_state = state;
    if (ft->phys.air_state != 0) return;
    if (target.move == MOVE_STAND) {
        if (target.shield) HoldShield(ft);
        return;
    }
    if (target.move == MOVE_DASH_FULL || target.move == MOVE_DASH_HALF) {
        UpdateEnds(ft);
        ft->cpu.lstickX = DanceStick(ft);
        return;
    }
    if (target.move == MOVE_RUN) {
        UpdateEnds(ft);
        RunInputs(ft);
        return;
    }
    Steer(ft);
    int dir = target.dir;
    switch (target.move) {
        case MOVE_WALK: ft->cpu.lstickX = WalkStick(ft, dir); break;
        case MOVE_DASH: ft->cpu.lstickX = DashStick(ft, dir); break;
        case MOVE_ROLL:
            // Shield held throughout, so each roll ends back in shield.
            HoldShield(ft);
            ft->cpu.lstickX = RollStick(ft, dir);
            break;
    }
}

// Hit mode's tech chase --------------------------------------------------------

// The way from you to the CPU, -1 or 1.
static int AwayFromYou(FighterData *ft) {
    return ft->phys.pos.X < ((FighterData *)Fighter_GetGObj(0)->userdata)->phys.pos.X ? -1 : 1;
}
/* Falling onto a floor within LAND_FRAMES frames at the speed it has now. */
static bool LandingSoon(FighterData *ft) {
    float fall = -(ft->phys.self_vel.Y + ft->phys.kb_vel.Y), y;
    if (fall <= 0) return false;
    for (int i = 0; i < LineCount(); ++i) {
        if (!LineY(i, ft->phys.pos.X, &y)) continue;
        float above = ft->phys.pos.Y - y;
        if (above >= -LAND_BELOW && above <= fall * LAND_FRAMES) return true;
    }
    return false;
}
/* In knockback, the tech is picked once a landing needs it, and stands until
 * the next hit or grab. L/R reads as pressed TECH_PRESS_AGO frames ago with no
 * press before it, as Techchase and the Training Lab write it, so the landing
 * techs whenever it comes, with no press to time to it. A missed tech reads as
 * never pressed. A tech roll holds the stick away from you or toward you while
 * it can't drift (in hitstun, ft_80084EEC) or is about to land, and never in
 * hitlag, where it would DI (that stick is CPU DI's, DIInputs). */
static void TechInputs(FighterData *ft) {
    if (target.landing < 0) target.landing = TC_PresetTech(TECH_PRESET, HSD_Randi(100));
    if (target.landing == TC_TECH_MISS) {
        ft->input.timer_LR = 255;
        return;
    }
    ft->input.timer_LR = TECH_PRESS_AGO;
    ft->input.since_rapid_lr = 255;
    if (target.landing == TC_TECH_IN_PLACE || Frozen(ft) || !(ft->flags.hitstun || LandingSoon(ft))) return;
    int away = AwayFromYou(ft);
    ft->cpu.lstickX = (target.landing == TC_TECH_AWAY ? away : -away) * TECH_STICK;
}
/* A missed tech's getup, picked as it lands, held through DownBound, which
 * gets up at its end, and DownWait, which gets up at once: the stick up or to
 * the side, or A pressed afresh every other frame (DownWait takes only a new
 * press). */
static void GetupInputs(FighterData *ft) {
    int state = ft->state_id;
    if (state != ASID_DOWNBOUNDU && state != ASID_DOWNBOUNDD && state != ASID_DOWNWAITU && state != ASID_DOWNWAITD)
        return;
    if (target.getup < 0) target.getup = TC_PresetGetup(TECH_PRESET, HSD_Randi(100));
    int away = AwayFromYou(ft);
    switch (target.getup) {
        case TC_GETUP_STAND: ft->cpu.lstickY = STICK_FULL; break;
        case TC_GETUP_AWAY: ft->cpu.lstickX = away * TECH_STICK; break;
        case TC_GETUP_TOWARD: ft->cpu.lstickX = -away * TECH_STICK; break;
        default: ft->cpu.held = ft->input.held & PAD_BUTTON_A ? 0 : PAD_BUTTON_A; break;
    }
}
/* Hit mode's juggle: how the CPU lands from knockback, and how it gets up. */
static void ChaseInputs(FighterData *ft) {
    if (ft->phys.air_state == 0) GetupInputs(ft);
    else if (KnockedBack(ft->state_id)) TechInputs(ft);
}

// Hit mode's DI -------------------------------------------------------------------

/* CPU DI's Random: the stick all the way one of its eight ways, the gate's
 * notches. A diagonal's 89 (0.70) on each axis makes a stick 0.99 long, as the
 * Training Lab's are, past the 0.7 (PlCo 0x4B0) ASDI takes. */
#define DI_DIAGONAL 89
static const s8 DI_Sticks[][2] = {
    {STICK_FULL, 0}, {DI_DIAGONAL, DI_DIAGONAL}, {0, STICK_FULL}, {-DI_DIAGONAL, DI_DIAGONAL},
    {-STICK_FULL, 0}, {-DI_DIAGONAL, -DI_DIAGONAL}, {0, -STICK_FULL}, {DI_DIAGONAL, -DI_DIAGONAL},
};
static bool RandomDI(void) { return HitMode() && Options_Main[OPT_DI].val == TM_GRABS_DI_RANDOM; }
/* Thrown by a throw, DK's cargo throws included, which lets go partway through. */
static bool Thrown(int state) {
    return (state >= ASID_THROWNF && state <= ASID_THROWNLWWOMEN) || (state >= ASID_THROWNFF && state <= ASID_THROWNFLW);
}
/* CPU DI (Hit mode): through a standard grab's throw and each hit's hitlag, the
 * CPU holds the stick all the way one of its eight ways, picked at random for
 * the throw and again for each hit (more hitlag than was left: a new hit, or one
 * during another's). The game reads the stick of last frame's input proc as the
 * throw lets go, in the thrower's animation (ftCo_800DE7C0, DI at once through
 * ftCo_8008E5A4), and as the hitlag ends, in the hitlag proc
 * (ftCo_Damage_OnExitHitlag: ASDI, then DI). Smash DI (ftCo_Damage_OnEveryHitlag)
 * takes only a flick, a stick whose tilt timer is under PlCo 0x4B4 (4), so the
 * processed stick and the timers are set as if it had been held all along: the
 * input proc sees no flick, and counts the timers up from there. Hitlag in
 * shield (GuardSetOff) isn't knockback, and a hold (CaptureWait, Shouldered)
 * isn't a throw, where the stick would mash out. */
static void DIInputs(FighterData *ft, int state) {
    bool thrown = target.grabbed && Thrown(state), hit = KnockedBack(state) && Frozen(ft);
    if (!RandomDI() || !(thrown || hit)) {
        target.di = -1;
        target.di_lag = 0;
        return;
    }
    float lag = hit ? ft->dmg.hitlag_frames : 0;
    if (target.di < 0 || lag > target.di_lag) target.di = HSD_Randi(countof(DI_Sticks));
    target.di_lag = lag;
    const s8 *stick = DI_Sticks[target.di];
    ft->cpu.lstickX = stick[0];
    ft->cpu.lstickY = stick[1];
    ft->input.lstick = (Vec2){stick[0] / 127.f, stick[1] / 127.f};
    ft->input.timer_lstick_tilt_x = ft->input.timer_lstick_tilt_y = 254;
}

static void Score(void) {
    if (grab_count < 999) grab_count++;
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
    /* Knocked off the top, by a hit or a throw: it's back somewhere else at
     * once, rather than after the KO and a respawn platform. */
    if (OffTheTop(state)) {
        Respawn();
        return;
    }
    if (Held(state)) {
        // A standard grab counts at once, or in Hit mode once a hit follows it.
        if (JustGrabbed(state) && !target.grabbed) {
            target.grabbed = true;
            if (!HitMode()) Score();
        }
        target.held = true;
        target.hit = false; // A hit that led into the grab: its window is over.
        // Hit mode: a grab in the juggle (a tech regrabbed) starts the juggle over at its throw.
        target.juggle = target.chase = false;
        target.landing = target.getup = -1;
        DIInputs(ft, state); // The throw's DI.
        return;
    }
    if (ft->flags.dead) return;
    /* Hit mode: let go after a standard grab, the CPU stays up to be hit. A hit
     * is new hitlag, or more hitlag than was left: a hit during another's.
     * Each one adds to the hits, and the first counts. */
    bool juggle = target.grabbed && HitMode();
    if (juggle) {
        if (!target.juggle) {
            target.juggle = target.hit = true; // A window, as after a hit,
            target.hit_frames = target.air_frames = 0;
            target.landing = target.getup = -1;
        } else if (ft->dmg.hitlag_frames > target.last_lag) {
            target.air_frames = 0;
            // A hit on a tech or a getup: the juggle goes on, and its next landing is picked anew.
            target.chase = false;
            target.landing = target.getup = -1;
            if (total_hits < 999) total_hits++;
            if (!target.scored) {
                target.scored = true;
                Score();
            }
        }
        target.last_lag = ft->dmg.hitlag_frames; // (the throw itself isn't a hit).
    }
    // Any other grab's let go, a throw included: it's gone.
    bool away = target.held && !juggle;
    /* A hit doesn't send it away at once: it stays, pressing nothing but its DI, for
     * GRAB_WINDOW frames after the hitlag, so a hit can lead into a grab, and
     * each new hit's hitlag starts the window over. A juggle's window starts
     * only once it lands. A knockdown, which no grab can catch, ends it then
     * and there, but in a juggle, it's a tech chase: a tech, or a knockdown and
     * its getup, holds the window, and once it's up unhit, it's gone. */
    if (!away && !Moving(state)) target.hit = true;
    if (juggle && !away) {
        if (Chased(state)) target.chase = true;
        else if (target.chase && Moving(state)) away = true;
        else target.chase = false;
    }
    if (!away && target.hit) {
        if (Frozen(ft) || (juggle && (ft->phys.air_state != 0 || target.chase))) target.hit_frames = 0;
        else if (++target.hit_frames > GRAB_WINDOW || Down(state)) away = true;
    }
    if (ft->phys.air_state != 0) {
        float left, right, bottom;
        // Hitstun doesn't count: a CPU sent flying is still on its way.
        if (ft->flags.hitstun) target.air_frames = 0;
        else target.air_frames++;
        if (StageBounds(&left, &right, &bottom) &&
            (ft->phys.pos.X < left - OFF_STAGE || ft->phys.pos.X > right + OFF_STAGE || ft->phys.pos.Y < bottom - OFF_STAGE))
            away = true;
        if (target.air_frames > AIR_FRAMES) away = true;
    } else {
        target.air_frames = 0;
    }
    // A throw, a hit's or a juggle's window run out, or a fall: after any
    // hitlag, it's gone and back somewhere else.
    if (away) {
        if (!Frozen(ft)) Respawn();
        return;
    }
    if (juggle) ChaseInputs(ft);
    DIInputs(ft, state); // Each hit's DI, in its hitlag.
    if (target.hit) return; // Left alone to be grabbed: back to its movement only at a new spot.
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
        Text_SetText(hud_hits_text, 0, "");
        return;
    }
    Text_SetText(hud_score_text, 0, "%d", grab_count);
    if (HitMode()) Text_SetText(hud_hits_text, 0, "Hits: %d", total_hits);
    else Text_SetText(hud_hits_text, 0, "");
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
    for (unsigned i = 0; i < countof(saved_rows); ++i) Options_Main[saved_rows[i]].disable = 1;
    if (Options_Main[OPT_FREEPRACTICE].val) EnterFreePractice();
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
    hud_hits_text = Text_CreateText(2, canvas);
    hud_hits_text->kerning = 1;
    hud_hits_text->use_aspect = 1;
    hud_hits_text->scale.X = 0.35;
    hud_hits_text->scale.Y = 0.35;
    hud_hits_text->viewport_scale.X = 1;
    hud_hits_text->viewport_scale.Y = 1;
    PlaceCounter(Options_Main[OPT_COUNTER].val);
    Text_AddSubtext(hud_score_text, 0, 0, "-");
    Text_AddSubtext(hud_best_text, 0, 0, "-");
    Text_AddSubtext(hud_hits_text, 0, 0, "-");

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
