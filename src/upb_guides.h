#ifndef TM_UPB_GUIDES_H
#define TM_UPB_GUIDES_H
/* Up-B sweetspot guides: each fighter's up-B, and where it has to start from to
 * grab a ledge. Shared by Up-B Sweetspot (its line guides, which add the live
 * lines, B0XX Angles and the Landing Line) and Edgeguard (its Player and CPU
 * Up-B Overlays, which draw UpB_DrawOverlay at both ledges). Everything here is
 * static inline, so each event's DAT gets its own copy. The guides and every
 * number behind them are in notes/UPB-SWEETSPOT-RESEARCH.md, and the Slippi
 * Up-B Overlay codes (slippi-gecko-codes/up-b-overlay) draw the same shapes. */
#include "events.h"

enum guide {
    GUIDE_NONE,  // The ledge target only.
    GUIDE_AIMED, // A ring at the travel distance, and the angle you're holding.
    GUIDE_PATH,  // Where an animated path should start.
};

/* Each fighter's up-B. States are its action states from the decompilation,
 * grounded and aerial; clones share their original's numbers. Path numbers
 * are the root motion in the ISO (scripts/fighter_move_data.py) at normal
 * size: the rise and forward movement at the peak, the fastest grab, and at
 * the end of the animation, a slower grab falling back to the ledge. Samus's
 * air Screw Attack is physics instead: 2.5 up against 0.066 gravity.
 * tilt_attr is the special attribute holding how many degrees holding toward
 * the stage turns the move before it locks; FullTilt has Game & Watch's. */
typedef struct UpBInfo {
    u16 first, last;
    u8 guide;
    u8 tilt_attr;          // GUIDE_PATH; 0 for none.
    u16 charge;            // GUIDE_AIMED: the aerial start-up, which reads the stick as it ends.
    float rise, forward;   // GUIDE_PATH
    float end_rise, end_forward;
} UpBInfo;

static const UpBInfo UpB_Info[FTKIND_ROY + 1] = {
    [FTKIND_MARIO]     = { 347, 348, GUIDE_PATH, 0x28, 0, 40.4f, 17.6f, 33.3f, 20.9f },
    [FTKIND_DRMARIO]   = { 347, 348, GUIDE_PATH, 0x28, 0, 40.4f, 17.6f, 33.3f, 20.9f },
    [FTKIND_LUIGI]     = { 355, 356, GUIDE_PATH, 0, 0, 45.9f, 0, 37.9f, 0 },
    [FTKIND_MARTH]     = { 367, 368, GUIDE_PATH, 0x38, 0, 47.3f, 10.2f, 34.5f, 11.5f },
    [FTKIND_ROY]       = { 367, 368, GUIDE_PATH, 0x38, 0, 44.5f, 12.3f, 32.4f, 13.0f },
    [FTKIND_FALCON]    = { 353, 356, GUIDE_PATH, 0, 0, 37.6f, 11.9f, 13.1f, 20.2f },
    [FTKIND_GANONDORF] = { 353, 356, GUIDE_PATH, 0, 0, 41.9f, 13.2f, 14.6f, 22.5f },
    [FTKIND_PEACH]     = { 361, 364, GUIDE_PATH, 0, 0, 36.8f, 3.4f, 34.4f, 4.1f },
    [FTKIND_KIRBY]     = { 385, 392, GUIDE_PATH, 0, 0, 52.2f, 0, 0.3f, 0 },
    [FTKIND_SAMUS]     = { 353, 354, GUIDE_PATH, 0, 0, 47.3f, 0, 44.0f, 0 },
    [FTKIND_GAW]       = { 373, 374, GUIDE_PATH, 0, 0, 75.6f, 9.1f, 65.8f, 11.9f },
    [FTKIND_FOX]       = { 353, 359, GUIDE_AIMED, 0, 354 },
    [FTKIND_FALCO]     = { 353, 359, GUIDE_AIMED, 0, 354 },
    [FTKIND_SHEIK]     = { 355, 360, GUIDE_AIMED, 0, 358 },
    [FTKIND_ZELDA]     = { 349, 354, GUIDE_AIMED, 0, 352 },
    [FTKIND_MEWTWO]    = { 353, 358, GUIDE_AIMED, 0, 356 },
    [FTKIND_PIKACHU]   = { 353, 358, GUIDE_AIMED, 0, 356 },
    [FTKIND_PICHU]     = { 353, 358, GUIDE_AIMED, 0, 356 },
    [FTKIND_DK]        = { 381, 382, GUIDE_NONE },
    [FTKIND_BOWSER]    = { 359, 360, GUIDE_NONE },
    [FTKIND_LINK]      = { 356, 357, GUIDE_NONE },
    [FTKIND_YOUNGLINK] = { 356, 357, GUIDE_NONE },
    [FTKIND_NESS]      = { 358, 366, GUIDE_NONE },
    [FTKIND_POPO]      = { 347, 356, GUIDE_NONE },
    [FTKIND_NANA]      = { 347, 356, GUIDE_NONE },
    // Yoshi and Jigglypuff have no recovery up-B.
};

