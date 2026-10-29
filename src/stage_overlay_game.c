#include "events.h"

/* Global stage overlays, drawn by one GX callback created at match start.
 *
 * Blast Zone Overlay: on every stage, a crisp red line on the blast zones and
 * a red glow fading out past them.
 *
 * Randall Overlay: Yoshi's Story's cloud (map GObj 2) rides a square track and
 * passes behind the stage. Draw its live platform and a cloud silhouette on top
 * of everything so its position is always readable. The platform comes from the
 * moving collision group attached to Randall's joints; the joint's world
 * position is the fallback when no group is found. */
#define RANDALL_MAP_GOBJ 2
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
static void StageOverlay_GX(GOBJ *gobj, int pass) {
    if (!live || pass != 2) return;
    if (Settings_Get(TM_SETTING_FLAG, TM_FLAG_BLAST_ZONES)) BlastZones_Draw();
    if (Settings_Get(TM_SETTING_FLAG, TM_FLAG_RANDALL)) Randall_Draw();
}
void StageOverlay_SceneChange(void) {
    live = 0;
    manager = 0; /* Native scene cleanup owns the old GOBJ. */
}
void StageOverlay_MatchStart(void) {
    if (manager) return;
    live = 1;
    manager = GObj_Create(0, 7, 0);
    if (!manager) { live = 0; return; }
    GObj_AddGXLink(manager, StageOverlay_GX, 5, 0);
}
