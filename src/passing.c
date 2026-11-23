#include "events.h"
#include "stage_points.h"

/* Passing Practice (Doubles page): your CPU teammate passes the enemy CPU to
 * you with an aerial, you hit the enemy back, and your teammate jumps up and
 * hits it back to you with the same aerial. Hits counts your hits in a row, and
 * the volley is over when the enemy lands. The VS character select picks the
 * three fighters and their teams, as in Grab Punish Practice, and both events
 * share the saved teams and characters (grabpunish_css.c and
 * TwoVsOne_PrepareMatch in events.c).
 *
 * Melee sends a fighter away from whoever hit it by where the two stand, not by
 * which way the attacker faces: in ftColl_8007A06C the victim faces the
 * attacker, and its knockback is sent the other way. So your teammate only has
 * to hit the enemy while the enemy is on your side of it. It faces you for
 * every aerial but Back Air, which it does facing away.
 *
 * Your teammate's aerial is measured, not looked up. On every frame of it, the
 * biggest hitbox whose spot the game has worked out (state 2 or 3) is recorded
 * relative to your teammate's feet, by frames since the event pressed the
 * button. That counts any input delay and covers every character. Before the
 * first measurement, the serve's enemy waits at a guess and moves into the
 * hitbox the frame it comes out; later serves put the enemy right there.
 *
 * Returns are planned every frame from the enemy's predicted flight, with the
 * game's own physics: gravity on self_vel (ftCommon_Fall), knockback that
 * loses kb_frameDecay a frame along its direction (Fighter_procUpdate), and
 * position += self_vel + kb_vel. The teammate's jump follows ftCo_800CB110 and
 * ftCo_Jump_Phys (the first frame in the air skips gravity and drift), and its
 * drift ftCommon_CalcSelfAccel_AccelToVelClampedFrom. It only goes for an
 * enemy that's coming down nearer it than you. One coming down nearer you is
 * still yours, as when an up tilt sets up your own back air, so your teammate
 * waits on its spot for where your last hit sends it. However many times you
 * hit it first, that's one pass.
 *
 * The volley is over when the enemy lands or catches the ledge: a fighter on
 * the ledge is airborne (ftCliffCommon_80081370 calls ftCommon_8007D5D4). With
 * Reset If Hitstun Ends on, it's also over once the enemy can act again in the
 * air: its hitstun flag (x221C_b6, MexTK's flags.hitstun) clears when the
 * hitstun timer runs out (ftCo_8008F744), and ftCo_Damage_IASA then lets it
 * double jump, which it does, away from whichever of you is nearer.
 *
 * Within a frame the fighter procs run: hitlag (0), animation and script (1),
 * input (3, where this event's think runs first, so a press takes effect that
 * frame) and physics (4). So this think sees positions from the end of last
 * frame, and a hitbox's spot one frame after it comes out. Index t of a
 * prediction is the end of frame (now - 1 + t), the same frame a hitbox
 * measured t frames after its press was checked against the enemy. */

#define SERVE_DELAY 30          // Frames after a reset before your teammate serves.
#define SERVE_RETRY 12          // A missed serve tries again this soon.
#define RESULT_FRAMES 45        // A dropped volley's count stays up this long.
#define TOUCH_TIMEOUT (8 * 60)  // A volley nobody touches for this long is over.
#define PATH_FRAMES 90          // How far ahead the enemy's flight is predicted.
#define HIT_FRAMES 48           // Frames of the aerial that are measured.
#define HIT_CHOICES 6           // The planner aims with the first hitting frame and this many after it.
#define JUMP_FRAMES 100         // How far into its jumps your teammate's height is worked out.
#define JUMP_HEIGHTS 64         // Whole heights a jump is looked up by.
#define AIR_PLAN_FRAMES 40      // In the air, the aerial can be planned this many frames ahead.
#define FLOOR_MAX 48            // Floor lines kept for landing checks.
#define EDGE_MARGIN 10.f        // Starting spots stay this far in from the ledges.
#define HOME_SLACK 2.f          // Your teammate stops walking home this close to its spot.
#define DIRECTION_MARGIN 1.5f   // At the hit, the enemy is this far on your side of your teammate.
#define BODY_RADIUS 4.f         // About how far a fighter's body reaches around its center, at scale 1.
#define FALL_LIMIT 40.f         // An enemy this far under the stage can't be returned.
#define WALK_STICK 70           // Walk, well short of a dash.
#define TURN_STICK 45           // A standing turn.
#define LCANCEL_LOOKAHEAD 4     // R this many frames or fewer before landing, of the 7 that count.

void Exit(GOBJ *menu);

enum menu_options { OPT_AERIAL, OPT_PERCENT, OPT_MODE, OPT_HITSTUN, OPT_DISTANCE, OPT_SIDE, OPT_FACING };
enum aerial { AERIAL_NAIR, AERIAL_FAIR, AERIAL_BAIR, AERIAL_UAIR, AERIAL_DAIR };
static const char *Values_Aerial[] = {"Neutral Air", "Forward Air", "Back Air", "Up Air", "Down Air"};
enum mode { MODE_FROZEN, MODE_RISING, MODE_COUNT };
static const char *Values_Mode[] = {"Frozen %", "Rising %"};
static const char *Values_Percent[] = {
    "0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%",
    "110%", "120%", "130%", "140%", "150%", "160%", "170%", "180%", "190%", "200%",
};
static const char *Values_OnOff[] = {"Off", "On"};
static const char *Values_Distance[] = {"Very Close", "Close", "Medium", "Far", "Very Far"};
static const float Distances[] = {20, 30, 42, 55, 70};
enum side { SIDE_LEFT, SIDE_RIGHT, SIDE_RANDOM };
static const char *Values_Side[] = {"Left", "Right", "Random"};
enum facing { FACING_TOWARD, FACING_AWAY };
static const char *Values_Facing[] = {"Toward", "Away"};

/* Before the first measurement, where each aerial is guessed to hit: in front
 * of and above your teammate's feet, at scale 1, this many frames in. */
static const Vec2 Guess_Hit[] = {{6, 6}, {9, 7}, {-9, 6}, {1, 15}, {1, -6}};
#define GUESS_FRAME 5

// Each saved row's preference ID is its row number.
typedef char passing_saved_rows[((int)OPT_AERIAL == TM_PASS_AERIAL && (int)OPT_PERCENT == TM_PASS_PERCENT &&
    (int)OPT_MODE == TM_PASS_MODE && (int)OPT_HITSTUN == TM_PASS_HITSTUN && (int)OPT_DISTANCE == TM_PASS_DISTANCE &&
    (int)OPT_SIDE == TM_PASS_SIDE && (int)OPT_FACING == TM_PASS_FACING && OPT_FACING + 1 == TM_PASS_PREF_COUNT &&
    countof(Values_Aerial) == TM_PASS_AERIAL_COUNT && countof(Values_Percent) == TM_PASS_PERCENT_COUNT &&
    countof(Values_Mode) == TM_PASS_MODE_COUNT && countof(Values_OnOff) == TM_PASS_HITSTUN_COUNT &&
    countof(Values_Distance) == TM_PASS_DISTANCE_COUNT && countof(Distances) == TM_PASS_DISTANCE_COUNT &&
    countof(Values_Side) == TM_PASS_SIDE_COUNT && countof(Values_Facing) == TM_PASS_FACING_COUNT &&
    countof(Guess_Hit) == TM_PASS_AERIAL_COUNT) ? 1 : -1];
