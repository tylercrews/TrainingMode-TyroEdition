#include "../src/fountain.c"

/* Two side platforms as Stages_Fountain_SidePlatformSpawn builds them, plus a
 * fountain (map 3) and a map-4 lookalike whose data isn't its own. The native
 * Stages_FoDPlatformThink runs on the real two. */
typedef struct Side {
    GOBJ gobj;
    FountainPlatform data;
    unsigned char rest[0x204 - sizeof(FountainPlatform)];
    JOBJ column, water, joint;
} Side;

static Side sides[4];  // left, right, fountain, lookalike
static GOBJ *plinks[64];
static int stage_id = GRKINDEXT_IZUMI;
static int remove_anim_calls, map_anim_calls, map_anim_args[4], collision_calls, collision_group, asserts;
static float map_anim_frames[2];

int Stage_GetExternalID(void) { return stage_id; }
void JOBJ_RemoveAnimAll(JOBJ *joint) { ++remove_anim_calls; }
void Stage_SetMapJOBJAnim(GOBJ *map, int index, int flags, int anim, float start, float rate)
{
    ++map_anim_calls;
    map_anim_args[0] = map == &sides[0].gobj ? 0 : map == &sides[1].gobj ? 1 : -1;
    map_anim_args[1] = index;
    map_anim_args[2] = flags;
    map_anim_args[3] = anim;
    map_anim_frames[0] = start;
    map_anim_frames[1] = rate;
}
void TestCollision(int group) { ++collision_calls; collision_group = group; }
void TestAssert(void) { ++asserts; }

static void TestJoint(JOBJ *joint)
{
    for (unsigned i = 0; i < sizeof(*joint); ++i)
        ((volatile unsigned char *)joint)[i] = 0;
    joint->scale.X = joint->scale.Y = joint->scale.Z = 1;
}

static void TestSide(int index, int map_id, int group, float height, int own_data)
{
    Side *side = &sides[index];
    for (unsigned i = 0; i < sizeof(*side); ++i)
        ((volatile unsigned char *)side)[i] = 0;
    TestJoint(&side->column);
    TestJoint(&side->water);
    TestJoint(&side->joint);
    side->column.child = &side->water;
    side->water.parent = &side->column;
    side->joint.trans.X = group ? 47 : -47;
    side->gobj.hsd_object = &side->column;
    side->gobj.userdata = &side->data;
    side->data.gobj = own_data ? &side->gobj : &sides[0].gobj;
    side->data.map_id = map_id;
    side->data.line_group = group;
    side->data.joint = &side->joint;
    side->data.height = side->data.target = height;
    side->data.column_height = 45;  // the spawn's fallback (r2 - 19416)
    side->data.resurface = 25;
}

/* The fountain and the lookalike come first on the map p_link, then left, right. */
int TestFountainInit(int stage)
{
    stage_id = stage;
    remove_anim_calls = map_anim_calls = collision_calls = asserts = 0;
    collision_group = -1;
    TestSide(0, 4, 0, 20, 1);
    TestSide(1, 4, 1, 28, 1);
    TestSide(2, 3, 0, 20, 1);
    TestSide(3, 4, 0, 20, 0);
    for (int i = 0; i < 64; ++i)
        plinks[i] = 0;
    plinks[MATCHPLINK_MAP] = &sides[2].gobj;
    sides[2].gobj.next = &sides[3].gobj;
    sides[3].gobj.next = &sides[0].gobj;
    sides[0].gobj.next = &sides[1].gobj;
    *stc_gobj_lookup = plinks;
    return 1;
}

void TestFountainApply(int left, int right) { Fountain_Apply(left, right); }
unsigned TestFountainGObj(int index) { return (unsigned)&sides[index].gobj; }
unsigned TestFountainSide(int index) { return (unsigned)&sides[index]; }
unsigned TestFountainLabel(int index) { return (unsigned)Fountain_Labels[index]; }
int TestFountainInfo(int what)
{
    switch (what) {
    case 0: return remove_anim_calls;
    case 1: return map_anim_calls;
    case 2: return collision_calls;
    case 3: return collision_group;
    case 4: return asserts;
    case 5: return FOUNTAIN_CHOICE_COUNT;
    case 6: return map_anim_args[0];
    case 7: return map_anim_args[1];
    case 8: return map_anim_args[2];
    case 9: return map_anim_args[3];
    case 10: return map_anim_frames[0] == 0.0f && map_anim_frames[1] == 1.0f;
    }
    return -1;
}
/* Offsets the test reads each Side at. */
int TestFountainLayout(int what)
{
    switch (what) {
    case 0: return __builtin_offsetof(Side, data);
    case 1: return __builtin_offsetof(Side, column);
    case 2: return __builtin_offsetof(Side, water);
    case 3: return __builtin_offsetof(Side, joint);
    case 4: return sizeof(Side);
    case 5: return __builtin_offsetof(JOBJ, flags);
    case 6: return __builtin_offsetof(JOBJ, scale);
    case 7: return __builtin_offsetof(JOBJ, trans);
    }
    return -1;
}
