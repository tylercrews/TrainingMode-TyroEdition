#include "events.h"

/* Escape Samus Up-B (Character-specific Tech): Samus Screw Attacks you in the
 * middle of Battlefield and you Smash DI out of it before its last hit
 * launches you. Each attempt is a snapshot, like Ledgedash Training: you're
 * put beside Samus, standing or just above the ground, and can't act until
 * her first hit lands, so it's all SDI from there. The numbers below are from
 * the ISO and the DOL; notes/ESCAPE-EVENTS-RESEARCH.md has the tables.
 *
 * Grounded Screw Attack (PlSs.dat SpecialHi, state 353): a 2% opener on frame
 * 4 (base knockback 130 low, 100 high, angle 100), a three-hit loop on frames
 * 6-10, then an eight-hit loop on 12-26, each 1% at angles that pull you back
 * toward her (105-110 and 200), and on frame 28 the launcher: 1%, angle 70,
 * weight-dependent set knockback 80, electric, and far bigger (7.8 against
 * 2.3-3.1). Its hitboxes are two columns either side of her, 5.9 out, at her
 * feet and 9.4 up. She doesn't rise until frame 5, then climbs 40 units by
 * frame 13 and hangs there (root motion: 44.5 by frame 25). The launcher is
 * the only Screw Attack hitbox with set knockback, which is how the event
 * knows it landed.
 *
 * Smash DI is the native 8008E4F0, which damage entry (8008E2A8) installs as
 * the victim's cb.EveryHitlag, called each hitlag frame from the physics proc
 * (Fighter_UpdatePhys, 8006BE00). A stick at least PlCo 0x4B0 (0.7) long whose
 * x or y crossed PlCo 0x8 under PlCo 0x4B4 (4) frames ago moves you PlCo
 * 0x4B8 (6) units along it; then both axis timers read 254 until a new
 * crossing (damage entry also sets them to 254, so a held stick never SDIs).
 * The event wraps that callback and counts the calls that moved you: those
 * are exactly the SDI inputs the game took, whether or not the SDI OSD is on.
 *
 * Goal: Punish. After you escape, hit or grab Samus before she can act again.
 * Her up-B ends in FallSpecial with can_fastfall set (8012A8A0, 8012A948) and
 * 24 frames of landing lag (PlSs special attribute 0x50). FallSpecial's
 * physics (80096B44) runs the common fast fall check (8007D528): falling, the
 * stick at or past PlCo 0x88 (0.6625) down, its y tilt timer under PlCo 0x8C
 * (4). So she flicks the stick down on each falling frame after a neutral one.
 * Once she's free again she turns invincible (hurt.kind_game 1, as respawning
 * does), with Lab's intangibility flash, and the attempt ends. */

// The native SDI callback. A test probe defines its own before including this.
#ifndef ESCAPE_SAMUS_SDI
#define ESCAPE_SAMUS_SDI 0x8008E4F0
#endif
#define NATIVE_SDI ((void (*)(GOBJ *))ESCAPE_SAMUS_SDI)

#define SAMUS_UPB_GROUND 353  // Samus's Screw Attack, grounded and aerial.
#define SAMUS_UPB_AIR 354
#define UPB_DONE_FRAME 33     // Every Screw Attack hitbox is gone by this frame.
#define SETUP_FRAMES 20       // You see where you are this long before she up-Bs.
#define RESULT_FRAMES 60      // The result stays up this long before the next attempt.
#define GIVE_UP_FRAMES 240    // An attempt that never resolves starts over.
#define AIR_HEIGHT 6.f        // In the air, your feet this high: in her opener's reach.
#define RANDOM_DISTANCE 10.f  // Random distances run from on top of you to this.
#define PIN_SLACK .01f        // How far a held fighter can drift before it's put back.
#define PUNISH_GIVE_UP_FRAMES 600 // A punish window that never closes ends anyway.
#define INVINCIBLE_COLANIM 10 // Lab's intangibility flash (INTANG_COLANIM).

