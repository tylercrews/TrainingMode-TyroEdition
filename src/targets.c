#include "events.h"
#include "fighter_swap.h"
#include "stage_points.h"
#include "stage_swap.h"

/* Target Practice (Minigames): Fox and Falco shoot targets with their blaster,
 * on the six legal stages. A target is a small bullseye somewhere over the
 * main stage, centered on its spot. Shoot it and the next one appears. A timed
 * minute counts targets, with the best saved; Free Practice has no clock, and
 * its SH Double Lasers makes about half the spawns a pair that one short hop
 * double laser can hit, off the main floor, a side platform or the top one.
 * FH Multi Lasers does the same with two or three targets for one full hop.
 *
 * Heights. Lasers fly flat, so a target's height is the height of the laser
 * that hits it. The lowest spot is the lowest laser that can be shot from the
 * floor under it; the highest is a laser shot at the peak of a full hop and a
 * double jump from the highest floor on the stage. Laser heights are from the
 * fighters' own skeletons (scripts/laser_heights.py):
 *
 * - ftFx_SpecialN_FtGetHoldJoint puts a shot at the RThumbNb joint's matrix
 *   times (0, 1.2325, 4.2636), with z zeroed, on the frame the Loop script
 *   sets cmd_vars[2]: Fox's frame 5, Falco's 8 (SpecialAirNLoop and
 *   SpecialNLoop alike). That's in the animation proc, before the frame's
 *   physics, so it's from where the fighter ended the frame before.
 * - Airborne, the fighter lands once TopN plus the ECB bottom reaches the
 *   floor (mpColl_LoadECB_JObj, with Fighter_procMap putting the skeleton at
 *   the new position first). In the laser loop the legs are tucked, so the ECB
 *   bottom is 4.43 (Fox) or 5.22 (Falco) above TopN, and TopN can be that far
 *   under the floor and still shoot. Fox's lowest laser is 9.28 - 4.43 = 4.85
 *   up. Falco's would start 1.42 under the floor, but itFoxlaser's first
 *   collision check runs from the fighter's ECB center to the shot, so a shot
 *   under the floor is destroyed at once: his lowest laser hugs the floor.
 * - Standing lasers come out 7.57 (Fox) and 2.72 (Falco) up. Both are 2.72
 *   above the lowest laser, and a laser's hitbox is 1.17 across each way
 *   (0x12C / 256), so a target smaller than 1.55 needs a low laser at the
 *   bottom of its range.
 *
 * Hits. A laser's hitboxes sweep from last frame's spot to this one's
 * (it_80272674: state 1 is a new hitbox, 2 and on hold both ends), so a laser
 * moving 5 to 7 units a frame can't skip a target. One whose swept capsule
 * touches a target breaks it, and the laser ends there the way it does on a
 * wall: no more hitboxes, one frame left to live.
 *
 * SHDL pairs. The aerial laser fires 9 frames after the B press (Start's 5
 * frames and the Loop's fire frame), then every Loop (Fox 10 frames, Falco 16)
 * while B is pressed again in time. ShortHopPresses walks a short hop with no
 * fastfall (fastfalling only lands sooner) and finds the press frames whose
 * second shot comes before the landing. For Fox that's B on the first airborne
 * frame: shots at 17.8 and 5.5 above the floor. Falco's slower laser never
 * fits, so the row reads (N/A) for him. A pair's floor is the main floor, a
 * side platform or the top platform, each kind as likely as the others where
 * the stage has one that holds still: Fountain's side platforms move, Stadium
 * has no top platform and FD no platforms.
 *
 * FH combos. FullHopCombos walks a full hop the same way and times one B
 * press per combo: Hi-Hi-Low (the first airborne frame, three shots),
 * Peak-Low (the first shot from the peak), High-Lowest (the last press whose
 * second shot comes before landing), and High-MedHigh and High-Medium, whose
 * second shots come halfway from Peak-Low's to the highest and the lowest. A
 * combo stacks over a floor the way a pair does. All Combos makes about half
 * the spawns a combo at random; one combo picked makes every spawn that
 * combo, but for SH pairs if they're on too.
 *
 * Hop Peak Lines. A jump's highest laser is the one shot from its peak (the
 * shot comes from where the fighter was the frame before, and the peak is the
 * highest it gets), so each floor gets two lines: teal for a short hop's, purple
 * for a full hop's, Rise + the aerial laser height above it. The main floor's
 * and the top platform's go from blast zone to blast zone; a side platform's
 * stop at the middle (x = 0), so the two sides' read apart (Dream Land's right
 * platform is 0.1 higher). They're found from the live lines every frame, so
 * Fountain's follow its side platforms, and go while one is sunk into the
 * stage.
 *
 * Double Jump Height. A double jump sets the vertical speed to the full hop's
 * times air_jump_v_multiplier whatever it was (ftCo_JumpAerial_Enter_Basic),
 * then falls the way a jump does, so from anywhere it rises the same Rise. So
 * while the fighter is in the air with its double jump, a white T runs from
 * TopN (the feet), up through the head's x (HeadN), to that Rise above: a
 * dashed stem and a solid bar where the feet would peak. Red goes on above it,
 * to the aerial laser height over that bar, and an arm out the way the fighter
 * faces: where a laser shot at the double jump's peak flies, as Hop Peak Lines
 * mark a hop's. Together they read as an F on the T. It goes once the jump is
 * spent (Fox's and Falco's up-B uses it up on launch, ftFx_SpecialHi) or
 * can't come: helpless, air dodging, on the ledge, dead, and during side-B and
 * up-B, which end helpless or on the ground. */

