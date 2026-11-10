#ifndef TM_SETTINGS_H
#define TM_SETTINGS_H

#include <stdint.h>

/* Serialized bytes, not a C struct/bitfield ABI. All offsets are within 0x1F24..0x1F4F. */
#define TM_SETTINGS_OFFSET 0x1F24
#define TM_SETTINGS_SIZE 44
#define TM_SETTINGS_VERSION 3
#define TM_SETTINGS_OVERLAYS 18
#define TM_SETTINGS_LEGACY_OVERLAYS 17
#define TM_SETTINGS_OVERLAY_CHOICES 11
#define TM_SETTINGS_OSDS 19
#define TM_OSD_CATEGORIES (TM_SETTINGS_OSDS + 2) /* Append-only runtime categories; core palette unchanged. */
#define TM_GLOBAL_ASMASH_RELEASE_ID 30
#define TM_GLOBAL_ASMASH_RELEASE_NAME "A-Smash Release"
#define TM_GLOBAL_ACT_OO_ATTACK_ID 27 /* TYEX byte 8 bits 4-6. */
#define TM_GLOBAL_ACT_OO_ATTACK_NAME "Act OoAttack"
/* OSD categories whose palette (and synthesized enable bit) live in the extension. */
#define TM_EXTENSION_OSD(id) ((id) == TM_GLOBAL_ASMASH_RELEASE_ID || (id) == TM_GLOBAL_ACT_OO_ATTACK_ID)
#define TM_SETTINGS_FLAGS_OFFSET 10
#define TM_SETTINGS_OVERLAYS_OFFSET 12
#define TM_SETTINGS_COLORS_OFFSET 30
/* Previously unused capacity: byte 6 bits 1-5 human ID+1, bits 6-7 marker 10;
 * byte 37 bits 1-5 CPU ID+1. Palette occupies byte 37 bit 0 only.
 * Byte 37 bit 6 extends Ledgedash's criterion; bit 7 remains unused.
 * Byte 43 bit 7 stores Side-B Sweetspot's zoomed-out camera. */
#define TM_SETTINGS_CRITERION_HIGH_MASK 0x40 /* Byte 37: criterion bit 2. */
#define TM_SETTINGS_SIGNATURE_OFFSET 38
#define TM_SETTINGS_RESERVED_OFFSET 40
#define TM_SETTINGS_EXTRA_FLAGS_OFFSET 40 /* Bits 0/1/2: TurnRun/protection/CPU OSD override. */
#define TM_SETTINGS_EVENT_INITIALIZED 0x08 /* Byte 40 bit 3; older format-3 saves leave it clear. */
#define TM_SETTINGS_EVENT_DELAY_MASK 0x30 /* Byte 40 bits 4-5: Ledgedash reset delay. */
#define TM_SETTINGS_EVENT_TIPS_MASK 0x40 /* Byte 40 bit 6: Ledgedash hints. */
#define TM_SETTINGS_SWEETSPOT_LINES_OFF_MASK 0x80 /* Byte 40 bit 7: inverted; unset defaults On. */
#define TM_SETTINGS_LAYOUT_MASK 0x60 /* Byte 43 bits 5-6: Recent / Fixed / Panel. */
#define TM_SETTINGS_SWEETSPOT_CAMERA_MASK 0x80 /* Byte 43 bit 7: Normal / Zoomed Out. */
#define TM_OSD_LAYOUT_NAMES "Recent", "Fixed Grid", "Practice Panel"
#define TM_OSD_DISPLAY_NAMES "HUD", "Sides", "Top Left", "Top Right", "Fixed Grid", "Practice Panel"
#define TM_SETTINGS_LEDGE_OFFSET 41
#define TM_SETTINGS_EGGS_OFFSET 42

