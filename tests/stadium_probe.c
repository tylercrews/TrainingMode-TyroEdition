#include "../src/stadium.c"

static GOBJ manager, terrain[4];
static StadiumData state;
static unsigned char terrain_data[4][0x180];
static JOBJ joints[4];
static int stage_id, removes, adds, wind_calls, coll_calls, load_calls;
static char *loaded_file;
static int terrain_count;
static int load_wait;

int Stage_GetExternalID(void) { return stage_id; }
GOBJ *Stage_GetMapGObj(int index) { return index == 2 ? &manager : 0; }
void GObj_RemoveProc(GOBJ *gobj) { ++removes; }
GOBJProc *GObj_AddProc(GOBJ *gobj, void *callback, int priority) { ++adds; return 0; }
void Stage_UpdateMovingColl(GOBJ *gobj) { ++coll_calls; }
void Dynamics_DecayWind(void) { ++wind_calls; }
void Stage_UnloadSecondaryArchive(void *archive) {}
void File_ReadAsync(char *name, void *buffer, unsigned int *size, void *cb, int arg)
{
    loaded_file = name;
    ++load_calls;
    load_wait = 3;
}
/* Native asynchronous completion registers the loaded secondary archive. */
int TestStadiumLoaded(void)
{
    if (load_wait-- > 0) return 0;
    state.flags &= ~0x40;
    state.archive = (void *)0x805F0000;
    return 1;
}
GOBJ *TestStadiumCreate(int kind)
{
    int i = (++terrain_count) % 4;
    terrain[i].userdata = terrain_data[i];
    terrain[i].hsd_object = &joints[i];
    joints[i].scale.X = joints[i].scale.Y = joints[i].scale.Z = 1;
    return &terrain[i];
}
int TestStadiumInit(int stage, int hazards_disabled)
{
    stage_id = stage;
    for (unsigned i = 0; i < sizeof(state); ++i)
        ((volatile unsigned char *)&state)[i] = 0;
    manager.userdata = &state;
    manager.hsd_object = &joints[0];
    state.kind = 5;
    state.terrain = TestStadiumCreate(5);
    state.file_buffer = (void *)0x805E0000;
    stc_stage->map_gobjs[2] = &manager;
    stc_stage->map_gobjs[1] = 0;
    stc_stage->grGroundParam = 0;
    Stadium_Init(hazards_disabled);
    return 1;
}
int TestStadiumTick(void) { Stadium_Think(&manager); return state.kind; }
int TestStadiumSelect(int value) { Stadium_Select(value); return requested; }
unsigned TestStadiumFile(void) { return (unsigned)loaded_file; }
unsigned TestStadiumState(void) { return (unsigned)&state; }
int TestStadiumInfo(int what)
{
    switch (what) {
    case 0: return state.state;
    case 1: return load_calls;
    case 2: return adds;
    case 3: return removes;
    case 4: return wind_calls;
    case 5: return coll_calls;
    case 6: return state.kind;
    }
    return -1;
}
