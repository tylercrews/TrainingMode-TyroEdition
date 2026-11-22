/* The Randall Timer Minimap from the production stage overlay. The stage, Randall's
 * map GObj and joints, the settings and the match timer's joints are fixtures;
 * tests/test_randall_timer.py drives it. Stage_GetMapGObjJObj is the game's
 * own (linked to Start.dol's 801C3FA4), walking a tree shaped like the game's:
 * the scale joint Ground_GetStageGObj puts over GrSt.dat's root, that root,
 * then the cloud and its sibling, the spline. The GX state calls are recorded
 * here, and the test reads the vertices from the GX FIFO writes. Blast Zone
 * stays Off; the Randall Overlay is only checked for drawing or not (its
 * puffs' shapes come from the cos/sin stubs). The match timer's clock is
 * stc_match's own fields. Floats cross as their bits. */
#include "../src/stage_overlay_game.c"

float cos(float x) { return 1; }
float sin(float x) { return 0; }

static int test_external, test_internal;
static unsigned test_flags; /* TM_SETTING_FLAG bits that read On. */
static int test_map_present;
static GOBJ test_map, test_timer_gobj;
static MapData test_map_data;
static JOBJ test_root, test_stage_root, test_cloud, test_spline, test_timer, test_digits[RANDALL_TIMER_DIGITS];

/* Every GXBegin, with the point size, line width and depth mode set before it. */
#define TEST_BEGINS 16
static int test_begins, test_point_size, test_line_width, test_z_compare, test_z_update;
static int test_begin[TEST_BEGINS][5];
/* GObj_Create and GObj_AddGXLink calls. */
#define TEST_GOBJS 4
static GOBJ test_gobjs[TEST_GOBJS];
static int test_gobj_count, test_link_count;
static int test_links[TEST_GOBJS][4];

static float test_float(uint32_t bits) {
    union { uint32_t u; float f; } v = {bits};
    return v.f;
}
static uint32_t test_bits(float value) {
    union { float f; uint32_t u; } v = {value};
    return v.u;
}

int Stage_GetExternalID() { return test_external; }
int Stage_ExternalToInternal(int ext_id) { return ext_id == test_external ? test_internal : -1; }
GOBJ *Stage_GetMapGObj(int id) { return id == RANDALL_MAP_GOBJ && test_map_present ? &test_map : 0; }
/* Translation-only joints: a joint's world position is the sum up its parents. */
int JOBJ_GetWorldPosition(JOBJ *source, Vec3 *add, Vec3 *dest) {
    Vec3 pos = {0, 0, 0};
    for (JOBJ *joint = source; joint; joint = joint->parent) {
        pos.X += joint->trans.X;
        pos.Y += joint->trans.Y;
        pos.Z += joint->trans.Z;
    }
    *dest = pos;
    return 0;
}
uint32_t Settings_Get(unsigned field, unsigned index) {
    return field == TM_SETTING_FLAG && index < 32 && (test_flags >> index & 1);
}
float sqrtf(float num) {
    if (!(num > 0)) return 0;
    float r = num > 1 ? num : 1;
    for (int i = 0; i < 40; ++i) r = 0.5f * (r + num / r);
    return r;
}
void HSD_ClearVtxDesc() {}
void GXSetCurrentMtx(GXPosNormMtx mtx) {}
COBJ *COBJ_GetCurrent(void) { return 0; }
void COBJ_GetViewingMtx(COBJ *cobj, Mtx *out) {}
void GXLoadPosMtxImm(Mtx mtx, GXPosNormMtx id) {}
void HSD_StateSetLineWidth(u8 width, int tex_offsets) { test_line_width = width; }
void HSD_StateSetPointSize(u8 size, int unk) { test_point_size = size; }
void HSD_SetupRenderMode(u32 params) {}
void HSD_StateSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable) {
    test_z_compare = compare_enable;
    test_z_update = update_enable;
}
void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttribute attribute, GXComponentContents contents,
                     GXComponentType type, u8 fracBits) {}
