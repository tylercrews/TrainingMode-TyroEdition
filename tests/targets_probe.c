/* Production Target Practice event, with fixtures only at native engine boundaries. */
#include "../src/targets.c"
#include <stdarg.h>

static EventVars vars;
static GOBJ event, you, other, drawer, *lookup[64];
static FighterData ft;
static CmSubject subject;
static Text texts[2];
static int text_count, score_shown = -1;
static const char *label_format;
static GXColor *label_color;
static unsigned prefs[TM_TARGETS_PREF_COUNT], counter;
static int retries, ended, end_graphic, end_sfx, record_voice, sfx_count, gx_link, gx_priority, restarts;
static int restart_stage = -1, fighter_restarts, restart_fighters[4], menu_sfx;
static void *gx_callback;
static int stage_id = GRKINDEXT_BATTLE, paused;
static unsigned seed = 12345;

/* Live floor lines, as the stage's collision would hold them. */
static CollDataStage stage;
static CollLine lines[180];
static CollLineDesc descriptors[180];
static CollVert vertices[360];
static int enabled[180];

/* Lasers: items in the match's item list, each with one hitbox. */
#define LASERS 4
static GOBJ laser_gobjs[LASERS];
static ItemData laser_items[LASERS];
static int laser_cleared[LASERS], laser_life[LASERS];

/* Drawing: each GFX_Start's shape, count, width and depth test. The vertices
 * go to the GX FIFO, which the test records. */
#define DRAWS 32
static int draws, draw_info[DRAWS][4];
static float blast_left = -224, blast_right = 224;
static void GFXStart(u16 count, GFX_Params params) {
    if (draws < DRAWS) {
        draw_info[draws][0] = params.shape; draw_info[draws][1] = count;
        draw_info[draws][2] = params.size; draw_info[draws][3] = params.no_depth;
    }
    ++draws;
}
float Stage_GetBlastzoneLeft() { return blast_left; }
float Stage_GetBlastzoneRight() { return blast_right; }
// Only the lines' batch is checked; a target's discs just need numbers.
float sin(float x) { return x; }
float cos(float x) { return 1 - x; }

