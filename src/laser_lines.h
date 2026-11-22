#ifndef TM_LASER_LINES_H
#define TM_LASER_LINES_H
/* Fox's and Falco's laser heights, and lines across the stage's floors where a
 * laser flies. Shared by Target Practice (its Hop Peak Lines and Lowest Laser
 * Lines) and Low Laser Puff Practice (its Lowest Laser Lines). How the heights
 * are measured is in src/targets.c's header comment. */
#include "stage_points.h"

/* Laser heights above TopN, the airborne ECB bottom on the frame before the
 * aerial shot, and the frames from a B press to the aerial shot and between
 * looped shots. Measured by scripts/laser_heights.py, which
 * tests/test_targets.py runs against melee.iso. */
typedef struct Blaster {
    float air, ground, ecb_bottom;
    int delay, loop;
} Blaster;
static const Blaster Blasters[2] = {
    {9.5471f, 7.6407f, 4.4322f, 9, 10},  // Fox
    {9.3201f, 8.0905f, 5.2207f, 12, 16}, // Falco
};

static inline const Blaster *BlasterOf(FighterData *ft) { return &Blasters[ft->kind == FTKIND_FALCO]; }

/* The lowest laser above the floor it's shot from: TopN as far under the
 * floor as the ECB bottom allows, never under the floor itself. */
static inline float LowestLaser(const Blaster *blaster) {
    float height = blaster->air - blaster->ecb_bottom;
    return height > 0 ? height : 0;
}

#define PEAK_LINE_WIDTH 12    // 2 pixels, in sixths.
#define PEAK_FLOORS 4         // The main floor and up to three platforms.
/* A side platform closer than this to the main floor is sunk: Fountain's
 * settle 0.37 above it, inside the stage, while they're hidden. */
#define SUNK_PLATFORM 1.f

/* The line colors: the OSD palette's (osd_style.c) White to Lavender,
 * TM_COLOR_WHITE + the index, and each one's RGB. */
static const u8 LineColors[TM_TARGETS_COLOR_COUNT][3] = {
    {255, 255, 255}, {255, 70, 70}, {141, 255, 110}, {70, 145, 255}, {255, 240, 0}, {0, 255, 255},
    {255, 80, 255}, {255, 150, 40}, {255, 140, 200}, {160, 100, 255}, {0, 180, 160}, {200, 255, 50},
    {255, 200, 50}, {120, 200, 255}, {200, 160, 255},
};

/* A floor's line in line color `value` (LineColors), at alpha 230,
 * premultiplied. Darker over the main floor (3/5 of each channel), as picked
 * over the side platforms, and lighter over the top platform (halfway to
 * white). */
#define PEAK_ALPHA 230
enum { SHADE_DARK, SHADE_BASE, SHADE_LIGHT };
static inline GXColor ShadedColor(int value, int shade) {
    u8 rgb[3];
    for (int i = 0; i < 3; ++i) {
        int v = LineColors[value][i];
        v = shade == SHADE_DARK ? v * 3 / 5 : shade == SHADE_LIGHT ? v + (255 - v) / 2 : v;
        rgb[i] = v * PEAK_ALPHA / 255;
    }
    return (GXColor){rgb[0], rgb[1], rgb[2], PEAK_ALPHA};
}

/* Each floor's line, from left to right x at its height, and its shade: the
 * main floor at the middle (dark), then each platform FindPlatform finds
 * (left, top, right) above it, a side one (base) only on its side of x = 0,
 * the top one light. */
typedef struct PeakFloor { float left, right, y; int shade; } PeakFloor;
static inline int PeakFloors(PeakFloor *floors) {
    Vec3 ground, at;
    int line, count = 0;
    if (!FindSolidGround(0, &ground, &line)) return 0;
    float left = Stage_GetBlastzoneLeft(), right = Stage_GetBlastzoneRight();
    floors[count++] = (PeakFloor){left, right, ground.Y, SHADE_DARK};
    for (int side = -1; side <= 1; ++side)
        if (FindPlatform(side, &at, &line) && at.Y >= ground.Y + SUNK_PLATFORM)
            floors[count++] = (PeakFloor){side > 0 ? 0 : left, side < 0 ? 0 : right, at.Y,
                                          side ? SHADE_BASE : SHADE_LIGHT};
    return count;
}
/* `lines` lines over each floor in one GX_LINES batch, line i at heights[i]
 * above it in line color colors[i], shaded for the floor. 2 pixels wide, with
 * no depth test so the stage never hides them. */
static inline void DrawFloorLines(int lines, const float *heights, const int *colors) {
    PeakFloor floors[PEAK_FLOORS];
    int count = PeakFloors(floors);
    if (!count) return;
    event_vars->GFX_Start(count * lines * 2,
                          (GFX_Params){.shape = GX_LINES, .size = PEAK_LINE_WIDTH, .no_depth = true});
    for (int f = 0; f < count; ++f)
        for (int i = 0; i < lines; ++i) {
            float y = floors[f].y + heights[i];
            GXColor color = ShadedColor(colors[i], floors[f].shade);
            GFX_AddVtx(floors[f].left, y, 0, color);
            GFX_AddVtx(floors[f].right, y, 0, color);
        }
}
/* The lowest laser's line over each floor, for Fox or Falco, in line color
 * `value`. */
static inline void DrawLowestLaserLines(FighterData *ft, int value) {
    float height = LowestLaser(BlasterOf(ft));
    DrawFloorLines(1, &height, &value);
}

#endif