/* Shared labels and native L-menu row bindings. Build-generated ASM uses these names too. */
#define TM_GLOBAL_TRAIL_VERY_FAST_ROW 2
#define TM_GLOBAL_TRAIL_INSTANT_ROW 4
#define TM_GLOBAL_OSDS_OFF_ROW 6
#define TM_GLOBAL_OSDS_OFF_NAME "OVERRIDE ALL OSDS OFF"
#define TM_GLOBAL_CPU_OSDS_OFF_ROW 29
#define TM_GLOBAL_CPU_OSDS_OFF_NAME "OVERRIDE CPU OSDS OFF"
#define TM_GLOBAL_MISSED_LCANCEL_ROW 7
#define TM_GLOBAL_ACTION_CUES_ROW 11
#define TM_GLOBAL_RUN_TURN_ROW 17
#define TM_GLOBAL_INFINITE_SHIELDS_ROW 23
#define TM_GLOBAL_INVINCIBILITY_ROW 25
#define TM_GLOBAL_INVINCIBILITY_NAME "Invincibility Overlay"
#define TM_GLOBAL_RANDALL_ROW 31 /* Editor-only logical row; never an OSD enable bit. */
#define TM_GLOBAL_RANDALL_NAME "Randall Overlay"
#define TM_GLOBAL_DONT_RESET_TIMERS_ROW 34 /* Editor-only logical row, after the positions page's 32/33. */
#define TM_GLOBAL_DONT_RESET_TIMERS_NAME "Don't Reset Timers"
#define TM_GLOBAL_BLAST_ZONES_ROW 35 /* Editor-only logical row. */
#define TM_GLOBAL_BLAST_ZONES_NAME "Blast Zone Overlay"
#define TM_GLOBAL_MISSED_LCANCEL_NAME "Missed L Cancel"
#define TM_GLOBAL_ACTION_CUES_NAME "Actionable Yellow>Green"
#define TM_GLOBAL_RUN_TURN_NAME "Run Turnaround"
#define TM_GLOBAL_INFINITE_SHIELDS_NAME "Infinite Shields"
#define TM_GLOBAL_TRAIL_VERY_FAST_NAME "Hitbox Trails Very Fast"
#define TM_GLOBAL_TRAIL_INSTANT_NAME "Hitbox Trails Instant"
#define TM_GLOBAL_TRAIL_STATE_NAMES "Global: Off", "Global: Very Fast", "Global: Instant", "Global: Both On"

enum TMSettingsStatus {
    TM_SETTINGS_READY,
    TM_SETTINGS_MIGRATED,
    TM_SETTINGS_REPAIRED,
    TM_SETTINGS_UNSUPPORTED,
    TM_SETTINGS_FOREIGN,
};

enum TMSettingsColor {
    TM_COLOR_OFF, TM_COLOR_WHITE, TM_COLOR_RED, TM_COLOR_GREEN,
    TM_COLOR_BLUE, TM_COLOR_YELLOW, TM_COLOR_CYAN, TM_COLOR_MAGENTA,
    TM_COLOR_LEGACY_COUNT, /* The saved 3-bit palettes hold Off..Magenta. */
    TM_COLOR_ORANGE = TM_COLOR_LEGACY_COUNT, TM_COLOR_PINK, TM_COLOR_PURPLE, TM_COLOR_TEAL,
    TM_COLOR_LIME, TM_COLOR_GOLD, TM_COLOR_SKY_BLUE, TM_COLOR_LAVENDER,
    TM_COLOR_COUNT, /* Orange..Lavender also use a TYEX nibble per category. */
};

enum TMSettingsFlag {
    TM_FLAG_OSDS_OFF, TM_FLAG_TRAILS_VERY_FAST, TM_FLAG_TRAILS_INSTANT,
    TM_FLAG_MISSED_LCANCEL, TM_FLAG_LAST_BLOCKED_FRAME, TM_FLAG_INFINITE_SHIELDS,
    TM_FLAG_RUN_TURNAROUND,
    TM_FLAG_INVINCIBILITY,
    TM_FLAG_CPU_OSDS_OFF,
    TM_FLAG_COUNT, /* Core-record flags end here. */
    TM_FLAG_RANDALL = TM_FLAG_COUNT, /* TYEX byte 8 bit 3; routed by Settings_Get/Set. */
    TM_FLAG_DONT_RESET_TIMERS, /* TYEX byte 8 bit 7: savestate loads keep the match clock. */
    TM_FLAG_BLAST_ZONES, /* TYEX word 20 bit 23: red lines on every stage's blast zones. */
};
/* Flags whose bit lives in the extension, not the full core record. */
#define TM_EXTENSION_FLAG(index) ((index) == TM_FLAG_RANDALL || (index) == TM_FLAG_DONT_RESET_TIMERS ||     (index) == TM_FLAG_BLAST_ZONES)

