#include "events.h"
#include "stage_points.h"
#include "stage_swap.h"

/* Target Practice (Minigames): Fox and Falco shoot targets with their blaster.
 * A target is a small bullseye somewhere over the main stage, centered on its
 * spot. Shoot it and the next one appears. A timed minute counts targets,
 * with the best saved; Free Practice has no clock, and its SH Double Lasers
 * makes about half the spawns a pair that one short hop double laser can hit.
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
 * fits, so the row reads (N/A) for him. */

#define TIMED_FRAMES 3600     // The timed run's minute.
#define TARGET_RADIUS 1.5f    // Under 2.72 - 1.17: see above.
#define SPAWN_INSET 8.f       // From the main floor's ends, and a platform's.
#define SPAWN_FROM_YOU 30.f
#define SPAWN_FROM_LAST 25.f
#define SPAWN_TRIES 40
#define MAX_TARGETS 2
#define POP_FRAMES 12         // A broken target's ring.
#define CIRCLE_STEPS 20
#define HOP_FRAMES 64         // Longer than any short hop.
#define CAMERA_MARGIN 10.f

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
    OPT_RETRY, OPT_STAGE, OPT_FREEPRACTICE, OPT_SHDL, OPT_COUNTER, OPT_DEFAULTS, OPT_EXIT, OPT_COUNT
};

static void Retry(GOBJ *menu);
static void Exit(GOBJ *menu);
static void ChangeFreePractice(GOBJ *menu, int value);
static void ChangeDoubleLasers(GOBJ *menu, int value);
static void ChangeCounterPosition(GOBJ *menu, int value);
static void ResetSettings(GOBJ *menu);

static const char *CounterPositionText[TM_EGGS_COUNTER_COUNT] = {
    "Top Left", "Top Right", "Bottom Left", "Bottom Right", "Under Timer"
};
static char DoubleLasersName[] = "SH Double Lasers";
static char DoubleLasersUnavailable[] = "SH Double Lasers (N/A)";

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
        .desc = {"About half the targets come in pairs, one at a",
                 "short hop's peak and one at a low laser's height,",
                 "for one short hop double laser. Fox only: Falco's",
                 "laser is too slow. Free Practice only. Saved."},
        .disable = 1,
        .OnChange = ChangeDoubleLasers,
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
static int score, best;
static bool reset_clock;
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
static float FullHopDoubleJump(FighterData *ft) {
    float jump = ft->attr.jump_v_initial_velocity;
    return Rise(jump, ft->attr.gravity) + Rise(jump * ft->attr.air_jump_v_multiplier, ft->attr.gravity);
}

/* Height after each airborne frame of a short hop with no fastfall, frame 1
 * the first. */
static void ShortHop(FighterData *ft, float *y) {
    float v = ft->attr.hop_v_initial_velocity, gravity = ft->attr.gravity, fall = ft->attr.terminal_velocity;
    y[0] = 0;
    for (int frame = 1; frame < HOP_FRAMES; ++frame) {
        v -= gravity;
        if (v < -fall) v = -fall;
        y[frame] = y[frame - 1] + v;
    }
}
/* B on airborne frame `press` shoots on press + delay and, looped, a Loop
 * later, each from where the fighter was the frame before. 0 if it lands
 * before the second shot. */
static int ShortHopShots(FighterData *ft, const Blaster *blaster, int press, float *high, float *low) {
    float y[HOP_FRAMES];
    int first = press + blaster->delay, second = first + blaster->loop;
    if (press < 1 || second >= HOP_FRAMES) return 0;
    ShortHop(ft, y);
    /* Past the apex each frame is lower than the last, so the frame before
     * the second shot decides whether it's still airborne. */
    if (y[second - 1] + blaster->ecb_bottom <= 0) return 0;
    *high = y[first - 1] + blaster->air;
    *low = y[second - 1] + blaster->air;
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

// Settings --------------------------------------------------------------------

static bool FreePractice(void) { return stc_match->match.timer == MATCH_TIMER_COUNTUP; }
static bool PairsOn(void) { return FreePractice() && Options_Main[OPT_SHDL].val && pair_presses > 0; }

/* Free Practice only, and only for a fighter whose short hop fits two lasers,
 * which isn't known until its first frame. */
static void GateDoubleLasers(void) {
    Options_Main[OPT_SHDL].disable = !FreePractice() || !pair_presses;
    Options_Main[OPT_SHDL].name = checked && !pair_presses ? DoubleLasersUnavailable : DoubleLasersName;
}
static void LoadSettings(void) {
    Options_Main[OPT_FREEPRACTICE].val = Options_Main[OPT_FREEPRACTICE].val_prev =
        TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_FREE_PRACTICE);
    Options_Main[OPT_SHDL].val = Options_Main[OPT_SHDL].val_prev =
        TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_SHDL);
    Options_Main[OPT_COUNTER].val = Options_Main[OPT_COUNTER].val_prev = TM_GetSetting(TM_SETTING_EGGS_COUNTER, 0);
    best = TM_GetSetting(TM_SETTING_TARGETS, TM_TARGETS_BEST);
}
// The next targets to appear follow it; the ones out stay.
static void ChangeDoubleLasers(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_SHDL, value);
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
    GateDoubleLasers();
}
static void ResetSettings(GOBJ *menu) {
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_FREE_PRACTICE, 0);
    TM_SetSetting(TM_SETTING_TARGETS, TM_TARGETS_SHDL, 0);
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
/* A pair over a floor at some x, the main floor or a platform, at the heights
 * of one short hop double laser from it. */