// The big SDI count under the match timer, where Grab Punish draws its countdown.
#define BIG_X -5.f
#define BIG_Y 12.9f
#define BIG_WIDTH 10.f
#define BIG_HEIGHT 2.5f
#define BIG_SIZE 1.f
#define BIG_BORDER 2.f

enum position { POS_GROUND, POS_AIR, POS_RANDOM };
enum distance { DIST_ON_TOP, DIST_SIDE, DIST_FAR, DIST_RANDOM };
enum side { SIDE_RANDOM, SIDE_LEFT, SIDE_RIGHT };
enum goal { GOAL_ESCAPE, GOAL_PUNISH };
// Samus's distance from you, center to center: right on you, your middle on her
// hitbox column (5.9 x 0.88 = 5.2 out), and you at its edge.
static const float Distances[] = { 0, 6, 10 };

static const char *Values_Position[] = { "Ground", "Air", "Random" };
static const char *Values_Distance[] = { "On Top Of", "Side", "Far Side", "Random" };
static const char *Values_Side[] = { "Random", "Left", "Right" };
static const char *Values_Percent[] = {
    "0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%",
    "110%", "120%", "130%", "140%", "150%", "160%", "170%", "180%", "190%", "200%",
};
static const char *Values_Goal[] = { "Escape", "Punish" };

enum menu_options { OPT_POSITION, OPT_DISTANCE, OPT_SIDE, OPT_PERCENT, OPT_GOAL, OPT_HELP };
// Each saved row's preference ID is its row number.
typedef char escape_samus_saved_rows[((int)OPT_POSITION == TM_ESC_SAMUS_POSITION &&
    (int)OPT_DISTANCE == TM_ESC_SAMUS_DISTANCE && (int)OPT_SIDE == TM_ESC_SAMUS_SIDE &&
    (int)OPT_PERCENT == TM_ESC_SAMUS_PERCENT && (int)OPT_GOAL == TM_ESC_SAMUS_GOAL &&
    (int)OPT_HELP == TM_ESC_SAMUS_PREF_COUNT &&
    countof(Values_Position) == TM_ESC_POSITION_COUNT && countof(Values_Distance) == TM_ESC_DISTANCE_COUNT &&
    countof(Values_Side) == TM_ESC_SIDE_COUNT && countof(Values_Percent) == TM_ESC_PERCENT_COUNT &&
    countof(Values_Goal) == TM_ESC_GOAL_COUNT && countof(Distances) == DIST_RANDOM) ? 1 : -1];

static void Exit(GOBJ *menu);
static void SaveSettings(GOBJ *menu, int value);

/* Help: how escaping and punishing work. The numbers are the game's: SDI is
 * 8008E4F0 and ASDI 8008E714 (see the top of this file and the research notes),
 * C-stick ASDI past PlCo 0x4B0 (800DF608), and the move data is PlSs.dat's. */
#define HELP_ROW(label, line1, line2, line3, line4) \
    { .kind = OPTKIND_INFO, .name = label, .desc = {line1, line2, line3, line4} }