static inline const UpBInfo *GetUpBInfo(FighterData *ft) {
    if (ft->kind < 0 || ft->kind > FTKIND_ROY || !UpB_Info[ft->kind].last)
        return 0;
    return &UpB_Info[ft->kind];
}

// An aimed or path guide: everyone but the ledge-target-only fighters, Yoshi and Jigglypuff.
static inline bool UpB_HasGuide(FighterData *ft) {
    const UpBInfo *info = GetUpBInfo(ft);
    return info && info->guide != GUIDE_NONE;
}

static inline bool IsSpacie(FighterData *ft) {
    return ft->kind == FTKIND_FOX || ft->kind == FTKIND_FALCO;
}

#define ATTR_F(ft, offset) (((float *)(ft)->special_attributes)[(offset) / 4])
#define ATTR_I(ft, offset) (((int *)(ft)->special_attributes)[(offset) / 4])

// Degrees a full stick toward the stage turns a path, or 0 if it can't. Game &
// Watch's attribute is radians per unit of stick past 0x58, not a top angle.
static inline float FullTilt(FighterData *ft, const UpBInfo *info) {
    if (ft->kind == FTKIND_GAW)
        return (1 - ATTR_F(ft, 0x58)) * ATTR_F(ft, 0x5C) / M_1DEGREE;
    return info->tilt_attr ? ATTR_F(ft, info->tilt_attr) : 0;
}

/* The least stick the aimed up-Bs take before they go straight up. Fox and
 * Falco add the two axes (|x| + |y|); the rest use the stick's length. */
static inline float AimedThreshold(FighterData *ft) {
    switch (ft->kind) {
        case FTKIND_FOX: case FTKIND_FALCO: return ATTR_F(ft, 0x64);
        case FTKIND_SHEIK: return ATTR_F(ft, 0x40);
        case FTKIND_ZELDA: return ATTR_F(ft, 0x50);
        case FTKIND_MEWTWO: return ATTR_F(ft, 0x58);
        case FTKIND_PIKACHU: case FTKIND_PICHU: return ATTR_F(ft, 0x8C);
    }
    return 1;
}

/* The teleports and Quick Attack's first zip move at a constant speed for a
 * set number of frames: speed = per_tilt * tilt + base. */
static inline int TravelFrames(FighterData *ft) {
    switch (ft->kind) {
        case FTKIND_SHEIK: return ATTR_I(ft, 0x38);
        case FTKIND_ZELDA: return ATTR_I(ft, 0x48);
        case FTKIND_MEWTWO: return ATTR_I(ft, 0x50);
        case FTKIND_PIKACHU: case FTKIND_PICHU: return ATTR_I(ft, 0x60);
    }
    return 0;
}

static inline float TravelSpeed(FighterData *ft, float tilt) {
    switch (ft->kind) {
        case FTKIND_SHEIK: return ATTR_F(ft, 0x44) * tilt + ATTR_F(ft, 0x48);
        case FTKIND_ZELDA: return ATTR_F(ft, 0x54) * tilt + ATTR_F(ft, 0x58);
        case FTKIND_MEWTWO: return ATTR_F(ft, 0x5C) * tilt + ATTR_F(ft, 0x60);
        case FTKIND_PIKACHU: case FTKIND_PICHU: return ATTR_F(ft, 0x90) * tilt + ATTR_F(ft, 0x94);
    }
    return 0;
}