#define TIMED_FRAMES 3600     // The timed run's minute.
#define TARGET_RADIUS 1.5f    // Under 2.72 - 1.17: see above.
#define SPAWN_INSET 8.f       // From the main floor's ends, and a platform's.
#define SPAWN_FROM_YOU 30.f
#define SPAWN_FROM_LAST 25.f
#define SPAWN_TRIES 40
#define MAX_SHOTS 3           // FH Multi Lasers' Hi-Hi-Low.
#define MAX_TARGETS MAX_SHOTS
#define POP_FRAMES 12         // A broken target's ring.
#define CIRCLE_STEPS 20
#define JUMP_FRAMES 96        // Longer than Fox's or Falco's full hop.
#define CAMERA_MARGIN 10.f
#define PEAK_LINE_WIDTH 12    // 2 pixels, in sixths.
#define PEAK_FLOORS 4         // The main floor and up to three platforms.
/* A side platform closer than this to the main floor is sunk: Fountain's
 * settle 0.37 above it, inside the stage, while they're hidden. */
#define SUNK_PLATFORM 1.f
/* Double Jump Height's T: its stem's dashes and gaps, counted down from the
 * bar so a whole dash meets it, the bar's reach each side, and the red laser
 * arm's length. Drawn PEAK_LINE_WIDTH wide. */
#define DJ_DASH 2.5f
#define DJ_GAP 1.5f
#define DJ_BAR 6.f
#define DJ_ARM 10.f
/* Fox's and Falco's side-B and up-B, from SpecialSStart to SpecialHiBound
 * (the decomp's ftFox_MotionState, after the common states' 341): each ends
 * helpless or on the ground, so the double jump can't come. */
#define FX_SPECIAL_S_FIRST 347
#define FX_SPECIAL_HI_LAST 359

/* Laser heights above TopN, the airborne ECB bottom on the frame before the
 * aerial shot, and the frames from a B press to the aerial shot and between
 * looped shots. Measured by scripts/laser_heights.py, which
 * tests/test_targets.py runs against melee.iso. */
typedef struct Blaster {
    float air, ground, ecb_bottom;
    int delay, loop;
} Blaster;
static const Blaster Blasters[2] = {
    {9.2829f, 7.5740f, 4.4322f, 9, 10},  // Fox
    {3.7980f, 2.7208f, 5.2207f, 12, 16}, // Falco
};

enum options_main {
    OPT_RETRY, OPT_STAGE, OPT_CHARACTER, OPT_FREEPRACTICE, OPT_SHDL, OPT_FULL_HOP, OPT_LINES, OPT_DOUBLE_JUMP,
    OPT_COUNTER, OPT_DEFAULTS, OPT_EXIT, OPT_COUNT
};

static void Retry(GOBJ *menu);
static void Exit(GOBJ *menu);
static void ChangeFreePractice(GOBJ *menu, int value);
static void ChangeDoubleLasers(GOBJ *menu, int value);
static void ChangeFullHop(GOBJ *menu, int value);
static void ChangePeakLines(GOBJ *menu, int value);
static void ChangeDoubleJump(GOBJ *menu, int value);
static void ChangeCounterPosition(GOBJ *menu, int value);
static void ResetSettings(GOBJ *menu);

static const char *CounterPositionText[TM_EGGS_COUNTER_COUNT] = {
    "Top Left", "Top Right", "Bottom Left", "Bottom Right", "Under Timer"
};
static char DoubleLasersName[] = "SH Double Lasers";
static char DoubleLasersUnavailable[] = "SH Double Lasers (N/A)";
// In TMTargetsFullHop's order: the saved values.
static const char *FullHopText[TM_TARGETS_FH_COUNT] = {
    "Off", "All Combos", "Hi-Hi-Low", "High-MedHigh", "Peak-Low", "High-Medium", "High-Lowest"
};
static char FullHopName[] = "FH Multi Lasers";
static char FullHopUnavailable[] = "FH Multi Lasers (N/A)";

/* Fox or Falco, the only characters the event's CSS offers. There's no CPU,
 * so the submenu lists only the player's row. */
static const u8 Character_Kinds[] = {CKIND_FOX, CKIND_FALCO};
static EventMenu Menu_Character = {
    .name = "Change Character",
    .option_num = 1,
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
        .name = "Change Character",
        .desc = {"Play as Fox or Falco in any costume, then restart.",
                 "A timed run starts over; Free Practice stays on",
                 "with its saved settings."},
        .menu = &Menu_Character,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Free Practice",
        .desc = {"Practice with no time limit. Off is the timed",
                 "minute, with your best saved.",
                 "Changing this restarts the event. Saved."},
        .OnChange = ChangeFreePractice,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = DoubleLasersName,
        .desc = {"About half the targets come in pairs for one short",
                 "hop double laser off the floor, a side platform or",
                 "the top platform. Fox only: Falco's laser is too",
                 "slow. Free Practice only. Saved."},
        .disable = 1,
        .OnChange = ChangeDoubleLasers,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = TM_TARGETS_FH_COUNT,
        .name = FullHopName,
        .desc = {"Targets stacked for one full hop multi laser, no",
                 "fastfall. All Combos: about half the targets, any",
                 "combo. A combo: every target, to drill it. Free",
                 "Practice only. Saved."},
        .values = FullHopText,
        .disable = 1,
        .OnChange = ChangeFullHop,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Hop Peak Lines",
        .desc = {"Lines where a laser shot at the peak of a short",
                 "hop (teal) or a full hop (purple) flies, off the",
                 "floor, the top platform and each side platform",
                 "(on that side of the middle). Saved."},
        .OnChange = ChangePeakLines,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump Height",
        .desc = {"In the air with a double jump left: a white T from",
                 "your feet to where they'd peak if you double jumped",
                 "now, and red where a laser shot at that peak",
                 "flies, the way you face. Saved."},
        .OnChange = ChangeDoubleJump,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = countof(CounterPositionText),
        .name = "Counter Position",
        .desc = {"Move the target count and the text under it to a",
                 "corner of the screen or under the timer.",
                 "Shared with Eggs-ercise and Grabs-ercise."},
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
    .name = "Target Practice",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};
