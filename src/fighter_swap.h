#ifndef TM_FIGHTER_SWAP_H
#define TM_FIGHTER_SWAP_H
/* "Player Character" and "CPU Character" selectors for events whose fighters
 * come from the CSS. Left/Right picks a character in its default costume, X/Y
 * cycles that character's costumes, and A restarts the match with both
 * choices, skipping the CSS/SSS (event_vars->restart_on_fighters). The value
 * is drawn in the costume's color, with its stock icon in the Lab's menu.
 *
 * Each including event lists FighterSwap_Options in a menu whose .row_hooks is
 * &FighterSwap_RowHooks, defines FighterSwap_BeforeRestart for its own cleanup
 * and calls FighterSwap_Init from Event_Init. A slot the event can't change
 * (fixed CPU) should be disabled after FighterSwap_Init. An event that only
 * supports some characters in a slot calls FighterSwap_Restrict first.
 * See notes/MID-EVENT-CHARACTER-STAGE-SWAP.md. */
#include "events.h"

#define FIGHTER_SWAP_COUNT 26
#define FIGHTER_SWAP_KINDS (CKIND_GANONDORF + 1)
/* Menu order is the Tyro tier list. */
static const u8 FighterSwap_Kinds[FIGHTER_SWAP_COUNT] = {
    CKIND_FOX, CKIND_MARTH, CKIND_JIGGLYPUFF, CKIND_FALCO, CKIND_PEACH, CKIND_SHEIK,
    CKIND_FALCON, CKIND_ICECLIMBERS, CKIND_LUIGI, CKIND_YOSHI, CKIND_PIKACHU, CKIND_SAMUS,
    CKIND_DK, CKIND_DRMARIO, CKIND_GANONDORF, CKIND_GAW, CKIND_LINK, CKIND_YOUNGLINK,
    CKIND_MARIO, CKIND_ROY, CKIND_NESS, CKIND_MEWTWO, CKIND_ZELDA, CKIND_PICHU,
    CKIND_KIRBY, CKIND_BOWSER,
};
static const char *FighterSwap_Names[FIGHTER_SWAP_COUNT] = {
    "Fox", "Marth", "Jigglypuff", "Falco", "Peach", "Sheik",
    "Captain Falcon", "Ice Climbers", "Luigi", "Yoshi", "Pikachu", "Samus",
    "Donkey Kong", "Dr. Mario", "Ganondorf", "Mr. Game & Watch", "Link", "Young Link",
    "Mario", "Roy", "Ness", "Mewtwo", "Zelda", "Pichu",
    "Kirby", "Bowser",
};

/* Each character's costumes in costume order, two letters per color code
 * below. Costume 0 is named Default; its code only picks its text color.
 * Counts match the game's table (CSS_GetCostumeNum). */
static const char *const FighterSwap_Costumes[FIGHTER_SWAP_KINDS] = {
    [CKIND_FALCON] = "NvBkReWhGrBu", [CKIND_DK] = "BrBkReBuGr", [CKIND_FOX] = "TnReBuGr",
    [CKIND_GAW] = "BkReBuGr", [CKIND_KIRBY] = "PiYeBuReGrWh", [CKIND_BOWSER] = "GrReBuBk",
    [CKIND_LINK] = "GrReBuBkWh", [CKIND_LUIGI] = "GrWhAqPi", [CKIND_MARIO] = "ReYeBkBuGr",
    [CKIND_MARTH] = "BuReGrBkWh", [CKIND_MEWTWO] = "PuReBuGr", [CKIND_NESS] = "ReYeBuGr",
    [CKIND_PEACH] = "PiYeWhBuGr", [CKIND_PIKACHU] = "YeReBuGr", [CKIND_ICECLIMBERS] = "BuGrOrRe",
    [CKIND_JIGGLYPUFF] = "PiReBuGrYe", [CKIND_SAMUS] = "OrPiBkGrPu", [CKIND_YOSHI] = "GrReBuYePiAq",
    [CKIND_ZELDA] = "PiReBuGrWh", [CKIND_SHEIK] = "NvReBuGrWh", [CKIND_FALCO] = "NvReBuGr",
    [CKIND_YOUNGLINK] = "GrReBuWhBk", [CKIND_DRMARIO] = "WhReBuGrBk", [CKIND_ROY] = "NvReBuGrYe",
    [CKIND_PICHU] = "YeReBuGr", [CKIND_GANONDORF] = "BrReBuGrPu",
};
typedef struct FighterSwapColor {
    char code[3];
    const char *name;
    GXColor color;
} FighterSwapColor;
static const FighterSwapColor FighterSwap_Colors[] = {
    {"Re", "Red", {255, 90, 90, 255}}, {"Bu", "Blue", {80, 170, 255, 255}},
    {"Gr", "Green", {90, 220, 90, 255}}, {"Ye", "Yellow", {255, 225, 60, 255}},
    {"Bk", "Black", {125, 125, 135, 255}}, {"Wh", "White", {255, 255, 255, 255}},
    {"Pi", "Pink", {255, 140, 205, 255}}, {"Or", "Orange", {255, 150, 50, 255}},
    {"Pu", "Purple", {185, 135, 255, 255}}, {"Aq", "Light Blue", {120, 225, 255, 255}},
    {"Nv", "Navy", {125, 115, 235, 255}}, {"Br", "Brown", {200, 140, 80, 255}},
    {"Tn", "Tan", {225, 185, 130, 255}},
};

