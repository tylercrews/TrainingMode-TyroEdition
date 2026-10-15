/* Production HUD/attempt code; native drawing, audio and formatting are boundaries. */
#if defined(STICK_HUD_SHARED)
#include "../src/events.c"
static GOBJ hud_camera;
static HUDCamData hud;
int CObj_SetCurrent(COBJ *camera) { return 1; }
COBJ *COBJ_GetCurrent(void) { return 0; }
void TestHUDInit(void) { hud_camera.userdata = &hud; stc_event_vars.hudcam_gobj = &hud_camera; }
void TestHUDDraw(int row, int hidden) {
    static const char *labels[] = {"Angle", "Timing", "Rate"};
    static const char *info[] = {"20.00", "Perfect", "50%"};
    Vec2 direction = {-95, -29}; hud.hide = hidden;
    HUD_DrawInfoPanelWithStick(labels, info, 3, row, &direction);
}
#else
#if defined(STICK_HUD_WAVEDASH)
#include "../src/wavedash.c"
static WavedashData data;
static HSD_PadData raw_pad;
#else
#include "../src/ledgedash.c"
static LedgedashData data;
static LdshHitlogData hitlog;
static GOBJ hitlog_object;
#endif
static EventVars vars;
static GOBJ event, fighter;
static FighterData ft;
static Vec2 drawn;
static int drawn_row;
static void Panel(const char **labels, const char **info, int count, int row, const Vec2 *direction) {
    drawn = *direction; drawn_row = row;
}
static void Rects(Rect *rects, GXColor *colors, int count) { }
static void Tris(Tri *tris, GXColor *colors, int count) { }
static void TextDraw(const char *text, Rect *rect, float size) { }
static Rect LogBar(u8 *log, GXColor *colors, int count) { return (Rect){0}; }
static void LogKey(char **names, GXColor *colors, int count) { }
GOBJ *Fighter_GetGObj(int player) { return &fighter; }
int SFX_Play(int id) { return 0; }
int SFX_PlayCommon(int id) { return 0; }
void Fighter_ColAnim_Apply(FighterData *data, int kind, int unused) { }
void Fighter_ColAnim_Remove(FighterData *data, int kind) { }
float atan2(float y, float x) { return .3f; } /* Signed snapshots are independent of folded degrees. */
int sprintf(char *out, const char *format, ...) { *out = '-'; out[1] = 0; return 1; }
void TestHUDInit(void) {
    *event_vars_ptr_loc = &vars; vars.event_gobj = &event;
    vars.HUD_DrawInfoPanelWithStick = Panel; vars.HUD_DrawRects = Rects;
    vars.HUD_DrawTris = Tris; vars.HUD_DrawText = TextDraw;
    vars.HUD_DrawActionLogBar = LogBar; vars.HUD_DrawActionLogKey = LogKey;
    event.userdata = &data; fighter.userdata = &ft;
    ft.phys.air_state = 1; ft.atk_kind = 1; ft.attr.jump_startup_time = 3;
#if defined(STICK_HUD_WAVEDASH)
    stc_padlibdata->qnum = 1; stc_padlibdata->qread = 0; stc_padlibdata->queue = &raw_pad;
    data.result = 0; data.wd_attempted = 1; WdOptions_Main[OPT_SHORT_HOP].val = 0;
#else
    LdshOptions_Main[OPT_TIPS].val = 0; LdshOptions_Main[OPT_CRITERION].val = LDSH_WAVELAND;
    data.attempt.phase = LDSH_RELEASED;
    hitlog_object.userdata = &hitlog; data.hitlog_gobj = &hitlog_object;
#endif
}
void TestHUDSample(int x, int y) {
    ft.state_id = ASID_ESCAPEAIR; ft.TM.state_frame = 0;
    ft.input.lstick = (Vec2){x, y}; ft.phys.air_state = 1;
#if defined(STICK_HUD_WAVEDASH)
    raw_pad.stat[0].stickX = x; raw_pad.stat[0].stickY = y;
    Wavedash_Think(&data, &ft);
#else
    Ledgedash_HUDThink(&data, &ft);
#endif
}
void TestHUDPublish(void) {
    ft.state_id = ASID_WAIT; ft.TM.state_frame = 1;
#if defined(STICK_HUD_WAVEDASH)
    data.timer = 12; data.airdodge_count = 1; data.airdodge_frame[0] = 4;
    Wavedash_Think(&data, &ft);
#else
    ft.phys.air_state = 0; ft.coll_data.envFlags = ECB_GROUND;
    Ledgedash_HUDThink(&data, &ft);
#endif
}
void TestHUDRetry(void) {
#if defined(STICK_HUD_WAVEDASH)
    data.timer = 0;
#else
    Ledgedash_InitVariables(&data); data.attempt.phase = LDSH_RELEASED;
#endif
}
void TestHUDDraw(int unused, int unused2) {
#if defined(STICK_HUD_WAVEDASH)
    HUD_GX(&event, 2);
#else
    Ledgedash_HitLogGX(&hitlog_object, 2);
#endif
}
int TestHUDDirection(unsigned axis) { return axis ? drawn.Y : drawn.X; }
int TestHUDRow(void) { return drawn_row; }
#endif
void memcpy(void *dst, const void *src, int size) { u8 *d = dst; const u8 *s = src; while(size--) *d++ = *s++; }
void memset(void *dst, int value, int size) { u8 *d = dst; while(size--) *d++ = value; }