EventMenu *Event_Menu = &Menu_Main;

// Session state, set up again by Event_Init.
static struct target { Vec2 pos; bool live; } targets[MAX_TARGETS];
static struct pop { Vec2 pos; int age; } pops[MAX_TARGETS]; // age 0: none.
static Vec2 last_spot;
static bool placed;          // Targets have appeared this match.
static bool checked;         // The fighter's been looked at (CheckFighter).
static int pair_presses;     // Press frames that fit a short hop double laser.
static float peak_lasers[2]; // A short hop's and a full hop's peak laser, above the floor.
static float double_jump;    // A double jump's rise, from wherever it starts.
static int score, best;
static bool reset_clock;
static bool restarting;      // Leaving a stage that isn't legal.
static CmSubject *cam;
static Text *hud_score_text, *hud_best_text;
static GXColor text_gold = {255, 211, 0, 255};
static GXColor text_white = {255, 255, 255, 255};

// Heights ---------------------------------------------------------------------

static const Blaster *BlasterOf(FighterData *ft) { return &Blasters[ft->kind == FTKIND_FALCO]; }

/* The lowest laser above the floor it's shot from: TopN as far under the
 * floor as the ECB bottom allows, never under the floor itself. */
static float LowestLaser(const Blaster *blaster) {
    float height = blaster->air - blaster->ecb_bottom;
    return height > 0 ? height : 0;
}

/* Apex rise for one jump: the native jump applies gravity before moving. */
static float Rise(float velocity, float gravity) {
    float height = 0;
    if (gravity <= 0) return 0;
    for (int i = 0; i < 256; ++i) {
        velocity -= gravity;
        if (velocity <= 0) break;
        height += velocity;
    }
    return height;
}
// A double jump sets its own speed, so it rises this much from anywhere.
static float DoubleJumpRise(FighterData *ft) {
    return Rise(ft->attr.jump_v_initial_velocity * ft->attr.air_jump_v_multiplier, ft->attr.gravity);
}
static float FullHopDoubleJump(FighterData *ft) {
    return Rise(ft->attr.jump_v_initial_velocity, ft->attr.gravity) + DoubleJumpRise(ft);
}

/* Height after each airborne frame of a jump from `velocity` with no
 * fastfall, frame 1 the first. */
static void Jump(FighterData *ft, float velocity, float *y) {
    float gravity = ft->attr.gravity, fall = ft->attr.terminal_velocity;
    y[0] = 0;
    for (int frame = 1; frame < JUMP_FRAMES; ++frame) {
        velocity -= gravity;
        if (velocity < -fall) velocity = -fall;
        y[frame] = y[frame - 1] + velocity;
    }
}
/* B on airborne frame `press` of a jump from `velocity` shoots on press +
 * delay and, looped, every Loop after, each from where the fighter was the
 * frame before. Fills `count` heights above the floor, or returns 0 if the
 * fighter lands first or a shot would start under the floor (it dies at once,
 * see LowestLaser). Past the apex each frame is lower than the last, so a shot
 * from the air means every frame before it was in the air. */
static int JumpShots(FighterData *ft, const Blaster *blaster, float velocity, int press, int count, float *heights) {
    float y[JUMP_FRAMES];
    if (press < 1) return 0;
    Jump(ft, velocity, y);
    for (int i = 0; i < count; ++i) {
        int shot = press + blaster->delay + i * blaster->loop;
        if (shot >= JUMP_FRAMES) return 0;
        float from = y[shot - 1];
        if (from + blaster->ecb_bottom <= 0 || from + blaster->air <= 0) return 0;
        heights[i] = from + blaster->air;
    }
    return 1;
}
// A short hop double laser's two shots. 0 if it lands before the second.
static int ShortHopShots(FighterData *ft, const Blaster *blaster, int press, float *high, float *low) {
    float shots[2];
    if (!JumpShots(ft, blaster, ft->attr.hop_v_initial_velocity, press, 2, shots)) return 0;
    *high = shots[0];
    *low = shots[1];
    return 1;
}
/* How many press frames from the first fit both shots. A later press is a
 * later second shot, so they're 1 to the count. */
static int ShortHopPresses(FighterData *ft, const Blaster *blaster) {
    float high, low;
    int count = 0;
    while (ShortHopShots(ft, blaster, count + 1, &high, &low)) ++count;
    return count;
}

/* FH Multi Lasers' combos, each one B press in a full hop with no fastfall.
 * A later press makes a lower second shot (it's past the apex for every
 * press), so the doubles run from press 1 to the last whose second shot
 * comes before landing:
 *
 * - Hi-Hi-Low: B on the first airborne frame, looped twice for three shots.
 * - Peak-Low: the first shot from the jump's peak, Hop Peak Lines' purple.
 * - High-Lowest: the last press, so the lowest second shot there is.
 * - High-MedHigh: the second shot nearest halfway from Peak-Low's up to the
 *   highest any double has; High-Medium: halfway down to High-Lowest's.
 *
 * A combo that doesn't fit the fighter has no shots. */
static struct full_hop { int press, shots; float heights[MAX_SHOTS]; } full_hops[TM_TARGETS_FH_COUNT];

