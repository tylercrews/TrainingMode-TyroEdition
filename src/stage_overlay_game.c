#include "events.h"

/* Global stage overlays, drawn by GX callbacks created at match start: one in
 * the match camera for the stage, one in the HUD camera for the timer.
 *
 * Blast Zone Overlay: on every stage, a crisp red line on the blast zones and
 * a red glow fading out past them.
 *
 * Randall Overlay: Yoshi's Story's cloud (map GObj 2) rides a square track and
 * passes behind the stage. Draw its live platform and a cloud silhouette on top
 * of everything so its position is always readable. The platform comes from the
 * moving collision group attached to Randall's joints; the joint's world
 * position is the fallback when no group is found.
 *
 * Randall Timer: on Yoshi's Story, a small map of Randall's loop runs through
 * the match timer, two dotted lines joined past each side of it, with a red
 * arrow where Randall is, pointing the way it moves. Randall's cloud joint
 * (GrSt.dat's root's first child) rides a path animation, every 1200 frames,
 * counter-clockwise: right along the bottom, out past the stage's right wall,
 * up, back left along the top, out past the left wall and down. GrSt.dat's
 * walls at both of Randall's heights are at X +/-75.25, which the map root's
 * grGroundParam scale (0.7) puts at +/-52.675. The map stretches the stage
 * between its walls to the timer's width and keeps the scale past them, and
 * Randall's speed is constant, so the arrow is behind the numbers exactly as
 * long as Randall is behind the stage, and on the track past the timer exactly
 * as long as it's out (see docs/RANDALL-TIMER.md). It's drawn from the live
 * cloud, so it starts where Randall starts and never drifts. With no Randall
 * (hazards off), it's where he'd be at the timer's time, as a match keeps him:
 * RANDALL_TIMER_OFFSET frames ahead of the timer, at a constant speed.
 *
 * RANDALL_CLOUD_JOINT (events.h) is the joint grStory_801E3370 keeps as
 * Randall's, for its puffs. The map GObj's JObj is the scale joint the game
 * puts over GrSt.dat's tree (801C1768), and Stage_GetMapGObjJObj counts from
 * the file's root as 0, so 1 is the root's first child: the cloud. (The map
 * GObj's JObj's own child is that root, which never moves.) The Training Lab
 * hides that joint while its Randall option is Off. */
#define RANDALL_TIMER_OUT_X 52.675f
#define RANDALL_TIMER_TOP_Y (-18.9f)    /* Randall's two heights, in stage units. */
#define RANDALL_TIMER_BOTTOM_Y (-38.5f)
/* The game's HUD camera draws links 8, 10 and 11 (gxlink_prios 0xD00), and no
 * other camera draws 8. The timer's joints are in 11, so link 8's pass 0 comes
 * before them in every pass, and the numbers cover the map. */
#define RANDALL_TIMER_GXLINK 8
/* HUD camera units: 64 from the camera, it shows about 59 across and 48.5 up. */
#define RANDALL_TIMER_LINE_Y 0.6f     /* Each line's distance from the timer's middle. */
#define RANDALL_TIMER_DOT_SPACING 0.8f
#define RANDALL_TIMER_DOTS_MAX 256
#define RANDALL_TIMER_DOT_SIZE 12     /* Points, in sixths of a pixel: 2 pixels... */
#define RANDALL_TIMER_DOT_EDGE 24     /* ...over a 4-pixel dark one. */
#define RANDALL_TIMER_ARROW_LENGTH 1.3f /* Tail to tip. */
#define RANDALL_TIMER_ARROW_HEAD 0.65f
#define RANDALL_TIMER_ARROW_WIDTH 0.48f /* Half the head's width... */
#define RANDALL_TIMER_ARROW_SHAFT 0.17f /* ...and the shaft's. */
#define RANDALL_TIMER_ARROW_EDGE 0.13f  /* The dark outline. */
/* Straight colors, with the alpha past the timer and behind it. */
#define RANDALL_TIMER_ARROW_COLOR {235, 40, 40, 255}
#define RANDALL_TIMER_ARROW_BEHIND 150
#define RANDALL_TIMER_ARROW_EDGE_COLOR {0, 0, 0, 220}
#define RANDALL_TIMER_ARROW_EDGE_BEHIND 55
#define RANDALL_TIMER_DOT_COLOR {235, 235, 235, 230}
#define RANDALL_TIMER_DOT_BEHIND 80
#define RANDALL_TIMER_DOT_EDGE_COLOR {0, 0, 0, 160}
#define RANDALL_TIMER_DOT_EDGE_BEHIND 55
/* ScInfTim's ten joints, in the order ifTime keeps them: the hour and minute
 * separators, then hours, minutes, centiseconds and seconds. The hours (and
 * their separator) show only after an hour. Half of each one's quad's width. */
