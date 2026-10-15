#ifndef TM_CPU_ACTIONS_H
#define TM_CPU_ACTIONS_H
#include "cpu_ground_actions.h"

typedef struct CPUAction {
    u16 state;                  // state to perform this action. -1 for last
    u8 frameLow;                // first possible frame to perform this action
    u8 frameHi;                 // last possible frame to perfrom this action
    s8 stickX;                  // left stick X value
    s8 stickY;                  // left stick Y value
    s8 cstickX;                 // c stick X value
    s8 cstickY;                 // c stick Y value
    int input;                  // button to input
    u16 isLast        : 1; // flag to indicate this was the final input
    u16 stickDir      : 3; // 0 = none, 1 = towards opponent, 2 = away from opponent, 3 = forward, 4 = backward
    u16 recSlot       : 4; // 0 = none, 1 = slot 1, ..., 6 = slot 6, -1 = random
    u16 noActAfter    : 1; // 0 = goto CPUSTATE_RECOVER, 1 = goto CPUSTATE_NONE
    u16 random        : 1; // 0 = none, 1 = random counter action
    u16 randomAdv     : 1; // 0 = none, 1 = random advanced custom counter action
    bool (*custom_check)(GOBJ *);
} CPUAction;

enum stick_dir
{
    STCKDIR_NONE,
    STCKDIR_TOWARD,
    STCKDIR_AWAY,
    STCKDIR_FRONT,
    STCKDIR_BACK,
    STICKDIR_RDM,
};

enum custom_asid_groups
{
    ASID_ACTIONABLE = 1000,
    ASID_ACTIONABLEGROUND,
    ASID_ACTIONABLEAIR,
    ASID_ACTIONABLESHIELD,
    ASID_DAMAGEAIR,
    ASID_ANY,
};

static bool CheckHasJump(GOBJ *g);

// ACTIONS #################################################

#define ActionEnd { .state = -1, }