/* These field numbers are mirrored in ASM/Globals.s. Append; never reorder. */
enum TMSettingsField {
    TM_SETTING_OSD_MASK, TM_SETTING_OSD_POSITION, TM_SETTING_EVENT_PAGE,
    TM_SETTING_RECOMMENDED, TM_SETTING_ADVANCE, TM_SETTING_DECREMENT,
    TM_SETTING_DPAD_UP, TM_SETTING_DPAD_DOWN, TM_SETTING_DPAD_LEFT,
    TM_SETTING_DPAD_RIGHT, TM_SETTING_INPUT_DISPLAY, TM_SETTING_FLAG,
    TM_SETTING_OVERLAY_HMN, TM_SETTING_OVERLAY_CPU, TM_SETTING_OSD_COLOR,
    TM_SETTING_OSD_ENABLED,
    TM_SETTING_NATIVE_ROW,
    TM_SETTING_EDITOR_ROW, /* Native physical row ID -> grouped editor binding. */
    TM_SETTING_LEDGEDASH,
    TM_SETTING_EGGS,
    TM_SETTING_EVENT_RESET, /* Write 1, index TM_EVENT_*: reset only this event's saved fields. */
    TM_SETTING_OSD_LAYOUT,
    TM_SETTING_OSD_DISPLAY, /* Composite editor choice: four Recent anchors, Fixed, Panel. */
    TM_SETTING_OSD_PAGE, /* Runtime-only manual page; no memory-card allocation. */
    TM_SETTING_CHARACTER, /* Index 0 human / 1 CPU; external ID, UINT32_MAX = unset. */
    TM_SETTING_SWEETSPOT_LINES, /* Index 0 only: Enable Line Guides; default On. */
    TM_SETTING_EVENT_SELECTION, /* Byte 5 high six bits; page remains in low two. */
    TM_SETTING_TECHCHASE, /* Separate extension; manual chances are session-only. */
    TM_SETTING_LEDGECHASE, /* Separate extension; manual ledge chances are session-only. */
    TM_SETTING_UNDERFIRE, /* Event-local saved preferences in the extension. */
    TM_SETTING_LOW_LASER, /* Event OSDs, suppression, Puff position/height in the extension. */
    TM_SETTING_POWERSHIELD, /* Powershield event menu, append-only identity. */
    TM_SETTING_SWEETSPOT_CAMERA, /* Index 0 only: Normal (0) / Zoomed Out (1). */
    TM_SETTING_REVERSAL,
    TM_SETTING_ATTACK_SHIELD,
    TM_SETTING_EDITOR_PAGE, /* Runtime-only Global Settings page; never saved. */
    TM_SETTING_OSD_COLOR_EXTRA, /* Internal: TYEX nibble by category slot. Use OSD_COLOR. */
    TM_SETTING_OSD_GRID_CELL, /* Index: OSD slot, or TM_OSD_CATEGORIES for JC Shine. 0 Auto, 1..30 a Fixed Grid cell. */
    TM_SETTING_COSTUME, /* Index 0 human / 1 CPU: costume of the saved CHARACTER, in the extension. */
    TM_SETTING_EDGEGUARD, /* Edgeguard event menu and each CPU's recovery toggles, in the extension. */
    TM_SETTING_SWEETSPOT_DJ, /* Index 0 only: Double-Jump Height Indicator, TYEX word 20 bit 22; default Off. */
    TM_SETTING_SWEETSPOT, /* Side-B Sweetspot's Success Criteria and Knockback, TYEX word 20 bits 26-29. */
    TM_SETTING_GRAB_PUNISH, /* 2v1 Grab Punish's menu and team colors; see TMGrabPunishPreference. */
    TM_SETTING_UPB_SWEETSPOT, /* Up-B Sweetspot's menu, TYEX word 68 bits 16-18. */
    TM_SETTING_EGGS_COUNTER, /* Index 0 only: Egg Counter Position, TYEX byte 51 bits 5-7. */
    TM_SETTING_EGGS_SPAWN, /* Free Practice's egg spots, TYEX byte 49 bits 6-7. */
    TM_SETTING_LEDGEGRAB_RESET, /* Index 0 only: Reset Speed, TYEX byte 34 bits 4-5. */
    TM_SETTING_DOOR_ZELDA, /* Index: CSS door. 1 if its Zelda icon means Zelda, not Sheik. Runtime-only. */
    TM_SETTING_PASSING, /* Passing Practice's menu, TYEX word 72 bits 14-27; see TMPassingPreference. */
};
enum TMOSDLayout { TM_OSD_RECENT, TM_OSD_FIXED, TM_OSD_PANEL, TM_OSD_LAYOUT_COUNT };
/* Event Select pages: Minigames, General Tech, Character-specific Tech and
 * Doubles (T10). The page is byte 5's low two bits, so four is the most. */
