/* Exercise the shared production input handler with native side effects stubbed. */
#include "../src/events.h"

static EventVars vars;
static GOBJ menu_gobj;
static MenuData data;
static EventMenu root, submenu;
static int exits, saves, destroyed, custom_thinks;
static uint32_t GetSetting(unsigned field, unsigned index) { return 0; }
static const TMSettingsAPI settings = {.get = GetSetting};
static void Exit(GOBJ *gobj) { if (gobj == &menu_gobj) ++exits; }
static void Destroy(GOBJ *gobj) { ++destroyed; }
static int CustomThink(GOBJ *gobj) { ++custom_thinks; return 0; }

void Memcard_SaveIfChanged(void) { ++saves; }
void Match_UnfreezeGame(int kind) {}
void Match_FreezeGame(int kind) {}
void Match_ShowHUD(void) {}
void Match_HideHUD(void) {}
void Match_AdjustSoundOnPause(int paused) {}
int SFX_PlayCommon(int id) { return 0; }

int TestMenuExitInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.settings = &settings;
    vars.game_timer = 10;
    menu_gobj.userdata = &data;
    root.on_exit = Exit;
    data.curr_menu = data.root_menu = &root;
    return 1;
}
void TestMenuExitState(int paused, int nested, int custom, int timer) {
    data.mode = paused;
    data.curr_menu = nested ? &submenu : &root;
    data.custom_gobj_think = custom ? CustomThink : 0;
    data.custom_gobj_destroy = custom ? Destroy : 0;
    data.custom_gobj = custom ? &menu_gobj : 0;
    vars.game_timer = timer;
}
void TestMenuExitPad(int port, int held, int down, int triggers) {
    HSD_Pad *pad = PadGetMaster(port);
    pad->held = held;
    pad->down = down;
    pad->triggerLeft = triggers >> 8;
    pad->triggerRight = triggers;
}
void TestMenuExitUpdate(void) { EventMenu_Update(&menu_gobj); }
int TestMenuExitResult(int which) {
    switch (which) {
    case 0: return exits;
    case 1: return saves;
    case 2: return destroyed;
    case 3: return custom_thinks;
    case 4: return data.mode;
    case 5: return data.controller_index;
    default: return 0;
    }
}