// CPU Action Definitions
static CPUAction Lab_CPUActionShield[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_TRIGGER_R,
        .isLast    = 1,
    },
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R,
        .isLast    = 1,
    },
    ActionEnd,
};
static CPUAction Lab_CPUActionGrab[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_BUTTON_A | PAD_TRIGGER_R,
        .isLast    = 1,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_TRIGGER_Z,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionRunUpGrab[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },
    {
        .state     = ASID_TURN,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },
    {
        .state     = ASID_DASH,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
        .input     = PAD_BUTTON_Y,
    },
    {
        .state     = ASID_KNEEBEND,
        .input     = PAD_TRIGGER_Z,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionDashAttack[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },
    {
        .state     = ASID_TURN,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },
    {
        .state     = ASID_DASH,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
        .frameLow  = 5,
        .input     = PAD_BUTTON_A,
        .isLast    = 1,
    },
    {
        .state     = ASID_DASH,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionUpB[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_KNEEBEND,
        .stickY    = 127,
        .input     = PAD_BUTTON_B,
        .isLast    = 1,
    },
    {
        .state     = ASID_ACTIONABLE,
        .stickY    = 127,
        .input     = PAD_BUTTON_B,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionSideBToward[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLE,
        .stickX    = 127,
        .input     = PAD_BUTTON_B,
        .isLast    = 1,
        .stickDir  = STCKDIR_TOWARD,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionSideBAway[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLE,
        .stickX    = 127,
        .input     = PAD_BUTTON_B,
        .isLast    = 1,
        .stickDir  = STCKDIR_AWAY,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionDownB[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLE,
        .stickY    = -127,
        .input     = PAD_BUTTON_B,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionNeutralB[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLE,
        .input     = PAD_BUTTON_B,
        .isLast    = 1,
    },
    ActionEnd
};

// We buffer this for a single frame.
// For some reason spotdodge is only possible frame 2 when floorhugging
// an attack that would have otherwise knocked you into the air without knockdown.
// This doesn't occur with rolls for some reason.
static CPUAction Lab_CPUActionSpotdodge[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickY    = -127,
        .input     = PAD_TRIGGER_R,
    },
    {
        .state     = ASID_ACTIONABLESHIELD,
        .stickY    = -127,
        .input     = PAD_TRIGGER_R,
    },
    {
        .state     = ASID_ESCAPE,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionRollAway[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .stickX    = 127,
        .input     = PAD_TRIGGER_R,
        .isLast    = 1,
        .stickDir  = STCKDIR_AWAY,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_TRIGGER_R,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionRollTowards[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .stickX    = 127,
        .input     = PAD_TRIGGER_R,
        .isLast    = 1,
        .stickDir  = STCKDIR_TOWARD,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_TRIGGER_R,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionRollRandom[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .stickX    = 127,
        .input     = PAD_TRIGGER_R,
        .isLast    = 1,
        .stickDir  = STICKDIR_RDM,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_TRIGGER_R,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionNair[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_BUTTON_A,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionFair[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .cstickX   = 127,
        .isLast    = 1,
        .stickDir  = 3,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionDair[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .cstickY   = -127,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionBair[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .cstickX   = 127,
        .isLast    = 1,
        .stickDir  = 4,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionUair[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .cstickY   = 127,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionJump[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
        .isLast    = 1,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionJumpFull[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_KNEEBEND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionJumpAway[] = {
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_BUTTON_X,
        .isLast    = 1,
        .custom_check = CheckHasJump,
        .stickX    = 127,
        .stickDir  = STCKDIR_AWAY,
    },

    // wiggle out if we can't jump
    {
        .state     = ASID_DAMAGEAIR,
        .stickX    = 127,
        .isLast    = 1,
    },
    
    // otherwise do nothing
    {
        .state     = ASID_ACTIONABLEAIR,
        .isLast    = 1,
    },

    ActionEnd
};
static CPUAction Lab_CPUActionJumpTowards[] = {
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_BUTTON_X,
        .isLast    = 1,
        .custom_check = CheckHasJump,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },

    // wiggle out if we can't jump
    {
        .state     = ASID_DAMAGEAIR,
        .stickX    = 127,
        .isLast    = 1,
    },
    
    // otherwise do nothing
    {
        .state     = ASID_ACTIONABLEAIR,
        .isLast    = 1,
    },

    ActionEnd
};
static CPUAction Lab_CPUActionJumpNeutral[] = {
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_BUTTON_X,
        .isLast    = 1,
        .custom_check = CheckHasJump,
    },

    // wiggle out if we can't jump
    {
        .state     = ASID_DAMAGEAIR,
        .stickX    = 127,
        .isLast    = 1,
    },
    
    // otherwise do nothing
    {
        .state     = ASID_ACTIONABLEAIR,
        .isLast    = 1,
    },
    
    ActionEnd
};
static CPUAction Lab_CPUActionAirdodge[] = {
    // wiggle out if we are in tumble
    {
        .state     = ASID_DAMAGEAIR,
        .stickX    = 127,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_TRIGGER_R,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionFFTumble[] = {
    {
        .state      = ASID_DAMAGEAIR,
        .stickY     = -127,
        .isLast     = 1,
        .noActAfter = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionFFWiggle[] = {
    {
        .state     = ASID_DAMAGEAIR,
        .stickX    = 127,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .stickY    = -127,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionJab[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_A,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionFTilt[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickX    = 80,
        .input     = PAD_BUTTON_A,
        .isLast    = 1,
        .stickDir  = STCKDIR_TOWARD,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionUTilt[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickY    = 80,
        .input     = PAD_BUTTON_A,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionDTilt[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickY    = -80,
        .input     = PAD_BUTTON_A,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionUSmash[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .cstickY   = 127,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionDSmash[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .cstickY   = -127,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionFSmash[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .cstickX   = 127,
        .isLast    = 1,
        .stickDir  = STCKDIR_TOWARD,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionUpSmashOOS[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_KNEEBEND,
        .cstickY   = 127,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionWavedashAway[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_TRIGGER_L,
        .stickY    = -45,
        .stickX    = 65,
        .stickDir  = STCKDIR_AWAY,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionWavedashTowards[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_TRIGGER_L,
        .stickY    = -45,
        .stickX    = 64,
        .stickDir  = STCKDIR_TOWARD,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionWavedashDown[] = {
    {
        .state     = ASID_ACTIONABLESHIELD,
        .input     = PAD_TRIGGER_R | PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEGROUND,
        .input     = PAD_BUTTON_X,
    },
    {
        .state     = ASID_ACTIONABLEAIR,
        .input     = PAD_TRIGGER_L,
        .stickY    = -80,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionDashAway[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickX    = 127,
        .stickDir  = STCKDIR_AWAY,
    },
    {
        .state     = ASID_TURN,
        .stickX    = 127,
        .stickDir  = STCKDIR_AWAY,
    },
    {
        .state     = ASID_DASH,
        .stickX    = 127,
        .stickDir  = STCKDIR_AWAY,
        .isLast    = 1,
    },
    ActionEnd
};
static CPUAction Lab_CPUActionDashTowards[] = {
    {
        .state     = ASID_ACTIONABLEGROUND,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },
    {
        .state     = ASID_TURN,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
    },
    {
        .state     = ASID_DASH,
        .stickX    = 127,
        .stickDir  = STCKDIR_TOWARD,
        .isLast    = 1,
    },
    ActionEnd
};

#define RECSLOT_RANDOM 15
static CPUAction Lab_CPUActionSlot1[] = { { .recSlot = 1 }, ActionEnd, };
static CPUAction Lab_CPUActionSlot2[] = { { .recSlot = 2 }, ActionEnd, };
static CPUAction Lab_CPUActionSlot3[] = { { .recSlot = 3 }, ActionEnd, };
static CPUAction Lab_CPUActionSlot4[] = { { .recSlot = 4 }, ActionEnd, };
static CPUAction Lab_CPUActionSlot5[] = { { .recSlot = 5 }, ActionEnd, };
static CPUAction Lab_CPUActionSlot6[] = { { .recSlot = 6 }, ActionEnd, };
static CPUAction Lab_CPUActionSlotRandom[] = { { .recSlot = RECSLOT_RANDOM }, ActionEnd, };

static CPUAction Lab_CPUActionRandom[] = { { .random = true }, ActionEnd, };
static CPUAction Lab_CPUActionRandomAdv[] = { { .randomAdv = true }, ActionEnd, };

static CPUAction *Lab_CPUActions[] = {
    0,
    Lab_CPUActionShield,
    Lab_CPUActionGrab,
    Lab_CPUActionRunUpGrab,
    Lab_CPUActionUpB,
    Lab_CPUActionSideBToward,
    Lab_CPUActionSideBAway,
    Lab_CPUActionDownB,
    Lab_CPUActionNeutralB,
    Lab_CPUActionSpotdodge,
    Lab_CPUActionRollAway,
    Lab_CPUActionRollTowards,
    Lab_CPUActionRollRandom,
    Lab_CPUActionNair,
    Lab_CPUActionFair,
    Lab_CPUActionDair,
    Lab_CPUActionBair,
    Lab_CPUActionUair,
    Lab_CPUActionJump,
    Lab_CPUActionJumpFull,
    Lab_CPUActionJumpAway,
    Lab_CPUActionJumpTowards,
    Lab_CPUActionJumpNeutral,
    Lab_CPUActionAirdodge,
    Lab_CPUActionFFTumble,
    Lab_CPUActionFFWiggle,
    Lab_CPUActionJab,
    Lab_CPUActionFTilt,
    Lab_CPUActionUTilt,
    Lab_CPUActionDTilt,
    Lab_CPUActionDashAttack,
    Lab_CPUActionUSmash,
    Lab_CPUActionDSmash,
    Lab_CPUActionFSmash,
    Lab_CPUActionUpSmashOOS,
    Lab_CPUActionWavedashAway,
    Lab_CPUActionWavedashTowards,
    Lab_CPUActionWavedashDown,
    Lab_CPUActionDashAway,
    Lab_CPUActionDashTowards,
    Lab_CPUActionSlot1,
    Lab_CPUActionSlot2,
    Lab_CPUActionSlot3,
    Lab_CPUActionSlot4,
    Lab_CPUActionSlot5,
    Lab_CPUActionSlot6,
    Lab_CPUActionSlotRandom,
    Lab_CPUActionRandomAdv,
    Lab_CPUActionRandom,
};

enum CPU_ACTIONS
{
    CPUACT_NONE,

    CPUACT_NORMAL_START, // start of normal counter actions
    
    CPUACT_SHIELD = CPUACT_NORMAL_START,
    CPUACT_GRAB,
    CPUACT_RUNUPGRAB,
    CPUACT_UPB,
    CPUACT_SIDEBTOWARD,
    CPUACT_SIDEBAWAY,
    CPUACT_DOWNB,
    CPUACT_NEUTRALB,
    CPUACT_SPOTDODGE,
    CPUACT_ROLLAWAY,
    CPUACT_ROLLTOWARDS,
    CPUACT_ROLLRDM,
    CPUACT_NAIR,
    CPUACT_FAIR,
    CPUACT_DAIR,
    CPUACT_BAIR,
    CPUACT_UAIR,
    CPUACT_SHORTHOP,
    CPUACT_FULLHOP,
    CPUACT_JUMPAWAY,
    CPUACT_JUMPTOWARDS,
    CPUACT_JUMPNEUTRAL,
    CPUACT_AIRDODGE,
    CPUACT_FFTUMBLE,
    CPUACT_FFWIGGLE,
    CPUACT_JAB,
    CPUACT_FTILT,
    CPUACT_UTILT,
    CPUACT_DTILT,
    CPUACT_DASHATTACK,
    CPUACT_USMASH,
    CPUACT_DSMASH,
    CPUACT_FSMASH,
    CPUACT_USMASHOOS,
    CPUACT_WAVEDASH_AWAY,
    CPUACT_WAVEDASH_TOWARDS,
    CPUACT_WAVEDASH_DOWN,
    CPUACT_DASH_AWAY,
    CPUACT_DASH_TOWARDS,

    CPUACT_NORMAL_END, // end of normal counter actions
    
    CPUACT_SLOT1 = CPUACT_NORMAL_END,
    CPUACT_SLOT2,
    CPUACT_SLOT3,
    CPUACT_SLOT4,
    CPUACT_SLOT5,
    CPUACT_SLOT6,
    CPUACT_SLOT_RANDOM,
    CPUACT_RANDOMADV,
    CPUACT_RANDOM,

    CPUACT_COUNT // total number of counter actions
};

#define SLOT_ACTIONS CPUACT_SLOT1, CPUACT_SLOT2, CPUACT_SLOT3, CPUACT_SLOT4, CPUACT_SLOT5, CPUACT_SLOT6, CPUACT_SLOT_RANDOM
#define SLOT_NAMES "Play Slot 1", "Play Slot 2", "Play Slot 3", "Play Slot 4", "Play Slot 5", "Play Slot 6", "Play Random Slot"

static u8 CPUCounterActionsGround[] = {CPU_GROUND_ACTIONS(CPU_GROUND_ACTION_ID) SLOT_ACTIONS, CPUACT_RANDOMADV, CPUACT_RANDOM};

static u8 CPUCounterActionsAir[] = {CPUACT_NONE, CPUACT_AIRDODGE, CPUACT_JUMPAWAY, CPUACT_JUMPTOWARDS, CPUACT_JUMPNEUTRAL, CPUACT_UPB, CPUACT_SIDEBTOWARD, CPUACT_SIDEBAWAY, CPUACT_DOWNB, CPUACT_NEUTRALB, CPUACT_NAIR, CPUACT_FAIR, CPUACT_DAIR, CPUACT_BAIR, CPUACT_UAIR, CPUACT_FFTUMBLE, CPUACT_FFWIGGLE, SLOT_ACTIONS, CPUACT_RANDOMADV, CPUACT_RANDOM};

static u8 CPUCounterActionsShield[] = {CPUACT_NONE, CPUACT_GRAB, CPUACT_SHORTHOP, CPUACT_FULLHOP, CPUACT_SPOTDODGE, CPUACT_ROLLAWAY, CPUACT_ROLLTOWARDS, CPUACT_ROLLRDM, CPUACT_USMASHOOS, CPUACT_UPB, CPUACT_DOWNB, CPUACT_NAIR, CPUACT_FAIR, CPUACT_DAIR, CPUACT_BAIR, CPUACT_UAIR, CPUACT_WAVEDASH_AWAY, CPUACT_WAVEDASH_TOWARDS, CPUACT_WAVEDASH_DOWN, SLOT_ACTIONS, CPUACT_RANDOMADV, CPUACT_RANDOM};

static const char *LabValues_CounterGround[] = {CPU_GROUND_ACTIONS(CPU_GROUND_ACTION_NAME) SLOT_NAMES, "Random Advanced", "Random"};
static const char *LabValues_CounterAir[] = {"None", "Airdodge", "Jump Away", "Jump Towards", "Jump Neutral", "Up B", "Side B Toward", "Side B Away", "Down B", "Neutral B", "Neutral Air", "Forward Air", "Down Air", "Back Air", "Up Air", "Tumble Fastfall", "Wiggle Fastfall", SLOT_NAMES, "Random Advanced", "Random"};
static const char *LabValues_CounterShield[] = {"None", "Grab", "Short Hop", "Full Hop", "Spotdodge", "Roll Away", "Roll Towards", "Roll Random", "Up Smash", "Up B", "Down B", "Neutral Air", "Forward Air", "Down Air", "Back Air", "Up Air", "Wavedash Away", "Wavedash Towards", "Wavedash Down", SLOT_NAMES, "Random Advanced", "Random"};

static u8 CPUCounterActionsGroundRandom[] = {CPUACT_SPOTDODGE, CPUACT_RUNUPGRAB, CPUACT_FSMASH, CPUACT_ROLLRDM, CPUACT_NAIR, CPUACT_FAIR, CPUACT_JAB, CPUACT_FTILT, CPUACT_DTILT, CPUACT_DASHATTACK, CPUACT_DASH_AWAY, CPUACT_DASH_TOWARDS};
static u8 CPUCounterActionsAirRandom[] = {CPUACT_AIRDODGE, CPUACT_JUMPNEUTRAL, CPUACT_NAIR, CPUACT_FAIR, CPUACT_DAIR, CPUACT_FFWIGGLE};
static u8 CPUCounterActionsShieldRandom[] = {CPUACT_GRAB, CPUACT_FULLHOP, CPUACT_SPOTDODGE, CPUACT_ROLLRDM, CPUACT_USMASHOOS, CPUACT_NAIR, CPUACT_FAIR, CPUACT_DAIR, CPUACT_WAVEDASH_AWAY};

#endif