static float FullHopVelocity(FighterData *ft) { return ft->attr.jump_v_initial_velocity; }
static float SecondShot(FighterData *ft, const Blaster *blaster, int press) {
    float shots[2];
    return JumpShots(ft, blaster, FullHopVelocity(ft), press, 2, shots) ? shots[1] : 0;
}
static void SetCombo(FighterData *ft, const Blaster *blaster, int combo, int press, int shots) {
    struct full_hop *c = &full_hops[combo];
    c->press = press;
    c->shots = JumpShots(ft, blaster, FullHopVelocity(ft), press, shots, c->heights) ? shots : 0;
}
// The press from `from` to `to` whose second shot comes nearest `height`.
static int PressNear(FighterData *ft, const Blaster *blaster, int from, int to, float height) {
    int best = from;
    float best_off = -1;
    for (int press = from; press <= to; ++press) {
        float off = SecondShot(ft, blaster, press) - height;
        if (off < 0) off = -off;
        if (best_off < 0 || off < best_off) {
            best = press;
            best_off = off;
        }
    }
    return best;
}
static void FullHopCombos(FighterData *ft, const Blaster *blaster) {
    float y[JUMP_FRAMES], shots[2], top = 0;
    int apex = 1, doubles = 0, highest = 1;
    for (int i = 0; i < TM_TARGETS_FH_COUNT; ++i) full_hops[i].shots = 0;
    while (JumpShots(ft, blaster, FullHopVelocity(ft), doubles + 1, 2, shots)) {
        ++doubles;
        if (shots[1] > top) {
            top = shots[1];
            highest = doubles;
        }
    }
    if (!doubles) return;
    Jump(ft, FullHopVelocity(ft), y);
    for (int frame = 2; frame < JUMP_FRAMES; ++frame)
        if (y[frame] > y[apex]) apex = frame;
    int peak = apex + 1 - blaster->delay;
    SetCombo(ft, blaster, TM_TARGETS_FH_HI_HI_LOW, 1, 3);
    SetCombo(ft, blaster, TM_TARGETS_FH_HIGH_LOWEST, doubles, 2);
    if (peak < 1 || peak > doubles) return; // The rest are timed around Peak-Low.
    float middle = SecondShot(ft, blaster, peak);
    SetCombo(ft, blaster, TM_TARGETS_FH_PEAK_LOW, peak, 2);
    SetCombo(ft, blaster, TM_TARGETS_FH_HIGH_MEDHIGH,
             PressNear(ft, blaster, highest < peak ? highest : peak, peak, (middle + top) * .5f), 2);
    SetCombo(ft, blaster, TM_TARGETS_FH_HIGH_MEDIUM,
             PressNear(ft, blaster, peak, doubles, (middle + SecondShot(ft, blaster, doubles)) * .5f), 2);
}
static bool FullHopFits(void) {
    for (int c = TM_TARGETS_FH_HI_HI_LOW; c < TM_TARGETS_FH_COUNT; ++c)
        if (full_hops[c].shots) return true;
    return false;
}

// Settings --------------------------------------------------------------------

static bool FreePractice(void) { return stc_match->match.timer == MATCH_TIMER_COUNTUP; }
static bool PairsOn(void) { return FreePractice() && Options_Main[OPT_SHDL].val && pair_presses > 0; }
/* The combo a full hop spawn uses: the row's, or for All Combos any that fits
 * the fighter. -1 if there's none. Only All Combos draws a number. */
static int FullHopCombo(bool pick) {
    int value = Options_Main[OPT_FULL_HOP].val, fits[TM_TARGETS_FH_COUNT], count = 0;
    if (!FreePractice() || value <= TM_TARGETS_FH_OFF || value >= TM_TARGETS_FH_COUNT) return -1;
    if (value != TM_TARGETS_FH_ALL) return full_hops[value].shots ? value : -1;
    for (int c = TM_TARGETS_FH_HI_HI_LOW; c < TM_TARGETS_FH_COUNT; ++c)
        if (full_hops[c].shots) fits[count++] = c;
    return !count ? -1 : pick ? fits[HSD_Randi(count)] : fits[0];
}

/* Free Practice only, and only for a fighter whose short hop fits two lasers
 * (or full hop, a combo), which isn't known until its first frame. */
static void GateLaserRows(void) {
    Options_Main[OPT_SHDL].disable = !FreePractice() || !pair_presses;
    Options_Main[OPT_SHDL].name = checked && !pair_presses ? DoubleLasersUnavailable : DoubleLasersName;
    Options_Main[OPT_FULL_HOP].disable = !FreePractice() || !FullHopFits();
    Options_Main[OPT_FULL_HOP].name = checked && !FullHopFits() ? FullHopUnavailable : FullHopName;
}
static void LoadSettings(void) {
    Options_Main[OPT_FREEPRACTICE].val = Options_Main[OPT_FREEPRACTICE].val_prev =
        TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_FREE_PRACTICE);
    Options_Main[OPT_SHDL].val = Options_Main[OPT_SHDL].val_prev =
        TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_SHDL);
    Options_Main[OPT_FULL_HOP].val = Options_Main[OPT_FULL_HOP].val_prev =
        TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_FULL_HOP);
    Options_Main[OPT_LINES].val = Options_Main[OPT_LINES].val_prev =
        TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_LINES);
    Options_Main[OPT_DOUBLE_JUMP].val = Options_Main[OPT_DOUBLE_JUMP].val_prev =
        TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_DOUBLE_JUMP);
    Options_Main[OPT_COUNTER].val = Options_Main[OPT_COUNTER].val_prev = TM_GetSetting(TM_SETTING_EGGS_COUNTER, 0);
    best = TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_BEST);
}
// The next targets to appear follow it; the ones out stay.
static void ChangeDoubleLasers(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_SHDL, value);
}
static void ChangeFullHop(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_FULL_HOP, value);
}
// The lines and the T show or go on the next frame drawn, behind the paused menu too.
static void ChangePeakLines(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_LINES, value);
}
static void ChangeDoubleJump(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_DOUBLE_JUMP, value);
}

/* Where the count goes for each Counter Position: Eggs-ercise's spots, with
 * the label COUNTER_LABEL_Y under the count. */
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
// Everything this event keeps is saved, so a restart keeps it.
static void StageSwap_BeforeRestart(void) { }
static void FighterSwap_BeforeRestart(void) { }