void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }
int sprintf(char *out, const char *format, ...) {
    va_list args;
    va_start(args, format);
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

void TestTargetsBlast(int left, int right) { blast_left = left / 1000.f; blast_right = right / 1000.f; }
// Run the draw callback; returns how many batches it started.
int TestTargetsDraw(int pass) {
    draws = 0;
    Targets_GX(&drawer, pass);
    return draws;
}
int TestTargetsDrawInfo(int draw, int key) { return draw < DRAWS ? draw_info[draw][key] : -1; }
int TestTargetsPeak(int hop) { return peak_lasers[hop] * 10000; }

/* The head: HeadN's bone index (-1 for none) and its world x, which only it
 * reports. */
static FighterBone bones[64];
static JOBJ head_joint;
static int head_bone = -1, head_lookups;
static float head_x;
s8 Fighter_BoneLookup(FighterData *fighter, int bone) {
    ++head_lookups;
    return fighter == &ft && bone == HeadN ? head_bone : -1;
}
int JOBJ_GetWorldPosition(JOBJ *source, Vec3 *add, Vec3 *dest) {
    *dest = (Vec3){source == &head_joint ? head_x : -999, 0, 0};
    return 0;
}
void TestTargetsHead(int bone, int x) {
    ft.bones = bones;
    for (int i = 0; i < 64; ++i) bones[i].joint = 0;
    head_bone = bone;
    if (bone >= 0) bones[bone].joint = &head_joint;
    head_x = x / 1000.f;
}
/* Airborne or not, the jumps used and allowed, the state and which way the
 * fighter faces (1 right, -1 left). */
void TestTargetsAir(int air, int used, int max, int state, int facing) {
    ft.phys.air_state = air;
    ft.jump.jumps_used = used;
    ft.attr.max_jumps = max;
    ft.state_id = state;
    ft.facing_direction = facing;
}
// Move a line's ends, as a moving platform does, in thousandths.
void TestTargetsMoveLine(int i, int ay, int by) {
    vertices[2 * i].pos_curr.Y = ay / 1000.f;
    vertices[2 * i + 1].pos_curr.Y = by / 1000.f;
}

static u32 Read(unsigned field, unsigned index) {
    if (field == TM_SETTING_TARGETS && index < TM_TARGETS_PREF_COUNT) return prefs[index];
    return field == TM_SETTING_EGGS_COUNTER ? counter : 0;
}
static void Write(unsigned field, unsigned index, u32 value) {
    if (field == TM_SETTING_TARGETS && index < TM_TARGETS_PREF_COUNT) prefs[index] = value;
    if (field == TM_SETTING_EGGS_COUNTER) counter = value;
}
static const TMSettingsAPI api = {Read, Write, 0};
static void RestartOnStage(int stage) { ++restarts; restart_stage = stage; }
static int RestartOnFighters(int hmn_kind, int hmn_costume, int cpu_kind, int cpu_costume) {
    ++fighter_restarts;
    restart_fighters[0] = hmn_kind; restart_fighters[1] = hmn_costume;
    restart_fighters[2] = cpu_kind; restart_fighters[3] = cpu_costume;
    return 1;
}

GOBJ *Fighter_GetGObj(int ply) { return ply ? 0 : &you; }
// The CSS slot: Fox or Falco in slot 0, and slot 1 empty (0x21), as CSS_InitPlayerData leaves it.
int Fighter_GetExternalID(int ply) { return ply ? 0x21 : ft.kind == FTKIND_FALCO ? CKIND_FALCO : CKIND_FOX; }
int Fighter_GetCostumeID(int ply) { return 0; }
int SFX_PlayCommon(int sfx) { menu_sfx = sfx; return 0; }
GOBJ *GObj_Create(int entity_class, int p_link, int p_priority) { return &drawer; }
void GObj_AddGXLink(GOBJ *gobj, void *cb, int link, int priority) { gx_callback = cb; gx_link = link; gx_priority = priority; }
CmSubject *CameraSubject_Alloc() { return &subject; }
int Stage_GetExternalID() { return stage_id; }
int GrColl_CheckIfLineEnabled(int line) { return enabled[line]; }
int GrColl_RaycastGround(Vec3 *pos, int *line, int *kind, Vec3 *unk1, int unk2, int unk3, int unk4, void *cb,
    float x, float y, float to_x, float to_y, float unk5) { return 0; }
int HSD_Randi(int max) {
    seed = seed * 1103515245 + 12345;
    return max > 0 ? (int)((seed >> 16 & 0x7FFF) % (unsigned)max) : 0;
}
float HSD_Randf() {
    seed = seed * 1103515245 + 12345;
    return (seed >> 16 & 0x7FFF) / 32768.f;
}
int Pause_CheckStatus(int type) { return paused ? 2 : 0; }
void Memcard_SaveIfChanged(void) { }
void Match_EndVS() { ++retries; }
void Match_SetEndGraphic(int graphic) { end_graphic = graphic; }
void MatchInfo_0x0010_store(int unk) { record_voice = unk; }
void Match_SetPostMatchSFX(int sfx) { end_sfx = sfx; }
void Match_EndImmediate() { ++ended; }
int SFX_PlayRaw(int sfx, int volume, int pan, int slot, int kind) { ++sfx_count; return 0; }
void Item_RemoveAllHitboxes(GOBJ *item) {
    ItemData *data = item->userdata;
    for (int i = 0; i < 4; ++i) data->hitbox[i].active = 0;
    laser_cleared[item - laser_gobjs] = 1;
}
void Item_SetLifeTimer(GOBJ *item, float lifetime) { laser_life[item - laser_gobjs] = lifetime; }
int Text_CreateCanvas(int sis, GOBJ *cam, int entity, int plink, int priority, int gxlink, int gxpri, int cobjpri) { return 1; }
Text *Text_CreateText(int sis, int canvas) { return &texts[text_count++ % 2]; }
int Text_AddSubtext(Text *text, float x, float y, char *fmt, ...) { return 0; }
void Text_SetColor(Text *text, int row, GXColor *color) { if (text == &texts[1]) label_color = color; }
void Text_SetText(Text *text, int row, const char *fmt, ...) {
    if (text == &texts[1]) { label_format = fmt; return; }
    va_list args;
    va_start(args, fmt);
    score_shown = fmt[0] == '%' ? va_arg(args, int) : -1;
    va_end(args);
}

void TestTargetsLoaded(void) { }
/* A fighter of `kind` (1 Fox, 22 Falco) with its ISO attributes, in ten-thousandths. */
void TestTargetsFighter(int kind, int jump, int hop, int mult, int gravity, int terminal) {
    ft.kind = kind;
    ft.attr.jump_v_initial_velocity = jump / 10000.f;
    ft.attr.hop_v_initial_velocity = hop / 10000.f;
    ft.attr.air_jump_v_multiplier = mult / 10000.f;
    ft.attr.gravity = gravity / 10000.f;
    ft.attr.terminal_velocity = terminal / 10000.f;
}
void TestTargetsInit(int free_practice, int shdl, int best_saved, int counter_saved, int peak_lines, int full_hop,
                     int double_jump_saved) {
    *event_vars_ptr_loc = &vars;
    vars.GFX_Start = GFXStart;
    prefs[TM_TARGETS_LINES] = peak_lines;
    prefs[TM_TARGETS_DOUBLE_JUMP] = double_jump_saved;
    *stc_gobj_lookup = lookup;
    *stc_colldata = &stage;
    *stc_collline = lines;
    *stc_collvert = vertices;
    vars.settings = &api;
    vars.restart_on_stage = RestartOnStage;
    vars.restart_on_fighters = RestartOnFighters;
    restarts = fighter_restarts = 0;
    restart_stage = -1;
    you.userdata = &ft;
    prefs[TM_TARGETS_FREE_PRACTICE] = free_practice;
    prefs[TM_TARGETS_SHDL] = shdl;
    prefs[TM_TARGETS_FULL_HOP] = full_hop;
    prefs[TM_TARGETS_BEST] = best_saved;
    counter = counter_saved;
    stc_match->match.timer = MATCH_TIMER_COUNTDOWN;
    stc_match->time_frames = 0;
    lookup[MATCHPLINK_ITEM] = 0;
    ft.flags.input_enable = 0;
    text_count = 0;
    Event_Init(&event);
}
void TestTargetsSeed(int value) { seed = value; }
void TestTargetsStage(int id) { stage_id = id; stage.line_num = stage.vert_num = 0; }
int TestTargetsAddLine(int ax, int ay, int bx, int by, int native_flags, int on) {
    int i = stage.line_num++;
    stage.vert_num += 2;
    lines[i].desc = &descriptors[i];
    memset(&descriptors[i], 0, sizeof(descriptors[i]));
    descriptors[i].vert_prev = 2 * i;
    descriptors[i].vert_next = 2 * i + 1;
    descriptors[i].is_floor = 1;
    ((u8 *)&descriptors[i])[0xE] = native_flags >> 8;
    ((u8 *)&descriptors[i])[0xF] = native_flags;
    enabled[i] = on;
    vertices[2 * i].pos_curr = (Vec2){ax / 1000.f, ay / 1000.f};
    vertices[2 * i + 1].pos_curr = (Vec2){bx / 1000.f, by / 1000.f};
    return i;
}
void TestTargetsYou(int x, int y, int control) {
    ft.phys.pos = (Vec3){x / 1000.f, y / 1000.f, 0};
    ft.flags.input_enable = control;
}
void TestTargetsPause(int value) { paused = value; }
void TestTargetsClock(int frames) { stc_match->time_frames = frames; }
void TestTargetsThink(void) { Event_Think(&event); }
/* Your laser (or another fighter's) in item slot `slot`, its hitbox sweeping
 * from a to b this frame, in thousandths. `what` packs the owner (bit 0, 1 is
 * you), the hitbox state (bits 1-3) and the item kind (bits 8 on), since only
 * eight arguments travel in registers. */
void TestTargetsLaser(int slot, int what, int ax, int ay, int bx, int by, int size) {
    int mine = what & 1, state = what >> 1 & 7, kind = what >> 8;
    GOBJ *gobj = &laser_gobjs[slot];
    ItemData *laser = &laser_items[slot];
    gobj->userdata = laser;
    laser->kind = kind;
    laser->fighter_gobj = mine ? &you : &other;
    for (int i = 0; i < 4; ++i) laser->hitbox[i].active = 0;
    laser->hitbox[0].active = state;
    laser->hitbox[0].size = size / 1000.f;
    laser->hitbox[0].pos_prev = (Vec3){ax / 1000.f, ay / 1000.f, 0};
    laser->hitbox[0].pos = (Vec3){bx / 1000.f, by / 1000.f, 0};
    laser_cleared[slot] = laser_life[slot] = 0;
    // The item list: every slot set up so far, in order.
    GOBJ **link = &lookup[MATCHPLINK_ITEM];
    while (*link && *link != gobj) link = &(*link)->next;
    if (!*link) { *link = gobj; gobj->next = 0; }
}
void TestTargetsClearLasers(void) { lookup[MATCHPLINK_ITEM] = 0; }
int TestTargetsLaserInfo(int slot, int key) { return key ? laser_life[slot] : laser_cleared[slot]; }
/* Break every live target with a laser swept straight through it. */
void TestTargetsShootAll(void) {
    TestTargetsClearLasers();
    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (!targets[i].live) continue;
        int x = targets[i].pos.X * 1000, y = targets[i].pos.Y * 1000;
        TestTargetsLaser(i, 1 | 3 << 1 | ITEM_FOXLASER << 8, x - 3000, y, x + 3000, y, 1172);
    }
    Event_Think(&event);
    TestTargetsClearLasers();
}
/* Put a single target at (x, y), in thousandths, as a spawn would. */
void TestTargetsPlace(int x, int y) {
    for (int i = 1; i < MAX_TARGETS; ++i) targets[i].live = false;
    targets[0] = (struct target){{x / 1000.f, y / 1000.f}, true};
    placed = true;
}
int TestTargetsTarget(int i, int key) {
    switch (key) {
    case 0: return targets[i].live;
    case 1: return targets[i].pos.X * 1000;
    case 2: return targets[i].pos.Y * 1000;
    }
    return -1;
}
/* ShortHopShots' heights for B on airborne frame `press`, in ten-thousandths. */
int TestTargetsShots(int press, int key) {
    float high = 0, low = 0;
    int fits = ShortHopShots(&ft, BlasterOf(&ft), press, &high, &low);
    return key == 0 ? fits : key == 1 ? high * 10000 : low * 10000;
}
/* FH Multi Lasers' combo `combo`: key 0 its shots (0 if it doesn't fit), 1
 * its press frame, 2 on its heights in ten-thousandths. */