static void PickPair(FighterData *ft, Vec2 you, Vec2 *upper, Vec2 *lower) {
    float high, low, left, right;
    ShortHopShots(ft, BlasterOf(ft), 1 + HSD_Randi(pair_presses), &high, &low);
    GroundEdges(&left, &right);
    for (int tries = 0; tries < SPAWN_TRIES; ++tries) {
        float floors[8], x = RandomRange(left + SPAWN_INSET, right - SPAWN_INSET);
        int count = 0;
        Vec3 ground;
        int line;
        if (FindSolidGround(x, &ground, &line)) floors[count++] = ground.Y;
        for (int i = 0; i < LineCount() && count < 8; ++i) {
            Vec2 a, b;
            int platform;
            float y;
            if (!FloorLine(i, &a, &b, &platform) || !platform || x < a.X + SPAWN_INSET || x > b.X - SPAWN_INSET ||
                !LineY(i, x, &y)) continue;
            floors[count++] = y;
        }
        if (!count) continue;
        float floor = floors[HSD_Randi(count)];
        Vec2 top = {x, floor + high}, bottom = {x, floor + low};
        if (!Clear(top, you, tries) || !Clear(bottom, you, tries)) continue;
        *upper = top;
        *lower = bottom;
        return;
    }
    Vec3 ground;
    int line;
    GroundPoint(0, &ground, &line);
    *upper = (Vec2){0, ground.Y + high};
    *lower = (Vec2){0, ground.Y + low};
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

static void Spawn(FighterData *ft) {
    Vec2 you = {ft->phys.pos.X, ft->phys.pos.Y};
    for (int i = 0; i < MAX_TARGETS; ++i) targets[i].live = false;
    if (PairsOn() && HSD_Randi(2)) {
        PickPair(ft, you, &targets[0].pos, &targets[1].pos);
        targets[1].live = true;
    } else {
        PickSingle(ft, you, &targets[0].pos);
    }
    targets[0].live = true;
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
static void Targets_GX(GOBJ *gobj, int pass) {
    if (pass != 2) return;
    for (int i = 0; i < MAX_TARGETS; ++i)
        if (pops[i].age > 0) DrawPop(&pops[i]);
    for (int i = 0; i < MAX_TARGETS; ++i)
        if (targets[i].live) DrawTarget(targets[i].pos);
}

// Event -----------------------------------------------------------------------

/* Pairs depend on the fighter, which isn't known until its first frame. */
static void CheckFighter(FighterData *ft) {
    checked = true;
    pair_presses = ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO ? ShortHopPresses(ft, BlasterOf(ft)) : 0;
    GateDoubleLasers();
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
    placed = checked = reset_clock = false;
    pair_presses = score = 0;
    LoadSettings();
    // A retry may reuse the module: rebuild the gating from the saved mode.
    Options_Main[OPT_SHDL].disable = 1;
    Options_Main[OPT_SHDL].name = DoubleLasersName;
    if (Options_Main[OPT_FREEPRACTICE].val) EnterFreePractice();
    StageSwap_Init();
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
        if (ft->flags.input_enable && (!placed || (!targets[0].live && !targets[1].live))) Spawn(ft);
        AimCamera();
    }
    UpdateHUD();
    if (stc_match->time_frames == TIMED_FRAMES && !FreePractice())
        FinishTimedRun();
}
