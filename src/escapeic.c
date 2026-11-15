#include "events.h"

/* Escape IC Grab (Character-specific Tech, beta): the Ice Climbers grab you in
 * the middle of Final Destination and you mash out as fast as you can. The
 * counter shows the mash inputs the game counted, and each percent keeps its
 * fastest escape for the session. This beta has no handoff: Popo just holds
 * you. notes/ESCAPE-EVENTS-RESEARCH.md has the handoffs a later version could
 * do, and how the numbers below were read from the DOL.
 *
 * How long a grab holds. When it connects (800DA8E4) the game sets the grabbed
 * fighter's timer (8007DBCC) from PlCo.dat's ftLoadCommonData:
 *     30 + 8 x (9 - handicap) + 15 x (3 - placing) + 1.6 x percent
 * with placing 0 for first. This event holds every grab as if the match were
 * tied with handicap off, 75 + 1.6 x percent frames without mashing (0%: 75,
 * 100%: 235, 200%: 395), the way Grab Punish Practice does.
 *
 * Mashing. CaptureWait's animation callback (800DB908) takes PlCo 0x3A4 (1)
 * off the timer a frame, and Fighter_CheckGrabBreakout (8007DC08) takes PlCo
 * 0x3A8 (6) more for a fresh press in input.down & 0x80000F00 (A, B, X, Y, and
 * the shield bit that the input build at 8006B070 sets for L, R or Z; Z also
 * sets A), and 6 more when the control stick crosses into a new direction
 * (past PlCo 0x308 on either axis; the grab starts each axis at neither), at
 * most one of each a frame: 13 a frame at most. It never reads the C-stick or
 * D-pad. CaptureWait lets go at 0. Nothing ticks while Popo pulls you in:
 * CapturePulled's callbacks are empty. His CatchPull (800D9E80) puts you in
 * CaptureWait when his Catch script's 0x50 command, on frame 8, sets the flag
 * it waits for, a frame or two after his grab connects. A pummel's flinch,
 * CaptureDamage, ticks and counts like CaptureWait (800DB8A4) once its hitlag
 * is over: hitlag skips every animation callback (8006AA64). So the timer
 * drops by 1 + 6 x the frame's inputs, and the counter reads that drop, from
 * the first frame it drops. It reads after the frame's hits (s_link 15), so it
 * sees the timer the grab set before the first frame's mash.
 *
 * The release. When CaptureWait lets go (800DB974), you're released into the
 * air, CaptureJump (800DC070), if you pressed X or Y while its frame count
 * (state_var1, zeroed when the grab connects and counted by CaptureWait and
 * CaptureDamage) was under PlCo 0x3AC (16): CaptureWait's interrupt check
 * (800DC014) sets a flag for it. Or if the control stick is up at least PlCo
 * 0x70 (0.6625) as you get out (800DC044). Otherwise it's on the ground,
 * CaptureCut (800DC750). CaptureCut slides you back and has no interrupt: you
 * can act when its animation ends (30 frames, DK's 20). CaptureJump pops you up
 * and back (PlCo 0x374, 0x378: 1 and 2) with drift, takes Fall's aerial
 * actions from its 30th frame (PlCo 0x3B8) and falls at its end (50). The
 * Actionable Flash shows that end with the global Actionable Yellow>Green cue:
 * event_vars->cue_remaining is the same check, and action_cues.c (compiled
 * into this event) the same yellow and green. */

#define SETUP_FRAMES 40   // A grab that hasn't connected by then is tried again.
#define RESULT_FRAMES 60  // The result stays up this long before the next grab.
#define NANA_GAP 6        // Nana stands this far behind Popo.
#define MAX_INPUTS 2      // A button and a stick direction a frame.

// The big mash count under the match timer, where Grab Punish draws its countdown.
#define BIG_X -5.f
#define BIG_Y 12.9f
#define BIG_WIDTH 10.f
#define BIG_HEIGHT 2.5f
#define BIG_SIZE 1.f
#define BIG_BORDER 2.f

// Popo-to-you distance (center to center) each time the grab is tried.
static const float Grab_Gaps[] = { 8, 5, 11, 14 };