int TestTargetsFullHop(int combo, int key) {
    struct full_hop *c = &full_hops[combo];
    return key == 0 ? c->shots : key == 1 ? c->press : c->heights[key - 2] * 10000;
}
int TestTargetsBlaster(int falco, int key) {
    const Blaster *b = &Blasters[falco];
    switch (key) {
    case 0: return b->air * 10000;
    case 1: return b->ground * 10000;
    case 2: return b->ecb_bottom * 10000;
    case 3: return b->delay;
    case 4: return b->loop;
    case 5: return LowestLaser(b) * 10000;
    case 6: return TARGET_RADIUS * 10000;
    }
    return -1;
}
int TestTargetsInfo(int key) {
    switch (key) {
    case 0: return score;
    case 1: return prefs[TM_TARGETS_BEST];
    case 2: return prefs[TM_TARGETS_FREE_PRACTICE];
    case 3: return prefs[TM_TARGETS_SHDL];
    case 4: return retries;
    case 5: return ended;
    case 6: return end_sfx;
    case 7: return record_voice;
    case 8: return sfx_count;
    case 9: return Options_Main[OPT_SHDL].disable;
    case 10: return Options_Main[OPT_SHDL].name == DoubleLasersUnavailable;
    case 11: return Options_Main[OPT_SHDL].val;
    case 12: return pair_presses;
    case 13: return stc_match->match.timer == MATCH_TIMER_COUNTUP;
    case 14: return subject.cam_pos.X * 1000;
    case 15: return subject.cam_pos.Y * 1000;
    case 16: return subject.boundtop_proj * 1000;
    case 17: return subject.boundbottom_proj * 1000;
    case 18: return score_shown;
    case 19: return label_color == &text_gold;
    case 20: return counter;
    case 21: return Options_Main[OPT_COUNTER].val;
    case 22: return texts[0].trans.X * 1000;
    case 23: return texts[0].trans.Y * 1000;
    case 24: return texts[1].trans.Y * 1000;
    case 25: return gx_callback == Targets_GX;
    case 26: return gx_link;
    case 27: return gx_priority;
    case 28: return Options_Main[OPT_FREEPRACTICE].val;
    case 29: return restarts;
    case 30: return end_graphic;
    case 31: return best;
    case 32: return FullHopDoubleJump(&ft) * 10000;
    case 33: return HighestFloor() * 1000;
    case 34: return pops[0].age;
    case 35: return pops[1].age;
    case 36: return Options_Main[OPT_LINES].val;
    case 37: return prefs[TM_TARGETS_LINES];
    case 38: return Options_Main[OPT_LINES].val_prev;
    case 39: return Options_Main[OPT_FULL_HOP].disable;
    case 40: return Options_Main[OPT_FULL_HOP].name == FullHopUnavailable;
    case 41: return Options_Main[OPT_FULL_HOP].val;
    case 42: return prefs[TM_TARGETS_FULL_HOP];
    case 43: return Options_Main[OPT_FULL_HOP].val_prev;
    case 44: return Options_Main[OPT_FULL_HOP].value_num;
    case 45: return Options_Main[OPT_DOUBLE_JUMP].val;
    case 46: return prefs[TM_TARGETS_DOUBLE_JUMP];
    case 47: return Options_Main[OPT_DOUBLE_JUMP].val_prev;
    case 48: return double_jump * 10000;
    case 49: return head_lookups;
    }
    return -1;
}
unsigned TestTargetsLabel(void) { return (unsigned)label_format; }
int TestTargetsRestart(int key) { return key ? restart_stage : restarts; }
/* Change Character: the submenu, its one row and what A asked for. */
int TestTargetsSwap(int key) {
    switch (key) {
    case 0: return Options_Main[OPT_CHARACTER].menu == &Menu_Character;
    case 1: return Menu_Character.option_num;
    case 2: return Menu_Character.options == FighterSwap_Options && Menu_Character.row_hooks == &FighterSwap_RowHooks;
    case 3: return FighterSwap_Options[0].value_num;
    case 4: return FighterSwap_SlotKind(0, 0);
    case 5: return FighterSwap_SlotKind(0, 1);
    case 6: return FighterSwap_Options[0].val;
    case 7: return FighterSwap_Options[0].disable;
    case 8: return FighterSwap_Options[1].disable;
    case 9: return fighter_restarts;
    case 10: return restart_fighters[0];
    case 11: return restart_fighters[1];
    case 12: return restart_fighters[2];
    case 13: return menu_sfx;
    case 14: return FighterSwap_Pick[0].costume;
    }
    return -1;
}
unsigned TestTargetsSwapText(int line) { return (unsigned)FighterSwap_Options[0].desc[line]; }
// Left/Right onto a character, as the menu does.
void TestTargetsSwapPick(int index) {
    FighterSwap_Options[0].val = index;
    FighterSwap_Options[0].OnChange(0, index);
}
// A (0), or X (1) for the next costume, on the row.
int TestTargetsSwapPress(int x) { return FighterSwap_Input(0, &FighterSwap_Options[0], x ? HSD_BUTTON_X : HSD_BUTTON_A); }
void TestTargetsChoose(int row, int value) {
    Options_Main[row].val = value;
    Options_Main[row].OnChange(0, value);
}
void TestTargetsSelect(int row) { Options_Main[row].OnSelect(0); }
unsigned TestTargetsFullHopText(int value) { return (unsigned)FullHopText[value]; }