#define RANDALL_TIMER_DIGITS 10
#define TIMER_SEPARATOR_HALF 0.6015625f
#define TIMER_DIGIT_HALF 1.30078125f
#define TIMER_SMALL_HALF 0.98828125f /* The centiseconds. */
static const float timer_half_widths[RANDALL_TIMER_DIGITS] = {
    TIMER_SEPARATOR_HALF, TIMER_SEPARATOR_HALF, TIMER_DIGIT_HALF, TIMER_DIGIT_HALF, TIMER_DIGIT_HALF,
    TIMER_DIGIT_HALF, TIMER_SMALL_HALF, TIMER_SMALL_HALF, TIMER_DIGIT_HALF, TIMER_DIGIT_HALF,
};
/* Randall's path in stage units: map GObj 2's spline (its root's second
 * child), turned by its joint's 180 degrees, moved by (75.25, -101.5) and
 * scaled by 0.7. Straight lines, from the bottom-left corner where the cloud
 * starts, counter-clockwise and back. */
#define RANDALL_PATH_POINTS 13
static const Vec2 randall_path[RANDALL_PATH_POINTS] = {
    {-95.725f, -38.5f}, {95.725f, -38.5f}, {96.95f, -37.975f}, {97.475f, -36.75f},
    {97.475f, -20.475f}, {96.95f, -19.25f}, {95.725f, -18.9f}, {-95.725f, -18.9f},
    {-96.95f, -19.425f}, {-97.475f, -20.475f}, {-97.475f, -36.75f}, {-96.95f, -37.8f},
    {-95.725f, -38.5f},
};
static GOBJ *manager;
static int live;

static int in_tree(JOBJ *joint, JOBJ *target) {
    for (; joint; joint = joint->sibling) {
        if (joint == target || in_tree(joint->child, target)) return 1;
    }
    return 0;
}
static GOBJ *randall(void) {
    if (Stage_ExternalToInternal(Stage_GetExternalID()) != GRKIND_STORY) return 0;
    GOBJ *map = Stage_GetMapGObj(RANDALL_MAP_GOBJ); /* Null once hazards remove it. */
    MapData *data = map ? map->userdata : 0;
    return data && data->index == RANDALL_MAP_GOBJ && map->hsd_object ? map : 0;
}
static CollGroup *randall_group(JOBJ *root) {
    for (CollGroup *group = *stc_firstcollgroup; group; group = group->next)
        if (group->jobj && (group->jobj == root || in_tree(root->child, group->jobj))) return group;
    return 0;
}
/* GFX_Start with depth testing off, through HSD's cached state so the stage's
 * own later draws restore their modes normally. */