void GXSetVtxDesc(GXAttribute attribute, GXAttributeType type) {}
void HSD_StateSetCullMode(GXCullMode cull_mode) {}
void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts) {
    if (test_begins >= TEST_BEGINS) return;
    int *begin = test_begin[test_begins++];
    begin[0] = type;
    begin[1] = nverts;
    begin[2] = test_point_size;
    begin[3] = test_z_compare;
    begin[4] = test_z_update;
}
GOBJ *GObj_Create(int entity_class, int p_link, int p_priority) {
    return test_gobj_count < TEST_GOBJS ? &test_gobjs[test_gobj_count++] : 0;
}
void GObj_AddGXLink(GOBJ *gobj, void *cb, int gx_link, int gx_pri) {
    if (test_link_count >= TEST_GOBJS) return;
    int *link = test_links[test_link_count++];
    link[0] = (int)(gobj - test_gobjs);
    link[1] = cb == (void *)StageOverlay_GX ? 1 : cb == (void *)RandallTimer_GX ? 2 : 0;
    link[2] = gx_link;
    link[3] = gx_pri;
}

static void TestClear(void *at, unsigned size) {
    for (unsigned i = 0; i < size; ++i) ((volatile unsigned char *)at)[i] = 0;
}
/* A timer like IfAll's ScInfTim: a root with ten child joints. The test places
 * them (TestDigit); until then they're all at the root. */
static void TestBuildTimer(void) {
    TestClear(&test_timer, sizeof(test_timer));
    TestClear(test_digits, sizeof(test_digits));
    TestClear(&test_timer_gobj, sizeof(test_timer_gobj));
    test_timer.child = &test_digits[0];
    for (int i = 0; i < RANDALL_TIMER_DIGITS; ++i) {
        test_digits[i].parent = &test_timer;
        test_digits[i].sibling = i + 1 < RANDALL_TIMER_DIGITS ? &test_digits[i + 1] : 0;
    }
    test_timer_gobj.hsd_object = &test_timer;
}
int TestInit(void) {
    test_external = GRKINDEXT_STORY;
    test_internal = GRKIND_STORY;
    test_flags = 1u << TM_FLAG_RANDALL_TIMER;
    test_map_present = 1;
    TestClear(&test_map, sizeof(test_map));
    TestClear(&test_map_data, sizeof(test_map_data));
    TestClear(&test_root, sizeof(test_root));
    TestClear(&test_stage_root, sizeof(test_stage_root));
    TestClear(&test_cloud, sizeof(test_cloud));
    TestClear(&test_spline, sizeof(test_spline));
    test_map_data.index = RANDALL_MAP_GOBJ;
    test_map.userdata = &test_map_data;
    test_map.hsd_object = &test_root;
    test_root.child = &test_stage_root;
    test_stage_root.parent = &test_root;
    test_stage_root.child = &test_cloud;
    test_cloud.parent = test_spline.parent = &test_stage_root;
    test_cloud.sibling = &test_spline;
    test_spline.flags = 0x4000; /* JOBJ_SPLINE */
    TestBuildTimer();
    stc_matchhud->timer_gobj = &test_timer_gobj;
    test_gobj_count = test_link_count = 0;
    StageOverlay_SceneChange();
    live = 1; /* As StageOverlay_MatchStart leaves it, without the GObjs. */
    return 1;
}
void TestSet(int what, int value) {
    switch (what) {
    case 0: test_flags = value; break;
    case 1: test_external = value; break;
    case 2: test_internal = value; break;
    case 3: test_map_present = value; break;
    case 4: stc_matchhud->timer_gobj = value ? &test_timer_gobj : 0; break;
    case 5: test_map_data.index = value; break;
    case 6: test_stage_root.child = value ? &test_cloud : 0; break;
    case 7: test_timer.flags = value; break;
    }
}
void TestTimer(uint32_t x, uint32_t y) { test_timer.trans = (Vec3){test_float(x), test_float(y), 0}; }
void TestDigit(int i, uint32_t x, uint32_t y, int hidden) {
    test_digits[i].trans = (Vec3){test_float(x), test_float(y), 0};
    test_digits[i].flags = hidden ? JOBJ_HIDDEN : 0;
}
void TestCloud(uint32_t x, uint32_t y) { test_cloud.trans = (Vec3){test_float(x), test_float(y), 0}; }
/* The Training Lab's Randall row hides the cloud joint. */
void TestCloudHidden(int hidden) { test_cloud.flags = hidden ? JOBJ_HIDDEN : 0; }
/* The match timer's mode and clock, and the time a countdown starts from. */
void TestTime(int mode, int seconds, int ms) {
    stc_match->match.timer = mode;
    stc_match->time_seconds = seconds;
    stc_match->time_ms = ms;
}
void TestTimeStart(int seconds, int subseconds) {
    stc_match->match.timer_seconds = seconds;
    stc_match->match.timer_subseconds = subseconds;
}
void TestDraw(int pass) {
    test_begins = 0;
    RandallTimer_GX(0, pass);
}
/* Only the arrow, for the frame-by-frame lap, from the joint the overlay reads. */
int TestArrow(void) {
    test_begins = 0;
    TimerMap m;
    Vec2 cloud;
    if (!cloud_position(randall(), &cloud) || !timer_map(&m)) return 0;
    draw_arrow(&m, cloud);
    return 1;
}
/* Which fixture joint the game's lookup returns: 1 the scale joint, 2 GrSt.dat's
 * root, 3 the cloud, 4 the spline, 0 none. */
