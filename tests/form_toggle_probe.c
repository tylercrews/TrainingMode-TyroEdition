/* The character select's Z toggle: production events.c on the CSS's own data
 * layout, with its native calls recorded. */
#include "../src/events.h"

int GetPageNum(void);
int GetPageEventNum(int page);

#define ZELDA_ICON 7
#define OTHER_ICON 3
#define NO_ICON 25

static u8 door_zelda[4];
static unsigned page;
static GOBJ proc_gobj;
static void (*proc)(GOBJ *gobj);
static int proc_link;
static MnSlChrTagData tags[4];
static Text names[4];
static CSSCursor cursors[4];
static int sheik_names[4], updates[4], voices, last_voice;

uint32_t Settings_Get(unsigned field, unsigned index) {
    if (field == TM_SETTING_DOOR_ZELDA) return index < 4 ? door_zelda[index] : 0;
    return field == TM_SETTING_EVENT_PAGE ? page : 0;
}
void Settings_Set(unsigned field, unsigned index, uint32_t value) {
    if (field == TM_SETTING_DOOR_ZELDA && index < 4) door_zelda[index] = value;
}
int Settings_DoorCharacter(int door, int kind) {
    return kind == CKIND_ZELDA && !door_zelda[door] ? CKIND_SHEIK : kind;
}

GOBJ *GObj_Create(int kind, int link, int priority) { return &proc_gobj; }
GOBJProc *GObj_AddProc(GOBJ *gobj, void *callback, int link) {
    proc = callback;
    proc_link = link;
    return 0;
}
char *CSS_GetFighterName(int kind) { return kind == CKIND_SHEIK ? "Sheik" : "Zelda"; }
void Text_SetText(Text *text, int subtext, const char *format, ...) {
    if (format[0] == 'S') ++sheik_names[text - names];
}
void CSS_UpdateCSP(int door) { ++updates[door]; }
void CSS_PlayFighterName(int kind) { ++voices; last_voice = kind; }

int TestFormInit(void) {
    *event_vars_ptr_loc = 0;
    return 1;
}
/* Native while the CSS loads: every door closed on no character. */
void TestFormEnter(EventDesc *event, int players) {
    for (page = 0; page <= GetPageNum(); ++page)
        for (int i = 0; i <= GetPageEventNum(page); ++i)
            if (GetEventDesc(page, i) == event) {
                stc_memcard->EventBackup.event = i;
                goto found;
            }
found:
    *stc_css_maxply = players;
    *stc_css_hmnport = 0;
    for (int i = 0; i < 25; ++i)
        stc_css_data->icons[i].c_kind = i == ZELDA_ICON ? CKIND_ZELDA : CKIND_FOX;
    for (int i = 0; i < 4; ++i) {
        stc_css_data->doors[i].sel_icon = NO_ICON;
        stc_css_data->doors[i].p_kind = PKIND_NONE;
        stc_css_data->tags[i].tag_data = &tags[i];
        tags[i].name = &names[i];
        tags[i].use_tag = 0;
        stc_css_cursors[i] = &cursors[i];
        cursors[i].state = SLCHRCUR_POINT;
        cursors[i].puck = 0;
        stc_css_pad[i].down = 0;
        door_zelda[i] = sheik_names[i] = updates[i] = 0;
    }
    voices = last_voice = 0;
    Settings_EventCharacters(TM_CHARACTERS_CSS_LOADED);
}
void TestFormDoor(int door, int icon) {
    stc_css_data->doors[door].sel_icon = icon;
    stc_css_data->doors[door].p_kind = icon == NO_ICON ? PKIND_NONE : PKIND_CPU;
}
void TestFormHold(int cursor, int puck) {
    cursors[cursor].state = SLCHRCUR_HOLD;
    cursors[cursor].puck = puck;
}
void TestFormTag(int door) { tags[door].use_tag = 1; }
void TestFormZelda(int door, int zelda) { door_zelda[door] = zelda; }
void TestFormPress(int port) {
    stc_css_pad[port].down = HSD_TRIGGER_Z;
    proc(&proc_gobj);
    stc_css_pad[port].down = 0;
}
void TestFormFrame(void) { proc(&proc_gobj); }
int TestFormCpu(void) { return Settings_EventCharacters(TM_CHARACTERS_CPU); }
int TestFormInfo(int key, int door) {
    switch (key) {
    case 0: return door_zelda[door];
    case 1: return sheik_names[door];
    case 2: return updates[door];
    case 3: return voices;
    case 4: return last_voice;
    case 5: return proc_link;
    }
    return -1;
}