static void begin(u16 count, u8 shape, u8 size) {
    Mtx mtx;
    HSD_ClearVtxDesc();
    GXSetCurrentMtx(0);
    COBJ_GetViewingMtx(COBJ_GetCurrent(), &mtx);
    GXLoadPosMtxImm(mtx, 0);
    HSD_StateSetLineWidth(size, 5);
    HSD_StateSetPointSize(size, 5);
    HSD_SetupRenderMode(0x68000002);
    HSD_StateSetZMode(GX_DISABLE, GX_ALWAYS, GX_DISABLE);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    HSD_StateSetCullMode(0);
    GXBegin(shape, GX_VTXFMT0, count);
}
#define ELLIPSE_STEPS 24
static void ellipse(float cx, float cy, float rx, float ry, GXColor fill, GXColor edge) {
    /* Premultiplied colors, as GFX_AddVtx expects. */
    begin(ELLIPSE_STEPS + 2, GX_TRIANGLEFAN, 0);
    GFX_AddVtx(cx, cy, 0, fill);
    for (int i = 0; i <= ELLIPSE_STEPS; ++i) {
        float angle = i * (2 * M_PI / ELLIPSE_STEPS);
        GFX_AddVtx(cx + cos(angle) * rx, cy + sin(angle) * ry, 0, fill);
    }
    begin(ELLIPSE_STEPS + 1, GX_LINESTRIP, 6);
    for (int i = 0; i <= ELLIPSE_STEPS; ++i) {
        float angle = i * (2 * M_PI / ELLIPSE_STEPS);
        GFX_AddVtx(cx + cos(angle) * rx, cy + sin(angle) * ry, 0, edge);
    }
}
#define BLAST_ZONE_LINE 18   /* 3 pixels, in sixths. */
#define BLAST_ZONE_GLOW 24.f /* How far past the line the glow fades out. */
static void BlastZones_Draw(void) {
    /* The getters add the stage's camera offset, exactly as the KO check
     * (Player_CheckForDeath) reads them. Read every frame in case a stage
     * moves them. */
    float left = Stage_GetBlastzoneLeft(), right = Stage_GetBlastzoneRight();
    float top = Stage_GetBlastzoneTop(), bottom = Stage_GetBlastzoneBottom();
    if (right <= left || top <= bottom) return;
    /* Premultiplied: about half-opaque red at the line, clear at the glow's edge. */
    static const GXColor line = {255, 0, 0, 255}, glow = {120, 0, 0, 120}, clear = {0, 0, 0, 0};
    const float g = BLAST_ZONE_GLOW;
    const Vec2 in[4] = {{left, bottom}, {right, bottom}, {right, top}, {left, top}};
    const Vec2 out[4] = {{left - g, bottom - g}, {right + g, bottom - g}, {right + g, top + g}, {left - g, top + g}};
    /* One band per side, mitered at the corners. */
    begin(16, GX_QUADS, 0);
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        GFX_AddVtx(in[i].X, in[i].Y, 0, glow);
        GFX_AddVtx(in[j].X, in[j].Y, 0, glow);
        GFX_AddVtx(out[j].X, out[j].Y, 0, clear);
        GFX_AddVtx(out[i].X, out[i].Y, 0, clear);
    }
    begin(5, GX_LINESTRIP, BLAST_ZONE_LINE);
    for (int i = 0; i <= 4; ++i) GFX_AddVtx(in[i % 4].X, in[i % 4].Y, 0, line);
    /* Wide lines end square at their vertices; a point as wide fills each corner. */
    begin(4, GX_POINTS, BLAST_ZONE_LINE);
    for (int i = 0; i < 4; ++i) GFX_AddVtx(in[i].X, in[i].Y, 0, line);
}
static void Randall_Draw(void) {
    GOBJ *map = randall();
    if (!map) return;
    JOBJ *cloud = Stage_GetMapGObjJObj(map, RANDALL_CLOUD_JOINT);
    if (cloud && cloud->flags & JOBJ_HIDDEN) return; /* Off in the Training Lab: no platform to show. */
    static const GXColor fill = {92, 92, 92, 92}, edge = {230, 230, 230, 230}, top = {255, 255, 255, 255};
    JOBJ *root = map->hsd_object;
    CollGroup *group = randall_group(root);
    CollGroupDesc *desc = group ? group->desc : 0;
    if (!desc || !desc->vert_num || !*stc_collvert || !*stc_collline) {
        Vec3 pos;
        JOBJ_GetWorldPosition(root, 0, &pos);
        ellipse(pos.X, pos.Y, 6, 6, fill, edge);
        return;
    }
    CollVert *verts = *stc_collvert;
    float left = 1e9f, right = -1e9f, high = -1e9f, low = 1e9f;
    for (unsigned i = 0; i < desc->vert_num; ++i) {
        Vec2 p = verts[desc->vert_start + i].pos_curr;
        if (p.X < left) left = p.X;
        if (p.X > right) right = p.X;
        if (p.Y > high) high = p.Y;
        if (p.Y < low) low = p.Y;
    }
    /* Cloud body: three puffs under the platform, sized to its live width. */
    float width = right - left, puff = width * 0.24f, base = high - puff * 0.55f;
    if (width <= 0) return;
    ellipse(left + puff * 0.95f, base, puff, puff * 0.8f, fill, edge);
    ellipse(right - puff * 0.95f, base, puff, puff * 0.8f, fill, edge);
    ellipse((left + right) * 0.5f, base - puff * 0.25f, puff * 1.25f, puff, fill, edge);
    /* Landing surface: every floor line of the group, drawn last and brightest. */
    CollLine *lines = *stc_collline;
    for (unsigned i = 0; i < desc->floor_num; ++i) {
        CollLineDesc *line = lines[desc->floor_start + i].desc;
        if (!line) continue;
        Vec2 a = verts[line->vert_prev].pos_curr, b = verts[line->vert_next].pos_curr;
        begin(2, GX_LINES, 18);
        GFX_AddVtx(a.X, a.Y, 0, top);
        GFX_AddVtx(b.X, b.Y, 0, top);
    }
}
/* Stage units to the HUD camera's: the stage between its walls becomes the
 * timer's width, the same scale past them, and Randall's heights the lines. */