static const char *Values_Percent[] = {
    "0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%",
    "110%", "120%", "130%", "140%", "150%", "160%", "170%", "180%", "190%", "200%",
};
// Handoffs to come; see the research notes. The row stays disabled until then.
static const char *Values_Handoff[] = { "None" };

enum menu_options { OPT_PERCENT, OPT_FLASH, OPT_HANDOFF, OPT_BESTS, OPT_HELP };
// Each TM_SETTING_ESCAPE_IC row's preference ID is its row number. The
// Actionable Flash is saved on its own (TM_SETTING_ESCAPE_IC_FLASH).
typedef char escape_ic_saved_rows[((int)OPT_PERCENT == TM_ESC_IC_PERCENT && TM_ESC_IC_PREF_COUNT == 1 &&
    countof(Values_Percent) == TM_ESC_PERCENT_COUNT) ? 1 : -1];

static void Exit(GOBJ *menu);
static void SaveSettings(GOBJ *menu, int value);
static void SaveFlash(GOBJ *menu, int value);

// Session Bests: one row a percent. Its value is the fastest escape and its inputs.
static char best_text[TM_ESC_PERCENT_COUNT][16] = {
    "-", "-", "-", "-", "-", "-", "-", "-", "-", "-", "-",
    "-", "-", "-", "-", "-", "-", "-", "-", "-", "-",
};
#define BEST_ROW(i, label) { .kind = OPTKIND_INFO, .name = label, .value_string = best_text[i], \
    .desc = {"Your fastest escape at this percent this", "session, in frames, with its mash inputs.", \
             "Kept until you leave the event."} }
static EventOption Options_Bests[] = {
    BEST_ROW(0, "0%"), BEST_ROW(1, "10%"), BEST_ROW(2, "20%"), BEST_ROW(3, "30%"), BEST_ROW(4, "40%"),
    BEST_ROW(5, "50%"), BEST_ROW(6, "60%"), BEST_ROW(7, "70%"), BEST_ROW(8, "80%"), BEST_ROW(9, "90%"),
    BEST_ROW(10, "100%"), BEST_ROW(11, "110%"), BEST_ROW(12, "120%"), BEST_ROW(13, "130%"),
    BEST_ROW(14, "140%"), BEST_ROW(15, "150%"), BEST_ROW(16, "160%"), BEST_ROW(17, "170%"),
    BEST_ROW(18, "180%"), BEST_ROW(19, "190%"), BEST_ROW(20, "200%"),
};
typedef char escape_ic_best_rows[(countof(Options_Bests) == TM_ESC_PERCENT_COUNT) ? 1 : -1];
static EventMenu Menu_Bests = {
    .name = "Session Bests",
    .option_num = countof(Options_Bests),
    .options = Options_Bests,
};

/* Help: how the escape works and how to mash faster. Each row is checked against
 * the game: 8007DC08 takes a fresh press in input.down & 0x80000F00, and input.down
 * is held & ~held_prev (8006B0E0); bit 31 is set for digital L/R, analog past PlCo
 * 0x10 (0.3), and Z, which also sets A (8006B070..8006B0D8). The stick's sticky
 * direction flips past PlCo 0x308 (0.5) on either axis. See the research notes. */
#define HELP_ROW(label, line1, line2, line3, line4) \
    { .kind = OPTKIND_INFO, .name = label, .desc = {line1, line2, line3, line4} }