int TestMapJoint(int index) {
    JOBJ *joint = Stage_GetMapGObjJObj(&test_map, index);
    return joint == &test_root ? 1 : joint == &test_stage_root ? 2 : joint == &test_cloud ? 3
         : joint == &test_spline ? 4 : 0;
}
void TestStageGX(int pass) {
    test_begins = 0;
    StageOverlay_GX(0, pass);
}
void TestSceneChange(void) { StageOverlay_SceneChange(); }
void TestMatchStart(void) { StageOverlay_MatchStart(); }
int TestBegins(void) { return test_begins; }
int TestBegin(int i, int what) { return test_begin[i][what]; }
int TestLinks(void) { return test_link_count; }
int TestLink(int i, int what) { return test_links[i][what]; }
int TestGObjs(void) { return test_gobj_count; }
int TestInfo(int what) {
    switch (what) {
    case 0: return TM_FLAG_RANDALL_TIMER;
    case 1: return GRKINDEXT_STORY;
    case 2: return GRKIND_STORY;
    case 3: return GRKINDEXT_BATTLE;
    case 4: return RANDALL_TIMER_DIGITS;
    case 5: return RANDALL_PATH_POINTS;
    case 6: return RANDALL_TIMER_GXLINK;
    case 7: return RANDALL_TIMER_DOTS_MAX;
    case 8: return RANDALL_TIMER_DOT_SIZE;
    case 9: return RANDALL_TIMER_DOT_EDGE;
    case 10: return JOBJ_HIDDEN;
    case 11: return live;
    case 12: return TM_FLAG_RANDALL;
    case 13: return RANDALL_TIMER_OFFSET;
    case 14: return RANDALL_LOOP_FRAMES;
    }
    return -1;
}
/* The build's numbers, as float bits. */
uint32_t TestConstant(int what) {
    switch (what) {
    case 0: return test_bits(RANDALL_TIMER_OUT_X);
    case 1: return test_bits(RANDALL_TIMER_TOP_Y);
    case 2: return test_bits(RANDALL_TIMER_BOTTOM_Y);
    case 3: return test_bits(RANDALL_TIMER_LINE_Y);
    case 4: return test_bits(RANDALL_TIMER_DOT_SPACING);
    case 5: return test_bits(RANDALL_TIMER_ARROW_LENGTH);
    case 6: return test_bits(RANDALL_TIMER_ARROW_EDGE);
    }
    return 0;
}
uint32_t TestHalfWidth(int i) { return test_bits(timer_half_widths[i]); }
uint32_t TestPath(int i, int axis) { return test_bits(axis ? randall_path[i].Y : randall_path[i].X); }
/* Straight colors and their alphas behind the timer, packed RGBA. */
uint32_t TestColor(int what) {
    static const GXColor colors[4] = {RANDALL_TIMER_ARROW_COLOR, RANDALL_TIMER_ARROW_EDGE_COLOR,
                                      RANDALL_TIMER_DOT_COLOR, RANDALL_TIMER_DOT_EDGE_COLOR};
    static const u8 behind[4] = {RANDALL_TIMER_ARROW_BEHIND, RANDALL_TIMER_ARROW_EDGE_BEHIND,
                                 RANDALL_TIMER_DOT_BEHIND, RANDALL_TIMER_DOT_EDGE_BEHIND};
    if (what >= 4) return behind[what - 4];
    GXColor c = colors[what];
    return (uint32_t)c.r << 24 | c.g << 16 | c.b << 8 | c.a;
}