static EventOption Options_Help[] = {
    HELP_ROW("About This Event",
             "Samus Screw Attacks you as each attempt",
             "starts, and you can't act until her first hit.",
             "D-Pad left/right picks her side and down",
             "switches ground/air. Both restart, uncounted."),
    HELP_ROW("Goal: Escape",
             "Fall out before her last hit, the launcher,",
             "lands. If it lands, you're launched: a miss.",
             "An attempt she never hits isn't counted.",
             "The result shows as her hitboxes end."),
    HELP_ROW("Goal: Punish",
             "Escape, then hit or grab her before she can",
             "act. She falls helpless, fast falling as soon",
             "as she can, and has 24 frames of landing lag.",
             "Once she can act she turns invincible."),
    HELP_ROW("What SDI Is",
             "Smash DI moves you during hitlag, the freeze",
             "as a hit lands. A flick of the control stick",
             "past 70% moves you up to 6 units its way.",
             "Each of her hits freezes you for 3 frames."),
    HELP_ROW("Fresh Flicks Only",
             "An SDI needs the stick to leave the middle",
             "(25%) and pass 70% within 4 frames. Then it's",
             "used up until an axis leaves the middle again.",
             "A stick held from before a hit never SDIs."),
    HELP_ROW("Quarter Circles",
             "Rolling from straight to a diagonal makes the",
             "other axis leave the middle, so it's fresh:",
             "away, down-away, away, down-away SDIs at each",
             "diagonal without crossing the middle."),
    HELP_ROW("Don't Go Back and Forth",
             "SDI goes the way the stick points, so left",
             "then right cancels out. Tap the same way, or",
             "roll between two neighbors. Going through the",
             "middle between flicks does nothing."),
    HELP_ROW("Which Way to SDI",
             "Her loop hits lift you and pull you toward",
             "her, and she rises fast. SDI away from her,",
             "and down and away once you're off the ground,",
             "so she rises past you."),
    HELP_ROW("ASDI",
             "As each hit's freeze ends you're nudged 3",
             "units: along the C-stick if it's past 70%,",
             "otherwise along the control stick if it is.",
             "Holding counts: no flick needed."),
    HELP_ROW("Hold the C-Stick",
             "The C-stick can't SDI in Melee, but it wins",
             "ASDI. Hold it away (or down and away) and",
             "every hit nudges you out while your control",
             "stick does the SDI."),
    HELP_ROW("Her Last Hit",
             "The launcher is about 3 times as wide as her",
             "loop hits and electric (4 frames of hitlag),",
             "so it reaches you at a column's edge. Be out",
             "before it, about frame 28 of her up-B."),
    HELP_ROW("Percent and Position",
             "Her grounded hits knock you farther at higher",
             "percents, so they hold you less. Air starts",
             "you in her opener's reach. Distance sets how",
             "deep in her hitboxes you start."),
    HELP_ROW("Reading the HUD",
             "The big SDI count is the inputs the game took",
             "this attempt: green on a success, yellow while",
             "you can punish, red on a miss. The panel has",
             "your results and SDI average."),
};
static EventMenu Menu_Help = {
    .name = "Help",
    .option_num = countof(Options_Help),
    .options = Options_Help,
};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_STRING,
        .name = "Your Position",
        .values = Values_Position,
        .value_num = countof(Values_Position),
        .OnChange = SaveSettings,
        .desc = {"Ground: standing beside Samus as she up-Bs.",
                 "Air: just above the ground, as after an aerial",
                 "on her shield. Random picks each attempt.",
                 "D-Pad down switches and restarts."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Distance",
        .values = Values_Distance,
        .value_num = countof(Values_Distance),
        .val = TM_ESC_DEFAULT_DISTANCE,
        .val_prev = TM_ESC_DEFAULT_DISTANCE,
        .OnChange = SaveSettings,
        .desc = {"How far Samus starts from you. On Top Of: right",
                 "on you. Side: her hitbox is on you. Far Side:",
                 "you're at its edge. Random: anywhere from on",
                 "top of you to Far Side."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Samus Side",
        .values = Values_Side,
        .value_num = countof(Values_Side),
        .OnChange = SaveSettings,
        .desc = {"Which side of you Samus is on.",
                 "Random picks each attempt.",
                 "D-Pad left/right picks a side and restarts."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Your Percent",
        .values = Values_Percent,
        .value_num = countof(Values_Percent),
        .OnChange = SaveSettings,
        .desc = {"Your percent as each attempt starts.",
                 "Her hits knock you farther at higher percents,",
                 "so they hold you less.",
                 "Takes effect on the next attempt."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Goal",
        .values = Values_Goal,
        .value_num = countof(Values_Goal),
        .OnChange = SaveSettings,
        .desc = {"Escape: fall out before her last hit lands.",
                 "Punish: escape, then hit or grab her before",
                 "she can act again, when she turns invincible.",
                 "Takes effect on the next attempt."},
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Help",
        .menu = &Menu_Help,
        .desc = {"How SDI and ASDI work, how to get out of her",
                 "up-B, and how each goal is judged."},
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
    .name = "Escape Samus Up-B",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};
EventMenu *Event_Menu = &Menu_Main;

// The menu is saved to the memory card as it changes.
static void SaveSettings(GOBJ *menu, int value) {
    for (int i = 0; i < TM_ESC_SAMUS_PREF_COUNT; i++)
        TM_SetSetting(TM_SETTING_ESCAPE_SAMUS, i, Options_Main[i].val);
}
static void LoadSettings(void) {
    for (int i = 0; i < TM_ESC_SAMUS_PREF_COUNT; i++)
        Options_Main[i].val = Options_Main[i].val_prev = TM_GetSetting(TM_SETTING_ESCAPE_SAMUS, i);
}

enum phase {
    PHASE_SETUP,   // You're in place; Samus waits a moment.
    PHASE_ATTACK,  // Samus is in her up-B.
    PHASE_PUNISH,  // Punish: you're out, and have until she can act to hit her.
    PHASE_RESULT,  // Counted; the next attempt follows.
};
enum result { RESULT_NONE, RESULT_ESCAPED, RESULT_LAUNCHED, RESULT_MISSED, RESULT_PUNISHED, RESULT_TOO_SLOW };
static const char *Result_Text[] = { "-", "Escaped", "Launched", "Samus Missed", "Punished", "Too Slow" };

static int phase = PHASE_SETUP, phase_timer;
static bool ready, restart;

// This attempt. A D-pad restart starts a new one without counting it.
static struct attempt {
    bool ground;      // Standing, or held in the air.
    int toward;       // From you toward Samus: -1 or 1.
    float distance;
    Vec3 you, samus;  // Where each of you starts.
    bool punish;      // The goal is Punish (read as the attempt starts).
    bool started;     // Samus is in her up-B.
    int hits, sdi;
    bool launched;    // Her last hit landed.
    bool punished;    // You hit or grabbed her while she couldn't act.
    bool closed;      // She can act again, and is invincible.
    float damage;     // Your percent plus pending damage, after last frame's hits.
    float samus_damage; // Hers, the same way.
    int result;
} attempt;

// This session's counted attempts, and the last one. Every counted attempt is
// an escape attempt; those with Punish as the goal are punish attempts too.
static struct stats {
    int attempts, escapes, sdi_total;
    int punish_attempts, punishes;
    int last, last_hits, last_sdi;
} stats;

static FighterData *You(void) { return Fighter_GetGObj(0)->userdata; }
static FighterData *Samus(void) { return Fighter_GetGObj(1)->userdata; }

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
// Nothing left over from before the reset keeps a hit off you.
static void MakeHittable(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
}
// The stage's floor under x: Battlefield's main stage, under its platforms.
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
static void StandAt(GOBJ *fighter, Vec3 ground, int line, int facing) {
    FighterData *ft = fighter->userdata;
    if (line >= 0) ft->coll_data.ground_index = line;
    ft->phys.pos = ground;
    ClearVelocity(ft);
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(ft);
    ft->facing_direction = ft->facing_direction_prev = facing;
    Fighter_EnterWait(fighter);
    Fighter_UpdateCameraBox(fighter);
    MakeHittable(fighter);
}

/* In the air you're held in Fall the way Passing Practice floats its enemy:
 * this physics callback pins you, a no-op collision callback keeps you from
 * landing, and with no interrupt callback you can't act. Her first hit's
 * damage state replaces all of them. */
static void HoldInAir(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->phys.pos = attempt.you;
    ClearVelocity(ft);
}
static void NoCollision(GOBJ *fighter) { }
static bool HeldInAir(FighterData *ft) { return ft->cb.Phys == HoldInAir; }
static void FloatAt(GOBJ *fighter, Vec3 feet, int facing) {
    FighterData *ft = fighter->userdata;
    ft->phys.pos = feet;
    ClearVelocity(ft);
    ft->facing_direction = ft->facing_direction_prev = facing;
    Fighter_SetAirborne(ft);
    ActionStateChange(0, 1, -1, fighter, ASID_FALL, 0, 0);
    ft->cb.Anim = 0;
    ft->cb.IASA = 0;
    ft->cb.Phys = HoldInAir;
    ft->cb.Coll = NoCollision;
    ft->jump.jumps_used = 1; // As after a jump: the double jump is still there.
    UpdatePosition(fighter);
    Fighter_UpdateCameraBox(fighter);
    MakeHittable(fighter);
}

/* Until her first hit you can't act. Standing, Wait's interrupt callback is
 * what would let you, so it's cleared every frame; the stick still feeds the
 * SDI timers, so SDIing the first hit works as in a match. Both of you are also
 * put back if pushed apart while you wait. */
static void Pin(GOBJ *fighter, Vec3 spot) {
    FighterData *ft = fighter->userdata;
    if (fabs(ft->phys.pos.X - spot.X) <= PIN_SLACK && fabs(ft->phys.pos.Y - spot.Y) <= PIN_SLACK) return;
    ft->phys.pos = spot;
    ClearVelocity(ft);
    UpdatePosition(fighter);
}
static void HoldYou(void) {
    if (attempt.hits) return;
    GOBJ *fighter = Fighter_GetGObj(0);
    FighterData *ft = fighter->userdata;
    if (HeldInAir(ft)) return;
    if (ft->state_id == ASID_WAIT) {
        ft->cb.IASA = 0;
        Pin(fighter, attempt.you);
    }
}

static void SetPercent(GOBJ *fighter, float percent) {
    FighterData *ft = fighter->userdata;
    ft->dmg.percent = percent;
    // Pending damage to add next frame, not a copy of the percent.
    ft->dmg.percent_temp = 0;
    Fighter_SetHUDDamage(ft->ply, percent);
}
static float Damage(FighterData *ft) { return ft->dmg.percent + ft->dmg.percent_temp; }

// Smash DI ----------------------------------------------------------------

/* The native callback only moves you when it takes an SDI input, so a move
 * through it is one input. */
static void CountSDI(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    float x = ft->phys.pos.X, y = ft->phys.pos.Y;
    NATIVE_SDI(fighter);
    if ((ft->phys.pos.X != x || ft->phys.pos.Y != y) && attempt.sdi < 999)
        attempt.sdi++;
}
// Each hit's damage entry puts the native callback back.
static void WrapSDI(FighterData *ft) {
    if (ft->cb.EveryHitlag == NATIVE_SDI)
        ft->cb.EveryHitlag = CountSDI;
}

// Samus -------------------------------------------------------------------

static bool InUpB(FighterData *samus) {
    return samus->state_id == SAMUS_UPB_GROUND || samus->state_id == SAMUS_UPB_AIR;
}
// The launcher is her only Screw Attack hitbox with set knockback.
static bool LauncherOut(FighterData *samus) {
    if (!InUpB(samus)) return false;
    for (int i = 0; i < (int)countof(samus->hitbox); i++)
        if (samus->hitbox[i].active && samus->hitbox[i].wdsk > 0) return true;
    return false;
}
/* A fresh press of B with the stick up. The game only takes presses, so a B
 * already down last frame is let go for a frame first. */
static void PressUpB(FighterData *samus) {
    samus->cpu.held |= PAD_BUTTON_B & ~samus->input.held;
    samus->cpu.lstickY = 127;
}

static bool InSpecialFall(FighterData *samus) {
    return samus->state_id >= ASID_FALLSPECIAL && samus->state_id <= ASID_FALLSPECIALB;
}
/* Fast fall as soon as she can: the check (8007D528) wants her falling and a
 * fresh flick down, so the stick goes down on a falling frame after one in the
 * middle (inside PlCo 0x8). The think runs before her input is read. */
static void FastFall(FighterData *samus) {
    if (!InSpecialFall(samus) || samus->flags.is_fastfall || samus->phys.self_vel.Y >= 0) return;
    if (samus->input.lstick.Y > -(*stc_ftcommon)->x8)
        samus->cpu.lstickY = -127;
}
// Held or being thrown: a grab is a punish too.
static bool Grabbed(FighterData *samus) {
    return (samus->state_id >= ASID_CAPTUREPULLEDHI && samus->state_id <= ASID_CAPTUREDAMAGELW) ||
        (samus->state_id >= ASID_CAPTURENECK && samus->state_id <= ASID_CAPTUREFOOT) ||
        (samus->state_id >= ASID_THROWNF && samus->state_id <= ASID_THROWNLWWOMEN);
}
/* Free to act: out of hitlag and hitstun and in a state she can act from. With
 * no AI, wherever she ends up comes back to one of these. After her up-B that's
 * Wait, the frame her landing lag ends. */
static bool SamusFree(FighterData *samus) {
    if (samus->flags.hitstun || samus->flags.hitlag) return false;
    switch (samus->state_id) {
        case ASID_WAIT: case ASID_FALL: case ASID_FALLF: case ASID_FALLB:
        case ASID_FALLAERIAL: case ASID_FALLAERIALF: case ASID_FALLAERIALB:
        case ASID_LANDING: case ASID_DAMAGEFALL: case ASID_DOWNWAITU: case ASID_DOWNWAITD:
            return true;
    }
    return false;
}
/* Invincible the way respawning is (hurt kind 1), which nothing resets while
 * she stands there, and flashing as Lab's intangible CPU does. */
static void MakeInvincible(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->hurt.kind_game = 1;
    Fighter_SetAllHurtboxState(fighter, 1);
    Fighter_ColAnim_Apply(ft, INVINCIBLE_COLANIM, 0);
}

// The attempt -------------------------------------------------------------

static int RandomSign(void) { return HSD_Randi(2) ? 1 : -1; }

static void ChooseLayout(void) {
    int position = Options_Main[OPT_POSITION].val;
    if (position == POS_RANDOM) position = HSD_Randi(2) ? POS_AIR : POS_GROUND;
    attempt.ground = position == POS_GROUND;
    int distance = Options_Main[OPT_DISTANCE].val;
    attempt.distance = distance == DIST_RANDOM ? RANDOM_DISTANCE * HSD_Randf() : Distances[distance];
    int side = Options_Main[OPT_SIDE].val;
    attempt.toward = side == SIDE_LEFT ? -1 : side == SIDE_RIGHT ? 1 : RandomSign();
}

static void Reset(void) {
    restart = false;
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    attempt = (struct attempt) { .punish = Options_Main[OPT_GOAL].val == GOAL_PUNISH };
    ChooseLayout();

    // You in the middle, Samus beside you, each facing the other. Right on top
    // of you, either way will do.
    GOBJ *you = Fighter_GetGObj(0), *samus = Fighter_GetGObj(1);
    int line;
    Vec3 ground;
    FloorUnder(0, &ground, &line);
    int facing = attempt.distance > .5f ? attempt.toward : RandomSign();
    attempt.you = ground;
    if (attempt.ground) {
        StandAt(you, ground, line, facing);
    } else {
        attempt.you.Y += AIR_HEIGHT;
        FloatAt(you, attempt.you, facing);
    }
    FloorUnder(attempt.toward * attempt.distance, &attempt.samus, &line);
    StandAt(samus, attempt.samus, line, attempt.distance > .5f ? -attempt.toward : RandomSign());
    Fighter_ColAnim_Remove(Samus(), INVINCIBLE_COLANIM); // StandAt made her hittable again.

    SetPercent(you, Options_Main[OPT_PERCENT].val * 10);
    SetPercent(samus, 0);
    attempt.damage = Damage(You());
    attempt.samus_damage = Damage(Samus());
    phase = PHASE_SETUP;
    phase_timer = 0;
    // Your input runs after this think, so this frame's interrupt goes too.
    HoldYou();
    Match_CorrectCamera();
}

static void SetResult(int result) { attempt.result = stats.last = result; }
static void ShowResult(void) {
    phase = PHASE_RESULT;
    phase_timer = 0;
}
/* A punish attempt counts once it's decided (launched, punished or too slow),
 * so a D-pad restart in the window leaves it out. */
static void CountPunishAttempt(bool punished) {
    if (stats.punish_attempts >= 99999) return;
    stats.punish_attempts++;
    stats.punishes += punished;
}
static void CountPunish(void) {
    CountPunishAttempt(true);
    SetResult(RESULT_PUNISHED);
    SFX_Play(303);
}

// Her up-B's hitboxes are done: you're out, launched, or she missed.
static void Resolve(void) {
    int result = attempt.launched ? RESULT_LAUNCHED : attempt.hits ? RESULT_ESCAPED : RESULT_MISSED;
    SetResult(result);
    stats.last_hits = attempt.hits;
    stats.last_sdi = attempt.sdi;
    if (result != RESULT_MISSED && stats.attempts < 99999) {
        stats.attempts++;
        stats.escapes += result == RESULT_ESCAPED;
        stats.sdi_total += attempt.sdi;
        if (attempt.punish && result == RESULT_LAUNCHED) CountPunishAttempt(false);
    }
    // Punish: out, so the window's open until she can act.
    if (attempt.punish && result == RESULT_ESCAPED) {
        if (attempt.punished) CountPunish(); // Already, in her up-B.
        phase = PHASE_PUNISH;
        phase_timer = 0;
        return;
    }
    if (result != RESULT_MISSED)
        SFX_Play(result == RESULT_ESCAPED ? 303 : 0xAF);
    ShowResult();
}

// She can act again: the window's shut and she's invincible.
static void ClosePunish(void) {
    attempt.closed = true;
    MakeInvincible(Fighter_GetGObj(1));
    if (!attempt.punished) {
        CountPunishAttempt(false);
        SetResult(RESULT_TOO_SLOW);
        SFX_Play(0xAF);
    }
    ShowResult();
}

/* At s_link 15, after the frame's hits and damage: a hit shows as new damage,
 * and Samus's hitboxes are still the ones that hit. */
static void AfterHits(GOBJ *gobj) {
    if (!ready || Pause_CheckStatus(1) == 2) return;
    FighterData *you = You(), *samus = Samus();
    WrapSDI(you);
    float damage = Damage(you);
    if (damage > attempt.damage + .001f && phase == PHASE_ATTACK) {
        if (attempt.hits < 99) attempt.hits++;
        if (LauncherOut(samus)) attempt.launched = true;
    }
    attempt.damage = damage;
    // Punish: any damage to her, or a grab, before she can act.
    float samus_damage = Damage(samus);
    if (attempt.punish && !attempt.punished && !attempt.closed && (phase == PHASE_ATTACK || phase == PHASE_PUNISH) &&
        (samus_damage > attempt.samus_damage + .001f || Grabbed(samus))) {
        attempt.punished = true;
        if (phase == PHASE_PUNISH) CountPunish();
    }
    attempt.samus_damage = samus_damage;
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
    if (down & (HSD_BUTTON_DPAD_LEFT | HSD_BUTTON_DPAD_RIGHT)) {
        int side = down & HSD_BUTTON_DPAD_LEFT ? SIDE_LEFT : SIDE_RIGHT;
        Options_Main[OPT_SIDE].val = Options_Main[OPT_SIDE].val_prev = side;
    } else if (down & HSD_BUTTON_DPAD_DOWN) {
        int position = Options_Main[OPT_POSITION].val == POS_GROUND ? POS_AIR : POS_GROUND;
        Options_Main[OPT_POSITION].val = Options_Main[OPT_POSITION].val_prev = position;
    } else {
        return;
    }
    SaveSettings(0, 0);
    restart = true;
}

// HUD ---------------------------------------------------------------------

static void DrawHUD(GOBJ *gobj, int pass) {
    if (pass != 2 || !ready)
        return;
    static char last_text[24], rate_text[24], hits_text[24], sdi_text[24], big_text[16];
    sprintf(last_text, "%s", Result_Text[stats.last]);
    if (stats.attempts) {
        sprintf(rate_text, "%d/%d (%.1f%%)", stats.escapes, stats.attempts, 100.f * stats.escapes / stats.attempts);
        sprintf(sdi_text, "%d (avg %.1f)", stats.last_sdi, (float)stats.sdi_total / stats.attempts);
    } else {
        sprintf(rate_text, "-");
        sprintf(sdi_text, stats.last ? "%d" : "-", stats.last_sdi);
    }
    if (stats.last) sprintf(hits_text, "%d", stats.last_hits);
    else sprintf(hits_text, "-");
    // Punish: the punish rate leads, and the escape rate takes Hits Taken's place.
    static const char *escape_labels[] = { "Last Attempt", "Escapes", "Hits Taken", "SDI Inputs" };
    static const char *punish_labels[] = { "Last Attempt", "Punishes", "Escapes", "SDI Inputs" };
    const char *info[] = { last_text, rate_text, hits_text, sdi_text };
    if (attempt.punish) {
        static char punish_text[24];
        if (stats.punish_attempts)
            sprintf(punish_text, "%d/%d (%.1f%%)", stats.punishes, stats.punish_attempts,
                100.f * stats.punishes / stats.punish_attempts);
        else
            sprintf(punish_text, "-");
        info[1] = punish_text;
        info[2] = rate_text;
    }
    event_vars->HUD_DrawInfoPanel(attempt.punish ? punish_labels : escape_labels, info, countof(info));

    /* This attempt's SDI inputs, big: green on a success (out, or punished),
     * yellow while a punish is still open, red if launched or too slow. */
    Rect rect = { BIG_X, BIG_Y, BIG_WIDTH, BIG_HEIGHT };
    int result = attempt.result;
    GXColor color = result == RESULT_PUNISHED || (result == RESULT_ESCAPED && !attempt.punish) ? (GXColor){40, 255, 60, 255}
        : result == RESULT_ESCAPED ? (GXColor){255, 220, 40, 255}
        : result == RESULT_LAUNCHED || result == RESULT_TOO_SLOW ? (GXColor)TM_CUE_RED_COLOR : (GXColor){255, 255, 255, 255};
    color.a = 255;
    sprintf(big_text, "SDI %d", attempt.sdi);
    event_vars->HUD_DrawTextEx(big_text, &rect, BIG_SIZE, color, (GXColor){0, 0, 0, 255}, BIG_BORDER, 1);
}

// Event -------------------------------------------------------------------

static void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

/* The session's totals need no clearing: this .dat is read again for every
 * match, statics and all. */
void Event_Init(GOBJ *gobj) {
    LoadSettings();
    GObj_AddGXLink(gobj, DrawHUD, GXLINK_HUD, 80);
    GObj_AddProc(GObj_Create(0, 7, 0), AfterHits, 15);
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

    FighterData *you = You(), *samus = Samus();
    ReadDPad(you);
    if (restart) {
        Reset();
        KeepDPadHeld(You());
        return;
    }

    samus->cpu.ai = 15;
    Fighter_ZeroCPUInputs(samus);
    WrapSDI(you);
    phase_timer++;

    switch (phase) {
        case PHASE_SETUP:
            HoldYou();
            Pin(Fighter_GetGObj(1), attempt.samus);
            if (phase_timer >= SETUP_FRAMES) {
                PressUpB(samus);
                phase = PHASE_ATTACK;
                phase_timer = 0;
            }
            break;
        case PHASE_ATTACK:
            HoldYou();
            if (!attempt.started) {
                if (InUpB(samus)) attempt.started = true;
                else PressUpB(samus);
            }
            if ((attempt.started && (!InUpB(samus) || samus->state.frame >= UPB_DONE_FRAME))
                || you->flags.dead || phase_timer >= GIVE_UP_FRAMES)
                Resolve();
            break;
        case PHASE_PUNISH:
            /* Her state is this frame's (her animation ran first), and hits
             * come after this, so she's invincible from her first free frame. */
            FastFall(samus);
            if (SamusFree(samus) || samus->flags.dead || you->flags.dead || phase_timer >= PUNISH_GIVE_UP_FRAMES)
                ClosePunish();
            break;
        case PHASE_RESULT:
            if (attempt.closed) samus->hurt.kind_game = 1; // Invincible until the reset.
            if (phase_timer >= RESULT_FRAMES)
                Reset();
            break;
    }
}
