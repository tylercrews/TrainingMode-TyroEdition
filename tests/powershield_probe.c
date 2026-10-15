/* Production event with fixtures only at engine/render/save boundaries. */
#include "../src/powershield.c"
#include <stdarg.h>
static EventVars vars;
static GOBJ event, fighter[3], menu_object;
static FighterData ft[3];
static JOBJ joint[3];
static CmSubject camera[3];
static PowershieldData data;
static MenuData menu_data;
static Text text[2];
static int paused, text_count, rows[2], result[2], saved, ended, rng_bound, overlay_color;
static u32 text_colors[2][3];
static char labels[2][3][80];
static unsigned preferences[TM_PS_PREF_COUNT] = {0,0,20,0,0,0,1,1,0,1};
static const s16 *suppression;
static void (*contact_cb)(FighterData *, int, int, int);
static void (*timing_cb)(FighterData *, int, int);
static void (*clear_cb)(void);
static u32 Read(unsigned field, unsigned index) { return preferences[index]; }
static void Write(unsigned field, unsigned index, u32 value) { preferences[index] = value; }
static const TMSettingsAPI settings_api = {Read, Write, 0};
void memset(void *to, int value, int size) { u8 *p = to; while (size--) *p++ = value; }
void memcpy(void *to, const void *from, int size) { u8 *d = to; const u8 *s = from; while(size--) *d++ = *s++; }
static void Observe(void (*contact)(FighterData *, int, int, int), void (*timing)(FighterData *, int, int), void (*clear)(void)) {
    contact_cb = contact; timing_cb = timing; clear_cb = clear;
}
static void Suppress(const s16 *value) { suppression = value; }
static void Overlay(GOBJ *obj, const GXColor *color) { overlay_color = color ? *(u32 *)color : 0; }
GOBJ *Fighter_GetGObj(int ply) { return &fighter[ply]; }
GOBJ *Fighter_GetSubcharGObj(int ply, int sub) { return &fighter[sub ? 2 : ply]; }
void Fighter_ZeroCPUInputs(FighterData *data) { memset(&data->cpu, 0, sizeof(data->cpu)); }
void Fighter_EnterSleep(GOBJ *object, int sub) { ((FighterData *)object->userdata)->state_id = ASID_SLEEP; }
void Fighter_EnterDeadDown(GOBJ *object) { ((FighterData *)object->userdata)->state_id = ASID_DEADDOWN; }
void Fighter_EnterWait(GOBJ *object) { ((FighterData *)object->userdata)->state_id = ASID_WAIT; }
void Fighter_SetGrounded(FighterData *data) { data->phys.air_state = 0; }
void Fighter_SetPosition(int ply, int sub, Vec3 *position) { }
void Fighter_UpdateCameraBox(GOBJ *object) { }
void JOBJ_SetMtxDirtySub(JOBJ *object) { }
void EnvironmentCollision_WaitLanding(GOBJ *object) { }
void Match_CorrectCamera(void) { }
void Match_EndVS(void) { ended = 1; }
void Memcard_SaveIfChanged(void) { ++saved; }
int HSD_Randi(int bound) { rng_bound = bound; return 0; }
void __assert(const char *file, int line, const char *message) { }
int Pause_CheckStatus(int arg) { return paused; }
static int Save(Savestate_v1 *state, int flags) { return 1; }
static int Load(Savestate_v1 *state, int flags) { if (clear_cb) clear_cb(); return 1; }
int Text_CreateCanvas(int a, GOBJ *b, int c, int d, int e, int f, int g, int h) { return 1; }
Text *Text_CreateText(int sis, int canvas) { return &text[text_count++]; }
int Text_AddSubtext(Text *t, float x, float y, char *format, ...) { return rows[t - text]++; }
void Text_SetColor(Text *t, int row, GXColor *color) { text_colors[t - text][row] = *(u32 *)color; }
void Text_SetText(Text *t, int row, const char *format, ...) {
    unsigned which = t - text;
    va_list args; va_start(args, format);
    const char *source = format;
    if (format[0] == '%' && format[1] == 's') source = va_arg(args, const char *);
    else if (row == 1) result[which] = format[0] == '-' ? -1 : va_arg(args, int);
    unsigned i = 0;
    for (; source[i] && i < 79; ++i) labels[which][row][i] = source[i];
    labels[which][row][i] = 0;
    va_end(args);
}
void TestPSInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event; vars.menu_gobj = &menu_object; vars.settings = &settings_api;
    vars.set_powershield_observer = Observe; vars.set_osd_suppression = Suppress; vars.set_timing_overlay = Overlay;
    vars.Savestate_Save_v1 = Save; vars.Savestate_Load_v1 = Load;
    event.userdata = &data; menu_object.userdata = &menu_data;
    for (int i = 0; i < 3; ++i) {
        fighter[i].userdata = &ft[i]; fighter[i].hsd_object = &joint[i];
        ft[i].fighter = &fighter[i]; ft[i].ply = i == 2 ? 0 : i; ft[i].flags.ms = i == 2;
        ft[i].camera_subject = &camera[i]; ft[i].state_id = ASID_WAIT;
    }
    ft[1].kind = FTKIND_FALCO;
    Event_Init(&event); Event_Update();
}
void TestPSReenter(void) { text_count = rows[0] = rows[1] = 0; TestPSInit(); }
void TestPSContact(int kind, int frame, int reason, int slot) { contact_cb(&ft[slot], kind, frame, reason); Event_Update(); }
void TestPSTiming(int phase, int next) { timing_cb(&ft[0], phase, next); Event_Update(); }
void TestPSSample(int tick, int state, int stun, int frozen) {
    stc_match->time_frames = tick; ft[0].state_id = state;
    float remaining = stun; memcpy(&ft[0].state_var.state_var1, &remaining, 4);
    ft[0].flags.hitstun = stun > 0; ft[0].flags.hitlag = frozen; ft[0].flags.freeze = frozen;
}
void TestPSChoice(unsigned row, int value) {
    Options_Main[row].val = value;
    if (Options_Main[row].OnChange) Options_Main[row].OnChange(0, value);
    else if (Options_Main[row].OnSelect) Options_Main[row].OnSelect(0);
    Event_Update();
}
void TestPSPause(int value) { paused = value ? 2 : 0; menu_data.mode = value; Event_Update(); }
void TestPSRestore(void) { clear_cb(); Event_Update(); }
void TestPSExit(void) { Exit(0); }
int TestPSDelay(int low, int high) {
    Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val = low;
    Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val = high;
    GetRandomLaserDelay(); return rng_bound;
}
int TestPSInfo(unsigned field) {
    switch (field) {
    case 0: return data.act_frame;
    case 1: return data.act_kind;
    case 2: return overlay_color;
    case 3: return data.opportunities;
    case 4: return text[0].hidden;
    case 5: return text[1].hidden;
    case 6: return suppression ? *suppression : -1;
    case 7: return text_colors[0][1];
    case 8: return text_colors[1][1];
    case 9: return data.ps_valid;
    case 10: return contact_cb != 0;
    case 11: return saved;
    case 12: return ended;
    case 13: return text_count;
    case 14: return result[1];
    case 15: return (unsigned)&Options_Main;
    case 16: return (unsigned)&help_menu;
    case 17: return (unsigned)&ft[0];
    }
    return 0;
}
unsigned TestPSLabel(unsigned which, unsigned row, unsigned index) { return labels[which][row][index]; }
unsigned TestPSPreference(unsigned index) { return preferences[index]; }
