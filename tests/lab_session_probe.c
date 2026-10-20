/* Production Lab session settings (stage-change carry and Reset to Default
 * Settings), with fixtures only at native engine boundaries. */
#include "../src/lab.c"
#include <stdarg.h>

static EventVars vars;
static GOBJ menu_gobj, fighters[2], *gobj_lookup[32];
static FighterData ft[2];
static RNGControl rng;
static Text watermark;
static u32 settings[64];
static int camera, stadium = -1, trails_on = -1, trails_decay = -1, colanim, hud_damage[2], restarted = -1;
static int frees, current_stage = GRKINDEXT_PSTAD, sounds;
/* Fighters in play (external kind and costume) and the last character restart. */
static int live_kind[2] = {CKIND_FOX, CKIND_MARTH}, live_costume[2], fighter_restart[4] = {-9, -9, -9, -9};
static u8 heap[4096];
static int heap_used;
/* events.c's carry buffer, copied between machines by the test like the
 * boot-loaded code that outlives each event .dat. */
static u32 carry_tag, carry_size;
static u8 carry_data[TM_EVENT_CARRY_SIZE];

void memset(void *dest, int c, int size) { u8 *p = dest; while (size-- > 0) *p++ = c; }
void memcpy(void *dest, const void *source, int size) {
    u8 *d = dest; const u8 *s = source;
    while (size-- > 0) *d++ = *s++;
}
int sprintf(char *restrict out, const char *restrict format, ...) {
    va_list args; va_start(args, format);
    char *start = out;
    for (; *format; ++format) {
        if (format[0] == '%' && format[1] == 's') {
            for (const char *s = va_arg(args, const char *); *s; ++s) *out++ = *s;
            ++format;
        } else *out++ = *format;
    }
    *out = 0;
    va_end(args);
    return out - start;
}
void *HSD_MemAlloc(int size) { void *p = heap + heap_used; heap_used += (size + 31) & ~31; return p; }
void HSD_Free(void *ptr) { ++frees; }
GOBJ *Fighter_GetGObj(int ply) { return &fighters[ply]; }
void Fighter_SetHUDDamage(int player, u16 damage) { hud_damage[player] = damage; }
void Fighter_ColAnim_Apply(FighterData *data, int kind, int unk) { colanim = kind; }
void Fighter_ColAnim_Remove(FighterData *data, int kind) { colanim = -kind; }
void Match_SetNormalCamera() { camera = 1; }
void Match_SetFreeCamera(int unk, int unk2) { camera = 2; }
void Match_SetFixedCamera() { camera = 3; }
void Match_SetDevelopCamera() { camera = 4; }
void Match_CorrectCamera() { }
void Stadium_Select(int value) { stadium = value; }
int Stage_GetExternalID() { return current_stage; }
int Fighter_GetExternalID(int ply) { return live_kind[ply]; }
int Fighter_GetCostumeID(int ply) { return live_costume[ply]; }
int SFX_PlayCommon(int sfx) { sounds = sounds * 10 + sfx; return 0; }
static u32 Get(unsigned field, unsigned index) { return settings[index & 63]; }
static void Set(unsigned field, unsigned index, u32 value) { settings[index & 63] = value; }
static const TMSettingsAPI settings_api = {Get, Set, 0};
static void Configure(unsigned enabled, unsigned mode) { trails_on = enabled; trails_decay = mode; }
static const TMTrailAPI trails_api = {Configure, 0, 0};
static void Restart(int stage) { restarted = stage; }
static int RestartFighters(int hmn_kind, int hmn_costume, int cpu_kind, int cpu_costume) {
    fighter_restart[0] = hmn_kind; fighter_restart[1] = hmn_costume;
    fighter_restart[2] = cpu_kind; fighter_restart[3] = cpu_costume;
    return 1;
}
static void *Store(u32 tag, u32 size) {
    if (size > sizeof(carry_data)) return 0;
    carry_tag = tag; carry_size = size; return carry_data;
}
static const void *Take(u32 tag, u32 size) {
    if (carry_tag != tag || carry_size != size) return 0;
    carry_tag = 0; return carry_data;
}

int TestLabBoot(void) {
    *event_vars_ptr_loc = &vars;
    vars.settings = &settings_api; vars.trails = &trails_api; vars.rng = &rng;
    vars.watermark = &watermark; vars.menu_gobj = &menu_gobj;
    vars.carry_store = Store; vars.carry_take = Take; vars.restart_on_stage = Restart;
    vars.restart_on_fighters = RestartFighters;
    *stc_gobj_lookup = gobj_lookup;
    gobj_lookup[MATCHPLINK_FIGHTER] = &fighters[0];
    fighters[0].next = &fighters[1];
    for (int i = 0; i < 2; ++i) { fighters[i].userdata = &ft[i]; ft[i].ply = i; }
    Lab_InitMenuUI();
    return 1;
}
/* Event_Init's per-match option setup for a Fox CPU with Peach's Character RNG
 * rows on Pokemon Stadium, then its production session tail. */
