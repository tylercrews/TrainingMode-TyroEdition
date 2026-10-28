#include "events.h"
#include "osd_layout.h"

/* Labels by stable OSD/flag identity; TMSettings_EditorIDs places them on a page. */
static const char *row_names[35] = {
    [0] = "Wavedash Info", [1] = "L-Cancel", [3] = "Act OoS Frame", [5] = "Dashback",
    [8] = "Fighter-specific Tech", [9] = "Powershield Frame", [10] = "SDI Inputs",
    [12] = "Lockout Timers", [13] = "Item Throw Interrupts", [14] = "Boost Grab",
    [16] = "Act OoWait", [18] = "Act OoJump", [19] = "Jump Cancel Timing",
    [20] = "Fastfall Timing", [21] = "Frame Advantage", [22] = "Combo Counter",
    [24] = "Grab Breakout", [26] = "Ledgedash Info", [28] = "Act OoHitstun",
    [TM_GLOBAL_ASMASH_RELEASE_ID] = TM_GLOBAL_ASMASH_RELEASE_NAME,
    [TM_GLOBAL_ACT_OO_ATTACK_ID] = TM_GLOBAL_ACT_OO_ATTACK_NAME,
    [TM_GLOBAL_CPU_OSDS_OFF_ROW] = TM_GLOBAL_CPU_OSDS_OFF_NAME,
    [TM_GLOBAL_OSDS_OFF_ROW] = TM_GLOBAL_OSDS_OFF_NAME,
    [TM_GLOBAL_TRAIL_VERY_FAST_ROW] = TM_GLOBAL_TRAIL_VERY_FAST_NAME,
    [TM_GLOBAL_TRAIL_INSTANT_ROW] = TM_GLOBAL_TRAIL_INSTANT_NAME,
    [TM_GLOBAL_MISSED_LCANCEL_ROW] = TM_GLOBAL_MISSED_LCANCEL_NAME,
    [TM_GLOBAL_RUN_TURN_ROW] = TM_GLOBAL_RUN_TURN_NAME,
    [TM_GLOBAL_ACTION_CUES_ROW] = TM_GLOBAL_ACTION_CUES_NAME,
    [TM_GLOBAL_INFINITE_SHIELDS_ROW] = TM_GLOBAL_INFINITE_SHIELDS_NAME,
    [TM_GLOBAL_INVINCIBILITY_ROW] = TM_GLOBAL_INVINCIBILITY_NAME,
    [TM_GLOBAL_RANDALL_ROW] = TM_GLOBAL_RANDALL_NAME,
    [TM_GLOBAL_DONT_RESET_TIMERS_ROW] = TM_GLOBAL_DONT_RESET_TIMERS_NAME,
    [TM_GRID_JC_SHINE_ROW] = "JC Shine (Fox/Falco)",
    [TM_GRID_RESET_ROW] = "Reset All to Auto",
};
static const char *page_titles[TM_EDITOR_PAGES] = {
    "Global OSDs (1/3)", "Global Overlays & Gameplay (2/3)", "Fixed Grid OSD Positions (3/3)",
};
static const char *color_names[] = { TM_OSD_COLOR_NAMES };
static const char *display_names[] = { TM_OSD_DISPLAY_NAMES };
/* Native RSS state shared with the ASM hooks (see "Toggle UI"). */
#define RSS_CURSOR (*(volatile u16 *)0x804A04F2)
#define RSS_SELECTED (*(volatile u8 *)0x804A04F4) /* Cursor row's live Boolean. */
#define RSS_BUSY R13_U8(-0x4AAC)                  /* Think ignores input while set. */
#define RSS_ENTER_END (*(float *)0x803ED498)      /* Entry animation's final frame. */
#define RSS_CHECKBOX_FRAMES ((float *)R13_OFFSET(-0x6AE8)) /* Off/On frames, as in 0x802368DC. */
#define RSS_TITLE_SUBTEXT 16
/* RSS think receives remapped menu bits, not raw pad bits. Start also sets
 * 0x10 (confirm) but exits first via "Start Exits Custom RSS". */
#define MENU_A 0x200
#define MENU_B 0x20
#define MENU_L 0x40
#define MENU_R 0x80
static int editor_armed, editor_switching;
static int reset_armed; /* Reset All to Auto waits for a second A on its row. */
static char cell_names[TM_EDITOR_ROWS][6]; /* "R6 C5"; the row Text keeps the pointer. */