typedef struct {
    float x, y;   /* The timer's middle. */
    float sx, sy; /* HUD units per stage unit. */
} TimerMap;
static Vec2 map_point(const TimerMap *m, Vec2 p) {
    return (Vec2){m->x + p.X * m->sx, m->y + (p.Y - (RANDALL_TIMER_TOP_Y + RANDALL_TIMER_BOTTOM_Y) / 2) * m->sy};
}
/* Where the timer is: its root's height, and the edges of the joints it shows.
 * 0 with no timer (none this match, or the last 5 seconds' countdown replaced
 * it) or while the game hides it. */
static int timer_map(TimerMap *m) {
    GOBJ *timer = stc_matchhud->timer_gobj;
    JOBJ *root = timer ? timer->hsd_object : 0;
    if (!root || root->flags & JOBJ_HIDDEN) return 0;
    float left = 1e9f, right = -1e9f;
    JOBJ *digit = root->child;
    for (int i = 0; digit && i < RANDALL_TIMER_DIGITS; ++i, digit = digit->sibling) {
        if (digit->flags & JOBJ_HIDDEN) continue;
        Vec3 pos;
        JOBJ_GetWorldPosition(digit, 0, &pos);
        if (pos.X - timer_half_widths[i] < left) left = pos.X - timer_half_widths[i];
        if (pos.X + timer_half_widths[i] > right) right = pos.X + timer_half_widths[i];
    }
    if (!(right > left)) return 0;
    Vec3 pos;
    JOBJ_GetWorldPosition(root, 0, &pos);
    m->x = (left + right) / 2;
    m->y = pos.Y;
    m->sx = (right - left) / 2 / RANDALL_TIMER_OUT_X;
    m->sy = RANDALL_TIMER_LINE_Y / ((RANDALL_TIMER_TOP_Y - RANDALL_TIMER_BOTTOM_Y) / 2);
    return 1;
}
/* Premultiplied, as GFX_AddVtx expects. */
static GXColor premultiply(GXColor color, u8 alpha) {
    return (GXColor){color.r * alpha / 255, color.g * alpha / 255, color.b * alpha / 255, alpha};
}
static GXColor shade(GXColor color, u8 behind_alpha, int behind) {
    return premultiply(color, behind ? behind_alpha : color.a);
}
static float distance(Vec2 a, Vec2 b) {
    return sqrtf((b.X - a.X) * (b.X - a.X) + (b.Y - a.Y) * (b.Y - a.Y));
}
/* Half of the loop, in stage units, from the bottom line's middle around one
 * end to the top line's middle, with its segments' lengths on the map: the
 * right half, around the end Randall rises at, or the left, mirrored. */
#define RANDALL_HALF_POINTS 8
typedef struct {
    Vec2 points[RANDALL_HALF_POINTS];
    float steps[RANDALL_HALF_POINTS - 1];
    float length;
} HalfLoop;
static const u8 half_path[2][RANDALL_HALF_POINTS - 2] = {{1, 2, 3, 4, 5, 6}, {0, 11, 10, 9, 8, 7}};
static void half_loop(const TimerMap *m, int side, HalfLoop *half) {
    const u8 *path = half_path[side];
    half->points[0] = (Vec2){0, randall_path[path[0]].Y};
    for (int i = 0; i < RANDALL_HALF_POINTS - 2; ++i) half->points[i + 1] = randall_path[path[i]];
    half->points[RANDALL_HALF_POINTS - 1] = (Vec2){0, randall_path[path[RANDALL_HALF_POINTS - 3]].Y};
    half->length = 0;
    for (int i = 1; i < RANDALL_HALF_POINTS; ++i) {
        half->steps[i - 1] = distance(map_point(m, half->points[i - 1]), map_point(m, half->points[i]));
        half->length += half->steps[i - 1];
    }
}
/* The point that far along a half on the map, in stage units. */
static Vec2 half_at(const HalfLoop *half, float at) {
    for (int i = 1; i < RANDALL_HALF_POINTS; ++i) {
        float step = half->steps[i - 1];
        if (at <= step || i == RANDALL_HALF_POINTS - 1) {
            float t = step > 0 ? at / step : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            Vec2 a = half->points[i - 1], b = half->points[i];
            return (Vec2){a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t};
        }
        at -= step;
    }
    return half->points[RANDALL_HALF_POINTS - 1];
}
/* Dots evenly spaced around the loop, dimmer behind the timer, the same on
 * both ends: an even number on each half, centered on it, at the same
 * fractions of the right half and the left. So the timer's middle falls between
 * two dots on each line, and each end's middle between two dots on its straight. */
