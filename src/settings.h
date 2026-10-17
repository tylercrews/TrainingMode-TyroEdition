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
#define TM_OSD_CATEGORIES (TM_SETTINGS_OSDS + 1) /* Append-only runtime category; core palette unchanged. */
#define TM_GLOBAL_ASMASH_RELEASE_ID 30
#define TM_GLOBAL_ASMASH_RELEASE_NAME "A-Smash Release"
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
};

enum TMSettingsFlag {
    TM_FLAG_OSDS_OFF, TM_FLAG_TRAILS_VERY_FAST, TM_FLAG_TRAILS_INSTANT,
    TM_FLAG_MISSED_LCANCEL, TM_FLAG_LAST_BLOCKED_FRAME, TM_FLAG_INFINITE_SHIELDS,
    TM_FLAG_RUN_TURNAROUND,
    TM_FLAG_INVINCIBILITY,
    TM_FLAG_CPU_OSDS_OFF,
    TM_FLAG_COUNT,
};

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
    TM_SETTING_LOW_LASER, /* Event OSDs and global suppression in the extension. */
    TM_SETTING_POWERSHIELD, /* Powershield event menu, append-only identity. */
    TM_SETTING_SWEETSPOT_CAMERA, /* Index 0 only: Normal (0) / Zoomed Out (1). */
    TM_SETTING_REVERSAL,
    TM_SETTING_ATTACK_SHIELD,
    TM_SETTING_EDITOR_PAGE, /* Runtime-only Global Settings page; never saved. */
};
enum TMOSDLayout { TM_OSD_RECENT, TM_OSD_FIXED, TM_OSD_PANEL, TM_OSD_LAYOUT_COUNT };

/* Stable preference IDs, independent of either event's menu row order. */
enum TMLedgedashPreference {
    TM_LEDGE_START, TM_LEDGE_RESET, TM_LEDGE_CRITERION, TM_LEDGE_RESET_DELAY, TM_LEDGE_TIPS,
    TM_LEDGE_PREF_COUNT,
};
enum TMEggsPreference {
    TM_EGGS_DAMAGE, TM_EGGS_SCALE, TM_EGGS_VELOCITY, TM_EGGS_COLLISION,
    TM_EGGS_FREE_PRACTICE, TM_EGGS_PREF_COUNT,
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
    TM_UNDERFIRE_PREF_COUNT
};
enum TMLowLaserPreference {
    TM_LOW_LASER_OSDS, TM_LOW_LASER_SUPPRESS_GLOBAL, TM_LOW_LASER_PREF_COUNT
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
#define TM_LEDGE_DEFAULT_RESET 1
#define TM_LEDGE_DEFAULT_DELAY 1
#define TM_LEDGE_DEFAULT_TIPS 1
#define TM_EGGS_DEFAULT_DAMAGE 12
#define TM_EGGS_DEFAULT_VELOCITY 1

extern const uint8_t TMSettings_OSDIDs[TM_SETTINGS_OSDS];
static inline unsigned TMSettings_OSDCategoryID(unsigned slot) {
    return slot < TM_SETTINGS_OSDS ? TMSettings_OSDIDs[slot] : TM_GLOBAL_ASMASH_RELEASE_ID;
}
#define TM_EDITOR_PAGES 2
#define TM_EDITOR_ROWS 29
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
void Settings_EventCharacters(unsigned save);
void Settings_ResetEventRecords(void *card);
void *Settings_EventScoreSlot(unsigned event);
unsigned Events_ScoreIndex(unsigned page, unsigned event);

#endif