/* How far the travel goes, from the live attributes, at a stick length of
 * tilt (the threshold up to 1).
 * - Firefox and Firebird ignore tilt: x74 speed for x68 frames, slowing by x78
 *   a frame from frame x70. Fox 81.5, Falco 60.1.
 * - The rest: Sheik 30-40, Zelda 62-84, Mewtwo 40-50, Pikachu 39.5-49.5,
 *   Pichu 26.4-35.2. */
static inline float AimedDistance(FighterData *ft, float tilt) {
    if (!IsSpacie(ft))
        return TravelFrames(ft) * TravelSpeed(ft, tilt);
    float speed = ATTR_F(ft, 0x74), slow = ATTR_F(ft, 0x78), distance = 0;
    int frames = ATTR_F(ft, 0x68), slow_from = ATTR_F(ft, 0x70);
    for (int i = 1; i <= frames; i++) {
        if (i >= slow_from)
            speed -= slow;
        distance += speed;
    }
    return distance;
}

// The ledge target ------------------------------------------------------

/* How far each fighter's ECB reaches toward the ledge standing: Wait1, which
 * the Up-B Sweetspot platform's RebirthWait plays, averaged over its loop with
 * the skeleton posed from the ISO (mpColl_LoadECB_JObj's six joints, centered
 * when narrower than 10, each side at least 2 out). Up-B Sweetspot reads the
 * live ECB on its platform instead; Edgeguard's fighters are never standing
 * still when it's needed, so its overlays use these, as the Gecko codes do.
 * They're scripts/upb_overlay_numbers.py's CHARACTERS, plus Game & Watch,
 * measured the same way (scripts/fighter_ecb_pose.py PlGw PlGwNr.dat Wait1).
 * Anyone else is at the game's 2-unit minimum here. */
#define ECB_FRONT_MIN 2.0f
static const float UpB_StandingFront[FTKIND_ROY + 1] = {
    [FTKIND_FOX] = 2.00f, [FTKIND_FALCO] = 2.30f, [FTKIND_MARTH] = 2.00f, [FTKIND_SHEIK] = 3.07f,
    [FTKIND_FALCON] = 4.07f, [FTKIND_PEACH] = 2.00f, [FTKIND_PIKACHU] = 2.00f, [FTKIND_SAMUS] = 2.03f,
    [FTKIND_LUIGI] = 2.36f, [FTKIND_MARIO] = 2.15f, [FTKIND_DRMARIO] = 2.14f, [FTKIND_GANONDORF] = 5.67f,
    [FTKIND_MEWTWO] = 3.14f, [FTKIND_ZELDA] = 2.00f, [FTKIND_ROY] = 2.21f, [FTKIND_PICHU] = 2.00f,
    [FTKIND_KIRBY] = 2.00f, [FTKIND_GAW] = 2.18f,
};

static inline float UpB_StandingECBFront(FighterData *ft) {
    if (ft->kind < 0 || ft->kind > FTKIND_ROY || !UpB_StandingFront[ft->kind])
        return ECB_FRONT_MIN;
    return UpB_StandingFront[ft->kind];
}

/* The ledge-grab box (mpColl_80044164) reaches cliffgrab_width past the ECB's
 * side toward the ledge (`front`, the side it faces), and is cliffgrab_height
 * tall, centered cliffgrab_y_offset above the feet. So it holds the ledge while
 * your position is from the ledge out to that reach, and from
 * cliffgrab_y_offset ± cliffgrab_height / 2 below it.
 * - The X (target) is the middle of that: the most room for an up-B that ends
 *   there, level or rising, and then falls into the grab.
 * - The game checks the box only while you move down, and then grabs as soon as
 *   you're in it. So coming down, the travel grabs at the top of the box, on its
 *   way to the X: snap is that spot, above the X.
 * outward is 1 off the right ledge and -1 off the left. */