static unsigned editor_page(void) { return Settings_Get(TM_SETTING_EDITOR_PAGE, 0); }
static unsigned row_id(unsigned row) {
    return row < TM_EDITOR_ROWS ? TMSettings_EditorIDs[editor_page()][row] : 255;
}
static const char *row_name(unsigned row) {
    unsigned id = row_id(row);
    return id < countof(row_names) ? row_names[id] : 0;
}
static int grid_page(void) { return editor_page() == TM_EDITOR_GRID_PAGE; }
/* The OSD whose On/Off and color a positions-page row shows; JC Shine shares
 * Fighter-specific Tech's setting. */
static unsigned shown_id(unsigned id) { return id == TM_GRID_JC_SHINE_ROW ? 8 : id; }
static int grid_slot(unsigned id) {
    return id == TM_GRID_JC_SHINE_ROW ? TM_OSD_CATEGORIES : TMSettings_OSDCategorySlot(id);
}
/* Step to the next cell no other OSD holds, through Auto (0), wrapping both ways. */
static unsigned next_cell(unsigned slot, unsigned cell, int forward) {
    uint32_t taken = 0;
    for (unsigned other = 0; other < TM_OSD_GRID_SLOTS; ++other)
        if (other != slot) taken |= 1u << Settings_Get(TM_SETTING_OSD_GRID_CELL, other);
    for (unsigned i = 0; i <= TM_OSD_GRID_POSITIONS; ++i) {
        cell = forward ? (cell + 1) % (TM_OSD_GRID_POSITIONS + 1) : (cell + TM_OSD_GRID_POSITIONS) % (TM_OSD_GRID_POSITIONS + 1);
        if (!cell || !(taken & (1u << cell))) return cell;
    }
    return 0;
}
static const char *cell_name(unsigned row, unsigned cell) {
    if (!cell) return "Auto";
    char *name = cell_names[row];
    name[0] = 'R'; name[1] = '1' + (cell - 1) / TM_OSD_GRID_COLUMNS;
    name[2] = ' '; name[3] = 'C'; name[4] = '1' + (cell - 1) % TM_OSD_GRID_COLUMNS; name[5] = 0;
    return name;
}
static void refresh_display(void *data) {
    Text *text = *(Text **)((uint8_t *)data + 0x40);
    unsigned subtext = ((uint8_t *)data)[0x48];
    Text_SetText(text, subtext, "OSD Display: %s", display_names[Settings_Get(TM_SETTING_OSD_DISPLAY, 0)]);
}
static void refresh_title(void *data) {
    Text *text = *(Text **)((uint8_t *)data + 0x40);
    unsigned page = editor_page();
    Text_SetText(text, RSS_TITLE_SUBTEXT, "%s", page_titles[page]);
    /* The longer second title must not reach the left-aligned display line. */
    float scale = page ? 0.8f : 1.0f;
    Text_SetScale(text, RSS_TITLE_SUBTEXT, scale, scale);
}
static void hide_tree(JOBJ *joint) {
    joint->flags |= JOBJ_HIDDEN;
    for (JOBJ *child = joint->child; child; child = child->sibling) hide_tree(child);
}
/* Same column-root/child/sibling layout as native RSS at 0x802364A0. */
static JOBJ *row_joint(void *data, unsigned row) {
    JOBJ *root = *(JOBJ **)((uint8_t *)data + (row < 15 ? 0x2C : 0x34));
    JOBJ *joint = root ? root->child : 0;
    unsigned index = row < 15 ? row : row - 15;
    while (index-- && joint) joint = joint->sibling;
    return joint;
}
static void hide_gaps(void *data) {
    /* Do not follow a row's next sibling when hiding its subtree. */
    for (unsigned row = 0; row < TM_EDITOR_ROWS; ++row) {
        JOBJ *joint = row_joint(data, row), *box = 0;
        if (!row_name(row)) { if (joint) hide_tree(joint); continue; }
        /* Reset All is an action, not an On/Off row: hide only its checkbox. */
        if (joint && row_id(row) == TM_GRID_RESET_ROW) {
            JOBJ_GetChild(joint, &box, 2, -1);
            if (box) hide_tree(box);
        }
    }
}
/* Pose a row's checkbox (child 2) for its Boolean, exactly as the native toggle
 * path does for the cursor row. Native RSS never re-poses other rows, so a page
 * switch would otherwise keep the previous page's ON/OFF boxes. */
