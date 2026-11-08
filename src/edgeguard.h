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
static void ChangeKnockbackHeight(GOBJ *menu_gobj, int value);
static void ChangeKnockbackDistance(GOBJ *menu_gobj, int value);
static void ChangeBlastZones(GOBJ *menu_gobj, int value);
static void ChangeCamera(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_MagMin(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_MagMax(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_AngMin(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_AngMax(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_DmgMin(GOBJ *menu_gobj, int value);
static void ChangeCustomKB_DmgMax(GOBJ *menu_gobj, int value);

static EventMenu Menu_Main;
static EventMenu Menu_CustomKnockback;
static EventMenu Menu_Characters;

// Common values -------------------------------------------------

/* Where the CPU can act again when hitstun ends: how high its feet are over
 * the ledge, and how far out from the ledge. The launch is worked out for the
 * CPU and stage (ChooseLaunch), so these mean the same for every CPU. */
enum knockback_height {
    KB_HEIGHT_RANDOM,
    KB_HEIGHT_VERY_HIGH,
    KB_HEIGHT_HIGH,
    KB_HEIGHT_LEVEL,
    KB_HEIGHT_LOW,
    KB_HEIGHT_VERY_LOW,
    KB_HEIGHT_CUSTOM,

    KB_HEIGHT_COUNT
};

enum knockback_distance {
    KB_DISTANCE_RANDOM,
    KB_DISTANCE_CLOSE,
    KB_DISTANCE_NEAR,
    KB_DISTANCE_MEDIUM,
    KB_DISTANCE_FAR,

    KB_DISTANCE_COUNT
};

typedef struct KBRange { float min, max; } KBRange;

static const KBRange Knockback_Heights[KB_HEIGHT_CUSTOM] = {
    [KB_HEIGHT_VERY_HIGH] = {  51,  61 },
    [KB_HEIGHT_HIGH]      = {  23,  33 },
    [KB_HEIGHT_LEVEL]     = {  -5,   5 },
    [KB_HEIGHT_LOW]       = { -33, -23 },
    [KB_HEIGHT_VERY_LOW]  = { -61, -51 },
};

static const KBRange Knockback_Distances[KB_DISTANCE_COUNT] = {
    [KB_DISTANCE_CLOSE]  = { 20, 30 },
    [KB_DISTANCE_NEAR]   = { 40, 50 },
    [KB_DISTANCE_MEDIUM] = { 60, 70 },
    [KB_DISTANCE_FAR]    = { 80, 90 },
};

// The CPU's percent, for every height but Custom.
#define KB_PERCENT_MIN 50
#define KB_PERCENT_MAX 80

#define CUSTOMKB_INIT_MAGMIN 100
#define CUSTOMKB_INIT_MAGMAX 120
#define CUSTOMKB_INIT_ANGMIN 45
#define CUSTOMKB_INIT_ANGMAX 60
#define CUSTOMKB_INIT_DMGMIN 50
#define CUSTOMKB_INIT_DMGMAX 80

static const char *Values_KnockbackHeight[] = {"Random", "Very High", "High", "Level", "Low", "Very Low", "Custom"};
typedef char edgeguard_height_names[(countof(Values_KnockbackHeight) == KB_HEIGHT_COUNT && KB_HEIGHT_COUNT == TM_EG_KB_HEIGHT_COUNT) ? 1 : -1];
static const char *Values_KnockbackDistance[] = {"Random", "Close", "Near", "Medium", "Far"};
typedef char edgeguard_distance_names[(countof(Values_KnockbackDistance) == KB_DISTANCE_COUNT && KB_DISTANCE_COUNT == TM_EG_KB_DISTANCE_COUNT) ? 1 : -1];
static const char *Values_PlayerPosition[] = {"At Ledge", "Close", "Halfway", "Mid Stage"};
typedef char edgeguard_position_names[(countof(Values_PlayerPosition) == TM_EG_POSITION_COUNT) ? 1 : -1];
static const char *Values_Side[] = {"Random", "Left", "Right"};
typedef char edgeguard_side_names[(countof(Values_Side) == TM_EG_SIDE_COUNT) ? 1 : -1];
enum options_initial {
    OPT_MAIN_RECOVERY,
    OPT_MAIN_SIDE,
    OPT_MAIN_PERCENT,
    OPT_MAIN_POSITION,
    OPT_MAIN_KB_HEIGHT,
    OPT_MAIN_KB_DISTANCE,
    OPT_MAIN_KB_CUSTOMMENU,
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
        .name = "Knockback Height",
        .desc = {"How high above or below the ledge the CPU is",
                 "when it can act again after hitstun. Random picks",
                 "one each reset. Custom uses Customize Knockback.",
                 "This setting is saved."},
        .values = Values_KnockbackHeight,
        .value_num = countof(Values_KnockbackHeight),
        .val = KB_HEIGHT_LOW,
        .OnChange = ChangeKnockbackHeight,
    },
    {
        .kind = OPTKIND_STRING,
        .name = "Knockback Distance",
        .desc = {"How far out from the ledge the CPU is when it can",
                 "act again after hitstun. Random picks one each",
                 "reset. This setting is saved."},
        .values = Values_KnockbackDistance,
        .value_num = countof(Values_KnockbackDistance),
        .val = KB_DISTANCE_NEAR,
        .OnChange = ChangeKnockbackDistance,
    },
    {
        .kind = OPTKIND_MENU,
        .menu = &Menu_CustomKnockback,
        .name = "Customize Knockback",
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

// Custom Knockback ------------------------------------

enum options_custom_knockback {
    OPT_CUSTOMKB_MAGMIN,
    OPT_CUSTOMKB_MAGMAX,
    OPT_CUSTOMKB_ANGMIN,
    OPT_CUSTOMKB_ANGMAX,
    OPT_CUSTOMKB_DMGMIN,
    OPT_CUSTOMKB_DMGMAX,
    
    OPT_CUSTOMKB_COUNT
};

static EventOption Options_CustomKnockback[] = {
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

static EventMenu Menu_CustomKnockback = {
    .name = "Custom Knockback",
    .option_num = sizeof(Options_CustomKnockback) / sizeof(EventOption),
    .options = Options_CustomKnockback,
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
