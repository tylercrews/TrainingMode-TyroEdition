/* The Randall Timer from the production stage overlay. The stage, Randall's
 * map GObj and cloud joint, the settings and the match timer's joints are
 * fixtures; tests/test_randall_timer.py drives it. Blast Zone and Randall
 * Overlay stay Off, so nothing here draws. */
#include "../src/stage_overlay_game.c"

/* A timer like IfAll's ScInfTim: a root whose children are ten joints of one
 * DObj each (parts 1-10), the first two gray separators, as in the game. Two
 * more joints cover the rest of the walk: joint 11 has no DObj, but its child
 * has two (parts 11 and 0), and joint 12 is a spline, whose union points at
 * part 12 but isn't a DObj, so part 12 must never change. */
#define TEST_PARTS 13
static int test_external, test_internal;
static unsigned test_flags; /* TM_SETTING_FLAG bits that read On. */
static int test_map_present, test_world_calls;
static GOBJ test_map, test_timers[2];
static MapData test_map_data;
static JOBJ test_root, test_cloud;
static JOBJ test_joints[2][TEST_PARTS], test_nested[2];
static DOBJ test_dobjs[2][TEST_PARTS];
static MOBJ test_mobjs[2][TEST_PARTS];
static HSD_Material test_materials[2][TEST_PARTS];

int Stage_GetExternalID() { return test_external; }
int Stage_ExternalToInternal(int ext_id) { return ext_id == test_external ? test_internal : -1; }
GOBJ *Stage_GetMapGObj(int id) { return id == RANDALL_MAP_GOBJ && test_map_present ? &test_map : 0; }
int JOBJ_GetWorldPosition(JOBJ *source, Vec3 *add, Vec3 *dest) {
    ++test_world_calls;
    *dest = source == &test_cloud && !add ? source->trans : (Vec3){1e9f, 1e9f, 1e9f};
    return 0;
}
uint32_t Settings_Get(unsigned field, unsigned index) {
    return field == TM_SETTING_FLAG && index < 32 && (test_flags >> index & 1);
}

static void TestClear(void *at, unsigned size) {
    for (unsigned i = 0; i < size; ++i) ((volatile unsigned char *)at)[i] = 0;
}
static void TestPart(int timer, int part, GXColor color) {
    test_dobjs[timer][part].mobj = &test_mobjs[timer][part];
    test_mobjs[timer][part].mat = &test_materials[timer][part];
    test_materials[timer][part].diffuse = color;
}
static void TestBuildTimer(int timer) {
    JOBJ *joints = test_joints[timer];
    TestClear(joints, sizeof(test_joints[timer]));
    TestClear(&test_nested[timer], sizeof(test_nested[timer]));
    TestClear(test_dobjs[timer], sizeof(test_dobjs[timer]));
    TestClear(test_mobjs[timer], sizeof(test_mobjs[timer]));
    TestClear(test_materials[timer], sizeof(test_materials[timer]));
    for (int part = 0; part < TEST_PARTS; ++part)
        TestPart(timer, part, part == 0 ? (GXColor){0x80, 0x40, 0x20, 0x7F} :
                              part <= 2 ? (GXColor){0xB3, 0xB3, 0xB3, 0xFF} : (GXColor){0xFF, 0xFF, 0xFF, 0xFF});
    joints[0].child = &joints[1];
    for (int i = 1; i <= 12; ++i) {
        joints[i].parent = &joints[0];
        joints[i].sibling = i < 12 ? &joints[i + 1] : 0;
        joints[i].dobj = i <= 10 ? &test_dobjs[timer][i] : 0;
    }
    joints[11].child = &test_nested[timer];
    test_nested[timer].parent = &joints[11];
    test_nested[timer].dobj = &test_dobjs[timer][11];
    test_dobjs[timer][11].next = &test_dobjs[timer][0];
    joints[12].flags = JOBJ_SPLINE;
    joints[12].dobj = &test_dobjs[timer][12];
    test_timers[timer].hsd_object = &joints[0];
}
int TestInit(void) {
    test_external = GRKINDEXT_STORY;
    test_internal = GRKIND_STORY;
    test_flags = 1u << TM_FLAG_RANDALL_TIMER;
    test_map_present = 1;
    test_world_calls = 0;
    TestClear(&test_map, sizeof(test_map));
    TestClear(&test_map_data, sizeof(test_map_data));
    TestClear(&test_root, sizeof(test_root));
    TestClear(&test_cloud, sizeof(test_cloud));
    test_map_data.index = RANDALL_MAP_GOBJ;
    test_map.userdata = &test_map_data;
    test_map.hsd_object = &test_root;
    test_root.child = &test_cloud;
    test_cloud.parent = &test_root;
    TestBuildTimer(0);
    TestBuildTimer(1);
    stc_matchhud->timer_gobj = &test_timers[0];
    StageOverlay_SceneChange();
    live = 1; /* As StageOverlay_MatchStart leaves it, without the GObj. */
    return 1;
}
void TestSet(int what, int value) {
    switch (what) {
    case 0: test_flags = value; break;
    case 1: test_external = value; break;
    case 2: test_internal = value; break;
    case 3: test_map_present = value; break;
    case 4: stc_matchhud->timer_gobj = value < 0 ? 0 : &test_timers[value]; break;
    case 5: test_map_data.index = value; break;
    case 6: test_root.child = value ? &test_cloud : 0; break;
    case 7: TestBuildTimer(value); break;
    }
}
/* X in hundredths, so the test needn't pass floats. */
void TestCloud(int x100) { test_cloud.trans.X = x100 / 100.0f; }
void TestDraw(int pass) { StageOverlay_GX(0, pass); }
void TestSceneChange(void) { StageOverlay_SceneChange(); live = 1; }
uint32_t TestColor(int timer, int part) {
    GXColor c = test_materials[timer][part].diffuse;
    return (uint32_t)c.r << 24 | c.g << 16 | c.b << 8 | c.a;
}
void TestSetColor(int timer, int part, uint32_t rgba) {
    test_materials[timer][part].diffuse = (GXColor){rgba >> 24, rgba >> 16, rgba >> 8, rgba};
}
int TestInfo(int what) {
    switch (what) {
    case 0: return test_world_calls;
    case 1: return timer_side;
    case 2: return timer_parts;
    case 3: return TEST_PARTS;
    case 4: return RANDALL_TIMER_PARTS;
    case 5: return TM_FLAG_RANDALL_TIMER;
    case 6: return GRKINDEXT_STORY;
    case 7: return GRKIND_STORY;
    case 8: return GRKINDEXT_BATTLE;
    }
    return -1;
}
/* The tints and the threshold, as the build has them. */
uint32_t TestConstant(int what) {
    static const GXColor tints[2] = {RANDALL_TIMER_LEFT, RANDALL_TIMER_RIGHT};
    union { float f; uint32_t u; } out = {RANDALL_TIMER_OUT_X};
    if (what < 2) return (uint32_t)tints[what].r << 24 | tints[what].g << 16 | tints[what].b << 8 | tints[what].a;
    return out.u;
}