#define TM_EVENT_PAGES 4

/* Stable preference IDs, independent of either event's menu row order. */
enum TMLedgedashPreference {
    TM_LEDGE_START, TM_LEDGE_RESET, TM_LEDGE_CRITERION, TM_LEDGE_RESET_DELAY, TM_LEDGE_TIPS,
    TM_LEDGE_PREF_COUNT,
};
enum TMEggsPreference {
    TM_EGGS_DAMAGE, TM_EGGS_SCALE, TM_EGGS_VELOCITY, TM_EGGS_COLLISION,
    TM_EGGS_FREE_PRACTICE, TM_EGGS_PREF_COUNT,
};
/* Where Eggs-ercise draws its egg count and the label under it. */
enum TMEggsCounterPosition {
    TM_EGGS_COUNTER_TOP_LEFT, TM_EGGS_COUNTER_TOP_RIGHT, TM_EGGS_COUNTER_BOTTOM_LEFT,
    TM_EGGS_COUNTER_BOTTOM_RIGHT, TM_EGGS_COUNTER_UNDER_TIMER, TM_EGGS_COUNTER_COUNT,
};
/* Eggs Under Platforms and Eggs In The Air, both off by default. */
enum TMEggsSpawnPreference { TM_EGGS_UNDER_PLATFORMS, TM_EGGS_IN_AIR, TM_EGGS_SPAWN_PREF_COUNT };
/* How long Ledgegrab Speedrun shows a result before the restart. */
enum TMLedgegrabResetSpeed {
    TM_LEDGEGRAB_RESET_INSTANT, TM_LEDGEGRAB_RESET_SHORT, TM_LEDGEGRAB_RESET_LINGER, TM_LEDGEGRAB_RESET_COUNT,
};
enum TMEventPreferenceID { TM_EVENT_LEDGEDASH, TM_EVENT_EGGS, TM_EVENT_PREF_COUNT };
enum TMTechchasePreference {
    TM_TECHCHASE_PRESET, TM_TECHCHASE_REACTION_OSD, TM_TECHCHASE_TIMING_OSD,
    TM_TECHCHASE_RESET, TM_TECHCHASE_INVISIBILITY, TM_TECHCHASE_CAMERA,
    TM_TECHCHASE_LOCATION, TM_TECHCHASE_SPOTDODGE, TM_TECHCHASE_CPU_COLOR,
    TM_TECHCHASE_HEIGHT, TM_TECHCHASE_DISTANCE,
    TM_TECHCHASE_PREF_COUNT,
};
enum TMLedgechasePreference {
    TM_LEDGECHASE_SIDE, TM_LEDGECHASE_CPU_100, TM_LEDGECHASE_PLAYER_PERCENT,
    TM_LEDGECHASE_PRESET, TM_LEDGECHASE_DISTANCE, TM_LEDGECHASE_CPU_COLOR,
    TM_LEDGECHASE_CPU_ACTION, TM_LEDGECHASE_AIR_ATTACK,
    TM_LEDGECHASE_ENTRY, TM_LEDGECHASE_HOP_HEIGHT, TM_LEDGECHASE_PREF_COUNT,
};
enum TMUnderFirePreference {
    TM_UNDERFIRE_ATTACK_PARTICLES,
    TM_UNDERFIRE_COUNTER_SCORE, /* Record-format marker; old survival frames are not counts. */
    TM_UNDERFIRE_LAVA_OVERLAY, /* TYEX word 20 bit 21, still version 19. */
    TM_UNDERFIRE_REMOVE_PILLARS, /* TYEX word 9 bit 23, no version bump. */
    TM_UNDERFIRE_PREF_COUNT
};
enum TMLowLaserPreference {
    TM_LOW_LASER_OSDS, TM_LOW_LASER_SUPPRESS_GLOBAL,
    TM_LOW_LASER_POSITION, TM_LOW_LASER_HEIGHT, /* TYEX byte 35, still version 15. */
    TM_LOW_LASER_PREF_COUNT
};
enum TMPowershieldPreference {
    TM_PS_FIRE_SPEED, TM_PS_DELAY_MIN, TM_PS_DELAY_MAX, TM_PS_HEIGHT,
    TM_PS_DIRECTION, TM_PS_MOVEMENT, TM_PS_NANA, TM_PS_OSDS,
    TM_PS_SUPPRESS_GLOBAL, TM_PS_OVERLAY, TM_PS_PREF_COUNT
};
enum TMReversalPreference {
    TM_REV_ATTACK, TM_REV_DISTANCE, TM_REV_PLAYER_FACING, TM_REV_CPU_FACING,
    TM_REV_POSITION, TM_REV_PREF_COUNT
};
enum TMAttackShieldPreference {
    TM_AOS_ACTION, TM_AOS_HITS, TM_AOS_INFINITE, TM_AOS_DISTANCE, TM_AOS_SHIELD_DI, TM_AOS_PREF_COUNT
};
/* Custom Knockback ranges, then the percents, then Knockback Height and one
 * recovery-toggle mask per CPU (bit N = row N of that CPU's recovery menu).
 * Append; the CPUs from Yoshi on live in a later TYEX word. Player Position
 * uses word 60's spare top bits, Camera (Normal / Zoomed Out) word 56's bit 30,
 * Side word 20's bits 24-25. Fox's and Falco's side-B rows past their first
 * six are a second 5-bit mask each, in word 68's bits 19-23 and 24-28.
 * Knockback Height took over Hit Strength's field; Knockback Distance, word
 * 68's bits 29-31, is stored plus 1, so 0 means that field is still Hit
 * Strength (LoadKnockback in edgeguard.c). */
