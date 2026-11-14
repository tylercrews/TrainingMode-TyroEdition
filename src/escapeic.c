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
 * Mashing. CapturePulled's and CaptureWait's animation callbacks (800DB8A4,
 * 800DB908) take PlCo 0x3A4 (1) off the timer a frame, and
 * Fighter_CheckGrabBreakout (8007DC08) takes PlCo 0x3A8 (6) more for a fresh
 * A, B, X or Y, and 6 more when the stick crosses into a new direction (past
 * PlCo 0x308 on either axis; the grab starts each axis at neither), at most one
 * of each a frame: 13 a frame at most. CaptureWait lets go at 0. A pummel's
 * flinch, CaptureDamage, runs neither, which is why wobbling's hits kept a
 * victim from mashing. So the timer drops by 1 + 6 x the frame's inputs, and the
 * counter reads that drop. It reads after the frame's hits (s_link 15), so it
 * sees the timer the grab set before the first frame's mash. */

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

enum menu_options { OPT_PERCENT, OPT_HANDOFF, OPT_BESTS };
// Each saved row's preference ID is its row number.
typedef char escape_ic_saved_rows[((int)OPT_PERCENT == TM_ESC_IC_PERCENT &&
    countof(Values_Percent) == TM_ESC_PERCENT_COUNT) ? 1 : -1];

static void Exit(GOBJ *menu);
static void SaveSettings(GOBJ *menu, int value);

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
        .kind = OPTKIND_INFO,
        .name = "Mashing Info",
        .desc = {"Each fresh A, B, X or Y and each new stick",
                 "direction takes 6 frames off the grab, on top",
                 "of 1 a frame: 13 a frame at most. The counter",
                 "shows the inputs the game took."},
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Help",
        .desc = {"The Ice Climbers grab you as each attempt",
                 "starts. Mash out as fast as you can.",
                 "D-Pad left/right restarts, uncounted.",
                 "Beta: no handoffs yet."},
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
static void LoadSettings(void) {
    for (int i = 0; i < TM_ESC_IC_PREF_COUNT; i++)
        Options_Main[i].val = Options_Main[i].val_prev = TM_GetSetting(TM_SETTING_ESCAPE_IC, i);
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
    int frames;     // Frames held, the frame you got out included.
    int inputs;     // Mash inputs the game counted.
    float timer;    // The grab timer after last frame.
    bool done, new_best;
} grab;

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
    attempt = (struct attempt) { .percent = Options_Main[OPT_PERCENT].val, .side = HSD_Randi(2) ? 1 : -1 };
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
}

/* At s_link 15, after the frame's hits: on the frame Popo's grab connects you
 * hold the timer it set, and from then on each frame's animation callback
 * (s_link 1) has taken that frame's mash off it, up to the frame you get out. */
static void MeasureGrab(GOBJ *gobj) {
    if (!ready || Pause_CheckStatus(1) == 2) return;
    FighterData *you = You();
    bool held = IsCaptured(you);
    if (phase == PHASE_SETUP) {
        if (held) StartGrab(you);
        return;
    }
    if (phase != PHASE_GRAB) return;
    if (grab.frames < 9999) grab.frames++;
    CountInputs(grab.timer - you->grab.grab_timer);
    grab.timer = you->grab.grab_timer;
    if (!held) Escaped();
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