/* The saved choice is the mode: Event_Init enters it as every match starts,
 * so a change restarts the match, which gives a timed run its countdown back
 * or takes it away. */
static void ChangeFreePractice(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_FREE_PRACTICE, value);
    Retry(menu);
}
static void EnterFreePractice(void) {
    reset_clock = true;
    stc_match->match.timer = MATCH_TIMER_COUNTUP;
    GateLaserRows();
}
static void ResetSettings(GOBJ *menu) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_FREE_PRACTICE, 0);
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_SHDL, 0);
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_FULL_HOP, TM_TARGETS_FH_OFF);
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_LINES, 0);
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_DOUBLE_JUMP, 0);
    LoadSettings();
    Retry(menu);
}

// Spots -----------------------------------------------------------------------

static float RandomRange(float low, float high) { return low + (high - low) * HSD_Randf(); }
static float DistanceSquared(Vec2 a, Vec2 b) {
    float x = a.X - b.X, y = a.Y - b.Y;
    return x * x + y * y;
}
// Far enough from you and the last targets, unless the tries are running out.
static bool Clear(Vec2 at, Vec2 you, int tries) {
    if (tries >= SPAWN_TRIES - 5) return true;
    return DistanceSquared(at, you) >= SPAWN_FROM_YOU * SPAWN_FROM_YOU &&
        (!placed || DistanceSquared(at, last_spot) >= SPAWN_FROM_LAST * SPAWN_FROM_LAST);
}
// The highest floor on the stage, platforms included.
static float HighestFloor(void) {
    float top = 0;
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        if (!FloorLine(i, &a, &b, &platform)) continue;
        float y = a.Y > b.Y ? a.Y : b.Y;
        if (!found || y > top) top = y;
        found = 1;
    }
    return top;
}
/* A spot over the main stage, between a low laser from the ground under it and
 * a laser at the peak of a full hop and a double jump off the highest floor. */
static void PickSingle(FighterData *ft, Vec2 you, Vec2 *spot) {
    const Blaster *blaster = BlasterOf(ft);
    float left, right, top = HighestFloor() + FullHopDoubleJump(ft) + blaster->air;
    GroundEdges(&left, &right);
    for (int tries = 0; tries < SPAWN_TRIES; ++tries) {
        Vec3 ground;
        int line;
        float x = RandomRange(left + SPAWN_INSET, right - SPAWN_INSET);
        if (!FindSolidGround(x, &ground, &line)) continue;
        float low = ground.Y + LowestLaser(blaster);
        Vec2 at = {x, RandomRange(low, top > low ? top : low)};
        if (!Clear(at, you, tries)) continue;
        *spot = at;
        return;
    }
    Vec3 ground;
    int line;
    GroundPoint(0, &ground, &line);
    *spot = (Vec2){0, ground.Y + LowestLaser(blaster)};
}
/* A spot on a floor, at least SPAWN_INSET from its ends: the main floor for
 * line -1, else that platform's line. */
static bool FloorSpot(int line, Vec2 *at) {
    float left, right;
    if (line < 0) {
        GroundEdges(&left, &right);
    } else {
        Vec2 a, b;
        int platform;
        if (!FloorLine(line, &a, &b, &platform)) return false;
        left = a.X;
        right = b.X;
    }
    at->X = RandomRange(left + SPAWN_INSET, right - SPAWN_INSET);
    if (line >= 0) return LineY(line, at->X, &at->Y);
    Vec3 ground;
    if (!FindSolidGround(at->X, &ground, &line)) return false;
    at->Y = ground.Y;
    return true;
}
/* Targets stacked at one x at the `count` heights of one multi laser off a
 * floor, over that floor. The floor is the main floor, a side platform (left
 * or right) or the top platform, each kind as likely as the others where the
 * stage has one that holds still: Fountain's side platforms move, so groups
 * leave them out, Stadium has no top platform and FD no platforms. The kind
 * holds for half the tries, so it stays even wherever you stand, and then
 * gives way when no spot on it is far enough from you (you're on the top
 * platform, say). */
static void PickGroup(Vec2 you, const float *heights, int count) {
    int sides[2], side_count = 0, top_line, line;
    Vec3 center;
    if (Stage_GetExternalID() != GRKINDEXT_IZUMI)
        for (int side = -1; side <= 1; side += 2)
            if (FindPlatform(side, &center, &line)) sides[side_count++] = line;
    if (!FindPlatform(0, &center, &top_line)) top_line = -1;
    // Kind 0 is the main floor, then the side platforms if any, then the top.
    int kinds = 1 + (side_count > 0) + (top_line >= 0), kind = HSD_Randi(kinds);
    for (int tries = 0; tries < SPAWN_TRIES; ++tries) {
        if (tries >= SPAWN_TRIES / 2) kind = HSD_Randi(kinds);
        line = !kind ? -1 : kind == 1 && side_count ? sides[HSD_Randi(side_count)] : top_line;
        Vec2 spot;
        bool clear = true;
        if (!FloorSpot(line, &spot)) continue;
        for (int i = 0; i < count && clear; ++i)
            clear = Clear((Vec2){spot.X, spot.Y + heights[i]}, you, tries);
        if (!clear) continue;
        for (int i = 0; i < count; ++i) targets[i].pos = (Vec2){spot.X, spot.Y + heights[i]};
        return;
    }
    Vec3 ground;
    GroundPoint(0, &ground, &line);
    for (int i = 0; i < count; ++i) targets[i].pos = (Vec2){0, ground.Y + heights[i]};
}