enum TMEdgeguardPreference {
    TM_EG_KB_MIN, TM_EG_KB_MAX, TM_EG_ANGLE_MIN, TM_EG_ANGLE_MAX,
    TM_EG_PLAYER_PERCENT, TM_EG_CPU_PERCENT_MIN, TM_EG_CPU_PERCENT_MAX,
    TM_EG_KB_HEIGHT, TM_EG_RECOVERY_FOX, TM_EG_RECOVERY_FALCO, TM_EG_RECOVERY_SHEIK,
    TM_EG_RECOVERY_FALCON, TM_EG_RECOVERY_MARTH,
    TM_EG_RECOVERY_YOSHI, TM_EG_RECOVERY_PEACH, TM_EG_RECOVERY_LUIGI, TM_EG_RECOVERY_DRMARIO,
    TM_EG_RECOVERY_ICECLIMBERS, TM_EG_RECOVERY_PUFF, TM_EG_RECOVERY_SAMUS,
    TM_EG_PLAYER_POSITION, TM_EG_CAMERA, TM_EG_SIDE,
    TM_EG_RECOVERY_FOX_SIDEB, TM_EG_RECOVERY_FALCO_SIDEB, TM_EG_KB_DISTANCE,
    TM_EG_RECOVERY_FOX_UPB, TM_EG_RECOVERY_FALCO_UPB, TM_EG_RECOVERY_LUIGI2, TM_EG_PREF_COUNT
};
/* Side-B Sweetspot: Success Criteria in word 20 bit 26, Knockback in bits
 * 27-29. Zero, which every record that owns word 20 has there, is the default. */
enum TMSweetspotPreference { TM_SWEETSPOT_SUCCESS, TM_SWEETSPOT_KNOCKBACK, TM_SWEETSPOT_PREF_COUNT };
#define TM_SWEETSPOT_SUCCESS_COUNT 2 /* Sweetspot / Sweetspot + Ledgedash. */
#define TM_SWEETSPOT_KNOCKBACK_COUNT 8 /* Random, six presets, Random Preset. */
/* 2v1 Grab Punish: its menu rows in row order (Grab Location, Your Location,
 * Enemy Percent, Teammate Percent, CPU Mashing, Teammate Facing and Release
 * Hit = Success), then the teams its character select last started a match with
 * and P3's and P4's last characters there. Those are this event's own, so the
 * normal saved costume stays out of it. The rows are in word 68 (version 20) but
 * for Teammate Percent, in byte 50's spare top bits; the teams take word 68's
 * bits 14-15 and word 20's bits 30-31. Both percents and CPU Mashing keep their
 * top bit apart from the rest, in word 32. The characters are in word 72 (version
 * 21): 0 means none yet, otherwise the external ID + 1. */