static int FighterSwap_Index(int kind)
{
    for (int i = 0; i < FIGHTER_SWAP_COUNT; ++i)
        if (FighterSwap_Kinds[i] == kind) return i;
    return -1;
}
static int FighterSwap_CostumeNum(int kind)
{
    if (kind < 0 || kind >= FIGHTER_SWAP_KINDS || !FighterSwap_Costumes[kind]) return 1;
    int length = 0;
    while (FighterSwap_Costumes[kind][length]) ++length;
    return length / 2;
}
static const FighterSwapColor *FighterSwap_Color(int kind, int costume)
{
    if (costume < 0 || costume >= FighterSwap_CostumeNum(kind) || !FighterSwap_Costumes[kind]) return 0;
    const char *code = FighterSwap_Costumes[kind] + costume * 2;
    for (int i = 0; i < (int)countof(FighterSwap_Colors); ++i)
        if (FighterSwap_Colors[i].code[0] == code[0] && FighterSwap_Colors[i].code[1] == code[1])
            return &FighterSwap_Colors[i];
    return 0;
}
static const char *FighterSwap_CostumeName(int kind, int costume)
{
    const FighterSwapColor *color = FighterSwap_Color(kind, costume);
    return costume == 0 || !color ? "Default" : color->name;
}

/* Slot 0 is the player, 1 the CPU. live is what is playing now; pick is the
 * menu's choice, applied by A. */
typedef struct FighterSwapPick { s8 kind; u8 costume; } FighterSwapPick;
static FighterSwapPick FighterSwap_Live[2], FighterSwap_Pick[2];
static char FighterSwap_Status[2][2][48];

/* Each slot's choices in menu order: everyone, unless FighterSwap_Restrict
 * narrowed that slot. */
static u8 FighterSwap_SlotKinds[2][FIGHTER_SWAP_COUNT];
static const char *FighterSwap_SlotNames[2][FIGHTER_SWAP_COUNT];
static int FighterSwap_SlotCount[2];
static int FighterSwap_SlotSize(int slot)
{
    return FighterSwap_SlotCount[slot] ? FighterSwap_SlotCount[slot] : FIGHTER_SWAP_COUNT;
}
static int FighterSwap_SlotKind(int slot, int index)
{
    return FighterSwap_SlotCount[slot] ? FighterSwap_SlotKinds[slot][index] : FighterSwap_Kinds[index];
}
static int FighterSwap_SlotIndex(int slot, int kind)
{
    for (int i = 0; i < FighterSwap_SlotSize(slot); ++i)
        if (FighterSwap_SlotKind(slot, i) == kind) return i;
    return -1;
}

/* The next costume from start in direction step that doesn't match the other
 * slot's pick of the same character, so the two never look identical. */
static int FighterSwap_FreeCostume(int slot, int kind, int start, int step)
{
    int count = FighterSwap_CostumeNum(kind);
    FighterSwapPick *other = &FighterSwap_Pick[!slot];
    for (int tries = 0; tries < count; ++tries) {
        int costume = ((start + tries * step) % count + count) % count;
        if (other->kind != kind || other->costume != costume) return costume;
    }
    return start;
}
static void FighterSwap_Describe(int slot)
{
    FighterSwapPick *pick = &FighterSwap_Pick[slot], *live = &FighterSwap_Live[slot];
    int index = FighterSwap_Index(pick->kind), playing = FighterSwap_Index(live->kind);
    sprintf(FighterSwap_Status[slot][0], "Selected: %s, %s.", index < 0 ? "?" : FighterSwap_Names[index],
        FighterSwap_CostumeName(pick->kind, pick->costume));
    sprintf(FighterSwap_Status[slot][1], "Playing now: %s, %s.", playing < 0 ? "?" : FighterSwap_Names[playing],
        FighterSwap_CostumeName(live->kind, live->costume));
}
static void FighterSwap_Select(int slot, int index)
{
    if (index < 0 || index >= FighterSwap_SlotSize(slot)) return;
    int kind = FighterSwap_SlotKind(slot, index);
    FighterSwap_Pick[slot].kind = kind;
    FighterSwap_Pick[slot].costume = FighterSwap_FreeCostume(slot, kind, 0, 1);
    FighterSwap_Describe(slot);
}
static void FighterSwap_ChangeHMN(GOBJ *menu, int value) { FighterSwap_Select(0, value); }
static void FighterSwap_ChangeCPU(GOBJ *menu, int value) { FighterSwap_Select(1, value); }