int TestLabInit(void) {
    LabOptions_CPU[OPTCPU_RECOVERY].menu = &RecMenu_Fox;
    LabOptions_CPU[OPTCPU_RECOVERY].disable = 0;
    for (int i = 0; i < RecMenu_Fox.option_num; ++i) RecMenu_Fox.options[i].val = 1;
    for (int i = 0; i < ADV_COUNTER_COUNT; ++i)
        memcpy(LabOptions_AdvCounter[i], LabOptions_AdvCounter_Default, sizeof(LabOptions_AdvCounter_Default));
    for (int i = 0; i < ACTION_LOG_MAX; ++i) {
        memcpy(LabOptions_ActionLog[i], LabOptions_ActionLog_Default, sizeof(LabOptions_ActionLog_Default));
        LabOptions_ActionLog[i][OPTACTIONLOG_NUMBER].val = LabOptions_ActionLog[i][OPTACTIONLOG_NUMBER].val_prev = i + 1;
        LabOptions_ActionLog[i][OPTACTIONLOG_ACTION].val = LabOptions_ActionLog[i][OPTACTIONLOG_ACTION].val_prev = i;
    }
    memcpy(LabOptions_InfoDisplayHMN, LabOptions_InfoDisplayDefault, sizeof(LabOptions_InfoDisplayDefault));
    memcpy(LabOptions_InfoDisplayCPU, LabOptions_InfoDisplayDefault, sizeof(LabOptions_InfoDisplayDefault));
    LabOptions_InfoDisplayHMN[OPTINF_PRESET].OnChange = Lab_ChangeInfoPresetHMN;
    LabOptions_InfoDisplayCPU[OPTINF_PRESET].OnChange = Lab_ChangeInfoPresetCPU;
    for (int i = 0; i < (int)countof(LabOptions_CharacterRngPeach); ++i)
        LabMenu_CharacterRng.options[i] = LabOptions_CharacterRngPeach[i];
    LabMenu_CharacterRng.option_num = countof(LabOptions_CharacterRngPeach);
    LabOptions_Main[OPTLAB_STAGE].disable = 0;
    LabOptions_Main[OPTLAB_STAGE].menu = &LabMenu_Stage_Stadium;
    Lab_SessionInit();
    return lab_pending_carry != 0;
}

static EventOption *Option(int table, int index) {
    switch (table) {
    case 0: return &LabOptions_General[index];
    case 1: return &LabOptions_CPU[index];
    case 2: return &LabOptions_Tech[index];
    case 3: return &LabOptions_InfoDisplayHMN[index];
    case 4: return &LabOptions_HitboxTrails[index];
    case 5: return &LabMenu_CharacterRng.options[index];
    case 6: return &RecMenu_Fox.options[index];
    case 7: return &LabOptions_AdvCounter[index / OPTCTR_COUNT][index % OPTCTR_COUNT];
    case 8: return &LabOptions_ActionLog[index / OPTACTIONLOG_COUNT][index % OPTACTIONLOG_COUNT];
    case 9: return &LabOptions_Stage_Stadium[index];
    case 10: return &LabOptions_Controls[index];
    case 11: return &LabOptions_Record[index];
    }
    return 0;
}
/* As EventMenu_ChangeOptionVal: what choosing a value in the menu does. */
void TestLabChoose(int table, int index, int value) {
    EventOption *option = Option(table, index);
    option->val_prev = option->val;
    option->val = value;
    if (option->OnChange) option->OnChange(&menu_gobj, value);
}
int TestLabValue(int table, int index) { return Option(table, index)->val; }
int TestLabDisabled(int table, int index) { return Option(table, index)->disable; }
void TestLabCustomOSD(int state, const char *name) { Lab_AddCustomOSDRow(state, name); }
void TestLabLogState(int page, int state, const char *name) {
    Lab_CopyName(action_log_state_name_buffers[page], name, sizeof(action_log_state_name_buffers[page]));
    ActionLog_SetState(page, state);
}
void TestLabTDI(int count) {
    stc_tdi_val_num = count;
    for (int i = 0; i < count; ++i) stc_tdi_vals[i].lstickX = i + .5f;
}
void TestLabHit(int player, int percent) { ft[player].dmg.percent = percent; }
void TestLabStageSwap(int row) { StageSwap_Options[row].OnSelect(&menu_gobj); }
/* Internal kinds pick recovery/RNG carry; external kinds and costumes are what
 * the selectors read. */
