#ifndef TM_STAGE_POINTS_H
#define TM_STAGE_POINTS_H
/* Standing points on the live stage, and putting a grounded fighter on one.
 * Shared by the events that place fighters by stage position (Low Laser Puff
 * Practice, Ledgegrab Speedrun). */
#include "events.h"

/* FD's main-stage edge. Ground positions scale from FD's original X values
 * to each stage's live edges, so FD keeps exactly 0/±35/±70 and the player ±50. */
#define FD_EDGE 85.5656967f

/* Live collision geometry, as in Techchase: current vertices follow Fountain's
 * moving platforms, and only enabled lines count (Stadium's base layout). */
static int IsPlatformLine(const CollLineDesc *line) {
    /* Native MapLine.lo_flags LINE_FLAG_PLATFORM is 0x0100. */
    return ((const u8 *)line)[0xE] & 1;
}
static int FloorLine(int i, Vec2 *a, Vec2 *b, int *platform) {
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts || i < 0 || i >= stage->line_num) return 0;
    CollLineDesc *line = lines[i].desc;
    if (!line || !line->is_floor || line->disabled || !GrColl_CheckIfLineEnabled(i) ||
        line->vert_prev < 0 || line->vert_next < 0 ||
        line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num) return 0;
    *a = verts[line->vert_prev].pos_curr;
    *b = verts[line->vert_next].pos_curr;
    if (a->X > b->X) { Vec2 swap = *a; *a = *b; *b = swap; }
    *platform = IsPlatformLine(line);
    return b->X - a->X >= 0.01f;
}
static int LineCount(void) { return *stc_colldata ? (*stc_colldata)->line_num : 0; }
static int LineY(int i, float x, float *y) {
    Vec2 a, b;
    int platform;
    if (!FloorLine(i, &a, &b, &platform) || x < a.X || x > b.X) return 0;
    *y = a.Y + (b.Y - a.Y) * (x - a.X) / (b.X - a.X);
    return 1;
}
static int FindSolidGround(float x, Vec3 *position, int *index) {
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        float y;
        if (!FloorLine(i, &a, &b, &platform) || platform || !LineY(i, x, &y)) continue;
        if (!found || y > position->Y) { *position = (Vec3){x, y, 0}; *index = i; found = 1; }
    }
    return found;
}
static void GroundEdges(float *left, float *right) {
    *left = -FD_EDGE; *right = FD_EDGE;
    int found = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        if (!FloorLine(i, &a, &b, &platform) || platform) continue;
        if (!found || a.X < *left) *left = a.X;
        if (!found || b.X > *right) *right = b.X;
        found = 1;
    }
}
/* Solid ground at x, moving toward center if a non-legal stage has a gap. */
static void GroundPoint(float x, Vec3 *position, int *index) {
    float inward = x;
    for (int i = 0; i < 4; ++i, inward *= .5f)
        if (FindSolidGround(inward, position, index)) return;
    Vec3 slope;
    int kind;
    *position = (Vec3){x, 0, 0};
    *index = -1;
    GrColl_RaycastGround(position, index, &kind, &slope, -1, -1, -1, 0, x, 10, x, -20, 0);
}
static int LegalStage(void) {
    int stage = Stage_GetExternalID();
    return stage == GRKINDEXT_STORY || stage == GRKINDEXT_IZUMI || stage == GRKINDEXT_FD ||
        stage == GRKINDEXT_PSTAD || stage == GRKINDEXT_BATTLE || stage == GRKINDEXT_OLDPU;
}
/* side -1/0/1: left, top, right one-way platform. Puff stands at its center. */
static int FindPlatform(int side, Vec3 *position, int *index) {
    if (!LegalStage()) return 0;
    int found = 0;
    float best = 0;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        float y;
        if (!FloorLine(i, &a, &b, &platform) || !platform || b.X - a.X < 10) continue;
        float x = (a.X + b.X) * .5f;
        if (side < 0 ? x >= -10 : side > 0 ? x <= 10 : a.X > 0 || b.X < 0) continue;
        if (!LineY(i, x, &y)) continue;
        float score = side ? -fabs(x) : y;
        if (!found || score > best) { *position = (Vec3){x, y, 0}; *index = i; best = score; found = 1; }
    }
    return found;
}
static void ClearVelocity(FighterData *ft) {
    ft->phys.self_vel = ft->phys.kb_vel = ft->phys.self_vel_ground = (Vec3){0};
    ft->phys.atk_shield_kb_vel = (Vec3){0};
    ft->phys.nudge_vel = (Vec2){0};
}
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
static void GroundAt(GOBJ *fighter, Vec3 ground, int line) {
    FighterData *ft = fighter->userdata;
    if (line >= 0) ft->coll_data.ground_index = line;
    ft->phys.pos = ground;
    ClearVelocity(ft);
    UpdatePosition(fighter);
    EnvironmentCollision_WaitLanding(fighter);
    Fighter_SetGrounded(ft);
    ft->hurt.kind_game = ft->hurt.kind_script = 0;
    ft->hurt.intang_frames.ledge = ft->hurt.intang_frames.respawn = 0;
    Fighter_SetAllHurtboxState(fighter, 0);
}

#endif