static void draw_track(const TimerMap *m) {
    static Vec2 dots[RANDALL_TIMER_DOTS_MAX];
    static u8 behind[RANDALL_TIMER_DOTS_MAX];
    HalfLoop right, left;
    half_loop(m, 0, &right);
    half_loop(m, 1, &left);
    if (!(right.length > 0 && left.length > 0)) return;
    float pairs = right.length / (2 * RANDALL_TIMER_DOT_SPACING) + 0.5f;
    int per_half = pairs < RANDALL_TIMER_DOTS_MAX / 4 ? 2 * (int)pairs : RANDALL_TIMER_DOTS_MAX / 2;
    if (per_half < 2) per_half = 2;
    int count = 0;
    for (int i = 0; i < per_half; ++i) {
        float t = (i + 0.5f) / per_half;
        Vec2 r = half_at(&right, t * right.length), l = half_at(&left, t * left.length);
        behind[count] = fabs(r.X) <= RANDALL_TIMER_OUT_X;
        dots[count++] = map_point(m, r);
        behind[count] = fabs(l.X) <= RANDALL_TIMER_OUT_X;
        dots[count++] = map_point(m, l);
    }
    static const GXColor edge = RANDALL_TIMER_DOT_EDGE_COLOR, dot = RANDALL_TIMER_DOT_COLOR;
    begin(count, GX_POINTS, RANDALL_TIMER_DOT_EDGE);
    for (int i = 0; i < count; ++i)
        GFX_AddVtx(dots[i].X, dots[i].Y, 0, shade(edge, RANDALL_TIMER_DOT_EDGE_BEHIND, behind[i]));
    begin(count, GX_POINTS, RANDALL_TIMER_DOT_SIZE);
    for (int i = 0; i < count; ++i)
        GFX_AddVtx(dots[i].X, dots[i].Y, 0, shade(dot, RANDALL_TIMER_DOT_BEHIND, behind[i]));
}
/* The path segment nearest the cloud: the way it's going. */
static int nearest_segment(Vec2 p) {
    int best = 0;
    float best_distance = 1e30f;
    for (int i = 0; i < RANDALL_PATH_POINTS - 1; ++i) {
        Vec2 a = randall_path[i], b = randall_path[i + 1];
        float dx = b.X - a.X, dy = b.Y - a.Y, t = ((p.X - a.X) * dx + (p.Y - a.Y) * dy) / (dx * dx + dy * dy);
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        float x = a.X + dx * t - p.X, y = a.Y + dy * t - p.Y, d = x * x + y * y;
        if (t > 0.999f) d += 1e-4f; /* On a corner, the segment leaving it wins (frame 0 heads right). */
        if (d < best_distance) {
            best_distance = d;
            best = i;
        }
    }
    return best;
}
/* An arrow centered on c, pointing along u, as a head and a shaft. grow moves
 * every edge out that far, for the outline under it. */