static EventOption Options_Help[] = {
    HELP_ROW("About This Event",
             "The Ice Climbers grab you as each attempt",
             "starts: mash out as fast as you can. D-Pad",
             "up/down changes your percent, left/right",
             "restarts. Beta: no handoffs yet."),
    HELP_ROW("How Escaping Works",
             "A grab holds you 75 frames + 1.6 a percent.",
             "It drops 1 a frame on its own, and 6 more for",
             "each input the game counts. A button and a",
             "stick input can both count in one frame."),
    HELP_ROW("Buttons That Count",
             "A, B, X, Y, Z, L and R. Only the press counts:",
             "holding or letting go does nothing, so let go",
             "and press again. A light L or R press past",
             "30% counts too, no click needed."),
    HELP_ROW("One Button a Frame",
             "Only one button counts each frame, so two",
             "pressed together count once. Roll your",
             "fingers across them, like a drumroll, so",
             "each press lands on its own frame."),
    HELP_ROW("L, R and Z",
             "L and R are one button to the game: pressing",
             "one while holding the other doesn't count.",
             "Z is A and a shield press at once, so with Z",
             "held, A, L and R presses don't count."),
    HELP_ROW("The Control Stick",
             "Pushing past halfway the other way counts:",
             "left, then right, then left. The game keeps",
             "left/right and up/down apart and counts a",
             "flip on either, one stick input a frame."),
    HELP_ROW("Neutral Doesn't Help",
             "The game remembers your last side even with",
             "the stick back in the middle, so right,",
             "middle, right counts once. Push the other",
             "way instead of tapping the same one."),
    HELP_ROW("Stick Patterns",
             "Waggling side to side counts every push.",
             "Circles count every quarter turn. Moving",
             "between neighbors (right, up, right, up)",
             "stops counting: neither side flips."),
    HELP_ROW("Use Both at Once",
             "Buttons and the stick count separately, so",
             "mash buttons while you waggle the stick.",
             "20 inputs a second gets you out at 100% in",
             "79 frames instead of 235."),
    HELP_ROW("Start Right Away",
             "Mashing counts once Popo has pulled you in,",
             "a frame or two after his grab connects. A",
             "stick held past halfway counts right then.",
             "A held button needs a fresh press."),
    HELP_ROW("What Doesn't Count",
             "The C-stick, D-pad and Start. Holding or",
             "letting go of a button. The stick under",
             "halfway, back in the middle, or pushed the",
             "same way again. L or R under 30%."),
    HELP_ROW("Ground or Air Release",
             "On the ground you slide back and can act 30",
             "frames later (DK: 20). In the air you pop up",
             "and back, drifting, and 30 frames later can",
             "do anything you could while falling."),
    HELP_ROW("What Pops You Up",
             "You're released into the air if you pressed",
             "X or Y in your first 16 frames held, or the",
             "stick is two-thirds up as you get out.",
             "Anything else gets a ground release."),
    HELP_ROW("Choosing Your Release",
             "For the ground, mash A, B, L, R or Z for the",
             "first 16 frames, not X or Y, and don't end",
             "with the stick up. For the air, hold up as",
             "you get out, or tap X or Y right away."),
    HELP_ROW("Waggling Up and Down",
             "Only where the stick is on the frame you get",
             "out counts, so waggling up and down can give",
             "either release. Waggle left and right to",
             "stay on the ground."),
    HELP_ROW("Actionable Flash",
             "With it on, you flash yellow on the last two",
             "frames of your release and green when you",
             "can act. It's the Actionable Yellow>Green",
             "cue, shown for you after a mash out only."),
    HELP_ROW("Pummels",
             "In a match, a pummel's hitlag freezes you:",
             "the grab timer stops and mashing doesn't",
             "count. The rest of the flinch counts. Not in",
             "this beta: Popo just holds you."),
    HELP_ROW("Reading the HUD",
             "Mash is the inputs the game took this grab.",
             "Held For against No-Mash Hold is what your",
             "mashing saved. Mash Speed is inputs a",
             "second: 120 (2 a frame) is the most."),
};
static EventMenu Menu_Help = {
    .name = "Help",
    .option_num = countof(Options_Help),
    .options = Options_Help,
};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_STRING,
        .name = "Your Percent",
        .values = Values_Percent,
        .value_num = countof(Values_Percent),
        .OnChange = SaveSettings,
        .desc = {"Your percent when grabbed. The grab holds",
                 "75 + 1.6 frames per percent before mashing",
                 "(0%: 75f, 100%: 235f, 200%: 395f).",
                 "D-Pad up/down changes it and restarts."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Actionable Flash",
        .val = 1,
        .OnChange = SaveFlash,
        .desc = {"Flash yellow, then green, as your grab",
                 "release ends, so you know when you can move.",
                 "The Actionable Yellow>Green cue, after a mash",
                 "out only. Defaults On. This setting is saved."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Handoff (Coming Soon)",
        .values = Values_Handoff,
        .value_num = countof(Values_Handoff),
        .disable = 1,
        .desc = {"Have the Ice Climbers throw and regrab you,",
                 "so you DI the handoff instead of only mashing.",
                 "Not in this beta: Popo just holds you."},
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Session Bests",
        .menu = &Menu_Bests,
        .desc = {"Your fastest escape at each percent,",
                 "kept until you leave the event."},
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Help",
        .menu = &Menu_Help,
        .desc = {"How this event works and how to mash out",
                 "faster: what the game counts as a mash input",
                 "and what it doesn't."},
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
    .name = "Escape IC Grab",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};
EventMenu *Event_Menu = &Menu_Main;

// The menu is saved to the memory card as it changes.
static void SaveSettings(GOBJ *menu, int value) {
    for (int i = 0; i < TM_ESC_IC_PREF_COUNT; i++)
        TM_SetSetting(TM_SETTING_ESCAPE_IC, i, Options_Main[i].val);
}
static void SaveFlash(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_ESCAPE_IC_FLASH, 0, Options_Main[OPT_FLASH].val);
}
static void LoadSettings(void) {
    for (int i = 0; i < TM_ESC_IC_PREF_COUNT; i++)
        Options_Main[i].val = Options_Main[i].val_prev = TM_GetSetting(TM_SETTING_ESCAPE_IC, i);
    Options_Main[OPT_FLASH].val = Options_Main[OPT_FLASH].val_prev = !!TM_GetSetting(TM_SETTING_ESCAPE_IC_FLASH, 0);
}

