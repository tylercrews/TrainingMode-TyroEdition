/* Production event, with fixtures only at native engine boundaries. */
#include "../src/eggs.c"
#include <stdarg.h>
static EventVars vars;
static GOBJ event, fighter, marker_gobj, *lookup[64];
static FighterData ft;
static CmSubject subject;
static Text texts[2];
static GXColor *label_color;
static const char *label_format;
static int text_count, saved_position, position_writes, retries;
static unsigned preferences[TM_EGGS_PREF_COUNT] = {TM_EGGS_DEFAULT_DAMAGE, 0, TM_EGGS_DEFAULT_VELOCITY, 0, 0};
static unsigned spawn_preferences[TM_EGGS_SPAWN_PREF_COUNT];
static int stale[11];
static int stage_id = GRKINDEXT_FD, platforms, rand_first, rand_pick, rand_side;
static unsigned rand_seed = 12345;
static SpawnItem spawned;
static GOBJ egg_object;
static ItemData egg_item;
static u8 egg_callbacks[64];
static JOBJ egg_joint;
/* Battlefield's side platforms, as live collision lines over its ground. */
#define PLATFORM_Y 27.f
static CollDataStage test_stage;
static CollLine test_lines[2];
static CollLineDesc test_descriptors[2];
static CollVert test_vertices[4];
void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
static u32 Read(unsigned field, unsigned index) {
    return field == TM_SETTING_EGGS_COUNTER ? saved_position :
        field == TM_SETTING_EGGS_SPAWN ? spawn_preferences[index] :
        field == TM_SETTING_EGGS ? preferences[index] : 0;
}
static void Write(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_EGGS_COUNTER) { saved_position = value; ++position_writes; }
    if (field == TM_SETTING_EGGS) preferences[index] = value;
    if (field == TM_SETTING_EGGS_SPAWN) spawn_preferences[index] = value;
    if (field == TM_SETTING_EVENT_RESET)
        for (int i = 0; i < TM_EGGS_PREF_COUNT; ++i)
            preferences[i] = i == TM_EGGS_DAMAGE ? TM_EGGS_DEFAULT_DAMAGE :
                i == TM_EGGS_VELOCITY ? TM_EGGS_DEFAULT_VELOCITY : 0;
}
static const TMSettingsAPI api = {Read, Write, 0};
static void Configure(unsigned enabled, unsigned mode) { }
static const TMTrailAPI trails = {.configure = Configure};
GOBJ *Fighter_GetGObj(int player) { return player ? 0 : &fighter; }
int *Fighter_GetStaleMoveTable(int player) { return stale; }
GOBJ *GObj_Create(int entity_class, int p_link, int p_priority) { return &marker_gobj; }
void GObj_AddObject(GOBJ *gobj, u8 kind, void *object) { }
void GObj_AddGXLink(GOBJ *gobj, void *cb, int link, int priority) { }
void GXLink_Common(GOBJ *gobj, int pass) { }
CmSubject *CameraSubject_Alloc() { return &subject; }
int Stage_GetExternalID(void) { return stage_id; }
float Stage_GetCameraLeft(void) { return -100; }
float Stage_GetCameraRight(void) { return 100; }
float Stage_GetCameraTop(void) { return 150; }
float Stage_GetCameraBottom(void) { return -50; }
int GrColl_CheckIfLineEnabled(int line) { return 1; }
float sqrtf(float value) {
    float root = value > 1 ? value : 1;
    for (int i = 0; i < 24; ++i) root = (root + value / root) / 2;
    return root;
}
int HSD_Randi(int max) {
    if (rand_first) { rand_first = 0; return rand_pick < max ? rand_pick : 0; }
    return rand_side;
}
float HSD_Randf(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16 & 0x7FFF) / 32768.f;
}
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *unk1, int unk2, int unk3, int unk4, void *cb,
    float x, float y, float to_x, float to_y, float unk5) {
    int over_platform = platforms && ((x >= 20 && x <= 58) || (x >= -58 && x <= -20));
    pos->X = x; pos->Z = 0;
    if (over_platform && y >= PLATFORM_Y) { pos->Y = PLATFORM_Y; return 1; }
    if (y >= 0 && x >= -85 && x <= 85) { pos->Y = 0; return 1; }
    return 0;
}
GOBJ *Item_CreateItem2(SpawnItem *item) { spawned = *item; return &egg_object; }
JOBJ *Item_GetBoneJOBJ(GOBJ *item, int bone) { return &egg_joint; }
int Events_GetSavedScore(int event) { return 0; }
int Pause_CheckStatus(int kind) { return 0; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { ++retries; }
int Text_CreateCanvas(int sis, GOBJ *cam, int entity, int plink, int priority, int gxlink, int gxpri, int cobjpri) { return 1; }
Text *Text_CreateText(int sis, int canvas) { return &texts[text_count++ % 2]; }
int Text_AddSubtext(Text *text, float x, float y, char *fmt, ...) { return 0; }
void Text_SetColor(Text *text, int row, GXColor *color) { if (text == &texts[1]) label_color = color; }
void Text_SetText(Text *text, int row, const char *fmt, ...) { if (text == &texts[1]) label_format = fmt; }
void TestEggsLoaded(void) { }
void TestEggsInit(int position, int free_practice) {
    *event_vars_ptr_loc = &vars;
    *stc_gobj_lookup = lookup;
    vars.settings = &api; vars.trails = &trails;
    fighter.userdata = &ft;
    saved_position = position;
    preferences[TM_EGGS_FREE_PRACTICE] = free_practice;
    stc_match->match.timer = 0;
    Event_Init(&event);
}
void TestEggsThink(void) { Event_Think(&event); }
void TestEggsChoose(int value) {
    Options_Main[OPT_COUNTERPOSITION].val = value;
    Options_Main[OPT_COUNTERPOSITION].OnChange(0, value);
}
void TestEggsStage(int stage, int with_platforms) {
    stage_id = stage;
    platforms = with_platforms;
    test_stage.line_num = with_platforms ? 2 : 0;
    test_stage.vert_num = 4;
    for (int i = 0; i < 2; ++i) {
        float side = i ? 1 : -1;
        test_lines[i].desc = &test_descriptors[i];
        test_descriptors[i].vert_prev = i * 2;
        test_descriptors[i].vert_next = i * 2 + 1;
        test_descriptors[i].is_floor = 1;
        ((u8 *)&test_descriptors[i])[0xE] = 1; /* LINE_FLAG_PLATFORM */
        test_vertices[i * 2].pos_curr = (Vec2){20 * side, PLATFORM_Y};
        test_vertices[i * 2 + 1].pos_curr = (Vec2){58 * side, PLATFORM_Y};
    }
    *stc_colldata = &test_stage;
    *stc_collline = test_lines;
    *stc_collvert = test_vertices;
}
int TestEggsStageID(int with_platforms) { return with_platforms ? GRKINDEXT_BATTLE : GRKINDEXT_FD; }
void TestEggsResetSpawn(void) { Eggs_ResetSettings(0); }
void TestEggsSpawnOption(int option, int value) {
    int row = spawn_eggs_rows[option];
    Options_Main[row].val = value;
    Options_Main[row].OnChange(0, value);
}
/* pick is the even draw between spots, side a side platform's. */
void TestEggsSpawn(int pick, int side) {
    rand_first = 1; rand_pick = pick; rand_side = side;
    Egg_Spawn();
}
int TestEggsSpawned(int key) {
    switch (key) {
    case 0: return spawned.pos.X * 1000;
    case 1: return spawned.pos.Y * 1000;
    case 2: return spawned.vel.Y * 1000;
    case 3: return spawned.pos2.Y * 1000;
    case 4: return last_coll_pos.Y * 1000;
    }
    return -1;
}
/* The egg as the event sees it, with the engine's velocity for this frame. */
int TestEggsFall(int velocity) {
    egg_object.userdata = &egg_item;
    egg_item.it_func = (void *)egg_callbacks;
    egg_gobj = &egg_object;
    egg_item.self_vel.Y = velocity / 1000.f;
    Event_Think(&event);
    return egg_item.self_vel.Y * 1000;
}
int TestEggsSpawnInfo(int key) {
    switch (key) {
    case 0: return spawn_preferences[0];
    case 1: return spawn_preferences[1];
    case 2: return Options_Main[OPT_UNDERPLATFORMS].val;
    case 3: return Options_Main[OPT_INAIR].val;
    case 4: return Options_Main[OPT_UNDERPLATFORMS].disable;
    case 5: return Options_Main[OPT_INAIR].disable;
    }
    return -1;
}
void TestEggsReset(void) { Eggs_ResetSettings(0); }
int TestEggsText(int text, int key) {
    switch (key) {
    case 0: return texts[text].trans.X * 1000;
    case 1: return texts[text].trans.Y * 1000;
    case 2: return texts[text].align;
    }
    return -1;
}
int TestEggsInfo(int key) {
    EventOption *row = &Options_Main[OPT_COUNTERPOSITION];
    switch (key) {
    case 0: return saved_position;
    case 1: return position_writes;
    case 2: return row->val;
    case 3: return row->val_prev;
    case 4: return row->disable;
    case 5: return row->value_num;
    case 6: return row->kind == OPTKIND_STRING;
    case 7: return retries;
    case 8: return preferences[TM_EGGS_FREE_PRACTICE];
    case 9: return label_color == &text_gold;
    }
    return -1;
}
unsigned TestEggsName(void) { return (unsigned)Options_Main[OPT_COUNTERPOSITION].name; }
unsigned TestEggsChoice(int value) { return (unsigned)Options_Main[OPT_COUNTERPOSITION].values[value]; }
unsigned TestEggsLabel(void) { return (unsigned)label_format; }