typedef struct UpBTargets {
    Vec2 target, snap;
    float reach;  // The grab box's reach out from the ledge.
} UpBTargets;

static inline UpBTargets UpB_GetTargets(FighterData *ft, Vec2 ledge, float outward, float front) {
    CollData *coll = &ft->coll_data;
    UpBTargets t;
    t.reach = front + coll->cliffgrab_width;
    t.target = (Vec2) { ledge.X + outward * t.reach * 0.5f, ledge.Y - coll->cliffgrab_y_offset };
    t.snap = (Vec2) { t.target.X, t.target.Y + coll->cliffgrab_height * 0.5f };
    return t;
}

// Drawing ---------------------------------------------------------------

#define ARC_STEP 4 // Degrees of ring per line.
// Fox's and Falco's ring: 2/3 of a circle, ending 30 degrees short of level
// under the stage so it doesn't stick up into the stage.
#define UNDER_STAGE_SWEEP 240
// Half the length of Fox's and Falco's ticks where holding straight sweetspots.
#define LEVEL_TICK 6
// Degrees either side of each straight mark that a bit of ring runs through
// it, at the mark's distance.
#define STRAIGHT_ARC 2
// Half the width of the X, and of the bar over it where coming down snaps.
#define TARGET_SIZE 2.5f

static const GXColor White = { 0xff, 0xff, 0xff, 0xff };
static const GXColor Blue = { 0x30, 0xc8, 0xff, 0xff };
static const GXColor DimBlue = { 0x1c, 0x6c, 0x94, 0xff };

/* A guide's colors: the ring at the full travel and the fastest path, the
 * least-tilt edge and the path for holding toward the stage, and the X, the
 * bar and the straight marks. */
typedef struct UpBColors {
    GXColor full, dim, mark;
} UpBColors;
static const UpBColors UpB_Colors = {
    { 0x30, 0xc8, 0xff, 0xff }, { 0x1c, 0x6c, 0x94, 0xff }, { 0xff, 0xff, 0xff, 0xff },
};

/* on_top draws over the stage, which is what Fox's and Falco's marks need to be
 * seen from under it. */
static inline void DrawLine(Vec2 a, Vec2 b, GXColor color, int width, bool on_top) {
    event_vars->GFX_Start(2, (GFX_Params) { .shape = GX_LINES, .size = width, .no_depth = on_top });
    GFX_AddVtx(a.X, a.Y, 0, color);
    GFX_AddVtx(b.X, b.Y, 0, color);
}

static inline void DrawDot(Vec2 at, GXColor color, bool on_top) {
    event_vars->GFX_Start(1, (GFX_Params) { .shape = GX_POINTS, .size = 42, .no_depth = on_top });
    GFX_AddVtx(at.X, at.Y, 0, color);
}

static inline void DrawCross(Vec2 at, float size, GXColor color) {
    event_vars->GFX_Start(4, (GFX_Params) { .shape = GX_LINES, .size = 12 });
    GFX_AddVtx(at.X - size, at.Y - size, 0, color);
    GFX_AddVtx(at.X + size, at.Y + size, 0, color);
    GFX_AddVtx(at.X - size, at.Y + size, 0, color);
    GFX_AddVtx(at.X + size, at.Y - size, 0, color);
}

/* The aimed up-Bs all take their angle from the stick the game gives fighters
 * (atan2 of input.lstick). It reads in 80ths of a full tilt, inside a circle of
 * radius 80 (gmmain.c's pad clamp), and Fighter_procInput reads an axis at or
 * under its deadzone as 0 (PlCo.dat: 0.28 across and up). So a tilted axis
 * reads at least 23/80, and the shallowest angle off straight is atan(23 / 76),
 * 16.8 degrees, 76 being the most the other axis can then read inside the
 * circle. Nothing between can be aimed: near straight reads straight. */
#define STICK_UNITS 80

typedef struct Stick {
    float least_x, least_y;       // The least a tilted axis reads, in 80ths.
    float gap_level, gap_upright; // Degrees from straight across and straight
                                  // up to the shallowest angle off them.
} Stick;