enum phase {
    PHASE_SETUP,  // Popo is grabbing you.
    PHASE_GRAB,   // You're held and mashing.
    PHASE_RESULT, // Out; the next grab follows.
};
static int phase = PHASE_SETUP, phase_timer;
static bool ready, restart;

// This attempt. A D-pad restart starts a new one without counting it.
static struct attempt {
    int percent;  // Its Your Percent choice.
    int side;     // Popo's side of you: -1 or 1.
    int tries;    // Grabs tried; picks the gap.
    Vec3 you;
} attempt;

// This grab, kept on screen until the next one connects. (FighterData's own
// nested struct already has the tag grab.)
static struct held_grab {
    int frames;     // Frames the timer ran, the frame you got out included.
    int inputs;     // Mash inputs the game counted.
    float timer;    // The grab timer after last frame.
    bool ticking;   // Popo has pulled you in, so the timer runs.
    bool done, new_best;
} grab;

// The Actionable Flash, from your release until its green is over.
static struct flash {
    bool armed, green;
    TMCueState cue;
} flash;

// This session's fastest escape at each percent; 0 frames is none yet.
static struct best { int frames, inputs; } bests[TM_ESC_PERCENT_COUNT];

static FighterData *You(void) { return Fighter_GetGObj(0)->userdata; }
static FighterData *Popo(void) { return Fighter_GetGObj(1)->userdata; }
static GOBJ *Nana(void) {
    GOBJ *popo = Fighter_GetGObj(1);
    if (!popo || !((FighterData *)popo->userdata)->flags.has_follower) return 0;
    return Fighter_GetSubcharGObj(1, 1);
}

/* Frames the grab holds without mashing, tied (first of four places) with
 * handicap off. Every one is a whole number of frames: 75 + 16 a step. */
static int NoMashHold(int percent) {
    ftCommonData *common = *stc_ftcommon;
    float hold = common->grab_mash_min + common->grab_placing_mult * (common->grab_placing_max - 1)
        + common->grab_mash_mult * (percent * 10);
    int whole = (int)hold;
    return hold - whole > .001f ? whole + 1 : whole;
}

