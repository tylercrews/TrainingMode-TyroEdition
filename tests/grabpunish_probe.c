/* Production event, with fixtures only at native engine boundaries. */
#include "../src/grabpunish.c"
#include <stdarg.h>
enum { HMN, MATE, ENEMY, PLAYERS };
static EventVars vars;
static GOBJ event, fighters[PLAYERS];
static FighterData ft[PLAYERS];
static JOBJ joints[PLAYERS];
static Playerblock players[6];
static CollDataStage test_stage;
static CollLine test_line;
static CollLineDesc test_descriptor;
static CollVert test_vertices[2];
static ftCommonData common;
static unsigned preferences[TM_GP_PREF_COUNT] = {0, 0, TM_GP_DEFAULT_PERCENT, TM_GP_DEFAULT_MASH, 0, 0};
static int loads, saves, sfx, hud_percent[PLAYERS], panel_rows, grab_misses, release_length = 30;
static float big_size;
static char panel_label[3][24], panel_info[3][24], big_text_shown[8];
static GXColor big_color, overlay[PLAYERS];
static int overlay_on[PLAYERS], overlay_max[PLAYERS];

void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
static void Copy(char *out, const char *in) { while ((*out++ = *in++)) { } }
static char *Number(char *out, int value) {
    char digits[12];
    int count = 0;
    if (value < 0) { *out++ = '-'; value = -value; }
    do { digits[count++] = '0' + value % 10; value /= 10; } while (value);
    while (count) *out++ = digits[--count];
    return out;
}
/* The event's formats: %d, %s, %.1f and %%. */
int sprintf(char *out, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (; *fmt; ++fmt) {
        if (*fmt != '%') { *out++ = *fmt; continue; }
        ++fmt;
        if (*fmt == 'd') out = Number(out, va_arg(args, int));
        else if (*fmt == 's') { const char *s = va_arg(args, const char *); while (*s) *out++ = *s++; }
        else if (*fmt == '%') *out++ = '%';
        else {
            int tenths = (int)(va_arg(args, double) * 10 + .5);
            fmt += 2; /* ".1f" */
            out = Number(out, tenths / 10);
            *out++ = '.';
            *out++ = '0' + tenths % 10;
        }
    }
    *out = 0;
    va_end(args);
    return 0;
}
static u32 Read(unsigned field, unsigned index) { return preferences[index]; }
static void Write(unsigned field, unsigned index, u32 value) { preferences[index] = value; }
static const TMSettingsAPI api = {Read, Write, 0};
static int Save(void *state, int flags) { ++saves; return 1; }
/* The real load rewinds game_timer, which is what shuts the pause menu. */
static int Load(void *state, int flags) {
    ++loads;
    vars.game_timer = 1;
    for (int i = 0; i < PLAYERS; ++i) {
        ft[i].dmg.percent = ft[i].dmg.percent_temp = 0;
        ft[i].state_id = ASID_FALL;
    }
    return 1;
}
static void Panel(const char **label, const char **info, int count) {
    panel_rows = count;
    for (int i = 0; i < count && i < 3; ++i) { Copy(panel_label[i], label[i]); Copy(panel_info[i], info[i]); }
}
static void DrawTextEx(const char *text, Rect *pos, float size, GXColor text_color, GXColor border_color,
    float border_offset, int align) {
    Copy(big_text_shown, text);
    big_color = text_color;
    big_size = size;
}
static void SetOverlay(GOBJ *fighter, const GXColor *color) {
    int i = fighter - fighters;
    overlay_on[i] = color != 0;
    if (!color) return;
    overlay[i] = *color;
    if (overlay_max[i] < color->a) overlay_max[i] = color->a;
}