static void pose_checkbox(void *data, unsigned row, unsigned on) {
    JOBJ *joint = row_joint(data, row), *box = 0;
    if (!joint) return;
    JOBJ_GetChild(joint, &box, 2, -1);
    if (!box) return;
    JOBJ_ReqAnimAll(box, RSS_CHECKBOX_FRAMES[!!on]);
    JOBJ_AnimAll(box);
}

static int flag_for(unsigned id) {
    return TMSettings_NativeFlag(id);
}

static void refresh_row(void *data, unsigned row) {
    unsigned id = row_id(row);
    int flag = grid_page() ? -1 : flag_for(id);
    unsigned value = Settings_Get(flag < 0 ? TM_SETTING_OSD_COLOR : TM_SETTING_FLAG,
                                  flag < 0 ? shown_id(id) : (unsigned)flag);
    Text *text = *(Text **)((uint8_t *)data + (row < 15 ? 0x40 : 0x44));
    int subtext = (row < 15 ? row : row - 15) + 1;
    const char *choice = flag < 0 ? color_names[value < TM_COLOR_COUNT ? value : 0] : (value ? "On" : "Off");
    uint32_t rgba = value ? (flag < 0 ? OSD_PaletteColor(value) : 0x8DFF6EFF) : 0xB4B4B4FF;
    if (grid_page()) {
        /* Positions page: the row keeps its OSD's color (grey when Off) and shows its cell. */
        if (id == TM_GRID_RESET_ROW) { choice = reset_armed ? "Press A Again" : "Press A"; rgba = 0xFFFFFFFF; }
        else choice = cell_name(row, Settings_Get(TM_SETTING_OSD_GRID_CELL, grid_slot(id)));
    }
    Text_SetText(text, subtext, "%s: %s", row_name(row), choice);
    /* Fit long names plus the palette value into the existing two-column layout. */
    Text_SetScale(text, subtext, 0.78f, 1.0f);
    GXColor color = {rgba >> 24, rgba >> 16, rgba >> 8, rgba};
    Text_SetColor(text, subtext, &color);
}

void OSD_EditorInit(void *data) {
    /* Opening the menu always starts on Global OSDs; a page switch re-enters
     * here through the native end-of-entry path with its page already chosen. */
    if (!editor_switching) { Settings_Set(TM_SETTING_EDITOR_PAGE, 0, 0); editor_armed = 0; }
    editor_switching = 0; reset_armed = 0;
    for (unsigned row = 0; row < TM_EDITOR_ROWS; ++row)
        if (row_name(row)) {
            refresh_row(data, row);
            /* Positions rows mirror their OSD's On/Off, so the native exit save
             * writes back the same value (JC Shine and Reset All are no-ops). */
            ((uint8_t *)data)[row + 2] = !!Settings_Get(TM_SETTING_NATIVE_ROW, grid_page() ? shown_id(row_id(row)) : row_id(row));
        } else {
            Text *text = *(Text **)((uint8_t *)data + (row < 15 ? 0x40 : 0x44));
            Text_SetText(text, (row < 15 ? row : row - 15) + 1, "");
            ((uint8_t *)data)[row + 2] = 0;
        }
    /* Native think animates the cursor row whenever its snapshot differs from
     * the live Boolean; match them so a new page does not fake a toggle. */
    if (RSS_CURSOR < TM_EDITOR_ROWS) RSS_SELECTED = ((uint8_t *)data)[RSS_CURSOR + 2];
    for (unsigned row = 0; row < TM_EDITOR_ROWS; ++row) pose_checkbox(data, row, ((uint8_t *)data)[row + 2]);
    hide_gaps(data);
    refresh_display(data);
    refresh_title(data);
}