// Fighter helpers -------------------------------------------------------

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
static void ClearVelocity(FighterData *ft) {
    ft->phys.self_vel = ft->phys.kb_vel = ft->phys.self_vel_ground = (Vec3){0};
    ft->phys.atk_shield_kb_vel = (Vec3){0};
    ft->phys.nudge_vel = (Vec2){0};
}
static void FloorUnder(float x, Vec3 *ground, int *line) {
    Vec3 slope;
    int kind;
    *ground = (Vec3){x, 0, 0};
    *line = -1;
    if (!GrColl_RaycastGround(ground, line, &kind, &slope, -1, -1, -1, 0, x, 10, x, -30, 0)) {
        *ground = (Vec3){x, 0, 0};
        *line = -1;
    }
}
static void StandAt(GOBJ *fighter, float x, int facing) {
    if (!fighter) return;
    FighterData *ft = fighter->userdata;
    Vec3 ground;
    int line;
    FloorUnder(x, &ground, &line);
    if (line >= 0) ft->coll_data.ground_index = line;
    ft->phys.pos = ground;
    ClearVelocity(ft);
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(ft);
    ft->facing_direction = ft->facing_direction_prev = facing;
    Fighter_EnterWait(fighter);
    Fighter_UpdateCameraBox(fighter);
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
}
static void SetPercent(GOBJ *fighter, float percent) {
    if (!fighter) return;
    FighterData *ft = fighter->userdata;
    ft->dmg.percent = percent;
    // Pending damage to add next frame, not a copy of the percent.
    ft->dmg.percent_temp = 0;
    Fighter_SetHUDDamage(ft->ply, percent);
}

/* Until the grab connects you can't act: Wait's interrupt callback is cleared
 * every frame, so mashing early can't dodge it, and you're put back if pushed. */
static void HoldYou(void) {
    GOBJ *fighter = Fighter_GetGObj(0);
    FighterData *ft = fighter->userdata;
    if (ft->state_id != ASID_WAIT) return;
    ft->cb.IASA = 0;
    if (fabs(ft->phys.pos.X - attempt.you.X) > .01f || fabs(ft->phys.pos.Y - attempt.you.Y) > .01f) {
        ft->phys.pos = attempt.you;
        ClearVelocity(ft);
        UpdatePosition(fighter);
    }
}
// The Ice Climbers do only what the event presses: no AI, Nana's following included.
static void HoldStill(GOBJ *fighter) {
    if (!fighter) return;
    FighterData *ft = fighter->userdata;
    ft->cpu.ai = 15;
    Fighter_ZeroCPUInputs(ft);
}

static bool IsCaptured(FighterData *ft) {
    return ft->state_id >= ASID_CAPTUREPULLEDHI && ft->state_id <= ASID_CAPTUREDAMAGELW;
}
static bool IsGrabbing(FighterData *ft) {
    return ft->state_id >= ASID_CATCH && ft->state_id <= ASID_CATCHWAIT;
}
static bool IsReleased(FighterData *ft) {
    return ft->state_id == ASID_CAPTURECUT || ft->state_id == ASID_CAPTUREJUMP;
}

/* The Actionable Flash: Actionable Yellow>Green on you, but only for the
 * release after you mash out. It's armed as you get out, and ends once its
 * green is over, or when you leave the release without one (into a state the
 * cue doesn't time, like a jump's squat or an air dodge). */
static void ShowFlash(unsigned color) {
    static const GXColor yellow = TM_CUE_YELLOW_COLOR, green = TM_CUE_GREEN_COLOR;
    event_vars->set_timing_overlay(Fighter_GetGObj(0),
        color == TM_CUE_YELLOW ? &yellow : color == TM_CUE_GREEN ? &green : 0);
}
static void ClearFlash(void) {
    flash = (struct flash) {0};
    ShowFlash(TM_CUE_NONE);
}
static void UpdateFlash(void) {
    if (!flash.armed) return;
    GOBJ *fighter = Fighter_GetGObj(0);
    FighterData *you = fighter->userdata;
    unsigned color = TMCue_Update(&flash.cue, you->state_id, you->state.frame, you->atk_instance,
        event_vars->cue_remaining(fighter), you->flags.hitlag || you->flags.freeze, 1, 0);
    if (color == TM_CUE_GREEN) flash.green = true;
    else if (flash.green || !IsReleased(you)) { ClearFlash(); return; }
    ShowFlash(Options_Main[OPT_FLASH].val ? color : TM_CUE_NONE);
}

// The attempt -------------------------------------------------------------

/* Popo facing you, a gap away that changes with each try, and Nana behind him. */
static void PlaceClimbers(void) {
    float gap = Grab_Gaps[attempt.tries % countof(Grab_Gaps)];
    float x = attempt.you.X + attempt.side * gap;
    StandAt(Fighter_GetGObj(1), x, -attempt.side);
    StandAt(Nana(), x + attempt.side * NANA_GAP, -attempt.side);
    Match_CorrectCamera();
}