#define FIGHTER_SWAP_ROW(slot, label, change) { .kind = OPTKIND_STRING, .name = label, \
    .value_num = FIGHTER_SWAP_COUNT, .values = FighterSwap_Names, .OnChange = change, \
    .desc = { FighterSwap_Status[slot][0], FighterSwap_Status[slot][1], \
        "Left/Right: character. X/Y: costume color. A: restart the match with both characters.", \
        "Skips the character and stage select." } }
static EventOption FighterSwap_Options[2] = {
    FIGHTER_SWAP_ROW(0, "Player Character", FighterSwap_ChangeHMN),
    FIGHTER_SWAP_ROW(1, "CPU Character", FighterSwap_ChangeCPU),
};
#undef FIGHTER_SWAP_ROW

/* Offer only these characters (CKIND_*, in menu order) in one slot. */
static void FighterSwap_Restrict(int slot, const u8 *kinds, int count)
{
    int n = 0;
    for (int i = 0; i < count && n < FIGHTER_SWAP_COUNT; ++i) {
        int index = FighterSwap_Index(kinds[i]);
        if (index < 0) continue;
        FighterSwap_SlotKinds[slot][n] = kinds[i];
        FighterSwap_SlotNames[slot][n++] = FighterSwap_Names[index];
    }
    if (!n) return;
    FighterSwap_SlotCount[slot] = n;
    FighterSwap_Options[slot].values = FighterSwap_SlotNames[slot];
    FighterSwap_Options[slot].value_num = n;
}

static void FighterSwap_BeforeRestart(void);
static int FighterSwap_Slot(EventOption *option)
{
    return option == &FighterSwap_Options[0] ? 0 : option == &FighterSwap_Options[1] ? 1 : -1;
}
static int FighterSwap_Changed(int slot)
{
    return !FighterSwap_Options[slot].disable && (FighterSwap_Pick[slot].kind != FighterSwap_Live[slot].kind ||
        FighterSwap_Pick[slot].costume != FighterSwap_Live[slot].costume);
}
/* Restart with both picks; a slot that isn't changing keeps its fighter. */
static int FighterSwap_Apply(void)
{
    int hmn = FighterSwap_Changed(0), cpu = FighterSwap_Changed(1);
    if (!hmn && !cpu) return 0;
    FighterSwap_BeforeRestart();
    return event_vars->restart_on_fighters(hmn ? FighterSwap_Pick[0].kind : -1, FighterSwap_Pick[0].costume,
        cpu ? FighterSwap_Pick[1].kind : -1, FighterSwap_Pick[1].costume);
}
static int FighterSwap_Input(GOBJ *menu, EventOption *option, int buttons)
{
    int slot = FighterSwap_Slot(option);
    if (slot < 0) return 0;
    if (buttons & HSD_BUTTON_A) {
        if (!FighterSwap_Apply()) SFX_PlayCommon(4);
        else SFX_PlayCommon(1);
        return 1;
    }
    int step = buttons & HSD_BUTTON_X ? 1 : buttons & HSD_BUTTON_Y ? -1 : 0;
    if (!step) return 0;
    FighterSwapPick *pick = &FighterSwap_Pick[slot];
    int costume = FighterSwap_FreeCostume(slot, pick->kind, pick->costume + step, step);
    if (costume == pick->costume) { SFX_PlayCommon(4); return 1; }
    pick->costume = costume;
    FighterSwap_Describe(slot);
    SFX_PlayCommon(2);
    return 1;
}
static int FighterSwap_Style(EventOption *option, EventMenuRowStyle *style)
{
    int slot = FighterSwap_Slot(option);
    if (slot < 0) return 0;
    FighterSwapPick *pick = &FighterSwap_Pick[slot];
    const FighterSwapColor *color = FighterSwap_Color(pick->kind, pick->costume);
    style->value_color = color ? color->color : (GXColor){255, 255, 255, 255};
    style->icon_kind = pick->kind;
    style->icon_costume = pick->costume;
    style->hint = FighterSwap_Changed(0) || FighterSwap_Changed(1) ?
        "Left/Right: Character   X/Y: Color   A: Restart" : "Left/Right: Character   X/Y: Color";
    return 1;
}
static const EventMenuRowHooks FighterSwap_RowHooks = { FighterSwap_Input, FighterSwap_Style };

/* Both selectors start on the fighters in play. A fighter outside the list
 * (e.g. a forced Popo) disables its row. */
static void FighterSwap_Init(void)
{
    for (int slot = 0; slot < 2; ++slot) {
        FighterSwapPick live = { Fighter_GetExternalID(slot), Fighter_GetCostumeID(slot) };
        int index = FighterSwap_SlotIndex(slot, live.kind);
        FighterSwap_Live[slot] = FighterSwap_Pick[slot] = live;
        FighterSwap_Options[slot].val = index < 0 ? 0 : index;
        FighterSwap_Options[slot].disable = index < 0;
    }
    FighterSwap_Describe(0);
    FighterSwap_Describe(1);
}

#endif
