#include "../MexTK/mex.h"
#include "events.h"
#include "fighter_swap.h"
#include "stage_swap.h"
#include "zoom_camera.h"

static void Exit(GOBJ *menu);
static void Reset(void);

static void ChangeSide(GOBJ *menu_gobj, int value);
static void ChangePlayerPercent(GOBJ *menu_gobj, int value);
static void ChangePlayerPosition(GOBJ *menu_gobj, int value);
static void ChangeHitStrength(GOBJ *menu_gobj, int value);
static void ChangeBlastZones(GOBJ *menu_gobj, int value);
static void ChangeCamera(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_MagMin(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_MagMax(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_AngMin(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_AngMax(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_DmgMin(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_DmgMax(GOBJ *menu_gobj, int value);

static EventMenu Menu_Main;
static EventMenu Menu_CustomHitStrength;
static EventMenu Menu_Characters;

// Common values -------------------------------------------------

typedef struct KBValues {
    float mag_min, mag_max;
    float ang_min, ang_max;
    float dmg_min, dmg_max;
} KBValues;

enum options_kbvals {
    KBVALS_WEAK,
    KBVALS_NORMAL,
    KBVALS_HARD,
    KBVALS_SHALLOW,
    KBVALS_CUSTOM,
    
    KBVALS_COUNT
};

#define CUSTOMKB_INIT_MAGMIN 100
#define CUSTOMKB_INIT_MAGMAX 120
#define CUSTOMKB_INIT_ANGMIN 45
#define CUSTOMKB_INIT_ANGMAX 60
#define CUSTOMKB_INIT_DMGMIN 50
#define CUSTOMKB_INIT_DMGMAX 80

static const char *Values_HitStrength[] = {"Weak", "Normal", "Hard", "Shallow", "Custom"};
static const char *Values_PlayerPosition[] = {"At Ledge", "Close", "Halfway", "Mid Stage"};
typedef char edgeguard_position_names[(countof(Values_PlayerPosition) == TM_EG_POSITION_COUNT) ? 1 : -1];
static const char *Values_Side[] = {"Random", "Left", "Right"};
typedef char edgeguard_side_names[(countof(Values_Side) == TM_EG_SIDE_COUNT) ? 1 : -1];
static KBValues HitStrength_KBRange[] = {
    {
         80.f, 100.f, // mag
         45.f,  60.f, // ang
         10.f,  40.f, // dmg
    },
    {
        106.f, 120.f, // mag
         45.f,  60.f, // ang
         50.f,  80.f, // dmg
    },
    {
        140.f, 200.f, // mag
         65.f,  70.f, // ang
         80.f, 110.f, // dmg
    },
    {
        110.f, 130.f, // mag
         20.f,  40.f, // ang
         60.f,  80.f, // dmg
    },
    {
        CUSTOMKB_INIT_MAGMIN, CUSTOMKB_INIT_MAGMAX,
        CUSTOMKB_INIT_ANGMIN, CUSTOMKB_INIT_ANGMAX,
        CUSTOMKB_INIT_DMGMIN, CUSTOMKB_INIT_DMGMAX,
    },
};

enum options_initial {
    OPT_MAIN_RECOVERY,
    OPT_MAIN_SIDE,
    OPT_MAIN_PERCENT,
    OPT_MAIN_POSITION,
    OPT_MAIN_HITSTRENGTH,
    OPT_MAIN_HITSTRENGTH_CUSTOMMENU,
    OPT_MAIN_BLAST_ZONES,
    OPT_MAIN_CAMERA,
    OPT_MAIN_CHARACTERS,
    OPT_MAIN_STAGE,
    OPT_MAIN_EXIT,
    
    OPT_MAIN_COUNT
};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_MENU,
        .menu = 0, // Set in Event_Init
        .name = "Recovery Options",
        .desc = {"Alter the CPU's recovery options.",
                 "Each character's options are saved."},
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Side",
        .desc = {"Which ledge each attempt starts at: a random one,",
                 "or always the left or the right.",
                 "Takes effect on the next reset. This setting is saved."},
        .values = Values_Side,
        .value_num = countof(Values_Side),
        .OnChange = ChangeSide,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 999,
        .name = "Player Percent",
        .desc = {"Adjust the player's percent.",
                 "This setting is saved."},
        .format = "%d%%",
        .OnChange = ChangePlayerPercent,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Player Position",
        .desc = {"Where you start each attempt: at the ledge, close,",
                 "halfway to the middle, or the middle of the stage.",
                 "Takes effect on the next reset. This setting is saved."},
        .values = Values_PlayerPosition,
        .value_num = countof(Values_PlayerPosition),
        .val = TM_EG_POSITION_CLOSE,
        .OnChange = ChangePlayerPosition,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Hit Strength",
        .desc = {"How far the CPU will be knocked back.",
                 "This setting is saved."},
        .values = Values_HitStrength,
        .value_num = countof(Values_HitStrength),
        .val = 1,
        .OnChange = ChangeHitStrength,
    },
    {
        .kind = OPTKIND_MENU,
        .menu = &Menu_CustomHitStrength,
        .name = "Customize Hit Strength",
        .desc = {"Manually adjust where and how far the CPU",
                 "will be knocked back. These settings are saved."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = TM_GLOBAL_BLAST_ZONES_NAME,
        .desc = {"Draw a red line on the stage's blast zones.",
                 "Shares the Global Overlays & Gameplay setting."},
        .OnChange = ChangeBlastZones,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Camera",
        .value_num = 2,
        .values = (const char *[]) { ZOOM_CAMERA_VALUES },
        .OnChange = ChangeCamera,
        .desc = {"Zoom out to see farther offstage.",
                 "This setting is saved."},
    },
    {
        .kind = OPTKIND_MENU,
        .menu = &Menu_Characters,
        .name = "Change Characters",
        .desc = {"Restart with another player or CPU character.",
                 "The CPU can be any character with Recovery",
                 "Options. Saved settings are kept."},
    },
    {
        .kind = OPTKIND_MENU,
        .menu = &StageSwap_Menu,
        .name = "Change Stage",
        .desc = {"Restart on another tournament-legal stage.",
                 "Saved settings are kept."},
    },
    {
        .kind = OPTKIND_FUNC,
        .name = "Exit",
        .desc = {"Return to the Event Select Screen."},
        .OnSelect = Exit,
    }};

static EventMenu Menu_Main = {
    .on_exit = Exit,
    .name = "Edgeguard",
    .option_num = sizeof(Options_Main) / sizeof(EventOption),
    .options = Options_Main,
};

// Custom Hit Strength ---------------------------------

enum options_custom_hit_strength {
    OPT_CUSTOMHIT_MAGMIN,
    OPT_CUSTOMHIT_MAGMAX,
    OPT_CUSTOMHIT_ANGMIN,
    OPT_CUSTOMHIT_ANGMAX,
    OPT_CUSTOMHIT_DMGMIN,
    OPT_CUSTOMHIT_DMGMAX,
    
    OPT_CUSTOMHIT_COUNT
};

static EventOption Options_CustomHitStrength[] = {
    {
        .kind = OPTKIND_INT,
        .value_num = 301,
        .val = CUSTOMKB_INIT_MAGMIN,
        .name = "Knockback Min",
        .desc = {"Adjust the minimum of how far the CPU will fly."},
        .format = "%d",
        .OnChange = ChangeCustomKB_MagMin,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 301,
        .val = CUSTOMKB_INIT_MAGMAX,
        .name = "Knockback Max",
        .desc = {"Adjust the maximum of how far the CPU will fly."},
        .format = "%d",
        .OnChange = ChangeCustomKB_MagMax,
    },
    
    {
        .kind = OPTKIND_INT,
        .value_num = 91,
        .val = CUSTOMKB_INIT_ANGMIN,
        .name = "Angle Min",
        .desc = {"Adjust the minimum angle the CPU will fly."},
        .format = "%d",
        .OnChange = ChangeCustomKB_AngMin,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 91,
        .val = CUSTOMKB_INIT_ANGMAX,
        .name = "Angle Max",
        .desc = {"Adjust the maximum angle the CPU will fly."},
        .format = "%d",
        .OnChange = ChangeCustomKB_AngMax,
    },
    
    {
        .kind = OPTKIND_INT,
        .value_num = 1000,
        .val = CUSTOMKB_INIT_DMGMIN,
        .name = "Percent Min",
        .desc = {"Adjust the CPU's minimum percent."},
        .format = "%d%",
        .OnChange = ChangeCustomKB_DmgMin,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 1000,
        .val = CUSTOMKB_INIT_DMGMAX,
        .name = "Percent Max",
        .desc = {"Adjust the CPU's maximum percent."},
        .format = "%d%",
        .OnChange = ChangeCustomKB_DmgMax,
    },
};

static EventMenu Menu_CustomHitStrength = {
    .name = "Custom Hit Strength",
    .option_num = sizeof(Options_CustomHitStrength) / sizeof(EventOption),
    .options = Options_CustomHitStrength,
};

// Change Characters ---------------------------------

/* Only these CPUs have recovery logic (RecoveryMenus in recovery.c), in the
 * character selector's tier-list order. */
static const u8 Edgeguard_CPUKinds[] = {
    CKIND_FOX, CKIND_MARTH, CKIND_JIGGLYPUFF, CKIND_FALCO, CKIND_PEACH, CKIND_SHEIK,
    CKIND_FALCON, CKIND_ICECLIMBERS, CKIND_LUIGI, CKIND_YOSHI, CKIND_SAMUS, CKIND_DRMARIO,
};

static EventMenu Menu_Characters = {
    .name = "Change Characters",
    .option_num = countof(FighterSwap_Options),
    .options = FighterSwap_Options,
    .row_hooks = &FighterSwap_RowHooks,
};