static void Reset(void) {
    restart = false;
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    ClearFlash();
    attempt =(struct attempt) { .percent = Options_Main[OPT_PERCENT].val, .side = HSD_Randi(2) ? 1 : -1 };
    int line;
    FloorUnder(0, &attempt.you, &line);
    StandAt(Fighter_GetGObj(0), attempt.you.X, attempt.side);
    PlaceClimbers();
    SetPercent(Fighter_GetGObj(0), attempt.percent * 10);
    SetPercent(Fighter_GetGObj(1), 0);
    SetPercent(Nana(), 0);
    phase = PHASE_SETUP;
    phase_timer = 0;
    // Your input runs after this think, so this frame's interrupt goes too.
    HoldYou();
}

/* As if the match were tied with handicap off (see the top of this file). */
static void StartGrab(FighterData *you) {
    ftCommonData *common = *stc_ftcommon;
    you->grab.grab_timer += common->grab_placing_mult * Fighter_GetCurrentPlacing(you->ply)
        - common->grab_mash_per_handicap * (common->grab_max_handicap - Fighter_GetHandicap(you->ply));
    grab = (struct held_grab) { .timer = you->grab.grab_timer };
    phase = PHASE_GRAB;
    phase_timer = 0;
}
// A frame's drop in the timer is 1 + 6 x its mash inputs.
static void CountInputs(float drop) {
    ftCommonData *common = *stc_ftcommon;
    if (common->grab_mash_per_input <= 0) return;
    int inputs = (int)((drop - common->grab_mash_per_frame) / common->grab_mash_per_input + .5f);
    if (inputs < 0) inputs = 0;
    if (inputs > MAX_INPUTS) inputs = MAX_INPUTS;
    if (grab.inputs < 9999) grab.inputs += inputs;
}
static void Escaped(void) {
    struct best *best = &bests[attempt.percent];
    grab.done = true;
    grab.new_best = !best->frames || grab.frames < best->frames;
    if (grab.new_best) {
        best->frames = grab.frames;
        best->inputs = grab.inputs;
        sprintf(best_text[attempt.percent], "%df (%d)", grab.frames, grab.inputs);
    }
    SFX_Play(grab.new_best ? 303 : 0xAD);
    phase = PHASE_RESULT;
    phase_timer = 0;
    flash = (struct flash) { .armed = true };
}

/* At s_link 15, after the frame's hits: on the frame Popo's grab connects you
 * hold the timer it set. Once he's pulled you in, each frame's animation
 * callback (s_link 1) has taken that frame's mash off it, up to the frame you
 * get out. The pull's frames, before the first drop, aren't counted: the
 * No-Mash Hold is the timer's, and the pull's length depends on his frame. */
static void MeasureGrab(GOBJ *gobj) {
    if (!ready || Pause_CheckStatus(1) == 2) return;
    FighterData *you = You();
    bool held = IsCaptured(you);
    if (phase == PHASE_SETUP) {
        if (held) StartGrab(you);
        return;
    }
    if (phase != PHASE_GRAB) {
        UpdateFlash();
        return;
    }
    if (!grab.ticking) {
        if (held && you->grab.grab_timer >= grab.timer) return;
        grab.ticking = true;
    }
    if (grab.frames < 9999) grab.frames++;
    CountInputs(grab.timer - you->grab.grab_timer);
    grab.timer = you->grab.grab_timer;
    if (!held) {
        Escaped();
        UpdateFlash(); // Your release's first frame.
    }
}

#define DPAD_BUTTONS (HSD_BUTTON_DPAD_LEFT | HSD_BUTTON_DPAD_RIGHT | HSD_BUTTON_DPAD_DOWN | HSD_BUTTON_DPAD_UP)
/* The savestate rewinds your buttons to none held, so a D-pad press that
 * restarted would look new again to your fighter next frame. */