static void AimCamera(void) {
    float bottom = 0, top = 0;
    int found = 0;
    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (!targets[i].live) continue;
        if (!found || targets[i].pos.Y < bottom) bottom = targets[i].pos.Y;
        if (!found || targets[i].pos.Y > top) top = targets[i].pos.Y;
        cam->cam_pos.X = targets[i].pos.X;
        found = 1;
    }
    if (!found) return;
    float half = (top - bottom) * .5f;
    cam->cam_pos.Y = bottom + half;
    cam->cam_pos.Z = 0;
    cam->boundleft_proj = -CAMERA_MARGIN;
    cam->boundright_proj = CAMERA_MARGIN;
    cam->boundtop_proj = half + CAMERA_MARGIN;
    cam->boundbottom_proj = -half - CAMERA_MARGIN;
}

static bool AnyLive(void) {
    for (int i = 0; i < MAX_TARGETS; ++i)
        if (targets[i].live) return true;
    return false;
}
/* Each spawn is a single, a short hop pair or a full hop combo, evenly among
 * those that are on. One FH combo picked (not All Combos) drops the singles,
 * so it comes every spawn but the pairs'. */
enum { SPAWN_SINGLE, SPAWN_PAIR, SPAWN_FULL_HOP };
static void Spawn(FighterData *ft) {
    Vec2 you = {ft->phys.pos.X, ft->phys.pos.Y};
    int kinds[3], count = 0, shots = 1;
    bool full_hop = FullHopCombo(false) >= 0;
    if (!full_hop || Options_Main[OPT_FULL_HOP].val == TM_TARGETS_FH_ALL) kinds[count++] = SPAWN_SINGLE;
    if (PairsOn()) kinds[count++] = SPAWN_PAIR;
    if (full_hop) kinds[count++] = SPAWN_FULL_HOP;
    int kind = count > 1 ? kinds[HSD_Randi(count)] : kinds[0];
    for (int i = 0; i < MAX_TARGETS; ++i) targets[i].live = false;
    if (kind == SPAWN_PAIR) {
        float pair[2];
        ShortHopShots(ft, BlasterOf(ft), 1 + HSD_Randi(pair_presses), &pair[0], &pair[1]);
        PickGroup(you, pair, shots = 2);
    } else if (kind == SPAWN_FULL_HOP) {
        struct full_hop *combo = &full_hops[FullHopCombo(true)];
        PickGroup(you, combo->heights, shots = combo->shots);
    } else {
        PickSingle(ft, you, &targets[0].pos);
    }
    for (int i = 0; i < shots; ++i) targets[i].live = true;
    last_spot = targets[0].pos;
    placed = true;
    AimCamera();
}

// Lasers ----------------------------------------------------------------------

static bool IsLaser(ItemData *item) { return item->kind == ITEM_FOXLASER || item->kind == ITEM_FALCOLASER; }

// The squared distance from p to the segment from a to b, in the stage's plane.
static float SegmentDistanceSquared(Vec3 a, Vec3 b, Vec2 p) {
    float dx = b.X - a.X, dy = b.Y - a.Y, length = dx * dx + dy * dy, t = 0;
    if (length > 0) {
        t = ((p.X - a.X) * dx + (p.Y - a.Y) * dy) / length;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
    }
    float x = a.X + dx * t - p.X, y = a.Y + dy * t - p.Y;
    return x * x + y * y;
}

static void Break(int i) {
    targets[i].live = false;
    pops[i].pos = targets[i].pos;
    pops[i].age = 1;
    ++score;
    SFX_PlayRaw(303, 255, 128, 0, 3); // Laser Land Training's success sound.
}

/* Your lasers whose hitboxes swept through a target this frame break it and
 * stop there. A laser breaks one target at most. */
static void ShootTargets(GOBJ *you) {
    for (GOBJ *item = (*stc_gobj_lookup)[MATCHPLINK_ITEM]; item; item = item->next) {
        ItemData *laser = item->userdata;
        if (!laser || !IsLaser(laser) || laser->fighter_gobj != you) continue;
        int hit = -1;
        for (int h = 0; h < 4 && hit < 0; ++h) {
            itHit *box = &laser->hitbox[h];
            if (box->active < 2) continue; // Off, or not placed yet.
            float reach = TARGET_RADIUS + box->size;
            for (int i = 0; i < MAX_TARGETS && hit < 0; ++i)
                if (targets[i].live && SegmentDistanceSquared(box->pos_prev, box->pos, targets[i].pos) <= reach * reach)
                    hit = i;
        }
        if (hit < 0) continue;
        Break(hit);
        Item_RemoveAllHitboxes(item);
        Item_SetLifeTimer(item, 1);
    }
}

// Drawing ---------------------------------------------------------------------

static const GXColor TargetRed = {230, 32, 32, 255}, TargetWhite = {255, 255, 255, 255},
    TargetEdge = {0, 0, 0, 255};

static void Disc(Vec2 center, float radius, GXColor color) {
    event_vars->GFX_Start(CIRCLE_STEPS + 2, (GFX_Params){.shape = GX_TRIANGLEFAN, .no_depth = true});
    GFX_AddVtx(center.X, center.Y, 0, color);
    for (int i = 0; i <= CIRCLE_STEPS; ++i) {
        float angle = i * (2 * M_PI / CIRCLE_STEPS);
        GFX_AddVtx(center.X + cos(angle) * radius, center.Y + sin(angle) * radius, 0, color);
    }
}
static void Circle(Vec2 center, float radius, GXColor color, u8 width) {
    event_vars->GFX_Start(CIRCLE_STEPS + 1, (GFX_Params){.shape = GX_LINESTRIP, .size = width, .no_depth = true});
    for (int i = 0; i <= CIRCLE_STEPS; ++i) {
        float angle = i * (2 * M_PI / CIRCLE_STEPS);
        GFX_AddVtx(center.X + cos(angle) * radius, center.Y + sin(angle) * radius, 0, color);
    }
}
/* A bullseye the size of the target, with four ticks around it so it's easy to
 * spot zoomed out. With no depth test the stage never hides it. */