enum TMGrabPunishPreference {
    TM_GP_GRAB_SPOT, TM_GP_YOUR_SPOT, TM_GP_PERCENT, TM_GP_MATE_PERCENT, TM_GP_MASH, TM_GP_FACING,
    TM_GP_RELEASE_HITS, TM_GP_ROW_COUNT,
    TM_GP_YOUR_TEAM = TM_GP_ROW_COUNT, TM_GP_ENEMY_TEAM, TM_GP_P3_CHARACTER, TM_GP_P4_CHARACTER, TM_GP_PREF_COUNT
};
#define TM_GP_LOCATION_COUNT 8 /* Random, then seven spots. */
#define TM_GP_PERCENT_COUNT 11 /* 0% to 200% in steps of 20. */
#define TM_GP_MASH_COUNT 6 /* No Mash, Light, Medium, Heavy, Very Heavy, Inhuman. */
#define TM_GP_FACING_COUNT 3
#define TM_GP_TEAM_COUNT 3 /* Red, Blue, Green. */
#define TM_GP_CHARACTER_COUNT 27 /* None yet, then the 26 external IDs. */
#define TM_GP_DEFAULT_PERCENT 3 /* 60% */
#define TM_GP_DEFAULT_MASH 1 /* Light Mash */
/* Passing Practice: its menu rows in row order (Teammate Aerial, Enemy Percent,
 * Percent Mode, Distance, Your Side), in word 72's spare bits 14-27 (version 21,
 * no bump). Each is stored as its distance from its default, so a record that
 * never wrote them, with those bits zero, reads the defaults. */
enum TMPassingPreference {
    TM_PASS_AERIAL, TM_PASS_PERCENT, TM_PASS_MODE, TM_PASS_DISTANCE, TM_PASS_SIDE, TM_PASS_PREF_COUNT
};
#define TM_PASS_AERIAL_COUNT 5 /* Neutral, Forward, Back, Up and Down Air. */
#define TM_PASS_PERCENT_COUNT 21 /* 0% to 200% in steps of 10. */
#define TM_PASS_MODE_COUNT 2 /* Frozen %, Rising %. */
#define TM_PASS_DISTANCE_COUNT 5 /* Very Close to Very Far. */
#define TM_PASS_SIDE_COUNT 3 /* Left, Right, Random. */
#define TM_PASS_DEFAULT_AERIAL 2 /* Back Air */
#define TM_PASS_DEFAULT_PERCENT 4 /* 40% */
#define TM_PASS_DEFAULT_DISTANCE 2 /* Medium */
/* Up-B Sweetspot: Enable Line Guides (default On), Marth (Forward Smash / Down
 * Tilt / Off, with its top bit in word 56) and Camera (Normal / Zoomed Out), in
 * word 68 (version 20). */
enum TMUpBSweetspotPreference { TM_UPB_GUIDES, TM_UPB_MARTH, TM_UPB_CAMERA, TM_UPB_PREF_COUNT };
enum TMUpBMarth { TM_UPB_MARTH_FSMASH, TM_UPB_MARTH_DTILT, TM_UPB_MARTH_OFF, TM_UPB_MARTH_COUNT };
/* Edgeguard Side choices: which ledge each attempt uses. */
enum { TM_EG_SIDE_RANDOM, TM_EG_SIDE_LEFT, TM_EG_SIDE_RIGHT, TM_EG_SIDE_COUNT };
#define TM_EG_KB_HEIGHT_COUNT 7 /* Random, five heights, Custom. */
#define TM_EG_KB_DISTANCE_COUNT 5 /* Random, four distances. */
/* Edgeguard Player Position choices, ledge to center. Close is the original spot. */
enum { TM_EG_POSITION_AT_LEDGE, TM_EG_POSITION_CLOSE, TM_EG_POSITION_HALFWAY,
    TM_EG_POSITION_MID_STAGE, TM_EG_POSITION_COUNT };
