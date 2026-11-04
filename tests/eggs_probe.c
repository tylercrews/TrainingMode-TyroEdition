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
static int stale[11];
void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
static u32 Read(unsigned field, unsigned index) {
    return field == TM_SETTING_EGGS_COUNTER ? saved_position :
        field == TM_SETTING_EGGS ? preferences[index] : 0;
}
static void Write(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_EGGS_COUNTER) { saved_position = value; ++position_writes; }
    if (field == TM_SETTING_EGGS) preferences[index] = value;
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
int Stage_GetExternalID(void) { return GRKINDEXT_FD; }
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