GOBJ *Fighter_GetGObj(int player) { return player < PLAYERS ? &fighters[player] : 0; }
Playerblock *Fighter_GetPlayerblock(int player) { return &players[player]; }
void GObj_AddGXLink(GOBJ *gobj, void *cb, int gx_link, int gx_pri) { }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Fighter_SetPosition(int ply, int ms, Vec3 *pos) { }
void Fighter_SetGrounded(FighterData *f) { f->phys.air_state = 0; }
int GrColl_CheckIfLineEnabled(int line) { return 1; }
int Stage_GetExternalID(void) { return GRKINDEXT_FD; }
void Fighter_SetAllHurtboxState(GOBJ *f, int kind) { }
void Fighter_UpdateCameraBox(GOBJ *f) { }
void EnvironmentCollision_WaitLanding(GOBJ *f) { }
void Fighter_EnterWait(GOBJ *f) { ((FighterData *)f->userdata)->state_id = ASID_WAIT; }
void Match_CorrectCamera(void) { }
void Fighter_SetHUDDamage(int player, u16 percent) { hud_percent[player] = percent; }
void Fighter_ZeroCPUInputs(FighterData *f) {
    f->cpu.held = 0;
    f->cpu.lstickX = f->cpu.lstickY = 0;
}
int Pause_CheckStatus(int kind) { return 0; }
int SFX_Play(int id) { sfx = id; return 0; }
int HSD_Randi(int max) { return 0; }
int Fighter_GetCurrentPlacing(int player) { return 0; }
int Fighter_GetHandicap(int player) { return 9; }
float Fighter_GetCurrentAnimLength(GOBJ *f) { return release_length; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS(void) { }

int TestGPInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event;
    vars.settings = &api;
    vars.Savestate_Save_v1 = (void *)Save;
    vars.Savestate_Load_v1 = (void *)Load;
    vars.HUD_DrawInfoPanel = Panel;
    vars.HUD_DrawTextEx = DrawTextEx;
    vars.set_timing_overlay = SetOverlay;
    *stc_ftcommon = &common;
    for (int i = 0; i < PLAYERS; ++i) {
        fighters[i].userdata = &ft[i];
        fighters[i].hsd_object = &joints[i];
        ft[i].ply = i;
        ft[i].state.rate = 1;
        players[i].state = 1;
        players[i].p_kind = i == HMN ? PKIND_HMN : PKIND_CPU;
        players[i].team = i == ENEMY ? 1 : 0;
    }
    /* Final Destination's ground, so every spot falls back to it. */
    test_stage.line_num = 1;
    test_stage.vert_num = 2;
    test_line.desc = &test_descriptor;
    test_descriptor.vert_prev = 0;
    test_descriptor.vert_next = 1;
    test_descriptor.is_floor = 1;
    test_vertices[0].pos_curr = (Vec2){-85.5f, 0};
    test_vertices[1].pos_curr = (Vec2){85.5f, 0};
    *stc_colldata = &test_stage;
    *stc_collline = &test_line;
    *stc_collvert = test_vertices;
    Event_Init(&event);
    return 0;
}
/* What the engine does between the event's thinks. */
static void Engine(void) {
    for (int i = 0; i < PLAYERS; ++i) {
        ft[i].dmg.percent += ft[i].dmg.percent_temp;
        ft[i].dmg.percent_temp = 0;
    }
    if (!grab_misses && ft[MATE].state_id == ASID_WAIT && (ft[MATE].cpu.held & PAD_TRIGGER_Z))
        ft[MATE].state_id = ASID_CATCH;
    FighterData *enemy = &ft[ENEMY];
    if (enemy->state_id == ASID_CAPTURECUT) {
        enemy->state.frame += enemy->state.rate;
        if (enemy->state.frame >= release_length) enemy->state_id = ASID_WAIT;
    }
}
void TestGPThink(int frames) {
    while (frames--) {
        Engine();
        ++vars.game_timer;
        Event_Think(&event);
        big_text_shown[0] = 0;
        DrawHUD(&event, 2);
    }
}
void TestGPMissGrabs(int value) { grab_misses = value; }
void TestGPGrabbed(int timer) {
    ft[MATE].state_id = ASID_CATCHWAIT;
    ft[ENEMY].state_id = ASID_CAPTUREWAITHI;
    ft[ENEMY].grab.grab_timer = timer;
}
void TestGPTimer(int timer) { ft[ENEMY].grab.grab_timer = timer; }
void TestGPLetGo(int length) {
    release_length = length;
    ft[ENEMY].state_id = ASID_CAPTURECUT;
    ft[ENEMY].state.frame = -ft[ENEMY].state.rate; /* Engine() makes the entry frame 0. */
    if (ft[MATE].state_id == ASID_CATCHWAIT) ft[MATE].state_id = ASID_WAIT;
}
void TestGPHit(int who, int damage) { ft[who].dmg.percent_temp = damage; }
void TestGPState(int who, int state) { ft[who].state_id = state; }
void TestGPMove(int who, int x) { ft[who].phys.pos.X = x; }
void TestGPDown(int buttons) { ft[HMN].input.down = buttons; }
void TestGPChoose(int row, int value) {
    Options_Main[row].val = value;
    Options_Main[row].OnChange(0, value);
}
int TestGPInfo(int key) {
    switch (key) {
    case 0: return loads;
    case 1: return saves;
    case 2: return vars.game_timer;
    case 3: return ft[ENEMY].dmg.percent;
    case 4: return ft[ENEMY].dmg.percent_temp;
    case 5: return hud_percent[ENEMY];
    case 6: return ft[HMN].phys.pos.X;
    case 7: return ft[MATE].phys.pos.X * 10;
    case 8: return ft[ENEMY].phys.pos.X * 10;
    case 9: return panel_rows;
    case 10: return overlay_on[ENEMY];
    case 11: return overlay[ENEMY].r << 16 | overlay[ENEMY].g << 8 | overlay[ENEMY].b;
    case 12: return overlay[ENEMY].a;
    case 13: return overlay_max[ENEMY];
    case 14: return big_color.r << 16 | big_color.g << 8 | big_color.b;
    case 15: return big_size * 100;
    case 16: return sfx;
    case 17: return preferences[TM_GP_RELEASE_HITS];
    case 18: return phase;
    case 19: return ft[MATE].cpu.held;
    }
    return -1;
}
char *TestGPText(int key) {
    return key < 3 ? panel_label[key] : key < 6 ? panel_info[key - 3] : big_text_shown;
}