#define TM_LEDGE_DEFAULT_RESET 1
#define TM_LEDGE_DEFAULT_DELAY 1
#define TM_LEDGE_DEFAULT_TIPS 1
#define TM_EGGS_DEFAULT_DAMAGE 12
#define TM_EGGS_DEFAULT_VELOCITY 1

extern const uint8_t TMSettings_OSDIDs[TM_SETTINGS_OSDS];
static inline unsigned TMSettings_OSDCategoryID(unsigned slot) {
    return slot < TM_SETTINGS_OSDS ? TMSettings_OSDIDs[slot] :
        slot == TM_SETTINGS_OSDS ? TM_GLOBAL_ASMASH_RELEASE_ID : TM_GLOBAL_ACT_OO_ATTACK_ID;
}
static inline int TMSettings_OSDCategorySlot(unsigned id) {
    for (unsigned slot = 0; slot < TM_OSD_CATEGORIES; ++slot)
        if (TMSettings_OSDCategoryID(slot) == id) return slot;
    return -1;
}
/* Fixed Grid positions: every category plus the Fox/Falco JC Shine cell. The
 * stored index is the runtime slot, so JC Shine (TM_OSD_CATEGORIES) must move
 * to a fixed index before another category is appended. */
#define TM_OSD_GRID_SLOTS (TM_OSD_CATEGORIES + 1)
#define TM_OSD_GRID_POSITIONS 30 /* Six rows of five on one page. */
#define TM_EDITOR_PAGES 3
#define TM_EDITOR_GRID_PAGE 2 /* Fixed Grid OSD Positions: page 0's OSD rows, editing positions. */
#define TM_EDITOR_ROWS 29
#define TM_GRID_JC_SHINE_ROW 32 /* Editor-only logical rows on the positions page. */
#define TM_GRID_RESET_ROW 33
extern const uint8_t TMSettings_EditorIDs[TM_EDITOR_PAGES][TM_EDITOR_ROWS];
/* Physical RSS row -> setting binding on one Global Settings page. */
int TMSettings_EditorIDFor(unsigned page, unsigned native_row);
int TMSettings_EditorID(unsigned native_row); /* Page 0. */
int TMSettings_NativeFlag(unsigned row);
void TMSettings_Init(uint8_t record[TM_SETTINGS_SIZE]);
int TMSettings_Prepare(uint8_t record[TM_SETTINGS_SIZE], int owns_save);
uint32_t TMSettings_Read(const uint8_t record[TM_SETTINGS_SIZE], unsigned field, unsigned index);
int TMSettings_Write(uint8_t record[TM_SETTINGS_SIZE], unsigned field, unsigned index, uint32_t value);

/* One runtime service in eventMenu.dat; event modules call it through EventVars. */
typedef struct TMSettingsAPI {
    uint32_t (*get)(unsigned field, unsigned index);
    void (*set)(unsigned field, unsigned index, uint32_t value);
    int (*status)(void);
} TMSettingsAPI;

uint32_t Settings_Get(unsigned field, unsigned index);
void Settings_Set(unsigned field, unsigned index, uint32_t value);
int Settings_Status(void);
void Settings_CommitPending(void);
void Settings_Characters(unsigned save, int hmn_mask, int cpu_mask, int cpu_selectable);
void Settings_RestoreCharacters(void);
void Settings_OpenTeamSelect(void);
void Settings_SaveTeamSelect(void);
void Settings_LoadDoorForms(void);
void Settings_ApplyDoorForms(void);
int Settings_DoorCharacter(int door, int kind);
/* What an event's character select and legacy events ask for. The ASM hooks pass
 * these numbers, and all of them share one rtoc slot. */
enum TMCharacterStep {
    TM_CHARACTERS_RESTORE, TM_CHARACTERS_SAVE, TM_CHARACTERS_OPEN,
    TM_CHARACTERS_NOTE_FORMS, TM_CHARACTERS_KEEP_FORMS, TM_CHARACTERS_CPU, TM_CHARACTERS_CSS_LOADED,
};
int Settings_EventCharacters(unsigned step);
void Settings_ResetEventRecords(void *card);
void *Settings_EventScoreSlot(unsigned event);
unsigned Events_ScoreIndex(unsigned page, unsigned event);

#endif