void TestLabKinds(int hmn, int cpu) { ft[0].kind = hmn; ft[1].kind = cpu; }
void TestLabLive(int slot, int kind, int costume) { live_kind[slot] = kind; live_costume[slot] = costume; }
/* As the menu: Left/Right sets the row's value; other buttons go to the row hook. */
void TestLabPickFighter(int slot, int index) {
    EventOption *option = &FighterSwap_Options[slot];
    option->val_prev = option->val; option->val = index;
    option->OnChange(&menu_gobj, index);
}
int TestLabFighterButton(int slot, int buttons) {
    return LabUI_Session.row_hooks->input(&menu_gobj, &FighterSwap_Options[slot], buttons);
}
static EventMenuRowStyle style;
int TestLabFighterStyle(int slot) { return LabUI_Session.row_hooks->style(&FighterSwap_Options[slot], &style); }
int TestLabFighterIndex(int kind) { return FighterSwap_Index(kind); }
int TestLabCostumeNum(int kind) { return FighterSwap_CostumeNum(kind); }
unsigned TestLabCostumeName(int kind, int costume) { return (unsigned)FighterSwap_CostumeName(kind, costume); }
unsigned TestLabFighterStatus(int slot, int line) { return (unsigned)FighterSwap_Options[slot].desc[line]; }
unsigned TestLabFighterOption(int slot) { return (unsigned)&FighterSwap_Options[slot]; }
unsigned TestLabSessionRow(int row) { return (unsigned)EventMenu_GetOption(&LabUI_Session, row); }
void TestLabFirstFrame(void) { Lab_ApplyPendingCarry(); }
void TestLabReset(void) { Lab_ResetSettings(&menu_gobj); }
void TestLabOnload(int file) { *onload_fileno = file; }
unsigned TestLabCustomOSDName(int i) { return (unsigned)LabOptions_CustomOSDs[OPTCUSTOMOSD_FIRST_CUSTOM + i].name; }
unsigned TestLabLogName(int page) { return (unsigned)LabOptions_ActionLog[page][OPTACTIONLOG_STATE].value_string; }
int TestLabInfo(int key) {
    switch (key) {
    case 0: return camera;
    case 1: return *(u8 *)(R13 + -0x4948);
    case 2: return watermark.hidden;
    case 3: return ft[0].dmg.percent;
    case 4: return ft[1].dmg.percent;
    case 5: return hud_damage[0];
    case 6: return hud_damage[1];
    case 7: return colanim;
    case 8: return rng.peach_item;
    case 9: return stadium;
    case 10: return trails_on;
    case 11: return trails_decay;
    case 12: return stc_custom_osd_state_num;
    case 13: return stc_custom_osd_states[0];
    case 14: return stc_custom_osd_states[1];
    case 15: return stc_tdi_val_num;
    case 16: return stc_tdi_vals[2].lstickX * 10;
    case 17: return LabOptions_ActionLog[2][OPTACTIONLOG_STATE].val;
    case 18: return (LabMenu_AdvCounter.options - LabOptions_AdvCounter[0]) / OPTCTR_COUNT;
    case 19: return (LabMenu_ActionLog.options - LabOptions_ActionLog[0]) / OPTACTIONLOG_COUNT;
    case 20: return Lab_SessionList(0);
    case 21: return Lab_SessionList(1);
    case 22: return restarted;
    case 23: return *onload_fileno;
    case 24: return frees;
    case 25: return ft[0].show_hit + ft[1].show_hit * 2;
    case 26: return ft[0].show_model + ft[1].show_model * 2;
    case 27: return stc_matchcam->hide_stage;
    case 28: return hmn_locked_percent;
    case 29: return cpu_locked_percent;
    case 30: return carry_tag == LAB_CARRY_TAG && carry_size == sizeof(LabCarry);
    case 31: return LabOptions_ActionLog[2][OPTACTIONLOG_STATE].OnSelect == Lab_RemoveActionLogState;
    case 32: return StageSwap_Options[5].disable;
    case 33: return FighterSwap_Pick[0].kind;
    case 34: return FighterSwap_Pick[0].costume;
    case 35: return FighterSwap_Pick[1].kind;
    case 36: return FighterSwap_Pick[1].costume;
    case 37: return FighterSwap_Options[0].val;
    case 38: return FighterSwap_Options[1].val;
    case 39: return fighter_restart[0];
    case 40: return fighter_restart[1];
    case 41: return fighter_restart[2];
    case 42: return fighter_restart[3];
    case 43: return sounds;
    case 44: return style.icon_kind;
    case 45: return style.icon_costume;
    case 46: return style.value_color.r << 16 | style.value_color.g << 8 | style.value_color.b;
    case 47: return (int)style.hint;
    case 48: return FighterSwap_Options[1].disable;
    }
    return -999;
}