static void switch_page(void *data, int forward) {
    /* Save this page exactly as the native exit pass would, including the
     * cursor row's not-yet-animated Boolean, before its rows are rebound. */
    for (unsigned row = 0; row < TM_EDITOR_ROWS; ++row) {
        if (!row_name(row)) continue;
        unsigned value = row == RSS_CURSOR ? RSS_SELECTED : ((uint8_t *)data)[row + 2];
        Settings_Set(TM_SETTING_NATIVE_ROW, row_id(row), !!value);
    }
    Settings_Set(TM_SETTING_EDITOR_PAGE, 0, (editor_page() + (forward ? 1 : TM_EDITOR_PAGES - 1)) % TM_EDITOR_PAGES);
    /* Re-run the native "entry finished" step (state 1 at its last frame): it
     * recreates both Texts through the Load Alt Text hook, which calls
     * OSD_EditorInit, then redraws every checkbox from the new row snapshot. */
    for (unsigned column = 0; column < 2; ++column) {
        Text **text = (Text **)((uint8_t *)data + 0x40 + column * 4);
        if (*text) Text_Destroy(*text);
        *text = 0;
    }
    editor_switching = 1;
    ((uint8_t *)data)[0x1F] = 1;
    JOBJ *menu = *(JOBJ **)((uint8_t *)data + 0x24);
    if (menu) JOBJ_ReqAnimAll(menu, RSS_ENTER_END);
    RSS_BUSY = 1;
}

/* Positions page: A moves the row's OSD to the next free cell and B to the
 * previous one. Reset All needs a second A on its row; B or leaving it cancels. */
static int grid_input(void *data, unsigned buttons, unsigned row) {
    unsigned id = row_id(row);
    if (reset_armed && id != TM_GRID_RESET_ROW) {
        reset_armed = 0;
        for (unsigned r = 0; r < TM_EDITOR_ROWS; ++r) if (row_id(r) == TM_GRID_RESET_ROW) refresh_row(data, r);
    }
    if (!(buttons & (MENU_A | MENU_B))) return 0;
    if (row >= TM_EDITOR_ROWS || !row_name(row)) return 1;
    if (id == TM_GRID_RESET_ROW) {
        int reset = (buttons & MENU_A) && reset_armed;
        reset_armed = (buttons & MENU_A) && !reset;
        /* Every slot has a row on this page; only rows with a cell change. */
        for (unsigned r = 0; reset && r < TM_EDITOR_ROWS; ++r) {
            int slot = row_name(r) ? grid_slot(row_id(r)) : -1;
            if (slot < 0 || !Settings_Get(TM_SETTING_OSD_GRID_CELL, slot)) continue;
            Settings_Set(TM_SETTING_OSD_GRID_CELL, slot, 0);
            refresh_row(data, r);
        }
        refresh_row(data, row);
    } else {
        unsigned slot = grid_slot(id);
        unsigned cell = Settings_Get(TM_SETTING_OSD_GRID_CELL, slot);
        Settings_Set(TM_SETTING_OSD_GRID_CELL, slot, next_cell(slot, cell, buttons & MENU_A));
        refresh_row(data, row);
    }
    /* Positions never toggle the On/Off box: keep the native Boolean unchanged. */
    RSS_SELECTED = ((uint8_t *)data)[row + 2];
    return 1;
}

/* Return whether input was consumed, including on unused rows. A steps a row's
 * choice forward and B backward; R goes to the next page and L to the previous
 * one; Start exits and saves (native, before this hook). D-pad/stick still
 * navigate and X/Y choose the display. */
int OSD_EditorInput(void *data, unsigned buttons, unsigned row) {
    hide_gaps(data); /* Native initialization can clear the model's hidden flags. */
    int armed = editor_armed; editor_armed = 1;
    if (buttons & (MENU_L | MENU_R)) {
        if (!armed) return 1; /* Do not treat the L press that opened this menu as paging. */
        switch_page(data, !!(buttons & MENU_R));
        return 1;
    }
    if (grid_page()) return grid_input(data, buttons, row);
    if (!(buttons & (MENU_A | MENU_B))) return 0;
    if (row >= TM_EDITOR_ROWS || !row_name(row)) return 1;
    unsigned id = row_id(row);
    int flag = flag_for(id);
    unsigned field = flag < 0 ? TM_SETTING_OSD_COLOR : TM_SETTING_FLAG;
    unsigned index = flag < 0 ? id : (unsigned)flag;
    unsigned value = Settings_Get(field, index);
    value = flag < 0 ? (value + ((buttons & MENU_A) ? 1 : TM_COLOR_COUNT - 1)) % TM_COLOR_COUNT : !value;
    Settings_Set(field, index, value);
    refresh_row(data, row);
    /* Leave the row byte at its old value: native think detects this difference,
     * animates the checkbox, then copies the selected Boolean into that byte. */
    RSS_SELECTED = !!value;
    return 1;
}