// The step past the deadzone, in 80ths.
static inline float LeastAxis(float deadzone) { return (int)(deadzone * STICK_UNITS + 1); }

/* Degrees from straight to the shallowest angle, when the axis across straight
 * reads `least` and the other as much as the circle lets it. */
static inline float Gap(float least) {
    return atan2(least, (int)sqrtf(STICK_UNITS * STICK_UNITS - least * least)) / M_1DEGREE;
}

static inline Stick GetStick(void) {
    ftCommonData *common = *stc_ftcommon;
    float x = LeastAxis(common->x0), y = LeastAxis(common->x4);
    return (Stick) { x, y, Gap(y), Gap(x) };
}

/* Ring angles are degrees around the X: 90 straight above it, 0 straight out
 * past the ledge, -90 straight below. Up-B from there, aimed at the X, holds
 * the stick the other way. This is the least tilt that aims along `angle`:
 * past the up-B's threshold, with both axes reading at least their least. */
static inline float LeastTilt(FighterData *ft, const Stick *stick, float angle) {
    float a = angle * M_1DEGREE, tilt = AimedThreshold(ft);
    float up = stick->least_y / STICK_UNITS / fabs(sin(a));
    float across = stick->least_x / STICK_UNITS / fabs(cos(a));
    if (up > tilt)
        tilt = up;
    if (across > tilt)
        tilt = across;
    return tilt > 1 ? 1 : tilt;
}

/* The stretch of the quarter below straight direction `top` (90, 0 or -90)
 * the stick can aim, down to `bottom`: a gap off each end. */
static inline bool Quarter(const Stick *stick, int top, float bottom, float *from, float *to) {
    bool upright = top != 0;
    *from = top - (upright ? stick->gap_upright : stick->gap_level);
    *to = top - 90 + (upright ? stick->gap_level : stick->gap_upright);
    if (*to < bottom)
        *to = bottom;
    return *from > *to;
}

/* How far round the ring goes from straight above the X. Fox and Falco can
 * recover from under the stage, so theirs carries on under it; the rest get
 * the offstage half. */
static inline float UpB_Sweep(FighterData *ft) {
    return IsSpacie(ft) ? UNDER_STAGE_SWEEP : 180;
}

/* A stretch of ring around center, from ring angle `from` down to `to`, at
 * radius, or with `least` at the least tilt that aims each way. */
static inline void DrawRingArc(FighterData *ft, const Stick *stick, Vec2 center, float from, float to,
                               float radius, bool least, GXColor color, bool on_top, float outward) {
    int steps = (int)((from - to) / ARC_STEP) + 1;
    if (least)
        steps *= 2; // It curves fastest at the ends.
    event_vars->GFX_Start(steps + 1, (GFX_Params) { .shape = GX_LINESTRIP, .size = 10, .no_depth = on_top });
    for (int i = 0; i <= steps; i++) {
        float angle = from - (from - to) * i / steps;
        if (least)
            radius = AimedDistance(ft, LeastTilt(ft, stick, angle));
        float a = angle * M_1DEGREE;
        GFX_AddVtx(center.X + outward * cos(a) * radius, center.Y + sin(a) * radius, 0, color);
    }
}

/* The ring at the travel around the X, sweep degrees from straight above it
 * out past the ledge and down, less the angles the stick can't aim: a gap
 * either side of straight across, up and down. A mark on each straight
 * direction is where holding straight sweetspots, with a bit of ring through
 * it at its distance. The teleports and Quick Attack go farther with more
 * tilt, so a dim edge inside follows the least tilt that aims each way, and
 * their marks run between the two, with a bit of each at its ends.
 * Above the X the up-B comes down, and grabs at the snap on its way to the X,
 * so the quarter above it, and the mark straight above, go around the snap. */