static void ChangeSetting(GOBJ *menu, int value);

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_STRING,
        .name = "Teammate Aerial",
        .values = Values_Aerial,
        .value_num = countof(Values_Aerial),
        .val = TM_PASS_DEFAULT_AERIAL,
        .val_prev = TM_PASS_DEFAULT_AERIAL,
        .OnChange = ChangeSetting,
        .desc = {"The aerial your teammate passes with. They face",
                 "you for all but Back Air, so the enemy flies back",
                 "your way. Most Down Airs spike, and an Up Air",
                 "sends the enemy up more than across."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Enemy Percent",
        .values = Values_Percent,
        .value_num = countof(Values_Percent),
        .val = TM_PASS_DEFAULT_PERCENT,
        .val_prev = TM_PASS_DEFAULT_PERCENT,
        .OnChange = ChangeSetting,
        .desc = {"The enemy's percent at the start of each volley.",
                 "Higher percents fly farther, so pass from farther",
                 "apart. Changing it starts a new volley."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Percent Mode",
        .values = Values_Mode,
        .value_num = countof(Values_Mode),
        .OnChange = ChangeSetting,
        .desc = {"Frozen %: every hit puts the enemy back at Enemy",
                 "Percent, so each pass flies about the same.",
                 "Rising %: hits add up, so passes fly farther.",
                 "Each keeps its own Session Best."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Reset If Hitstun Ends",
        .values = Values_OnOff,
        .value_num = countof(Values_OnOff),
        .OnChange = ChangeSetting,
        .desc = {"On: if the enemy's hitstun runs out before the",
                 "next hit, it jumps away and the volley is over, as",
                 "in a match. Low percents and long distances make",
                 "that harder. Off: it never acts."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Distance",
        .values = Values_Distance,
        .value_num = countof(Values_Distance),
        .val = TM_PASS_DEFAULT_DISTANCE,
        .val_prev = TM_PASS_DEFAULT_DISTANCE,
        .OnChange = ChangeSetting,
        .desc = {"How far apart you and your teammate start: 20,",
                 "30, 42, 55 or 70 units. Final Destination is 171",
                 "wide. Your teammate walks back to its spot after",
                 "each pass."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Your Side",
        .values = Values_Side,
        .value_num = countof(Values_Side),
        .OnChange = ChangeSetting,
        .desc = {"Which side you start on. Your teammate takes the",
                 "other. Random picks again each volley.",
                 "D-Pad left/right also picks a side and restarts."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Your Facing",
        .values = Values_Facing,
        .value_num = countof(Values_Facing),
        .OnChange = ChangeSetting,
        .desc = {"Whether you start facing your teammate, or facing",
                 "away from them, as for a back air.",
                 "D-Pad down also flips it and restarts."},
    },
    {
        .kind = OPTKIND_INFO,
        .name = "Help",
        .desc = {"Your teammate serves the enemy to you. Hit it back",
                 "and they pass it back with their aerial. Hits counts",
                 "your hits in a row until the enemy lands or grabs",
                 "the ledge. The D-Pad picks your side and facing."},
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
    .name = "Passing Practice",
    .option_num = countof(Options_Main),
    .options = Options_Main,
};
EventMenu *Event_Menu = &Menu_Main;

static bool restart_pending;

// The menu is saved to the memory card as it changes. The menu runs paused,
// so the new volley starts on the next frame.
static void ChangeSetting(GOBJ *menu, int value) {
    for (int i = 0; i < TM_PASS_PREF_COUNT; i++)
        TM_SetSetting(TM_SETTING_PASSING, i, Options_Main[i].val);
    restart_pending = true;
}
static void LoadSettings(void) {
    for (int i = 0; i < TM_PASS_PREF_COUNT; i++)
        Options_Main[i].val = Options_Main[i].val_prev = TM_GetSetting(TM_SETTING_PASSING, i);
}

// Players ------------------------------------------------------------------

// Ports of the three fighters; -1 when the character select didn't give one.
static int hmn_ply = -1, mate_ply = -1, enemy_ply = -1;
static bool roles_found;

static GOBJ *Body(int ply, int ms) {
    GOBJ *leader = Fighter_GetGObj(ply);
    if (!ms) return leader;
    // Zelda and Sheik's second GOBJ is a sleeping form; only Nana follows along.
    if (!leader || !((FighterData *)leader->userdata)->flags.has_follower) return 0;
    return Fighter_GetSubcharGObj(ply, ms);
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

static void SetPercent(int ply, float percent) {
    for (int ms = 0; ms < 2; ms++) {
        GOBJ *fighter = Body(ply, ms);
        if (!fighter) continue;
        FighterData *ft = fighter->userdata;
        ft->dmg.percent = percent;
        // Pending damage to add, not a copy of the percent.
        ft->dmg.percent_temp = 0;
    }
    Fighter_SetHUDDamage(ply, percent);
}
static float BasePercent(void) { return Options_Main[OPT_PERCENT].val * 10; }

static float Sign(float x) { return x < 0 ? -1 : 1; }
static float Clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static int Ceil(float x) {
    int whole = (int)x;
    return whole < x ? whole + 1 : whole;
}

// The stage --------------------------------------------------------------

/* The live floor lines, gathered once a frame: Fountain's platforms move. */
static struct floors { int count; Vec2 a[FLOOR_MAX], b[FLOOR_MAX]; u8 platform[FLOOR_MAX]; } floors;
static void GatherFloors(void) {
    floors.count = 0;
    for (int i = 0; i < LineCount() && floors.count < FLOOR_MAX; ++i) {
        Vec2 a, b;
        int platform;
        if (!FloorLine(i, &a, &b, &platform)) continue;
        floors.a[floors.count] = a;
        floors.b[floors.count] = b;
        floors.platform[floors.count] = platform;
        floors.count++;
    }
}
/* Whether a fall from y_from to y_to at x lands on a floor: the main stage's,
 * or a platform's, which can only be landed on from above. */
static bool Lands(float x, float y_from, float y_to) {
    if (y_to >= y_from) return false;
    for (int i = 0; i < floors.count; ++i) {
        Vec2 a = floors.a[i], b = floors.b[i];
        if (x < a.X || x > b.X) continue;
        float y = a.Y + (b.Y - a.Y) * (x - a.X) / (b.X - a.X);
        if (y_from >= y && y_to < y) return true;
    }
    return false;
}

// Where everyone stands this volley.
static struct layout {
    Vec3 you, mate;      // Ground spots.
    int you_line, mate_line;
    int toward;          // From your teammate toward you: -1 or 1.
    int facing;          // Your teammate's facing for its aerial.
    int you_facing;      // Yours: toward your teammate, or away.
    float floor;         // The main stage's lowest ground.
} layout;

static void ChooseLayout(void) {
    float left, right;
    GroundEdges(&left, &right);
    int side = Options_Main[OPT_SIDE].val;
    if (side == SIDE_RANDOM) side = HSD_Randi(2) ? SIDE_RIGHT : SIDE_LEFT;
    layout.toward = side == SIDE_RIGHT ? 1 : -1;
    float half = Distances[Options_Main[OPT_DISTANCE].val] * .5f;
    float mid = (left + right) * .5f;
    float room = (right - left) * .5f - EDGE_MARGIN;
    if (half > room) half = room > 1 ? room : 1;
    GroundPoint(mid + layout.toward * half, &layout.you, &layout.you_line);
    GroundPoint(mid - layout.toward * half, &layout.mate, &layout.mate_line);
    layout.facing = Options_Main[OPT_AERIAL].val == AERIAL_BAIR ? -layout.toward : layout.toward;
    layout.you_facing = Options_Main[OPT_FACING].val == FACING_AWAY ? layout.toward : -layout.toward;
    layout.floor = layout.you.Y < layout.mate.Y ? layout.you.Y : layout.mate.Y;
}

static void PlaceAt(GOBJ *fighter, Vec3 ground, int line, int facing) {
    if (!fighter) return;
    FighterData *ft = fighter->userdata;
    GroundAt(fighter, ground, line);
    ft->facing_direction = ft->facing_direction_prev = facing;
    Fighter_EnterWait(fighter);
    Fighter_UpdateCameraBox(fighter);
}
// The fighter, and Nana a little behind Popo.
static void PlacePlayer(int ply, Vec3 ground, int line, int facing) {
    PlaceAt(Body(ply, 0), ground, line, facing);
    ground.X -= facing * 6;
    Vec3 behind;
    int behind_line;
    GroundPoint(ground.X, &behind, &behind_line);
    PlaceAt(Body(ply, 1), behind, behind_line, facing);
}
// A CPU's Nana sits out: only Popo passes, and only Popo is passed around.
static void BenchFollower(int ply) {
    GOBJ *nana = Body(ply, 1);
    if (!nana) return;
    FighterData *ft = nana->userdata;
    if (ft->state_id != ASID_SLEEP) Fighter_EnterSleep(nana, 1);
    ft->phys.pos.X = 999.f;
    ft->phys.pos.Y = -999.f;
}

/* Where a fighter's body is: the middle of its hurtboxes, from its feet. */
static Vec2 BodyOffset(FighterData *ft) {
    float x = 0, y = 0;
    int count = 0;
    for (int i = 0; i < ft->hurt_num && i < 15; i++) {
        FtHurt *hurt = &ft->hurtbox[i];
        x += (hurt->hurt1_pos.X + hurt->hurt2_pos.X) * .5f;
        y += (hurt->hurt1_pos.Y + hurt->hurt2_pos.Y) * .5f;
        count++;
    }
    Vec2 standing = {0, 8 * ft->scale.Y};
    if (!count) return standing;
    Vec2 offset = {x / count - ft->phys.pos.X, y / count - ft->phys.pos.Y};
    // Right after it's moved, the hurtboxes are still where it was.
    if (fabs(offset.X) > 15 * ft->scale.Y || offset.Y < -10 * ft->scale.Y || offset.Y > 25 * ft->scale.Y)
        return standing;
    return offset;
}

// The floating enemy -------------------------------------------------------

/* While it waits to be served, the enemy is held in Fall, as Low Laser Puff
 * Practice holds Puff: its own physics callback pins it, and a no-op
 * collision callback keeps it from landing. Any hit replaces both. */
static Vec3 float_spot;   // Feet.
static void HoldEnemy(GOBJ *fighter) {
    FighterData *ft = fighter->userdata;
    ft->phys.pos = float_spot;
    ClearVelocity(ft);
}
static void HoldEnemyCollision(GOBJ *fighter) { }
static bool Floating(FighterData *ft) { return ft->cb.Phys == HoldEnemy; }

static void FloatEnemy(Vec3 feet) {
    GOBJ *fighter = Body(enemy_ply, 0);
    FighterData *ft = fighter->userdata;
    float_spot = feet;
    ft->phys.pos = feet;
    ClearVelocity(ft);
    ft->facing_direction = ft->facing_direction_prev = -layout.facing;
    Fighter_SetAirborne(ft);
    ActionStateChange(0, 1, -1, fighter, ASID_FALL, 0, 0);
    ft->cb.Anim = 0;
    ft->cb.IASA = 0;
    ft->cb.Phys = HoldEnemy;
    ft->cb.Coll = HoldEnemyCollision;
    ft->cpu.ai = 15;
    Fighter_ZeroCPUInputs(ft);
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
    UpdatePosition(fighter);
    Fighter_UpdateCameraBox(fighter);
}
// Puts the enemy's body, not its feet, at a spot.
static void MoveFloat(Vec2 body) {
    FighterData *ft = Data(enemy_ply);
    Vec2 offset = BodyOffset(ft);
    float_spot = (Vec3){body.X - offset.X, body.Y - offset.Y, 0};
}

// The volley ---------------------------------------------------------------

enum phase {
    PHASE_SERVE,    // Your teammate serves the floating enemy.
    PHASE_VOLLEY,   // The enemy is in play.
    PHASE_RESULT,   // Dropped: the count stays up, then a new volley.
};
static int phase = PHASE_SERVE, phase_timer;
static int frame; // Frames this event has run, for the teammate's timings.

enum toucher { TOUCH_NONE, TOUCH_MATE, TOUCH_YOU };
static struct volley {
    int hits;          // Your hits this volley: each first touch after your teammate's.
    int last;          // Who touched the enemy last.
    int since_touch;
    float enemy_pct;   // The enemy's percent last frame.
    bool new_best;
} volley;

/* Session-only: this .dat reloads with each match, which clears them. */
static struct record { int best, previous; bool played; } records[MODE_COUNT];

static void EndVolley(void) {
    struct record *record = &records[Options_Main[OPT_MODE].val];
    volley.new_best = volley.hits > record->best || (!record->played && volley.hits > 0);
    if (volley.hits > record->best) record->best = volley.hits;
    record->previous = volley.hits;
    record->played = true;
    if (volley.new_best) SFX_Play(303);
    phase = PHASE_RESULT;
    phase_timer = 0;
}

/* New damage on the enemy since last frame. The game sets the victim's
 * source_ply to whoever hit it (ftColl_8007861C). */
static void TrackHits(FighterData *enemy) {
    float percent = enemy->dmg.percent;
    if (percent > volley.enemy_pct) {
        int who = enemy->dmg.source_ply;
        if (who == hmn_ply) {
            if (volley.last != TOUCH_YOU && phase != PHASE_RESULT) volley.hits++;
            volley.last = TOUCH_YOU;
        } else if (who == mate_ply) {
            volley.last = TOUCH_MATE;
        }
        volley.since_touch = 0;
        if (phase == PHASE_SERVE) {
            phase = PHASE_VOLLEY;
            phase_timer = 0;
        }
        if (Options_Main[OPT_MODE].val == MODE_FROZEN) {
            percent = BasePercent();
            SetPercent(enemy_ply, percent);
        }
    }
    volley.enemy_pct = percent;
}

static bool OnLedge(FighterData *ft) { return ft->state_id >= ASID_CLIFFCATCH && ft->state_id <= ASID_CLIFFJUMPQUICK2; }

/* Reset If Hitstun Ends: the frame the enemy can act in the air again, it
 * double jumps away from whichever of you is nearer, and the volley is over. */
static struct escape { int dir, at; } escape;
static bool CanAct(FighterData *enemy) {
    return enemy->phys.air_state && !enemy->flags.hitstun && !enemy->flags.freeze && !Floating(enemy);
}
static void Escape(FighterData *enemy, FighterData *hmn, FighterData *mate) {
    float from_you = enemy->phys.pos.X - hmn->phys.pos.X, from_mate = enemy->phys.pos.X - mate->phys.pos.X;
    escape.dir = Sign(fabs(from_you) < fabs(from_mate) ? from_you : from_mate);
    escape.at = frame;
    EndVolley();
}
/* The enemy never acts on its own: no DI, no tech, no jump out of tumble. An
 * escape holds the stick away, and presses X on its first frame, which the
 * enemy's own input proc reads later this frame. */
static void EnemyInputs(FighterData *enemy) {
    enemy->cpu.ai = 15;
    Fighter_ZeroCPUInputs(enemy);
    if (!escape.dir) return;
    enemy->cpu.lstickX = escape.dir > 0 ? 127 : -128;
    if (frame == escape.at) enemy->cpu.held = PAD_BUTTON_X;
}

// Your teammate's aerial and jumps ------------------------------------------

typedef struct HitFrame { float x, y, r; } HitFrame; // Forward and up from the feet; r 0 for no hit.
static struct measured {
    int kind, aerial;      // Whose aerial this is: kind -1 for nobody's yet.
    int first, last;       // The first and last frames that hit, 0 for none.
    int length;            // Frames until it ends in the air, 0 until one has.
    HitFrame hit[HIT_FRAMES];
    bool serve_known;
    Vec2 serve_hit;        // The serve's first hitbox, forward and up from where the jump began.
} measured = { .kind = -1 };

static struct jumps {
    int kind;
    int liftoff;                    // Frames from the jump press to the jump: measured, or the jumpsquat.
    bool liftoff_measured;
    float height[2][JUMP_FRAMES];   // Short hop, full hop: over the ground, by frames since the press.
    float reach[JUMP_FRAMES];       // The farthest drift either way by then, from a standing jump.
    int apex[2], land[2];           // The highest frame, and the first back on the ground.
    // By whole heights: the first frame at or over it on the way up, and at
    // or under it on the way down. -1 where the jump never gets there.
    s8 rise_at[2][JUMP_HEIGHTS], fall_at[2][JUMP_HEIGHTS];
} jumps = { .kind = -1 };

/* Horizontal air speed after a frame of drift with the stick at s, as in
 * ftCommon_CalcSelfAccel_DriftFrom. */
static float DriftStep(FighterData *ft, float vx, float s) {
    float target = s * ft->attr.aerial_drift_max;
    float friction = ft->attr.aerial_friction;
    float most = ft->attr.horizontal_air_mobility_constant;
    float accel;
    if (target == 0) {
        if (fabs(friction) >= fabs(vx)) accel = -vx;
        else accel = vx > 0 ? -friction : friction;
        return vx + accel;
    }
    accel = s * ft->attr.aerial_drift_stick_mult + (s > 0 ? ft->attr.aerial_drift_base : -ft->attr.aerial_drift_base);
    if (!(vx * accel < 0)) {
        if (accel > 0) {
            if (vx + accel > target) {
                accel = -friction;
                if (vx + accel < target) accel = target - vx;
                if (vx + accel > most) accel = most - vx;
            }
        } else if (vx + accel < target) {
            accel = friction;
            if (vx + accel > target) accel = target - vx;
            if (vx + accel < -most) accel = -most - vx;
        }
    }
    return vx + accel;
}
static float FallStep(FighterData *ft, float vy) {
    vy -= ft->attr.gravity;
    return vy < -ft->attr.terminal_velocity ? -ft->attr.terminal_velocity : vy;
}
static float JumpSpeed(FighterData *ft, float ground_vx, float s) {
    float vx = ground_vx * ft->attr.ground_to_air_jump_momentum_multiplier + s * ft->attr.jump_h_initial_velocity;
    float most = ft->attr.jump_h_max_velocity;
    return Clamp(vx, -most, most);
}

static void WorkOutJumps(FighterData *ft) {
    if (jumps.kind != ft->kind) {
        jumps.kind = ft->kind;
        jumps.liftoff_measured = false;
    }
    if (!jumps.liftoff_measured) jumps.liftoff = (int)ft->attr.jump_startup_time;
    if (jumps.liftoff < 1) jumps.liftoff = 1;
    for (int full = 0; full < 2; full++) {
        float vy = full ? ft->attr.jump_v_initial_velocity : ft->attr.hop_v_initial_velocity;
        float y = 0;
        jumps.apex[full] = jumps.land[full] = 0;
        for (int t = 0; t < JUMP_FRAMES; t++) {
            if (t > jumps.liftoff) {
                if (t > jumps.liftoff + 1) vy = FallStep(ft, vy); // Its first frame up skips gravity.
                y += vy;
            }
            if (y <= 0 && t > jumps.liftoff) {
                y = 0;
                if (!jumps.land[full]) jumps.land[full] = t;
            }
            jumps.height[full][t] = jumps.land[full] ? 0 : y;
            if (!jumps.land[full] && y > jumps.height[full][jumps.apex[full]]) jumps.apex[full] = t;
        }
        if (!jumps.land[full]) jumps.land[full] = JUMP_FRAMES;
        const float *height = jumps.height[full];
        int apex = jumps.apex[full], land = jumps.land[full];
        for (int h = 0; h < JUMP_HEIGHTS; h++) {
            int rise = -1, fall = -1;
            for (int t = jumps.liftoff + 1; t <= apex && rise < 0; t++)
                if (height[t] >= h) rise = t;
            for (int t = apex; t < land && t < JUMP_FRAMES && fall < 0; t++)
                if (height[t] <= h) fall = t;
            if (rise < 0) fall = -1; // Never this high.
            jumps.rise_at[full][h] = rise;
            jumps.fall_at[full][h] = fall;
        }
    }
    float vx = JumpSpeed(ft, 0, 1), x = 0;
    for (int t = 0; t < JUMP_FRAMES; t++) {
        if (t > jumps.liftoff) {
            if (t > jumps.liftoff + 1) vx = DriftStep(ft, vx, 1);
            x += vx;
        }
        jumps.reach[t] = x;
    }
}

/* The frame since the press when the jump is height h, on its way up or its
 * way down: the closest frame, or -1 if it never gets there. */
static int JumpFrameAt(int full, int falling, float h) {
    const float *height = jumps.height[full];
    if (h < 0 || h > height[jumps.apex[full]] + 1) return -1;
    int whole = (int)h;
    if (whole >= JUMP_HEIGHTS) return -1;
    int t = falling ? jumps.fall_at[full][whole] : jumps.rise_at[full][whole];
    if (t < 0) return -1;
    // The frame just before can be closer: the table rounds toward the apex on
    // the way up, and toward the ground on the way down.
    int other = t - 1;
    int lo = falling ? jumps.apex[full] : jumps.liftoff + 1;
    if (other >= lo && fabs(height[other] - h) < fabs(height[t] - h)) t = other;
    return t;
}

static void ForgetAerial(int kind, int aerial) {
    measured = (struct measured) { .kind = kind, .aerial = aerial };
}
static bool AerialKnown(void) { return measured.first > 0; }

// The enemy's flight ---------------------------------------------------------

typedef struct Path {
    int frames;               // Predicted frames, up to its landing.
    bool lands;
    Vec2 body[PATH_FRAMES + 1];
    float feet_x[PATH_FRAMES + 1];
} Path;

static void PredictEnemy(FighterData *ft, Path *path) {
    Vec2 offset = BodyOffset(ft);
    float x = ft->phys.pos.X, y = ft->phys.pos.Y;
    float vx = ft->phys.self_vel.X, vy = ft->phys.self_vel.Y;
    float kx = ft->phys.kb_vel.X, ky = ft->phys.kb_vel.Y;
    float decay = (*stc_ftcommon)->kb_frameDecay;
    float bottom = ft->coll_data.ecbCurr_bot.Y;
    if (bottom < 0) bottom = 0;
    int frozen = 0;
    if (ft->flags.freeze || ft->dmg.hitlag_frames > 0) {
        frozen = Ceil(ft->dmg.hitlag_frames);
        if (frozen < 1) frozen = 1;
    }
    path->lands = false;
    path->body[0] = (Vec2){x + offset.X, y + offset.Y};
    path->feet_x[0] = x;
    int t = 1;
    for (; t <= PATH_FRAMES; t++) {
        if (frozen > 0) {
            frozen--;
        } else {
            vy = FallStep(ft, vy);
            vx = DriftStep(ft, vx, 0);
            float k = sqrtf(kx * kx + ky * ky);
            if (k < decay) kx = ky = 0;
            else { kx -= decay * kx / k; ky -= decay * ky / k; }
            float nx = x + vx + kx, ny = y + vy + ky;
            if (Lands(nx, y + bottom, ny + bottom)) {
                path->lands = true;
                break;
            }
            x = nx;
            y = ny;
        }
        path->body[t] = (Vec2){x + offset.X, y + offset.Y};
        path->feet_x[t] = x;
        if (y < layout.floor - FALL_LIMIT) break;
    }
    path->frames = t - 1;
}

// Your teammate's inputs -----------------------------------------------------

typedef struct Input { int held; s8 lx, ly, cx, cy; } Input;

static s8 StickValue(float s) {
    s = Clamp(s, -1, 1);
    return (s8)(s > 0 ? s * 127 : s * 128);
}

/* Where your teammate is in a jump that the event pressed. */
static struct control {
    int jump_at;        // The frame jump was pressed; -1 for none.
    bool full_hop;
    float stick;        // Held through the jumpsquat, for the jump's drift.
    int lifted_at;      // The first frame it was seen in the air; -1 before.
    int aerial_at;      // The frame the aerial was pressed; -1 for none.
    int aerial_index;   // Frames of the aerial since then, not counting hitlag.
    int aerial_state;   // The state the aerial went into; -1 until it has.
    int measured_at;    // The last frame MeasureAerial counted.
    bool frozen_now;    // In hitlag this frame, as the think saw it.
    bool serving;       // This jump is the serve.
    bool lcancelled;    // This aerial's R is pressed.
    bool aimed;         // The planner's aerial: its hit index and frame.
    int aim_index;
} control;

static void ResetControl(void) {
    control = (struct control) { .jump_at = -1, .lifted_at = -1, .aerial_at = -1, .aerial_state = -1, .measured_at = -1 };
}

enum category { CAT_BUSY, CAT_GROUND, CAT_KNEEBEND, CAT_AIR, CAT_AERIAL };
static int Category(FighterData *ft) {
    int state = ft->state_id;
    if (state == ASID_KNEEBEND) return CAT_KNEEBEND;
    if (control.aerial_at >= 0 && state == control.aerial_state) return CAT_AERIAL;
    if (ft->phys.air_state == 0)
        return (state >= ASID_WAIT && state <= ASID_RUNBRAKE) || (state >= ASID_SQUAT && state <= ASID_SQUATRV)
            ? CAT_GROUND : CAT_BUSY;
    return state >= ASID_JUMPF && state <= ASID_FALLAERIALB ? CAT_AIR : CAT_BUSY;
}

/* No stick on the press itself, so Wait can't take it as a dash. The jump's
 * drift comes from the stick held through the jumpsquat (ftCo_800CB110). */
static void PressJump(FighterData *mate, Input *in, bool full, float stick) {
    in->held |= PAD_BUTTON_X;
    in->lx = 0;
    control.jump_at = frame;
    control.full_hop = full;
    control.stick = stick;
    control.lifted_at = -1;
    control.aerial_at = -1;
    control.aimed = false;
}
/* The C-stick for all but Neutral Air, which is A with the stick in the middle,
 * so that frame has no drift. */
static void PressAerial(FighterData *mate, Input *in) {
    int aerial = Options_Main[OPT_AERIAL].val;
    int facing = mate->facing_direction > 0 ? 1 : -1;
    switch (aerial) {
        case AERIAL_NAIR: in->held |= PAD_BUTTON_A; in->lx = in->ly = 0; break;
        case AERIAL_FAIR: in->cx = facing * 127; break;
        case AERIAL_BAIR: in->cx = -facing * 127; break;
        case AERIAL_UAIR: in->cy = 127; break;
        case AERIAL_DAIR: in->cy = -128; break;
    }
    control.aerial_at = frame;
    control.aerial_index = 0;
    control.lcancelled = false;
    control.aerial_state = -1;
}

/* After an aerial press: which state it went into, and every hitbox the game
 * placed and checked for hits this frame, by frames of the aerial since the
 * press. It runs at s_link 15, after the fighters' hit detection (13) and
 * damage (14), so even a hitbox that lasts one frame is seen where it hit. The
 * count starts with the press's own frame, so a frame of input delay would be
 * in the measurement, and leaves out frames in hitlag, which don't run the
 * aerial. */
static void MeasureAerial(FighterData *mate) {
    if (control.aerial_at < 0 || control.measured_at == frame) return;
    control.measured_at = frame;
    int in_aerial = !(mate->state_id >= ASID_JUMPF && mate->state_id <= ASID_FALLAERIALB) && mate->phys.air_state;
    if (control.aerial_state < 0) {
        if (!in_aerial) {
            // The press hasn't taken: give it a frame, then let it go.
            if (++control.aerial_index > 1) control.aerial_at = -1;
            return;
        }
        control.aerial_state = mate->state_id;
    } else if (mate->state_id != control.aerial_state) {
        // Over in the air, not cut short by a landing or a hit: its length.
        if (mate->phys.air_state && !in_aerial && control.aerial_index > measured.length)
            measured.length = control.aerial_index;
        control.aerial_at = -1;
        control.aimed = false;
        return;
    }
    if (!control.frozen_now) control.aerial_index++;
    int index = control.aerial_index;
    if (index < 1 || index >= HIT_FRAMES) return;
    int best = -1;
    for (int i = 0; i < 4; i++)
        if (mate->hitbox[i].active >= 2 && (best < 0 || mate->hitbox[i].size > mate->hitbox[best].size)) best = i;
    if (best < 0) return;
    ftHit *hit = &mate->hitbox[best];
    float facing = mate->facing_direction > 0 ? 1 : -1;
    if (!measured.hit[index].r) {
        measured.hit[index] = (HitFrame) {
            (hit->pos.X - mate->phys.pos.X) * facing, hit->pos.Y - mate->phys.pos.Y, hit->size * mate->scale.Y,
        };
        if (!measured.first || index < measured.first) measured.first = index;
        if (index > measured.last) measured.last = index;
    }
    /* The serve's first hitbox: where later serves' enemies wait. If the guess
     * missed it, the enemy moves into it now: the hitbox sweeps from this spot
     * to its next, so it still lands (if it lasts two frames). */
    if (control.serving && index == measured.first && !measured.serve_known) {
        measured.serve_known = true;
        measured.serve_hit = (Vec2){(hit->pos.X - layout.mate.X) * layout.facing, hit->pos.Y - layout.mate.Y};
        if (phase == PHASE_SERVE && Floating(Data(enemy_ply))) MoveFloat((Vec2){hit->pos.X, hit->pos.Y});
    }
}

// Where the serve's enemy waits: its body where the hitbox will be.
static Vec2 ServeSpot(FighterData *mate) {
    if (measured.serve_known)
        return (Vec2){layout.mate.X + layout.facing * measured.serve_hit.X, layout.mate.Y + measured.serve_hit.Y};
    Vec2 guess = Guess_Hit[Options_Main[OPT_AERIAL].val];
    int full = Options_Main[OPT_AERIAL].val == AERIAL_DAIR;
    int t = jumps.liftoff + 1 + GUESS_FRAME;
    float height = jumps.height[full][t < JUMP_FRAMES ? t : JUMP_FRAMES - 1];
    float scale = mate->scale.Y;
    return (Vec2){layout.mate.X + layout.facing * guess.X * scale, layout.mate.Y + height + guess.Y * scale};
}

/* The jump, then the aerial one frame after leaving the ground. Down Air
 * serves from a full hop, so its hitbox isn't at the floor. */
static void WalkHome(FighterData *mate, Input *in);
static bool AtHome(FighterData *mate) {
    return fabs(layout.mate.X - mate->phys.pos.X) <= HOME_SLACK && (mate->facing_direction > 0 ? 1 : -1) == layout.facing;
}
static void Serve(FighterData *mate, int category, Input *in) {
    phase_timer++;
    switch (category) {
        case CAT_GROUND:
            if (control.serving) {
                // Back down without a hit: the enemy waits where the hitbox was now, so again.
                control.serving = false;
                phase_timer = SERVE_DELAY - SERVE_RETRY;
            }
            if (!AtHome(mate)) WalkHome(mate, in);
            else if (phase_timer >= SERVE_DELAY) {
                PressJump(mate, in, Options_Main[OPT_AERIAL].val == AERIAL_DAIR, 0);
                control.serving = true;
            }
            break;
        case CAT_KNEEBEND:
            if (control.full_hop) in->held |= PAD_BUTTON_X;
            break;
        case CAT_AIR:
            if (control.serving && control.aerial_at < 0 && control.lifted_at >= 0 && frame > control.lifted_at)
                PressAerial(mate, in);
            break;
    }
}

// Planning the return -------------------------------------------------------

typedef struct Plan {
    bool valid;
    int wait;          // Frames until the jump (ground) or the aerial (air).
    int full;
    int index;         // The aerial's hit frame it aims with.
    int at;            // The prediction frame of the hit.
    float stick;
    float x;           // Where your teammate's feet have to be then.
    float cost;
} Plan;

/* How a hit at prediction frame t with the aerial's hit frame `index` lines up
 * with the enemy: the error in the hitbox's height, and where your teammate's
 * feet would have to be. False if it can't hit. */
static bool Aim(const Path *path, int t, int index, float facing, float feet_y, float enemy_scale, float *error, float *x) {
    HitFrame *hit = &measured.hit[index];
    if (!hit->r) return false;
    Vec2 body = path->body[t];
    float reach = hit->r + BODY_RADIUS * enemy_scale;
    float dy = feet_y + hit->y - body.Y;
    float need = body.X - facing * hit->x;
    // Knockback goes away from your teammate, so keep the enemy on your side.
    float side = (path->feet_x[t] - need) * layout.toward;
    float shift = side < DIRECTION_MARGIN ? DIRECTION_MARGIN - side : 0;
    need -= layout.toward * shift;
    float squared = dy * dy + shift * shift, most = reach * .75f;
    if (squared > most * most) return false;
    *error = sqrtf(squared);
    *x = need;
    return true;
}

static float PlanCost(float error, int index, int t, float stick, float x) {
    float cost = error + .4f * (index - measured.first) + .03f * t + .02f * fabs(x - layout.mate.X);
    float s = fabs(stick);
    if (s > .6f) cost += 4 * (s - .6f);
    return cost;
}

/* On the ground: the best jump, now or later, with your teammate standing
 * still until then. A moving teammate only gets plans that jump now. */
static Plan PlanFromGround(FighterData *mate, const Path *path, Plan *nearest) {
    Plan best = { .valid = false, .cost = 1e9f };
    *nearest = (Plan) { .valid = false, .cost = 1e9f };
    float facing = mate->facing_direction > 0 ? 1 : -1;
    float enemy_scale = Data(enemy_ply)->scale.Y;
    float ground_vx = mate->phys.self_vel.X;
    bool moving = fabs(ground_vx) > .05f;
    // From a run, the jump keeps some of its speed: where that alone carries it.
    static float carried[JUMP_FRAMES];
    if (moving) {
        float vx = JumpSpeed(mate, ground_vx, 0), drift = 0;
        for (int i = 0; i < JUMP_FRAMES; i++) {
            if (i > jumps.liftoff) {
                if (i > jumps.liftoff + 1) vx = DriftStep(mate, vx, 0);
                drift += vx;
            }
            carried[i] = drift;
        }
    }
    float x0 = mate->phys.pos.X, ground = mate->phys.pos.Y;
    int last = measured.first + HIT_CHOICES;
    if (last > measured.last) last = measured.last;
    // How far above and below its feet the aerial reaches, to skip frames
    // where the enemy is out of any jump's reach.
    float top = -1e9f, bottom = 1e9f;
    for (int index = measured.first; index <= last; index++) {
        HitFrame *hit = &measured.hit[index];
        if (!hit->r) continue;
        if (hit->y + hit->r > top) top = hit->y + hit->r;
        if (hit->y - hit->r < bottom) bottom = hit->y - hit->r;
    }
    float highest = jumps.height[1][jumps.apex[1]];
    for (int t = 1; t <= path->frames; t++) {
        float over = path->body[t].Y - ground;
        if (over > highest + top + BODY_RADIUS * 2 || over < bottom - BODY_RADIUS * 2) continue;
        for (int full = 0; full < 2; full++) {
            for (int index = measured.first; index <= last; index++) {
                HitFrame *hit = &measured.hit[index];
                if (!hit->r) continue;
                float h = path->body[t].Y - hit->y - ground;
                for (int falling = 0; falling < 2; falling++) {
                    int since = JumpFrameAt(full, falling, h);
                    if (since < 0) continue;
                    int wait = t - since;
                    if (wait < 0 || (moving && wait)) continue;
                    if (since - index < jumps.liftoff + 1) continue; // The aerial waits a frame after liftoff.
                    float error, x;
                    if (!Aim(path, t, index, facing, ground + jumps.height[full][since], enemy_scale, &error, &x)) continue;
                    float dx = x - x0 - (moving ? carried[since] : 0), reach = jumps.reach[since];
                    float stick = reach > .01f ? dx / reach : dx ? Sign(dx) * 2 : 0;
                    float cost = PlanCost(error, index, t, stick, x);
                    Plan plan = { true, wait, full, index, t, Clamp(stick, -1, 1), x, cost };
                    if (fabs(stick) <= 1) {
                        if (cost < best.cost) best = plan;
                    } else if (fabs(stick) < fabs(nearest->stick) || !nearest->valid) {
                        plan.stick = stick;
                        *nearest = plan;
                    }
                }
            }
        }
    }
    return best;
}

/* Your teammate's flight from now with the stick held at s: x and feet height
 * at each prediction frame, and the first frame back on the ground. */
typedef struct Flight { float x[PATH_FRAMES + 1], y[PATH_FRAMES + 1]; int land; } Flight;
static void Fly(FighterData *mate, float s, int frames, Flight *flight) {
    float x = mate->phys.pos.X, y = mate->phys.pos.Y;
    float vx = mate->phys.self_vel.X, vy = mate->phys.self_vel.Y;
    float bottom = mate->coll_data.ecbCurr_bot.Y;
    if (bottom < 0) bottom = 0;
    // Its first frame in the air skips gravity and drift (ftCo_Jump_Phys), and
    // a fast fall holds its speed (ftCommon_FallFast).
    bool first = control.lifted_at == frame && mate->state_id >= ASID_JUMPF && mate->state_id <= ASID_JUMPB;
    bool fast = mate->flags.is_fastfall;
    flight->land = frames + 1;
    flight->x[0] = x;
    flight->y[0] = y;
    for (int t = 1; t <= frames; t++) {
        if (!first || t > 1) {
            vx = DriftStep(mate, vx, s);
            vy = fast ? -mate->attr.fastfall_velocity : FallStep(mate, vy);
        }
        float ny = y + vy;
        x += vx;
        if (flight->land > frames && Lands(x, y + bottom, ny + bottom)) flight->land = t;
        y = ny;
        flight->x[t] = x;
        flight->y[t] = y;
    }
}

/* In the air: when to press the aerial, and which way to drift. */
static Flight flights[3]; // Stick left, still, right.
static Plan PlanInAir(FighterData *mate, const Path *path, int soonest) {
    Plan best = { .valid = false, .cost = 1e9f };
    float facing = mate->facing_direction > 0 ? 1 : -1;
    float enemy_scale = Data(enemy_ply)->scale.Y;
    int frames = path->frames;
    for (int i = 0; i < 3; i++) Fly(mate, i - 1, frames, &flights[i]);
    int last = measured.first + HIT_CHOICES;
    if (last > measured.last) last = measured.last;
    for (int wait = soonest; wait <= AIR_PLAN_FRAMES; wait++) {
        for (int index = measured.first; index <= last; index++) {
            int t = wait + index;
            if (t > frames) break;
            if (t >= flights[1].land) break;
            float error, x;
            if (!Aim(path, t, index, facing, flights[1].y[t], enemy_scale, &error, &x)) continue;
            float lo = flights[0].x[t], mid = flights[1].x[t], hi = flights[2].x[t];
            if (x < lo - .5f || x > hi + .5f) continue;
            float stick = x >= mid ? (hi - mid > .01f ? (x - mid) / (hi - mid) : 0)
                                   : (mid - lo > .01f ? -(mid - x) / (mid - lo) : 0);
            float cost = PlanCost(error, index, t, stick, x);
            if (cost < best.cost) best = (Plan){ true, wait, 0, index, t, Clamp(stick, -1, 1), x, cost };
        }
    }
    return best;
}

/* The stick that brings your teammate's feet to x at prediction frame t. */
static float SteerTo(FighterData *mate, float x, int t) {
    if (t < 1) t = 1;
    if (t > PATH_FRAMES) t = PATH_FRAMES;
    Flight *flight = flights;
    for (int i = 0; i < 3; i++) Fly(mate, i - 1, t, &flight[i]);
    float lo = flight[0].x[t], mid = flight[1].x[t], hi = flight[2].x[t];
    if (x >= mid) return hi - mid > .01f ? Clamp((x - mid) / (hi - mid), 0, 1) : 0;
    return mid - lo > .01f ? Clamp(-(mid - x) / (mid - lo), -1, 0) : 0;
}

// Your teammate ---------------------------------------------------------------

static void WalkHome(FighterData *mate, Input *in) {
    float dx = layout.mate.X - mate->phys.pos.X;
    int facing = mate->facing_direction > 0 ? 1 : -1;
    if (fabs(dx) > HOME_SLACK) in->lx = Sign(dx) * WALK_STICK;
    else if (facing != layout.facing) in->lx = layout.facing * TURN_STICK;
}

/* In the air with nothing to hit: drift home and fast fall, so it's back on
 * the ground, where it has every option, as soon as it can be. */
static void FallHome(FighterData *mate, Input *in) {
    in->lx = StickValue((layout.mate.X - mate->phys.pos.X) * .1f);
    if (mate->phys.self_vel.Y < 0) in->ly = -128;
}
static void GoHome(FighterData *mate, int category, Input *in) {
    if (category == CAT_GROUND) WalkHome(mate, in);
    else if (category == CAT_AIR || category == CAT_AERIAL) FallHome(mate, in);
}

/* Whether the enemy comes down nearer your teammate than you: a pass. One that
 * comes down nearer you, as after an up tilt you'll back air, is still yours. */
static bool PassToMate(const Path *path, FighterData *hmn, FighterData *mate) {
    float end = path->feet_x[path->frames];
    return fabs(end - mate->phys.pos.X) <= fabs(end - hmn->phys.pos.X);
}

static Path path;
static void Intercept(FighterData *hmn, FighterData *mate, FighterData *enemy, int category, Input *in) {
    PredictEnemy(enemy, &path);
    // Not passed yet: back to its spot, ready for where your next hit sends it.
    // A jump already started goes ahead.
    if (category != CAT_KNEEBEND && !PassToMate(&path, hmn, mate)) {
        GoHome(mate, category, in);
        return;
    }
    int facing = mate->facing_direction > 0 ? 1 : -1;
    switch (category) {
        case CAT_GROUND: {
            if (facing != layout.facing) {
                in->lx = layout.facing * TURN_STICK;
                break;
            }
            Plan nearest, plan = PlanFromGround(mate, &path, &nearest);
            if (plan.valid) {
                if (!plan.wait) PressJump(mate, in, plan.full, plan.stick);
            } else if (nearest.valid && Sign(nearest.stick) == facing) {
                // Too far ahead: run there. It can't run backward, but it can drift.
                in->lx = facing * 127;
            } else if (nearest.valid && !nearest.wait) {
                PressJump(mate, in, nearest.full, Clamp(nearest.stick, -1, 1));
            }
            break;
        }
        case CAT_KNEEBEND:
            if (control.full_hop) in->held |= PAD_BUTTON_X;
            in->lx = StickValue(control.stick);
            break;
        case CAT_AIR: {
            if (control.lifted_at < 0) control.lifted_at = frame;
            // Pressed on the frame it leaves the ground, the aerial's physics
            // would skip the jump's first frame, so it waits one.
            Plan plan = PlanInAir(mate, &path, control.lifted_at == frame ? 1 : 0);
            if (!plan.valid) {
                // Nothing to hit from this jump: down to the ground, toward where it lands.
                in->lx = StickValue((path.body[path.frames].X - mate->phys.pos.X) * .1f);
                if (mate->phys.self_vel.Y < 0) in->ly = -128;
                break;
            }
            in->lx = StickValue(plan.stick);
            if (!plan.wait) {
                PressAerial(mate, in);
                control.aimed = true;
                control.aim_index = plan.index;
            }
            break;
        }
        case CAT_AERIAL: {
            // Keep the hitbox on the enemy: the planned frame, or a later one.
            int since = control.aerial_index;
            float best_cost = 1e9f, best_x = mate->phys.pos.X;
            int best_t = 1;
            float enemy_scale = enemy->scale.Y;
            for (int i = 0; i < 3; i++) Fly(mate, i - 1, path.frames, &flights[i]);
            for (int index = since + 1; index <= measured.last && index < HIT_FRAMES; index++) {
                int t = index - since;
                if (t > path.frames || t >= flights[1].land) break;
                float error, x;
                if (!Aim(&path, t, index, facing, flights[1].y[t], enemy_scale, &error, &x)) continue;
                float cost = error + .5f * (index - since) + (index == control.aim_index ? 0 : 1);
                if (cost < best_cost) { best_cost = cost; best_x = x; best_t = t; }
            }
            if (best_cost < 1e9f) {
                in->lx = StickValue(SteerTo(mate, best_x, best_t));
                break;
            }
            // Too late for this one: another aerial once it ends, if it's
            // still in the air then. Else down to the ground to try from there.
            if (measured.length > since) {
                Plan plan = PlanInAir(mate, &path, measured.length - since);
                if (plan.valid) {
                    in->lx = StickValue(plan.stick);
                    break;
                }
            }
            FallHome(mate, in);
            break;
        }
    }
}

static void TeammateThink(FighterData *hmn, FighterData *mate, FighterData *enemy) {
    Input in = { 0 };
    int category = Category(mate);
    if (category == CAT_AIR && control.lifted_at < 0 && control.jump_at >= 0) {
        control.lifted_at = frame;
        if (control.serving && !jumps.liftoff_measured && frame - control.jump_at > 0) {
            jumps.liftoff = frame - control.jump_at;
            jumps.liftoff_measured = true;
            WorkOutJumps(mate);
        }
    }
    if (category == CAT_GROUND) control.jump_at = -1;
    if (phase == PHASE_SERVE) Serve(mate, category, &in);
    else if (phase == PHASE_VOLLEY && volley.last == TOUCH_YOU && AerialKnown()) Intercept(hmn, mate, enemy, category, &in);
    else GoHome(mate, category, &in);

    /* L-cancel: an R within 7 frames of landing halves the aerial's landing lag
     * (ftCo_LandingAir_Enter checks the frames since the last L or R, x67F).
     * Only while the aerial can't be interrupted: after that, R air dodges. */
    if (category == CAT_AERIAL && !control.lcancelled && !mate->flags.past_iasa) {
        static Flight landing;
        Fly(mate, Clamp(in.lx / 127.f, -1, 1), LCANCEL_LOOKAHEAD, &landing);
        if (landing.land <= LCANCEL_LOOKAHEAD) {
            in.held |= PAD_TRIGGER_R;
            control.lcancelled = true;
        }
    }
    mate->cpu.ai = 15;
    Fighter_ZeroCPUInputs(mate);
    mate->cpu.held = in.held;
    mate->cpu.lstickX = in.lx;
    mate->cpu.lstickY = in.ly;
    mate->cpu.cstickX = in.cx;
    mate->cpu.cstickY = in.cy;
}

// The HUD --------------------------------------------------------------------

static GXColor White = {0xff, 0xff, 0xff, 0xff};
static GXColor Cyan = {0x00, 0xff, 0xff, 0xff};
static GXColor Red = {0xff, 0x28, 0x28, 0xff};
static GXColor Gold = {0xff, 0xe1, 0x41, 0xff};
static GXColor Outline = {0x00, 0x00, 0x00, 0xff};

/* Under the match timer: its digits end 74 down the 640x480 screen, and the
 * count's start about 9.6 into its line at this size, so they clear it by 5.
 * A black outline of four, then the colored text on top, as in Ledgegrab
 * Speedrun. */
#define HUD_Y 70
#define ROW_SUBTEXTS 5
#define ROW_FOREGROUND (ROW_SUBTEXTS - 1)
enum hud_row { ROW_COUNTER, ROW_BEST, ROW_PREVIOUS, ROW_COUNT };
static Text *hud_text;
static int shown[ROW_COUNT][2];

static void CreateHUD(void) {
    int canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 81, 19);
    Text *text = hud_text = Text_CreateText(2, canvas);
    if (!text) return;
    text->kerning = text->align = 1;
    text->trans = (Vec3){320, HUD_Y, 0};
    text->viewport_scale.X = text->viewport_scale.Y = 1;
    text->scale.X = text->scale.Y = .8f;
    static const Vec2 offsets[ROW_SUBTEXTS] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}, {0, 0}};
    static const float row_y[ROW_COUNT] = {0, 34, 62};
    for (int row = 0; row < ROW_COUNT; ++row) {
        for (int i = 0; i < ROW_SUBTEXTS; ++i) {
            int subtext = Text_AddSubtext(text, offsets[i].X, row_y[row] + offsets[i].Y, "");
            Text_SetColor(text, subtext, i < ROW_FOREGROUND ? &Outline : row == ROW_COUNTER ? &White : &Gold);
            if (row != ROW_COUNTER) Text_SetScale(text, subtext, .5625f, .5625f);
        }
        shown[row][0] = shown[row][1] = -2;
    }
}

static void SetRow(int row, int value, int extra, const char *format, const char *name) {
    if (shown[row][0] == value && shown[row][1] == extra) return;
    shown[row][0] = value;
    shown[row][1] = extra;
    for (int i = 0; i < ROW_SUBTEXTS; ++i) {
        int subtext = row * ROW_SUBTEXTS + i;
        if (value < 0) Text_SetText(hud_text, subtext, format, name, "--");
        else Text_SetText(hud_text, subtext, format, name, value);
    }
}

void Event_Update(void) {
    if (!hud_text) return;
    hud_text->hidden = !roles_found || !RolesReady() ||
        ((MenuData *)event_vars->menu_gobj->userdata)->mode == MenuMode_Paused;
    int mode = Options_Main[OPT_MODE].val;
    struct record *record = &records[mode];
    GXColor *color = phase != PHASE_RESULT ? &White : volley.new_best ? &Cyan : &Red;
    Text_SetColor(hud_text, ROW_COUNTER * ROW_SUBTEXTS + ROW_FOREGROUND, color);
    SetRow(ROW_COUNTER, volley.hits, 0, "%sHits: %d", "");
    // Session Best Frozen %: 5, with the mode named apart so its % isn't a format.
    if (record->played) {
        SetRow(ROW_BEST, record->best, mode, "Session Best %s: %d", Values_Mode[mode]);
        SetRow(ROW_PREVIOUS, record->previous, mode, "%sPrevious: %d", "");
    } else {
        SetRow(ROW_BEST, -1, mode, "Session Best %s: %s", Values_Mode[mode]);
        SetRow(ROW_PREVIOUS, -1, mode, "%sPrevious: %s", "");
    }
}

static void DrawMissing(GOBJ *gobj, int pass) {
    if (pass != 2 || !roles_found || RolesReady()) return;
    static const char *labels[] = {"Teammate CPU", "Enemy CPU"};
    const char *info[] = {mate_ply >= 0 ? "Ready" : "Missing", enemy_ply >= 0 ? "Ready" : "Missing"};
    event_vars->HUD_DrawInfoPanel(labels, info, countof(labels));
}

// The event -------------------------------------------------------------------

/* At s_link 15, after the frame's hits: what the teammate's aerial did. */
static void AfterHits(GOBJ *gobj) {
    if (!RolesReady() || Pause_CheckStatus(1) == 2) return;
    MeasureAerial(Data(mate_ply));
}

#define DPAD_BUTTONS (HSD_BUTTON_DPAD_LEFT | HSD_BUTTON_DPAD_RIGHT | HSD_BUTTON_DPAD_DOWN | HSD_BUTTON_DPAD_UP)
/* The savestate rewinds the fighter's buttons to none held, so the D-pad press
 * that restarted would look new again next frame. */
static void KeepDPadHeld(FighterData *ft) {
    int dpad = PadGetEngine(ft->pad_index)->held & DPAD_BUTTONS;
    ft->input.held |= dpad;
    ft->input.x664 |= dpad;
}
/* From the pad, not ft->input, which a restart rewinds. */
static void ReadDPad(FighterData *hmn) {
    int down = PadGetEngine(hmn->pad_index)->down;
    int side = down & HSD_BUTTON_DPAD_LEFT ? SIDE_LEFT : down & HSD_BUTTON_DPAD_RIGHT ? SIDE_RIGHT : -1;
    if (side >= 0) {
        Options_Main[OPT_SIDE].val = Options_Main[OPT_SIDE].val_prev = side;
        ChangeSetting(0, side);
    } else if (down & HSD_BUTTON_DPAD_DOWN) {
        int facing = Options_Main[OPT_FACING].val == FACING_AWAY ? FACING_TOWARD : FACING_AWAY;
        Options_Main[OPT_FACING].val = Options_Main[OPT_FACING].val_prev = facing;
        ChangeSetting(0, facing);
    }
}

static void Reset(void) {
    restart_pending = false;
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    FighterData *hmn = Data(hmn_ply), *mate = Data(mate_ply);
    int aerial = Options_Main[OPT_AERIAL].val;
    if (measured.kind != mate->kind || measured.aerial != aerial) ForgetAerial(mate->kind, aerial);
    WorkOutJumps(mate);
    GatherFloors();
    ChooseLayout();
    PlacePlayer(hmn_ply, layout.you, layout.you_line, layout.you_facing);
    PlacePlayer(mate_ply, layout.mate, layout.mate_line, layout.facing);
    BenchFollower(mate_ply);
    BenchFollower(enemy_ply);
    SetPercent(enemy_ply, BasePercent());
    SetPercent(mate_ply, 0);
    volley = (struct volley) { .last = TOUCH_NONE, .enemy_pct = BasePercent() };
    escape = (struct escape) { 0 };
    phase = PHASE_SERVE;
    phase_timer = 0;
    ResetControl();
    Vec2 spot = ServeSpot(mate);
    FloatEnemy((Vec3){spot.X, spot.Y, 0});
    MoveFloat(spot);
    Match_CorrectCamera();
    KeepDPadHeld(hmn);
}

/* The records and measurements need no clearing: this .dat is read again for
 * every match, statics and all. */
void Event_Init(GOBJ *gobj) {
    LoadSettings();
    ResetControl();
    CreateHUD();
    GObj_AddGXLink(gobj, DrawMissing, GXLINK_HUD, 80);
    GObj_AddProc(GObj_Create(0, 7, 0), AfterHits, 15);
}

void Event_Think(GOBJ *gobj) {
    if (Pause_CheckStatus(1) == 2) return;
    if (event_vars->game_timer == 1) {
        FindRoles();
        if (RolesReady()) {
            event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
            Reset();
        }
    }
    if (!RolesReady()) return;
    frame++;
    FighterData *hmn = Data(hmn_ply), *mate = Data(mate_ply), *enemy = Data(enemy_ply);
    ReadDPad(hmn);
    if (restart_pending) {
        Reset();
        return;
    }
    GatherFloors();
    // Hold the floating enemy's body on its spot as its pose changes.
    if (phase == PHASE_SERVE && Floating(enemy) && !control.serving) MoveFloat(ServeSpot(mate));
    TrackHits(enemy);
    control.frozen_now = mate->flags.freeze;

    if (phase == PHASE_VOLLEY) {
        volley.since_touch++;
        if (enemy->flags.dead || hmn->flags.dead || mate->flags.dead || enemy->phys.air_state == 0 || OnLedge(enemy) ||
            enemy->phys.pos.Y < layout.floor - FALL_LIMIT || volley.since_touch > TOUCH_TIMEOUT)
            EndVolley();
        else if (Options_Main[OPT_HITSTUN].val && CanAct(enemy))
            Escape(enemy, hmn, mate);
    } else if (phase == PHASE_RESULT) {
        if (++phase_timer >= RESULT_FRAMES) {
            Reset();
            return;
        }
    } else if (hmn->flags.dead || mate->flags.dead || enemy->flags.dead) {
        Reset();
        return;
    }
    EnemyInputs(enemy);
    TeammateThink(hmn, mate, enemy);
}

void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}