static void arrow(Vec2 c, Vec2 u, float grow, GXColor color) {
    const float L = RANDALL_TIMER_ARROW_LENGTH, H = RANDALL_TIMER_ARROW_HEAD, W = RANDALL_TIMER_ARROW_WIDTH;
    const float side = sqrtf(H * H + W * W), base = L / 2 - H - grow;
    const float tip = L / 2 + grow * side / W, wide = (grow * side + (H + grow) * W) / H;
    const float shaft = RANDALL_TIMER_ARROW_SHAFT + grow, tail = -L / 2 - grow;
    const Vec2 v = {-u.Y, u.X};
    const Vec2 local[9] = {{tip, 0}, {base, wide}, {base, -wide},
                           {tail, -shaft}, {base, shaft}, {tail, shaft},
                           {tail, -shaft}, {base, -shaft}, {base, shaft}};
    begin(9, GX_TRIANGLES, 0);
    for (int i = 0; i < 9; ++i)
        GFX_AddVtx(c.X + u.X * local[i].X + v.X * local[i].Y, c.Y + u.Y * local[i].X + v.Y * local[i].Y, 0, color);
}
static void draw_arrow(const TimerMap *m, Vec2 cloud) {
    int seg = nearest_segment(cloud);
    Vec2 a = map_point(m, randall_path[seg]), b = map_point(m, randall_path[seg + 1]);
    float length = distance(a, b);
    if (!(length > 0)) return;
    Vec2 u = {(b.X - a.X) / length, (b.Y - a.Y) / length}, c = map_point(m, cloud);
    int behind = fabs(cloud.X) <= RANDALL_TIMER_OUT_X;
    static const GXColor edge = RANDALL_TIMER_ARROW_EDGE_COLOR, fill = RANDALL_TIMER_ARROW_COLOR;
    arrow(c, u, RANDALL_TIMER_ARROW_EDGE, shade(edge, RANDALL_TIMER_ARROW_EDGE_BEHIND, behind));
    arrow(c, u, 0, shade(fill, RANDALL_TIMER_ARROW_BEHIND, behind));
}
/* Where Randall's cloud joint is, in stage units. 0 without one, or without a
 * real position. */
static int cloud_position(GOBJ *map, Vec2 *out) {
    JOBJ *cloud = map ? Stage_GetMapGObjJObj(map, RANDALL_CLOUD_JOINT) : 0;
    if (!cloud) return 0;
    Vec3 pos;
    JOBJ_GetWorldPosition(cloud, 0, &pos);
    if (!(fabs(pos.X) < 1000 && fabs(pos.Y) < 1000)) return 0;
    *out = (Vec2){pos.X, pos.Y};
    return 1;
}
/* Where Randall would be at the match timer's time, in stage units, with no
 * Randall to read: his path, which he rides at a constant speed from the
 * bottom-left corner on his animation's frame 0, as a match keeps him. */
static Vec2 timer_position(void) {
    unsigned frame = ((unsigned)Match_TimerFrames() + RANDALL_TIMER_OFFSET) % RANDALL_LOOP_FRAMES;
    float total = 0;
    for (int i = 1; i < RANDALL_PATH_POINTS; ++i) total += distance(randall_path[i - 1], randall_path[i]);
    float at = total * frame / RANDALL_LOOP_FRAMES;
    for (int i = 1; i < RANDALL_PATH_POINTS; ++i) {
        Vec2 a = randall_path[i - 1], b = randall_path[i];
        float step = distance(a, b);
        if (at <= step || i == RANDALL_PATH_POINTS - 1) {
            float t = step > 0 ? at / step : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            return (Vec2){a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t};
        }
        at -= step;
    }
    return randall_path[0];
}
static void RandallTimer_GX(GOBJ *gobj, int pass) {
    if (!live || pass != 0 || !Settings_Get(TM_SETTING_FLAG, TM_FLAG_RANDALL_TIMER)) return;
    if (Stage_ExternalToInternal(Stage_GetExternalID()) != GRKIND_STORY) return;
    Vec2 cloud;
    TimerMap m;
    GOBJ *map = randall();
    if (!map) cloud = timer_position(); /* Hazards removed him. */
    else if (!cloud_position(map, &cloud)) return;
    if (!timer_map(&m)) return;
    draw_track(&m);
    draw_arrow(&m, cloud);
}
static void StageOverlay_GX(GOBJ *gobj, int pass) {
    if (!live || pass != 2) return;
    if (Settings_Get(TM_SETTING_FLAG, TM_FLAG_BLAST_ZONES)) BlastZones_Draw();
    if (Settings_Get(TM_SETTING_FLAG, TM_FLAG_RANDALL)) Randall_Draw();
}
void StageOverlay_SceneChange(void) {
    live = 0;
    manager = 0; /* Native scene cleanup owns the old GOBJs. */
}
void StageOverlay_MatchStart(void) {
    if (manager) return;
    live = 1;
    manager = GObj_Create(0, 7, 0);
    if (!manager) { live = 0; return; }
    GObj_AddGXLink(manager, StageOverlay_GX, 5, 0);
    GOBJ *hud = GObj_Create(0, 7, 0); /* The Randall Timer, in the HUD camera. */
    if (hud) GObj_AddGXLink(hud, RandallTimer_GX, RANDALL_TIMER_GXLINK, 0);
}