static void DrawTarget(Vec2 c) {
    static const float ticks[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    event_vars->GFX_Start(16, (GFX_Params){.shape = GX_LINES, .size = 18, .no_depth = true});
    for (int i = 0; i < 4; ++i)
        for (int end = 0; end < 2; ++end) {
            float r = TARGET_RADIUS + (end ? 3.5f : 1.f);
            GFX_AddVtx(c.X + ticks[i][0] * r, c.Y + ticks[i][1] * r, 0, TargetEdge);
        }
    for (int i = 0; i < 4; ++i)
        for (int end = 0; end < 2; ++end) {
            float r = TARGET_RADIUS + (end ? 3.2f : 1.3f);
            GFX_AddVtx(c.X + ticks[i][0] * r, c.Y + ticks[i][1] * r, 0, TargetWhite);
        }
    Disc(c, TARGET_RADIUS + .25f, TargetEdge);
    Disc(c, TARGET_RADIUS, TargetRed);
    Disc(c, TARGET_RADIUS * 2 / 3, TargetWhite);
    Disc(c, TARGET_RADIUS / 3, TargetRed);
}
// A white ring that grows and fades where a target broke.
static void DrawPop(struct pop *pop) {
    float t = pop->age / (float)POP_FRAMES;
    u8 alpha = 255 * (1 - t);
    GXColor color = {alpha, alpha, alpha, alpha}; // Premultiplied.
    Circle(pop->pos, TARGET_RADIUS + 6 * t, color, 18);
}
/* Training Mode's Purple (A064FF) and Teal (00B4A0) at alpha 230, premultiplied. */
static const GXColor PeakColors[2] = {{0, 162, 144, 230}, {144, 90, 230, 230}}; // Short hop, full hop.

/* Each floor's line, from left to right x at its height: the main floor at the
 * middle, then each platform FindPlatform finds (left, top, right) above it,
 * a side one only on its side of x = 0. */
typedef struct PeakFloor { float left, right, y; } PeakFloor;
static int PeakFloors(PeakFloor *floors) {
    Vec3 ground, at;
    int line, count = 0;
    if (!FindSolidGround(0, &ground, &line)) return 0;
    float left = Stage_GetBlastzoneLeft(), right = Stage_GetBlastzoneRight();
    floors[count++] = (PeakFloor){left, right, ground.Y};
    for (int side = -1; side <= 1; ++side)
        if (FindPlatform(side, &at, &line) && at.Y >= ground.Y + SUNK_PLATFORM)
            floors[count++] = (PeakFloor){side > 0 ? 0 : left, side < 0 ? 0 : right, at.Y};
    return count;
}
static void DrawPeakLines(void) {
    PeakFloor floors[PEAK_FLOORS];
    int count = PeakFloors(floors);
    if (!count) return;
    event_vars->GFX_Start(count * 4, (GFX_Params){.shape = GX_LINES, .size = PEAK_LINE_WIDTH, .no_depth = true});
    for (int i = 0; i < count; ++i)
        for (int hop = 0; hop < 2; ++hop) {
            float y = floors[i].y + peak_lasers[hop];
            GFX_AddVtx(floors[i].left, y, 0, PeakColors[hop]);
            GFX_AddVtx(floors[i].right, y, 0, PeakColors[hop]);
        }
}
/* Double Jump Height's white T and red F arm, at alpha 230, premultiplied as
 * the peak lines are. */
static const GXColor DoubleJumpWhite = {230, 230, 230, 230}, DoubleJumpRed = {230, 30, 30, 230};

/* In the air with the double jump left, and able to use it later: not
 * helpless, air dodging (it ends helpless), on the ledge, dead or on the
 * revival platform, or in side-B or up-B (see FX_SPECIAL_S_FIRST). */
static bool DoubleJumpReady(FighterData *ft) {
    int state = ft->state_id;
    if (!ft->phys.air_state || ft->jump.jumps_used >= ft->attr.max_jumps) return false;
    if (state <= ASID_REBIRTHWAIT || (state >= ASID_FALLSPECIAL && state <= ASID_FALLSPECIALB) ||
        state == ASID_ESCAPEAIR || (state >= ASID_CLIFFCATCH && state <= ASID_CLIFFESCAPEQUICK))
        return false;
    return state < FX_SPECIAL_S_FIRST || state > FX_SPECIAL_HI_LAST;
}
// The stem's x: the head's (HeadN), or TopN's if there's no such bone.
static float HeadX(FighterData *ft) {
    int bone = Fighter_BoneLookup(ft, HeadN);
    Vec3 head;
    if (bone < 0 || !ft->bones || !ft->bones[bone].joint) return ft->phys.pos.X;
    JOBJ_GetWorldPosition(ft->bones[bone].joint, 0, &head);
    return head.X;
}
/* A dashed white stem from the feet up through the head to a double jump's
 * rise, a white bar there, then red on up to the aerial laser's height above
 * the bar and an arm out the way the fighter faces. One GX_LINES batch. */
static void DrawDoubleJump(FighterData *ft) {
    float x = HeadX(ft), feet = ft->phys.pos.Y, top = feet + double_jump, period = DJ_DASH + DJ_GAP;
    float laser = top + BlasterOf(ft)->air, arm = x + (ft->facing_direction < 0 ? -DJ_ARM : DJ_ARM);
    int dashes = 0;
    while (dashes * period < double_jump) ++dashes;
    event_vars->GFX_Start(dashes * 2 + 6, (GFX_Params){.shape = GX_LINES, .size = PEAK_LINE_WIDTH, .no_depth = true});
    for (int i = 0; i < dashes; ++i) {
        float end = top - i * period, start = end - DJ_DASH;
        GFX_AddVtx(x, start > feet ? start : feet, 0, DoubleJumpWhite);
        GFX_AddVtx(x, end, 0, DoubleJumpWhite);
    }
    GFX_AddVtx(x - DJ_BAR, top, 0, DoubleJumpWhite);
    GFX_AddVtx(x + DJ_BAR, top, 0, DoubleJumpWhite);
    GFX_AddVtx(x, top, 0, DoubleJumpRed);
    GFX_AddVtx(x, laser, 0, DoubleJumpRed);
    GFX_AddVtx(x, laser, 0, DoubleJumpRed);
    GFX_AddVtx(arm, laser, 0, DoubleJumpRed);
}

static void Targets_GX(GOBJ *gobj, int pass) {
    if (pass != 2) return;
    // Under the targets. The heights are the fighter's, known from its first frame.
    if (checked && Options_Main[OPT_LINES].val) DrawPeakLines();
    GOBJ *you = Fighter_GetGObj(0);
    if (checked && Options_Main[OPT_DOUBLE_JUMP].val && you && DoubleJumpReady(you->userdata))
        DrawDoubleJump(you->userdata);
    for (int i = 0; i < MAX_TARGETS; ++i)
        if (pops[i].age > 0) DrawPop(&pops[i]);
    for (int i = 0; i < MAX_TARGETS; ++i)
        if (targets[i].live) DrawTarget(targets[i].pos);
}

// Event -----------------------------------------------------------------------

/* Pairs, the peak lines and the double jump's T depend on the fighter, which
 * isn't known until its first frame. */
static void CheckFighter(FighterData *ft) {
    const Blaster *blaster = BlasterOf(ft);
    checked = true;
    pair_presses = ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO ? ShortHopPresses(ft, blaster) : 0;
    if (ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO) FullHopCombos(ft, blaster);
    peak_lasers[0] = Rise(ft->attr.hop_v_initial_velocity, ft->attr.gravity) + blaster->air;
    peak_lasers[1] = Rise(ft->attr.jump_v_initial_velocity, ft->attr.gravity) + blaster->air;
    double_jump = DoubleJumpRise(ft);
    GateLaserRows();
}
static void FinishTimedRun(void) {
    Match_SetEndGraphic(MATCHENDKIND_VSGAME);
    if (score > best) {
        MatchInfo_0x0010_store(0x9c40); // The "new record" voice, as Eggs-ercise does.
        Match_SetPostMatchSFX(325);
        TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_BEST, score < TM_TARGETS_BEST_MAX ? score : TM_TARGETS_BEST_MAX);
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
    Text_SetText(hud_score_text, 0, "%d", score);
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
static Text *CreateText(int canvas, float scale) {
    Text *text = Text_CreateText(2, canvas);
    text->kerning = 1;
    text->use_aspect = 1;
    text->scale.X = text->scale.Y = scale;
    text->viewport_scale.X = text->viewport_scale.Y = 1;
    Text_AddSubtext(text, 0, 0, "-");
    return text;
}

void Event_Init(GOBJ *gobj) {
    for (int i = 0; i < MAX_TARGETS; ++i) {
        targets[i].live = false;
        pops[i].age = 0;
    }
    placed = checked = reset_clock = restarting = false;
    pair_presses = score = 0;
    for (int i = 0; i < TM_TARGETS_FH_COUNT; ++i) full_hops[i].shots = 0;
    LoadSettings();
    // A retry may reuse the module: rebuild the gating from the saved mode.
    Options_Main[OPT_SHDL].disable = Options_Main[OPT_FULL_HOP].disable = 1;
    Options_Main[OPT_SHDL].name = DoubleLasersName;
    Options_Main[OPT_FULL_HOP].name = FullHopName;
    if (Options_Main[OPT_FREEPRACTICE].val) EnterFreePractice();
    StageSwap_Init();
    // No CPU: slot 1 is empty, so FighterSwap_Init greys out its row.
    FighterSwap_Restrict(0, Character_Kinds, countof(Character_Kinds));
    FighterSwap_Init();
    FighterSwap_Options[0].desc[2] = "Left/Right: Fox or Falco. X/Y: costume color. A: restart the match.";
    cam = CameraSubject_Alloc();
    cam->boundleft_proj = cam->boundbottom_proj = -CAMERA_MARGIN;
    cam->boundright_proj = cam->boundtop_proj = CAMERA_MARGIN;
    int canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 81, 19);
    hud_score_text = CreateText(canvas, 1);
    hud_best_text = CreateText(canvas, .35f);
    PlaceCounter(Options_Main[OPT_COUNTER].val);
    GObj_AddGXLink(GObj_Create(0, 7, 0), Targets_GX, MATCHGX_EFFECT, 255);
    stc_match->end_kind = MATCHENDKIND_NONE;
}
void Event_Think(GOBJ *event) {
    // Only the six legal stages: the stage select's others restart on Battlefield.
    if (restarting) return;
    if (!LegalStage()) {
        restarting = true;
        event_vars->restart_on_stage(GRKINDEXT_BATTLE);
        return;
    }
    // The match starts with the timed minute loaded; Free Practice counts up from 0:00.
    if (reset_clock) {
        stc_match->time_seconds = 0;
        stc_match->time_ms = 0;
        reset_clock = false;
    }
    GOBJ *you = Fighter_GetGObj(0);
    if (you && Pause_CheckStatus(1) != 2) {
        FighterData *ft = you->userdata;
        if (!checked) CheckFighter(ft);
        for (int i = 0; i < MAX_TARGETS; ++i)
            if (pops[i].age > 0 && ++pops[i].age > POP_FRAMES) pops[i].age = 0;
        // The first targets wait for you to have control: Go, in a timed run.
        if (placed) ShootTargets(you);
        if (ft->flags.input_enable && (!placed || !AnyLive())) Spawn(ft);
        AimCamera();
    }
    UpdateHUD();
    if (stc_match->time_frames == TIMED_FRAMES && !FreePractice())
        FinishTimedRun();
}