static void KeepDPadHeld(FighterData *ft) {
    int dpad = PadGetEngine(ft->pad_index)->held & DPAD_BUTTONS;
    ft->input.held |= dpad;
    ft->input.x664 |= dpad;
}
// Read from the pad, not your fighter's buttons, which a restart rewinds.
static void ReadDPad(FighterData *you) {
    int down = PadGetEngine(you->pad_index)->down;
    int percent = Options_Main[OPT_PERCENT].val;
    if (down & HSD_BUTTON_DPAD_UP && percent + 1 < TM_ESC_PERCENT_COUNT) percent++;
    else if (down & HSD_BUTTON_DPAD_DOWN && percent > 0) percent--;
    else if (!(down & (HSD_BUTTON_DPAD_LEFT | HSD_BUTTON_DPAD_RIGHT | HSD_BUTTON_DPAD_UP | HSD_BUTTON_DPAD_DOWN))) return;
    if (percent != Options_Main[OPT_PERCENT].val) {
        Options_Main[OPT_PERCENT].val = Options_Main[OPT_PERCENT].val_prev = percent;
        SaveSettings(0, percent);
    }
    restart = true;
}

// HUD ---------------------------------------------------------------------

static void DrawHUD(GOBJ *gobj, int pass) {
    if (pass != 2 || !ready)
        return;
    static char held_text[16], hold_text[16], best_label[24], best_hud[16], speed_text[16], big_text[16];
    int percent = Options_Main[OPT_PERCENT].val;
    struct best *best = &bests[percent];
    if (phase == PHASE_GRAB || grab.done) sprintf(held_text, "%df", grab.frames);
    else sprintf(held_text, "-");
    sprintf(hold_text, "%df", NoMashHold(percent));
    sprintf(best_label, "Best at %s", Values_Percent[percent]);
    if (best->frames) sprintf(best_hud, "%df", best->frames);
    else sprintf(best_hud, "-");
    if (grab.frames) sprintf(speed_text, "%.1f/s", grab.inputs * 60.f / grab.frames);
    else sprintf(speed_text, "-");
    const char *labels[] = { "Held For", "No-Mash Hold", best_label, "Mash Speed" };
    const char *info[] = { held_text, hold_text, best_hud, speed_text };
    event_vars->HUD_DrawInfoPanel(labels, info, countof(labels));

    // The mash count, big: cyan when the grab that just ended is a new best.
    Rect rect = { BIG_X, BIG_Y, BIG_WIDTH, BIG_HEIGHT };
    GXColor color = phase != PHASE_GRAB && grab.done && grab.new_best
        ? (GXColor){0, 255, 255, 255} : (GXColor){255, 255, 255, 255};
    sprintf(big_text, "Mash %d", grab.inputs);
    event_vars->HUD_DrawTextEx(big_text, &rect, BIG_SIZE, color, (GXColor){0, 0, 0, 255}, BIG_BORDER, 1);
}

// Event -------------------------------------------------------------------

static void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

/* The session bests need no clearing: this .dat is read again for every match,
 * statics and all. */
void Event_Init(GOBJ *gobj) {
    LoadSettings();
    GObj_AddGXLink(gobj, DrawHUD, GXLINK_HUD, 80);
    GObj_AddProc(GObj_Create(0, 7, 0), MeasureGrab, 15);
}

void Event_Think(GOBJ *gobj) {
    if (Pause_CheckStatus(1) == 2)
        return;
    if (event_vars->game_timer == 1) {
        ready = Fighter_GetGObj(0) && Fighter_GetGObj(1);
        if (ready) {
            event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
            Reset();
        }
        return; // Every reset frame is the setup's frame 0.
    }
    if (!ready)
        return;

    FighterData *you = You(), *popo = Popo();
    ReadDPad(you);
    if (restart || you->flags.dead || popo->flags.dead) {
        Reset();
        KeepDPadHeld(You());
        return;
    }

    HoldStill(Fighter_GetGObj(1));
    HoldStill(Nana());
    phase_timer++;

    switch (phase) {
        case PHASE_SETUP:
            HoldYou();
            if (phase_timer == 2) {
                popo->cpu.held = PAD_TRIGGER_Z;
            } else if (phase_timer > SETUP_FRAMES || (phase_timer > 4 && !IsGrabbing(popo))) {
                // The grab missed: try again with another gap.
                attempt.tries++;
                PlaceClimbers();
                phase_timer = 0;
            }
            break;
        case PHASE_GRAB:
            break; // Measured after the frame's hits.
        case PHASE_RESULT:
            if (phase_timer >= RESULT_FRAMES)
                Reset();
            break;
    }
}