static inline void DrawAimedRing(FighterData *ft, Vec2 target, Vec2 snap, float sweep, bool on_top,
                                 float outward, const UpBColors *colors) {
    Stick stick = GetStick();
    bool spacie = IsSpacie(ft);
    float bottom = 90 - sweep, from, to;
    float travel = AimedDistance(ft, 1), least = spacie ? travel : AimedDistance(ft, AimedThreshold(ft));
    for (int full = spacie; full < 2; full++) {
        GXColor color = full ? colors->full : colors->dim;
        for (int top = 90; top > bottom; top -= 90)
            if (Quarter(&stick, top, bottom, &from, &to))
                DrawRingArc(ft, &stick, top == 90 ? snap : target, from, to, travel, !full, color, on_top, outward);
        for (int straight = 90; straight >= bottom; straight -= 90)
            DrawRingArc(ft, &stick, straight == 90 ? snap : target, straight + STRAIGHT_ARC,
                        straight - STRAIGHT_ARC, full ? travel : least, false, color, on_top, outward);
    }
    float inner = spacie ? travel - LEVEL_TICK : least;
    float outer = spacie ? travel + LEVEL_TICK : travel;
    int marks = 0;
    for (int straight = 90; straight >= bottom; straight -= 90)
        marks++;
    event_vars->GFX_Start(marks * 2, (GFX_Params) { .shape = GX_LINES, .size = 12, .no_depth = on_top });
    for (int straight = 90; straight >= bottom; straight -= 90) {
        float a = straight * M_1DEGREE, x = outward * cos(a), y = sin(a);
        Vec2 center = straight == 90 ? snap : target;
        GFX_AddVtx(center.X + x * inner, center.Y + y * inner, 0, colors->mark);
        GFX_AddVtx(center.X + x * outer, center.Y + y * outer, 0, colors->mark);
    }
}

// The bar over the X, where coming down snaps to the ledge.
static inline void DrawSnapBar(Vec2 snap, GXColor color) {
    DrawLine((Vec2) { snap.X - TARGET_SIZE, snap.Y }, (Vec2) { snap.X + TARGET_SIZE, snap.Y }, color, 12, false);
}

// Where to start, with the path turned `tilt` degrees toward the stage.
static inline void DrawPathStart(const UpBInfo *info, Vec2 target, float scale, float tilt, GXColor color,
                                 float outward) {
    float a = tilt * M_1DEGREE, c = cos(a), s = sin(a), out = outward * scale;
    Vec2 fast = { target.X + out * (info->forward * c + info->rise * s),
                  target.Y - scale * (info->rise * c - info->forward * s) };
    Vec2 slow = { target.X + out * (info->end_forward * c + info->end_rise * s),
                  target.Y - scale * (info->end_rise * c - info->end_forward * s) };
    DrawLine(fast, slow, color, 12, false);
    DrawDot(fast, color, false);
}

/* A path's starts: the dim one for holding toward the stage, if the move
 * turns that way, then the blue one with the stick held up. */
static inline void DrawPathStarts(FighterData *ft, const UpBInfo *info, Vec2 target, float outward,
                                  const UpBColors *colors) {
    float scale = ft->scale.Y, tilt = FullTilt(ft, info);
    if (tilt)
        DrawPathStart(info, target, scale, tilt, colors->dim, outward);
    DrawPathStart(info, target, scale, 0, colors->full, outward);
}

/* The whole guide at one ledge, as the Slippi Up-B Overlay codes draw it: the
 * ring and the bar over the X for an aimed up-B, or the path starts, then the
 * X, all from the fighter's standing ECB. Fighters without a guide get nothing.
 * Draw with the match camera current. */
static inline void UpB_DrawOverlay(FighterData *ft, Vec2 ledge, float outward, const UpBColors *colors) {
    const UpBInfo *info = GetUpBInfo(ft);
    if (!info || info->guide == GUIDE_NONE)
        return;
    UpBTargets t = UpB_GetTargets(ft, ledge, outward, UpB_StandingECBFront(ft));
    if (info->guide == GUIDE_AIMED) {
        DrawAimedRing(ft, t.target, t.snap, UpB_Sweep(ft), IsSpacie(ft), outward, colors);
        DrawSnapBar(t.snap, colors->mark);
    } else {
        DrawPathStarts(ft, info, t.target, outward, colors);
    }
    DrawCross(t.target, TARGET_SIZE, colors->mark);
}

#endif
