// Include this file in your event source and call Recovery_Think. Easy!
//
// You can set opt_flags manually, or use RecoveryMenuOptFlags with the given menus.

#include "../MexTK/mex.h"
#include "events.h"

static void Think_Spacies(GOBJ *cpu, u32 opt_flags);
static void Think_Sheik(GOBJ *cpu, u32 opt_flags);
static void Think_Falcon(GOBJ *cpu, u32 opt_flags);
static void Think_Marth(GOBJ *cpu, u32 opt_flags);
static void Think_Yoshi(GOBJ *cpu, u32 opt_flags);
static void Think_Peach(GOBJ *cpu, u32 opt_flags);
static void Think_Luigi(GOBJ *cpu, u32 opt_flags);
static void Think_DrMario(GOBJ *cpu, u32 opt_flags);
static void Think_IceClimbers(GOBJ *cpu, u32 opt_flags);
static void Think_Puff(GOBJ *cpu, u32 opt_flags);
static void Think_Samus(GOBJ *cpu, u32 opt_flags);
static bool RecoveryAssignsInputs(int kind);

typedef void (*ThinkFn)(GOBJ *, u32);
static ThinkFn ThinkFnLookup[] = {
    0, // MARIO
    Think_Spacies, // FOX
    Think_Falcon, // FALCON
    0, // DK
    0, // KIRBY
    0, // BOWSER
    0, // LINK
    Think_Sheik, // SHEIK
    0, // NESS
    Think_Peach, // PEACH
    Think_IceClimbers, // POPO
    0, // NANA (follows Popo with the game's own AI)
    0, // PIKACHU
    Think_Samus, // SAMUS
    Think_Yoshi, // YOSHI
    Think_Puff, // JIGGLYPUFF
    0, // MEWTWO
    Think_Luigi, // LUIGI
    Think_Marth, // MARTH
    0, // ZELDA
    0, // YOUNGLINK
    Think_DrMario, // DRMARIO
    Think_Spacies, // FALCO
    0, // PICHU
    0, // GAW,
    0, // GANONDORF
    0, // ROY
};

static Vec2 ledge_positions[2];

static void GetLedgePositions(Vec2 coords_out[2]) {
    static char ledge_ids[34][2] = {
        { 0xFF, 0xFF }, { 0xFF, 0xFF }, { 0x03, 0x07 }, { 0x33, 0x36 },
        { 0x03, 0x0D }, { 0x29, 0x45 }, { 0x05, 0x11 }, { 0x09, 0x1A },
        { 0x02, 0x06 }, { 0x15, 0x17 }, { 0x00, 0x00 }, { 0x43, 0x4C },
        { 0x00, 0x00 }, { 0x00, 0x00 }, { 0x0E, 0x0D }, { 0x00, 0x00 },
        { 0x00, 0x05 }, { 0x1E, 0x2E }, { 0x0C, 0x0E }, { 0x02, 0x04 },
        { 0x03, 0x05 }, { 0x00, 0x00 }, { 0x06, 0x12 }, { 0x00, 0x00 },
        { 0xD7, 0xE2 }, { 0x00, 0x00 }, { 0x00, 0x00 }, { 0x00, 0x00 },
        { 0x03, 0x05 }, { 0x03, 0x0B }, { 0x06, 0x10 }, { 0x00, 0x05 },
        { 0x00, 0x02 }, { 0x01, 0x01 },
    };

    int stage_id = Stage_GetExternalID();
    char left_id = ledge_ids[stage_id][0];
    char right_id = ledge_ids[stage_id][1];

    Vec3 pos;
    Stage_GetLeftOfLineCoordinates(left_id, &pos);
    coords_out[0] = (Vec2) { pos.X, pos.Y };
    Stage_GetRightOfLineCoordinates(right_id, &pos);
    coords_out[1] = (Vec2) { pos.X, pos.Y };
}

static void DIIn(FighterData *cpu_data) {
    bool right = cpu_data->phys.pos.X > 0.f;

    cpu_data->cpu.lstickX = right ? -90 : 90;
    cpu_data->cpu.lstickY = 90;

    // Spoof last input as current input to prevent SDI
    cpu_data->input.lstick.X = right ? -0.7f : 0.7f;
    cpu_data->input.lstick.Y = 0.7f;
}

#define OptEnabled(opt) (((opt_flags) & (1u << (opt))) != 0)

#define CMN_FALL_MIN_DISTANCE 20.0f

void Recovery_Think(GOBJ *cpu, u32 opt_flags) {
    if (ledge_positions[0].X == 0.0f)
        GetLedgePositions(ledge_positions);
        
    FighterData *cpu_data = cpu->userdata;
    cpu_data->cpu.ai = 15; // noact

    // The newer recoveries set every button and stick themselves each frame,
    // so a press lasts exactly the frames that ask for it (mashing needs that).
    if (RecoveryAssignsInputs(cpu_data->kind)) {
        cpu_data->cpu.held = 0;
        cpu_data->cpu.lstickX = cpu_data->cpu.lstickY = 0;
        cpu_data->cpu.cstickX = cpu_data->cpu.cstickY = 0;
    }

    // Common thinking -----------------------
    
    float x = cpu_data->phys.pos.X;
    int state = cpu_data->state_id;
    
    // wall tech
    cpu_data->input.timer_LR = 0;
    
    // if falling on stage, don't do anything
    if (
        (ASID_FALL <= state && state <= ASID_FALLAERIALB)
        || (ASID_JUMPF <= state && state <= ASID_JUMPAERIALB)
    ) {
        float lx = ledge_positions[0].X;
        float rx = ledge_positions[1].X;
        
        // if withing stage, at greater than CMN_FALL_MIN_DISTANCE units from ledge, just drop down.
        if (lx + CMN_FALL_MIN_DISTANCE <= x && x <= rx - CMN_FALL_MIN_DISTANCE) {
            cpu_data->cpu.lstickX = 0;
            cpu_data->cpu.lstickY = 0;
            return;
        
        // if within stage, but not greater than CMN_FALL_MIN_DISTANCE units from ledge, drift inwards.
        } else if (lx <= x && x <= rx) {
            cpu_data->cpu.lstickX = -127 * sign(x);
            cpu_data->cpu.lstickY = 0;
            return;
        } 
    }
    
    // normal getup
    if (state == ASID_CLIFFWAIT && cpu_data->TM.state_frame & 2) {
        cpu_data->cpu.lstickX = -127 * sign(x);
        cpu_data->cpu.lstickY = 0;
        return;
    }
    
    // angel platform
    if (state == ASID_REBIRTHWAIT) {
        cpu_data->cpu.lstickX = 0;
        cpu_data->cpu.lstickY = -127;
        return;
    }
    
    // Character specific thinking ------------
    
    ThinkFn think_fn = ThinkFnLookup[cpu_data->kind];
    if (think_fn) {
        think_fn(cpu, opt_flags);
    } else {
        cpu_data->cpu.ai = 0;
    }
}

enum options_spacies {
    OPT_SPACIES_FF_LOW,
    OPT_SPACIES_FF_MID,
    OPT_SPACIES_FF_HIGH,
    OPT_SPACIES_JUMP,
    OPT_SPACIES_ILLUSION,
    OPT_SPACIES_FASTFALL,
    
    OPT_SPACIES_COUNT
};

enum options_sheik {
    OPT_SHEIK_UPB_LEDGE,
    OPT_SHEIK_UPB_STAGE,
    OPT_SHEIK_UPB_HIGH,
    OPT_SHEIK_JUMP,
    OPT_SHEIK_FAIR,
    OPT_SHEIK_AMSAH_TECH,
    
    OPT_SHEIK_COUNT
};

enum options_falcon {
    OPT_FALCON_UPB,
    OPT_FALCON_DRIFT_BACK,
    OPT_FALCON_JUMP,
    OPT_FALCON_FASTFALL,
    OPT_FALCON_DOWNB,

    OPT_FALCON_COUNT
};

enum options_marth {
    OPT_MARTH_UPB_EARLY,
    OPT_MARTH_JUMP,
    OPT_MARTH_SIDEB,
    OPT_MARTH_FAIR,

    OPT_MARTH_COUNT
};

// Row order is the saved bit order (Edgeguard's TM_EG_RECOVERY_* masks).

enum options_yoshi {
    OPT_YOSHI_JUMP_HIGH,
    OPT_YOSHI_JUMP_LOW,
    OPT_YOSHI_AIRDODGE_LEDGE,
    OPT_YOSHI_AIRDODGE_STAGE,
    OPT_YOSHI_EGG,

    OPT_YOSHI_COUNT
};

enum options_peach {
    OPT_PEACH_FLOAT_STAGE,
    OPT_PEACH_FLOAT_LOW,
    OPT_PEACH_PARASOL,
    OPT_PEACH_BOMBER,
    OPT_PEACH_FLOAT_AERIALS,

    OPT_PEACH_COUNT
};

enum options_luigi {
    OPT_LUIGI_JUMP,
    OPT_LUIGI_CYCLONE,
    OPT_LUIGI_MISSILE,
    OPT_LUIGI_UPB_EARLY,
    OPT_LUIGI_FAST_MASH,

    OPT_LUIGI_COUNT
};

enum options_drmario {
    OPT_DRMARIO_JUMP,
    OPT_DRMARIO_TORNADO,
    OPT_DRMARIO_CAPE,
    OPT_DRMARIO_UPB_EARLY,
    OPT_DRMARIO_FAST_MASH,

    OPT_DRMARIO_COUNT
};

enum options_iceclimbers {
    OPT_ICS_BELAY,
    OPT_ICS_SQUALL,
    OPT_ICS_FAST_MASH,

    OPT_ICS_COUNT
};

enum options_puff {
    OPT_PUFF_JUMP_HIGH,
    OPT_PUFF_JUMP_LOW,
    OPT_PUFF_POUND,
    OPT_PUFF_AERIALS,

    OPT_PUFF_COUNT
};

enum options_samus {
    OPT_SAMUS_JUMP,
    OPT_SAMUS_BOMBS,
    OPT_SAMUS_GRAPPLE,
    OPT_SAMUS_SCREW,
    OPT_SAMUS_SCREW_HIGH,

    OPT_SAMUS_COUNT
};
 
// Fox -------------------------------------------------

static EventOption RecOptions_Fox[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Firefox Low",
        .desc = {"Allow Fox to aim his up special to the ledge."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Firefox Mid",
        .desc = {"Allow Fox to aim his up special to the stage."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Firefox High",
        .desc = {"Allow Fox to aim his up special high."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Fox to double jump."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Illusion",
        .desc = {"Allow Fox to side special."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Fall",
        .desc = {"Allow Fox to fast fall."},
    },
};
static EventMenu RecMenu_Fox = {
    .name = "Fox Recovery",
    .option_num = sizeof(RecOptions_Fox) / sizeof(EventOption),
    .options = RecOptions_Fox,
};

// Falco -------------------------------------------------

static EventOption RecOptions_Falco[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Firebird Low",
        .desc = {"Allow Falco to aim his up special to the ledge."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Firebird Mid",
        .desc = {"Allow Falco to aim his up special to the stage."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Firebird High",
        .desc = {"Allow Falco to aim his up special high."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Falco to double jump."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Phantasm",
        .desc = {"Allow Falco to side special."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Fall",
        .desc = {"Allow Falco to fast fall."},
    },
};
static EventMenu RecMenu_Falco = {
    .name = "Falco Recovery",
    .option_num = sizeof(RecOptions_Falco) / sizeof(EventOption),
    .options = RecOptions_Falco,
};

// Sheik -------------------------------------------------

static EventOption RecOptions_Sheik[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Vanish to Ledge",
        .desc = {"Allow Sheik to vanish to ledge and the",
                 "tip of the stage."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Vanish to Stage",
        .desc = {"Allow Sheik to vanish deep into stage."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Vanish High",
        .desc = {"Allow Sheik to vanish above the ledge",
                 "and to platforms."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Sheik to double jump."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fair",
        .desc = {"Allow Sheik to fair the player."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Amsah Tech",
        .desc = {"Allow Sheik to Amsah Tech."},
    },
};

static EventMenu RecMenu_Sheik = {
    .name = "Sheik Recovery",
    .option_num = sizeof(RecOptions_Sheik) / sizeof(EventOption),
    .options = RecOptions_Sheik,
};

// Captain Falcon --------------------------------------

static EventOption RecOptions_Falcon[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Falcon Dive",
        .desc = {"Allow Falcon to Up B."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Drift Back",
        .desc = {"Allow Falcon to drift backwards."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Falcon to double jump."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Fall",
        .desc = {"Allow Falcon to fast fall."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Falcon Kick",
        .desc = {"Allow Falcon to Down B."},
    },
};

static EventMenu RecMenu_Falcon = {
    .name = "Falcon Recovery",
    .option_num = sizeof(RecOptions_Falcon) / sizeof(EventOption),
    .options = RecOptions_Falcon,
};

// Marth --------------------------------------

static EventOption RecOptions_Marth[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Dolphin Slash Early",
        .desc = {"Allow Marth to Up B above the ledge."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Marth to double jump."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Dancing Blade",
        .desc = {"Allow Marth to use SideB to recover and",
                "delay his timing."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fair",
        .desc = {"Allow Marth to fair the player."},
    },
};

static EventMenu RecMenu_Marth = {
    .name = "Marth Recovery",
    .option_num = sizeof(RecOptions_Marth) / sizeof(EventOption),
    .options = RecOptions_Marth,
};

// Yoshi --------------------------------------

static EventOption RecOptions_Yoshi[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Jump High",
        .desc = {"Allow Yoshi to double jump at or above",
                 "ledge height and drift onto the stage."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Jump Low",
        .desc = {"Allow Yoshi to save the double jump",
                 "until below the ledge."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Air Dodge to Ledge",
        .desc = {"Allow Yoshi to air dodge up to the ledge",
                 "after the double jump."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Air Dodge to Stage",
        .desc = {"Allow Yoshi to air dodge onto the stage."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Egg Toss",
        .desc = {"Allow Yoshi to throw eggs at the player",
                 "while recovering high."},
    },
};

static EventMenu RecMenu_Yoshi = {
    .name = "Yoshi Recovery",
    .option_num = sizeof(RecOptions_Yoshi) / sizeof(EventOption),
    .options = RecOptions_Yoshi,
};

// Peach --------------------------------------

static EventOption RecOptions_Peach[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Float to Stage",
        .desc = {"Allow Peach to double jump, float at the",
                 "top and drift onto the stage."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Float Low",
        .desc = {"Allow Peach to float near ledge height."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Parasol",
        .desc = {"Allow Peach to up special to the ledge."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Peach Bomber",
        .desc = {"Allow Peach to side special toward the",
                 "stage."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Float Aerials",
        .desc = {"Allow Peach to attack the player while",
                 "floating."},
    },
};

static EventMenu RecMenu_Peach = {
    .name = "Peach Recovery",
    .option_num = sizeof(RecOptions_Peach) / sizeof(EventOption),
    .options = RecOptions_Peach,
};

// Luigi --------------------------------------

static EventOption RecOptions_Luigi[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Luigi to double jump."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Cyclone",
        .desc = {"Allow Luigi to mash down special for",
                 "height. It lifts only once, until a",
                 "Cyclone touches the ground."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Green Missile",
        .desc = {"Allow Luigi to side special toward the",
                 "stage. It misfires 1 time in 8, as in",
                 "the game."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Up B Early",
        .desc = {"Allow Luigi to up special onto the stage",
                 "from above the ledge."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Mashing",
        .desc = {"Mash B about 15 times a second instead",
                 "of about 10. Luigi's Cyclone needs it",
                 "to gain much height."},
    },
};

static EventMenu RecMenu_Luigi = {
    .name = "Luigi Recovery",
    .option_num = sizeof(RecOptions_Luigi) / sizeof(EventOption),
    .options = RecOptions_Luigi,
};

// Dr. Mario ----------------------------------

static EventOption RecOptions_DrMario[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Dr. Mario to double jump."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Dr. Tornado",
        .desc = {"Allow Dr. Mario to mash down special for",
                 "height. Only the first one each airtime",
                 "lifts."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Cape Stall",
        .desc = {"Allow Dr. Mario to side special to stall",
                 "and change the timing."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Up B Early",
        .desc = {"Allow Dr. Mario to up special onto the",
                 "stage from above the ledge."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Mashing",
        .desc = {"Mash B about 15 times a second instead",
                 "of about 10."},
    },
};

static EventMenu RecMenu_DrMario = {
    .name = "Dr. Mario Recovery",
    .option_num = sizeof(RecOptions_DrMario) / sizeof(EventOption),
    .options = RecOptions_DrMario,
};

// Ice Climbers -------------------------------

static EventOption RecOptions_IceClimbers[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Belay",
        .desc = {"Allow the Ice Climbers to up special when",
                 "Nana is close enough to help."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Squall Hammer",
        .desc = {"Allow the Ice Climbers to mash side",
                 "special for height and distance."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Mashing",
        .desc = {"Mash B about 15 times a second instead",
                 "of about 10. Squall Hammer can't lift",
                 "more than once every 7 frames."},
    },
};

static EventMenu RecMenu_IceClimbers = {
    .name = "Ice Climbers Recovery",
    .option_num = sizeof(RecOptions_IceClimbers) / sizeof(EventOption),
    .options = RecOptions_IceClimbers,
};

// Jigglypuff ---------------------------------

static EventOption RecOptions_Puff[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Jump High",
        .desc = {"Allow Jigglypuff to jump above the ledge",
                 "and drift onto the stage."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Jump Low",
        .desc = {"Allow Jigglypuff to save jumps and stay",
                 "near ledge height."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Pound",
        .desc = {"Allow Jigglypuff to side special toward",
                 "the stage."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Aerials",
        .desc = {"Allow Jigglypuff to attack the player",
                 "when they get close."},
        .val = 1,
    },
};

static EventMenu RecMenu_Puff = {
    .name = "Jigglypuff Recovery",
    .option_num = sizeof(RecOptions_Puff) / sizeof(EventOption),
    .options = RecOptions_Puff,
};

// Samus --------------------------------------

static EventOption RecOptions_Samus[] = {
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Samus to double jump."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Bomb Jumps",
        .desc = {"Allow Samus to lay bombs to stall and",
                 "drift toward the stage."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Grapple",
        .desc = {"Allow Samus to tether to the ledge. A",
                 "miss leaves Samus helpless, so it's",
                 "only used close to the ledge."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Screw Attack",
        .desc = {"Allow Samus to up special to the ledge."},
        .val = 1,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Screw Attack High",
        .desc = {"Allow Samus to up special onto the stage",
                 "from near ledge height."},
    },
};

static EventMenu RecMenu_Samus = {
    .name = "Samus Recovery",
    .option_num = sizeof(RecOptions_Samus) / sizeof(EventOption),
    .options = RecOptions_Samus,
};

EventMenu *RecoveryMenus[] = {
    0, // MARIO
    &RecMenu_Fox,
    &RecMenu_Falcon,
    0, // DK
    0, // KIRBY
    0, // BOWSER
    0, // LINK
    &RecMenu_Sheik,
    0, // NESS
    &RecMenu_Peach,
    &RecMenu_IceClimbers, // POPO
    0, // NANA
    0, // PIKACHU
    &RecMenu_Samus,
    &RecMenu_Yoshi,
    &RecMenu_Puff,
    0, // MEWTWO
    &RecMenu_Luigi,
    &RecMenu_Marth,
    0, // ZELDA
    0, // YOUNGLINK
    &RecMenu_DrMario,
    &RecMenu_Falco,
    0, // PICHU
    0, // GAW,
    0, // GANONDORF
    0, // ROY
};

// Helper FNs ---------------------------------------------

u32 RecoveryMenuOptFlags(GOBJ *cpu) {
    FighterData *cpu_data = cpu->userdata;
    EventMenu *menu = RecoveryMenus[cpu_data->kind];
    if (menu == 0)
        return 0;

    u32 opt_flags = 0;
    for (u32 opt = 0; opt < (u32)menu->option_num; ++opt)
        opt_flags |= (u32)menu->options[opt].val << opt;
    return opt_flags;
}

static inline float Progress(float n, float a, float b) { return (n - a) / (b - a); }
static inline bool Within(float a, float b, float tolerance) { return fabs(b - a) < tolerance; }

static int HitstunEnded(GOBJ *fighter) {
    FighterData *data = fighter->userdata;
    float hitstun;
    memcpy(&hitstun, &data->state_var.state_var1, sizeof(float));
    return hitstun == 0.0;
}

static int InHitstunAnim(int state) {
    return ASID_DAMAGEHI1 <= state && state <= ASID_DAMAGEFLYROLL;
}

static bool IsAirActionable(GOBJ *fighter) {
    FighterData *data = fighter->userdata;
    
    // ensure airborne
    if (data->phys.air_state == 0)
        return false;

    int state = data->state_id;

    if (InHitstunAnim(state)) {
        if (HitstunEnded(fighter))
            return true;
        
        FtDamage *dmg = (FtDamage *)&data->state_var;
        if (dmg->is_meteor && dmg->meteor_lockout == 0)
            return true;
    }
    
    if (state == ASID_PASSIVEWALL && data->TM.state_frame > 5)
        return true;
        
    if (state == ASID_PASSIVEWALLJUMP)
        return true;
        
    if (ASID_ATTACKAIRN <= state && state <= ASID_ATTACKAIRLW && data->flags.past_iasa)
        return true;

    return (ASID_JUMPF <= state && state <= ASID_FALLAERIALB)
        || state == ASID_DAMAGEFALL;
}

static float Vec2_Distance(Vec2 *a, Vec2 *b) {
    float dx = a->X - b->X;
    float dy = a->Y - b->Y;
    return sqrtf(dx*dx + dy*dy);
}

static float Vec2_Length(Vec2 *a) {
    float x = a->X;
    float y = a->Y;
    return sqrtf(x*x + y*y);
}

static void PhysStep(FighterData *data, Vec2 *pos, Vec2 *vel) {
    float gravity = data->phys.air_state == 0 ? 0.f : data->attr.gravity;
    float min_y_vel = data->flags.is_fastfall ?
        -data->attr.fastfall_velocity
        : -data->attr.terminal_velocity;
        
    pos->X += vel->X;
    pos->Y += vel->Y;
    vel->Y -= gravity;
    vel->Y = fmax(vel->Y, min_y_vel);
}

static Vec2 SimulatePhys(FighterData *data, int future) {
    Vec2 pos = { data->phys.pos.X, data->phys.pos.Y };
    Vec2 vel = { data->phys.self_vel.X, data->phys.self_vel.Y };
    for (int i = 0; i < future; ++i)
        PhysStep(data, &pos, &vel);
    
    return pos;
}

static bool SimulatePhys_CanReachPoint(FighterData *data, Vec2 target) {
    if (data->phys.air_state == 0) return false; 

    Vec2 pos = { data->phys.pos.X, data->phys.pos.Y };
    Vec2 vel = { data->phys.self_vel.X, data->phys.self_vel.Y };
    
    float dir = sign(target.X - pos.X);
    if (sign(vel.X) != dir)
        return false;
    
    while (target.Y < pos.Y || vel.Y >= 0.f)
        PhysStep(data, &pos, &vel);
        
    return dir != sign(target.X - pos.X);
}

static float ProjectedDistance(FighterData *a, FighterData *b, int future) {
    Vec2 a_future = SimulatePhys(a, future);
    Vec2 b_future = SimulatePhys(b, future);
    return Vec2_Distance(&a_future, &b_future);
}

// Spacies ------------------------------------------------

#define SP_UPB_CHANCE 12 
#define SP_JUMP_CHANCE_BELOW_LEDGE 4
#define SP_JUMP_CHANCE_ABOVE_LEDGE 30
#define SP_ILLUSION_CHANCE_ABOVE_LEDGE 30
#define SP_ILLUSION_CHANCE_TO_LEDGE 12
#define SP_FASTFALL_CHANCE 8

#define SP_MAX_ILLUSION_HEIGHT 30.f
#define SP_MIN_ILLUSION_HEIGHT -15.f
#define SP_ILLUSION_DISTANCE 75.f
#define SP_FIREFOX_DISTANCE 80.f
#define SP_FIREBIRD_DISTANCE 70.f

#define SP_SWEETSPOT_OFFSET_X 16
#define SP_SWEETSPOT_OFFSET_Y 5
    
static void Think_Spacies(GOBJ *cpu, u32 opt_flags) {
    FighterData *cpu_data = cpu->userdata;

    Vec2 pos = { cpu_data->phys.pos.X, cpu_data->phys.pos.Y };
    Vec2 vel = { cpu_data->phys.self_vel.X, cpu_data->phys.self_vel.Y };
    int state = cpu_data->state_id;
    float dir = pos.X > 0.f ? -1.f : 1.f;
    bool can_jump = cpu_data->jump.jumps_used < 2 && OptEnabled(OPT_SPACIES_JUMP);
    
    Vec2 target_ledge = ledge_positions[pos.X > 0.f];
    Vec2 target_ledgegrab = {
        .X = target_ledge.X - SP_SWEETSPOT_OFFSET_X*dir,
        .Y = target_ledge.Y - SP_SWEETSPOT_OFFSET_Y,
    };
    
    Vec2 vec_to_ledgegrab = {
        .X = target_ledgegrab.X - pos.X,
        .Y = target_ledgegrab.Y - pos.Y,
    };
    float distance_to_ledgegrab = Vec2_Length(&vec_to_ledgegrab);
    
    int dj_chance, illusion_chance;
    if (cpu_data->phys.pos.Y > target_ledge.Y) {
        dj_chance = SP_JUMP_CHANCE_ABOVE_LEDGE;
        illusion_chance = SP_ILLUSION_CHANCE_ABOVE_LEDGE;
    } else {
        dj_chance = SP_JUMP_CHANCE_BELOW_LEDGE;
        illusion_chance = SP_ILLUSION_CHANCE_TO_LEDGE;
    }
    
    bool is_past_ledge = fabs(target_ledgegrab.X) > fabs(pos.X);
    bool can_fall_to_ledge = is_past_ledge
        | SimulatePhys_CanReachPoint(cpu_data, target_ledgegrab);
    
    bool can_upb = OptEnabled(OPT_SPACIES_FF_LOW)
        | OptEnabled(OPT_SPACIES_FF_MID)
        | OptEnabled(OPT_SPACIES_FF_HIGH);
    
    float upb_distance = cpu_data->kind == FTKIND_FOX ?
        SP_FIREFOX_DISTANCE : SP_FIREBIRD_DISTANCE;
    
    // HITSTUN
    if (cpu_data->flags.hitlag) {
        DIIn(cpu_data);
        
    // ACTIONABLE
    } else if (IsAirActionable(cpu)) {

        // JUMP
        if (
            OptEnabled(OPT_SPACIES_JUMP)
            && can_jump
            && (
                // force jump if at end of range
                distance_to_ledgegrab > upb_distance
                
                // otherwise, random chance to jump
                || HSD_Randi(dj_chance) == 0
            )
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_Y;
            if (distance_to_ledgegrab > upb_distance)
                cpu_data->cpu.lstickX = 127 * dir;
            else 
                cpu_data->cpu.lstickX = 127 * (HSD_Randi(3) - 1);
            
        // ILLUSION
        } else if (
            OptEnabled(OPT_SPACIES_ILLUSION) && (
                // force illusion to ledge if no jump and cannot upb
                (
                    !can_upb && !can_jump
                    && SP_MIN_ILLUSION_HEIGHT < pos.Y
                    && pos.Y < SP_MIN_ILLUSION_HEIGHT + 5.f
                )
            
                // random chance to illusion
                || (
                    pos.Y > SP_MIN_ILLUSION_HEIGHT
                    && pos.Y < SP_MAX_ILLUSION_HEIGHT
                    && fabs(vec_to_ledgegrab.X) < SP_ILLUSION_DISTANCE
                    && HSD_Randi(illusion_chance) == 0
                )
            )
        ) {
            cpu_data->cpu.lstickX = 127 * dir;
            cpu_data->cpu.held |= PAD_BUTTON_B;
            
        // FIREFOX
        } else if (
            vel.Y <= 1.5f
            && can_upb
            && !can_fall_to_ledge
            && (
                // force upb if at end of range
                (pos.Y < 0.f && distance_to_ledgegrab > upb_distance)
                || (pos.Y < Stage_GetBlastzoneBottom() + 10.f)
    
                // otherwise, random chance to upb
                || (
                    distance_to_ledgegrab < upb_distance
                    && HSD_Randi(SP_UPB_CHANCE) == 0
                )
            )
        ) {
            cpu_data->cpu.lstickY = 127;
            cpu_data->cpu.held |= PAD_BUTTON_B;
            
        // FASTFALL
        } else if (
            OptEnabled(OPT_SPACIES_FASTFALL)
            && !cpu_data->flags.is_fastfall
            && vel.Y < 0.f
            && HSD_Randi(SP_FASTFALL_CHANCE) == 0
        ) {
            cpu_data->cpu.lstickY = -127;
            
        // WAIT
        } else {
            // drift towards stage
            cpu_data->cpu.lstickX = 127 * dir;
        }
        
    // FIREFOX STARTUP
    } else if (0x161 <= state && state <= 0x162) {
        // compute firefox angle
        
        int low = OptEnabled(OPT_SPACIES_FF_LOW);
        int mid = OptEnabled(OPT_SPACIES_FF_MID);
        int high = OptEnabled(OPT_SPACIES_FF_HIGH);
        int option_count = low + mid + high;
        int choice = HSD_Randi(option_count);
        
        float high_y;
        if (vec_to_ledgegrab.X < upb_distance) {
            high_y = pos.Y + sqrtf(
                upb_distance*upb_distance
                - vec_to_ledgegrab.X*vec_to_ledgegrab.X
            );
        } else {
            high_y = vec_to_ledgegrab.Y;
        }
        
        Vec2 target = { .X = target_ledgegrab.X };
        if (low && choice-- == 0) {
            target.Y = target_ledgegrab.Y;
        } else if (mid && choice-- == 0) {
            target.Y = (target_ledgegrab.Y + high_y) / 2.f;
        } else if (high && choice-- == 0) {
            target.Y = high_y;
        }
        
        Vec2 vec_to_target = {
            .X = target.X - pos.X,
            .Y = target.Y - pos.Y,
        };
        Vec2_Normalize(&vec_to_target);
        
        // place angles barely inside deadzone outside of deadzone
        if (-0.2875f < vec_to_target.X && vec_to_target.X <= -0.1375f)
            vec_to_target.X = -0.2875f;
        else if (0.2875f > vec_to_target.X && vec_to_target.X >= 0.1375f)
            vec_to_target.X = 0.2875f;

        cpu_data->cpu.lstickX = (s8)(vec_to_target.X * 127.f);
        cpu_data->cpu.lstickY = (s8)(vec_to_target.Y * 127.f);
        
    // INACTIONABLE WITH DRIFT
    } else if (
        // if in firefox ending states
        (0x162 <= state && state <= 0x167)
        
        // or special fall
        || (ASID_FALLSPECIAL <= state && state <= ASID_FALLSPECIALB)
    ) {
        // drift towards stage
        cpu_data->cpu.lstickX = 127 * dir;
    }
}

// Sheik ------------------------------------------------

#define SK_UPB_POOF_NOT_ON_LEDGE_CHANCE 10
#define SK_UPB_POOF_ON_LEDGE_CHANCE 5
#define SK_UPB_JUMP_HEIGHT 48.f
#define SK_UPB_JUMP_MAX_X 50.f
#define SK_UPB_POOF_DISTANCE 40.f
#define SK_FAIR_SIZE 18

#define SK_JUMP_CHANCE_FAR_BELOW_LEDGE 4
#define SK_JUMP_CHANCE_BELOW_LEDGE 30
#define SK_JUMP_CHANCE_ABOVE_LEDGE 120
#define SK_FAIR_CHANCE 5
#define SK_AMSAH_TECH_CHANCE 1

#define SK_SWEETSPOT_OFFSET_X 15
#define SK_SWEETSPOT_OFFSET_Y 5

// There is a frame between inputting the DI for the amsah tech
// and inputting the tech direction. We need to make sure that
// we tech the same as we DI, so we set this flag to prevent the CPU
// from DIing in as usual in that frame.
static bool amsah_teching = false;

static void Think_Sheik(GOBJ *cpu, u32 opt_flags) {
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;

    Vec2 pos = { cpu_data->phys.pos.X, cpu_data->phys.pos.Y };
    Vec2 vel = { cpu_data->phys.self_vel.X, cpu_data->phys.self_vel.Y };
    int state = cpu_data->state_id;
    float dir = pos.X > 0.f ? -1.f : 1.f;
    bool can_jump = cpu_data->jump.jumps_used < 2 && OptEnabled(OPT_SHEIK_JUMP);
    bool hmn_on_ledge = hmn_data->state_id == ASID_CLIFFWAIT;
    
    Vec2 target_ledge = ledge_positions[pos.X > 0.f];
    Vec2 target_ledgegrab = {
        .X = target_ledge.X - SK_SWEETSPOT_OFFSET_X*dir,
        .Y = target_ledge.Y - SK_SWEETSPOT_OFFSET_Y,
    };
    
    Vec2 vec_to_ledgegrab = {
        .X = target_ledgegrab.X - pos.X,
        .Y = target_ledgegrab.Y - pos.Y,
    };
    Vec2 vec_to_ledge = {
        .X = target_ledge.X - pos.X,
        .Y = target_ledge.Y - pos.Y,
    };
    
    // how close sheik will be to the ledge after the jump
    Vec2 post_jump_vec_to_ledge = {
        .X = fmax(fabs(vec_to_ledge.X) - SK_UPB_JUMP_MAX_X, 0.f) * dir,
        .Y = vec_to_ledge.Y - SK_UPB_JUMP_HEIGHT,
    };
    float post_jump_dist_to_ledge = Vec2_Length(&post_jump_vec_to_ledge);
    bool can_upb = post_jump_dist_to_ledge < SK_UPB_POOF_DISTANCE
        && vel.Y < 1.f
        && (!can_jump || post_jump_dist_to_ledge < 1.f)
        && (
            OptEnabled(OPT_SHEIK_UPB_LEDGE)
            || OptEnabled(OPT_SHEIK_UPB_STAGE)
            || OptEnabled(OPT_SHEIK_UPB_HIGH)
        );
    
    int dj_chance;
    if (cpu_data->phys.pos.Y > target_ledge.Y - 10.f) {
        dj_chance = SK_JUMP_CHANCE_ABOVE_LEDGE;
    } else if (cpu_data->phys.pos.Y > target_ledge.Y - 50.f) {
        dj_chance = SK_JUMP_CHANCE_BELOW_LEDGE;
    } else {
        dj_chance = SK_JUMP_CHANCE_FAR_BELOW_LEDGE;
    }
    
    if (!cpu_data->flags.hitstun)
        amsah_teching = false;
    
    // AMSAH TECH
    if (
        OptEnabled(OPT_SHEIK_AMSAH_TECH)
        && InHitstunAnim(state)
        && cpu_data->TM.state_prev[0] == ASID_LANDINGFALLSPECIAL
        && cpu_data->flags.hitlag
        && cpu_data->dmg.hitlag_frames == 1.f
        && HSD_Randi(SK_AMSAH_TECH_CHANCE) == 0
    ) {
        cpu_data->cpu.held |= PAD_TRIGGER_R;
        cpu_data->cpu.lstickX = 80 * -(int)cpu_data->dmg.hit_log.direction;
        cpu_data->cpu.lstickY = -80;
        cpu_data->cpu.cstickY = -127;
        amsah_teching = true;

    // HITSTUN
    } else if (cpu_data->flags.hitlag && !amsah_teching) {
        DIIn(cpu_data);
    } else if (cpu_data->flags.hitstun && amsah_teching) {
        cpu_data->cpu.lstickX = pos.X < hmn_data->phys.pos.X ? -80 : 80;
        cpu_data->cpu.lstickY = -80;
        
    // ACTIONABLE
    } else if (IsAirActionable(cpu)) {
        // JUMP
        if (
            can_jump
            && (
                // force jump if at end of range
                (
                    post_jump_dist_to_ledge > SK_UPB_POOF_DISTANCE
                    && vec_to_ledgegrab.Y > 10.f
                )
                
                // otherwise, random chance to jump
                || HSD_Randi(dj_chance) == 0
            )
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_Y;
            cpu_data->cpu.lstickX = 127 * dir;
            
        // FAIR
        } else if (
            OptEnabled(OPT_SHEIK_FAIR)
            && !cpu_data->flags.is_fastfall
            && pos.X * dir < hmn_data->phys.pos.X * dir
            && hmn_data->hurt.intang_frames.ledge < 5
            && ProjectedDistance(hmn_data, cpu_data, 6) < SK_FAIR_SIZE
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_A;
            cpu_data->cpu.lstickX = 127 * dir;
            
        // UP B
        } else if (
            // random chance to upb
            (
                vec_to_ledgegrab.Y > 10.f
                && can_upb
                && !can_jump
                && !hmn_on_ledge
                && HSD_Randi(SK_UPB_POOF_NOT_ON_LEDGE_CHANCE) == 0
            )
            
            || (
                can_upb
                && !can_jump
                && hmn_on_ledge
                && HSD_Randi(SK_UPB_POOF_ON_LEDGE_CHANCE) == 0
            )
            
            // force upb if at end of range
            || (
                vec_to_ledgegrab.Y > SK_UPB_JUMP_HEIGHT 
                && can_upb
                && !can_jump
                && post_jump_dist_to_ledge < SK_UPB_POOF_DISTANCE
            )
        ) {
            cpu_data->cpu.lstickY = 127;
            cpu_data->cpu.held |= PAD_BUTTON_B;
        
        // WAIT 
        } else {
            if (pos.X * dir < target_ledgegrab.X * dir)
                cpu_data->cpu.lstickX = 127 * dir;
            else
                cpu_data->cpu.lstickX = -127 * dir;
        }
    // VANISH STARTUP
    } else if (state == 0x166) {
        // choose poof direction
        if (cpu_data->TM.state_frame == 34) {
            int ledge = OptEnabled(OPT_SHEIK_UPB_LEDGE);
            int stage_tip = OptEnabled(OPT_SHEIK_UPB_LEDGE);
            int above_ledge = OptEnabled(OPT_SHEIK_UPB_HIGH) && vec_to_ledgegrab.X < SK_UPB_POOF_DISTANCE;
            int high = OptEnabled(OPT_SHEIK_UPB_HIGH);
            int stage_inland = OptEnabled(OPT_SHEIK_UPB_STAGE) && pos.Y > 0.f;
            
            // double stage_inland chance
            int option_count = ledge + above_ledge + stage_tip + stage_inland + stage_inland + high;
            int choice = HSD_Randi(option_count);
            
            Vec2 target = {0};
            
            // upb to ledge
            if ((ledge && choice-- == 0) || option_count == 0) {
                target = target_ledge;
                target.Y -= 13.f;
                    
            // upb to stage tip
            } else if (stage_tip && choice-- == 0) {
                target = target_ledge;
                target.Y -= 2.f;
                if (pos.Y < target_ledge.Y)
                    target.Y -= 2.f;
                if (pos.Y < target_ledge.Y-10.f)
                    target.Y += 1.f;
                if (pos.Y < target_ledge.Y-28.f)
                    target.Y += 0.5f;
                
            // upb above ledge
            } else if (above_ledge && choice-- == 0) {
                float high_y = pos.Y + sqrtf(
                    SK_UPB_POOF_DISTANCE*SK_UPB_POOF_DISTANCE
                    - vec_to_ledgegrab.X*vec_to_ledgegrab.X
                );
                target = (Vec2) { target_ledge.X, high_y };
                
            // upb high to platform
            } else if (high && choice-- == 0) {
                target = (Vec2) { pos.X + dir, pos.Y + 1.f }; // TODO BETTER TARGETING
                
            // upb inwards to stage (doubled chance)
            } else if (stage_inland && choice-- <= 1) {
                target = (Vec2) { 0.f, 0.f }; // TODO BETTER TARGETING
            }
            
            Vec2 poof_dir = {
                target.X - pos.X,
                target.Y - pos.Y,
            };
            Vec2_Normalize(&poof_dir);
            
            cpu_data->cpu.lstickX = (s8)(poof_dir.X * 127.f);
            cpu_data->cpu.lstickY = (s8)(poof_dir.Y * 127.f);
            
        } else {
            if (pos.Y > target_ledge.Y) {
                cpu_data->cpu.lstickX = dir * 127.f;
            } else {
                cpu_data->cpu.lstickX = sign(vec_to_ledgegrab.X) * 127.f;
            }
        }
    
    // SPECIAL FALL
    } else if (ASID_FALLSPECIAL <= state && state <= ASID_FALLSPECIALB) {
        // drift towards stage
        cpu_data->cpu.lstickX = 127 * dir;
    }
}

// Falcon ------------------------------------------------

#define CF_SWEETSPOT_OFFSET_X 13
#define CF_SWEETSPOT_OFFSET_Y 8
#define CF_UPB_HEIGHT 48
#define CF_JUMP_HEIGHT 40

#define CF_UPB_CHANCE_FAR 17
#define CF_UPB_CHANCE_WITH_JUMP 50
#define CF_UPB_CHANCE_CLOSE 100
#define CF_UPB_DRIFT_CHANGE_CHANCE 20
#define CF_FALL_DRIFT_CHANGE_CHANCE 40
#define CF_DOWNB_CHANCE 3
#define CF_JUMP_CHANCE_BELOW_LEDGE 20
#define CF_JUMP_CHANCE_ABOVE_LEDGE 40 
#define CF_JUMP_CHANCE_TO_LEDGE 8 
#define CF_FASTFALL_CHANCE 15

#define CF_DRIFT_DURATION_LENGTH_CONST 5
#define CF_DRIFT_DURATION_LENGTH_RNG 30

static int drift_back_timer = 0;

static void Think_Falcon(GOBJ *cpu, u32 opt_flags) {
    FighterData *cpu_data = cpu->userdata;

    Vec2 pos = { cpu_data->phys.pos.X, cpu_data->phys.pos.Y };
    Vec2 vel = { cpu_data->phys.self_vel.X, cpu_data->phys.self_vel.Y };
    int state = cpu_data->state_id;
    float dir = pos.X > 0.f ? -1.f : 1.f;
    
    Vec2 target_ledge = ledge_positions[pos.X > 0.f];
    Vec2 target_ledgegrab = {
        .X = target_ledge.X - CF_SWEETSPOT_OFFSET_X*dir,
        .Y = target_ledge.Y - CF_SWEETSPOT_OFFSET_Y,
    };
    
    Vec2 vec_to_ledgegrab = {
        .X = target_ledgegrab.X - pos.X,
        .Y = target_ledgegrab.Y - pos.Y,
    };
    Vec2 vec_to_ledge = {
        .X = target_ledge.X - pos.X,
        .Y = target_ledge.Y - pos.Y,
    };
    bool past_ledge = fabs(pos.X) < fabs(target_ledgegrab.X);
    bool can_jump = cpu_data->jump.jumps_used < 2 && OptEnabled(OPT_FALCON_JUMP);
    
    int dj_chance;
    if (Within(vec_to_ledge.Y, CF_JUMP_HEIGHT - 5.f, 10.f) && vec_to_ledge.X * dir < 50.f) {
        dj_chance = CF_JUMP_CHANCE_TO_LEDGE;
    } else if (cpu_data->phys.pos.Y > target_ledge.Y - 10.f) {
        dj_chance = CF_JUMP_CHANCE_ABOVE_LEDGE;
    } else {
        dj_chance = CF_JUMP_CHANCE_BELOW_LEDGE;
    }
    
    bool can_fall_to_ledge = SimulatePhys_CanReachPoint(cpu_data, target_ledgegrab);
    int upb_chance;
    if (can_fall_to_ledge) {
        upb_chance = CF_UPB_CHANCE_CLOSE;
    } else if (can_jump) {
        upb_chance = CF_UPB_CHANCE_WITH_JUMP;
    } else {
        upb_chance = CF_UPB_CHANCE_FAR;
    }
    
    bool in_drift_back_zone = past_ledge
        || (
            vec_to_ledgegrab.Y < -30.f
            && (
                // rising drift
                (vel.Y > 1.f && -vec_to_ledgegrab.Y / 1.5f > fabs(vec_to_ledgegrab.X))
                
                // falling drift
                || (vel.Y <= 1.f && -vec_to_ledgegrab.Y / 3.f > fabs(vec_to_ledgegrab.X))
            )
        );
        
    // HITSTUN
    if (cpu_data->flags.hitlag) {
        DIIn(cpu_data);
        
    // STOP DRIFT BACK
    } else if (!in_drift_back_zone && drift_back_timer) {
        drift_back_timer = 0;
        cpu_data->cpu.lstickX = 0;
    
    // DRIFT BACK
    } else if (drift_back_timer) {
        drift_back_timer--;
        if (drift_back_timer)
            cpu_data->cpu.lstickX = -127 * dir;
        else
            // frame with no drift - this allows faster drift change
            cpu_data->cpu.lstickX = 0;
            
    } else if (IsAirActionable(cpu)) {
        // JUMP
        if (
            OptEnabled(OPT_FALCON_JUMP)
            && can_jump
            && (
                // force jump if at end of range
                vec_to_ledgegrab.Y > CF_UPB_HEIGHT
                
                // otherwise, random chance to jump
                || HSD_Randi(dj_chance) == 0
            )
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_Y;
            float jump_dir;
            if (Within(vec_to_ledge.Y, CF_JUMP_HEIGHT - 5.f, 10.f) && vec_to_ledge.X * dir < 50.f)
                jump_dir = sign(vec_to_ledgegrab.X);
            else
                jump_dir = HSD_Randf() * 2.f - 1.f;
            cpu_data->cpu.lstickX = 127 * jump_dir;
            
        // UPB
        } else if (
            OptEnabled(OPT_FALCON_UPB) 
            && (
                // random change to upb
                (
                    vel.Y <= 1.f
                    && !past_ledge
                    && vec_to_ledgegrab.Y < CF_UPB_HEIGHT
                    && HSD_Randi(upb_chance) == 0
                )
                // force upb if at end of range
                || (
                    vel.Y <= 0.f
                    && !can_jump
                    && vec_to_ledgegrab.Y > CF_UPB_HEIGHT
                )
            )
        ) {
            cpu_data->cpu.lstickY = 127;
            cpu_data->cpu.held |= PAD_BUTTON_B;
            
        // FALCON KICK
        } else if (
            OptEnabled(OPT_FALCON_DOWNB)
            && pos.Y > 60.f
            && vel.Y < 0.f
            && fabs(vec_to_ledge.X) > 50.f
            && cpu_data->jump.jumps_used == 2  
            && HSD_Randi(CF_DOWNB_CHANCE) == 0
        ) {
            cpu_data->cpu.lstickY = -127;
            cpu_data->cpu.held |= PAD_BUTTON_B;
            
        // DRIFT BACK
        } else if (
            OptEnabled(OPT_FALCON_DRIFT_BACK)
            && in_drift_back_zone
            && vec_to_ledgegrab.Y < 0.f
            && HSD_Randi(CF_FALL_DRIFT_CHANGE_CHANCE) == 0
        ) {
            drift_back_timer = HSD_Randi(CF_DRIFT_DURATION_LENGTH_RNG) + CF_DRIFT_DURATION_LENGTH_CONST;
            // frame with no drift - this allows faster drift change
            cpu_data->cpu.lstickX = 0;
        } else {
            cpu_data->cpu.lstickX = 127 * dir;
        }
        
    // UPB DRIFT
    } else if (state == 0x162) {
        if (
            OptEnabled(OPT_FALCON_DRIFT_BACK)
            && vec_to_ledgegrab.Y < 0.f
            && cpu_data->TM.state_frame > 14
            && in_drift_back_zone
            && HSD_Randi(CF_UPB_DRIFT_CHANGE_CHANCE) == 0
        ) {
            drift_back_timer = HSD_Randi(20) + 5;
            // frame with no drift - this allows faster drift change
            cpu_data->cpu.lstickX = 0;
        } else {
            cpu_data->cpu.lstickX = 127 * dir;
        }
    } else if (ASID_FALLSPECIAL <= state && state <= ASID_FALLSPECIALB) {
        // FASTFALL
        if (
            OptEnabled(OPT_FALCON_FASTFALL)
            && !cpu_data->flags.is_fastfall
            && vel.Y < 0.f
            && (past_ledge ||
                -vec_to_ledgegrab.Y / 3.f > fabs(vec_to_ledgegrab.X)
            )
            && HSD_Randi(CF_FASTFALL_CHANCE) == 0
        ) {
            cpu_data->cpu.lstickY = -127;
        } else {
            cpu_data->cpu.lstickX = 127 * dir;
        }
    }
}

// Marth ------------------------------------------------

#define MT_SWEETSPOT_OFFSET_X 12
#define MT_SWEETSPOT_OFFSET_Y 8
#define MT_JUMP_HEIGHT 36
#define MT_JUMP_DISTANCE 30
#define MT_UPB_STRAIGHT_HEIGHT 60
#define MT_UPB_STRAIGHT_DISTANCE 10
#define MT_UPB_CURLED_HEIGHT 55
#define MT_UPB_CURLED_DISTANCE 25
#define MT_FAIR_SIZE 25

#define MT_JUMP_CHANCE 5
#define MT_UPB_EARLY_CHANCE 30
#define MT_SIDEB_DELAY_CHANCE 10
#define MT_FAIR_CHANCE 3

static void Think_Marth(GOBJ *cpu, u32 opt_flags) {
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;
    
    Vec2 pos = { cpu_data->phys.pos.X, cpu_data->phys.pos.Y };
    Vec2 vel = { cpu_data->phys.self_vel.X, cpu_data->phys.self_vel.Y };
    float dir = pos.X > 0.f ? -1.f : 1.f;
    int state = cpu_data->state_id;
    
    Vec2 target_ledge = ledge_positions[pos.X > 0.f];
    Vec2 target_ledgegrab = {
        .X = target_ledge.X - MT_SWEETSPOT_OFFSET_X*dir,
        .Y = target_ledge.Y - MT_SWEETSPOT_OFFSET_Y,
    };
    
    Vec2 vec_to_ledgegrab = {
        .X = target_ledgegrab.X - pos.X,
        .Y = target_ledgegrab.Y - pos.Y,
    };
    
    bool past_ledgegrab = fabs(pos.X) < fabs(target_ledgegrab.X);
    bool can_jump = cpu_data->jump.jumps_used < 2;
    bool can_upb = vec_to_ledgegrab.Y < MT_UPB_STRAIGHT_HEIGHT
        && vec_to_ledgegrab.X * dir < MT_UPB_CURLED_DISTANCE
        && (stc_stage->kind != GRKIND_BATTLE || vec_to_ledgegrab.X * dir > 3.f);

    if (cpu_data->flags.hitlag) {
        DIIn(cpu_data);
    } else if (IsAirActionable(cpu)) {
        // SIDE B FAR
        if (
            OptEnabled(OPT_MARTH_SIDEB)
            && fabs(vec_to_ledgegrab.X) > 60.f
            && !past_ledgegrab
            && vel.Y < -0.5f
            && fabs(vel.X) > 0.8f
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_B;
            cpu_data->cpu.lstickX = 127 * dir;
            
        // SIDE B WAIT
        } else if (
            OptEnabled(OPT_MARTH_SIDEB)
            && vel.Y < 0.0f
            && vec_to_ledgegrab.Y > 10.f 
            && vec_to_ledgegrab.Y < 35.f 
            && (
                stc_stage->kind != GRKIND_BATTLE ||
                fabs(SimulatePhys(cpu_data, 25).X) > fabs(target_ledgegrab.X)
            )
            && HSD_Randi(MT_SIDEB_DELAY_CHANCE) == 0
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_B;
            cpu_data->cpu.lstickX = 127 * dir;
            
        // JUMP
        } else if (
            // random chance
            (
                OptEnabled(OPT_MARTH_JUMP)
                && MT_JUMP_HEIGHT - 5.f < vec_to_ledgegrab.Y
                && vec_to_ledgegrab.Y < MT_JUMP_HEIGHT
                && can_jump
                && fabs(vec_to_ledgegrab.X) < MT_JUMP_DISTANCE
                && HSD_Randi(MT_JUMP_CHANCE) == 0
            )
            
            // force jump if can't upb
            || (
                OptEnabled(OPT_MARTH_JUMP)
                && vec_to_ledgegrab.Y > MT_UPB_STRAIGHT_HEIGHT
                && can_jump
            )
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_Y;
            if (past_ledgegrab)
                cpu_data->cpu.lstickX = -127 * dir;
            else if (vec_to_ledgegrab.X * dir < MT_UPB_CURLED_DISTANCE)
                cpu_data->cpu.lstickX = 0;
            else
                cpu_data->cpu.lstickX = 127 * dir;
        
        // FAIR
        } else if (
            OptEnabled(OPT_MARTH_FAIR)
            && !cpu_data->flags.is_fastfall
            && pos.X * dir < hmn_data->phys.pos.X * dir
            && hmn_data->hurt.intang_frames.ledge < 4
            && ProjectedDistance(hmn_data, cpu_data, 5) < MT_FAIR_SIZE
            && target_ledge.Y - SimulatePhys(cpu_data, 30).Y < MT_UPB_STRAIGHT_HEIGHT
            && HSD_Randi(MT_FAIR_CHANCE) == 0
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_A;
            cpu_data->cpu.lstickX = 127 * dir;
            
        // UP B
        } else if (
            // random chance
            (
                OptEnabled(OPT_MARTH_UPB_EARLY)
                && vec_to_ledgegrab.Y > 35.f
                && vec_to_ledgegrab.Y < MT_UPB_STRAIGHT_HEIGHT
                && can_upb
                && !can_jump
                && vel.Y < 1.f
                && HSD_Randi(MT_UPB_EARLY_CHANCE) == 0
            )
        
            // force upb
            || (
                (
                    vec_to_ledgegrab.Y > MT_UPB_STRAIGHT_HEIGHT
                    || (
                        vec_to_ledgegrab.X * dir < MT_UPB_CURLED_DISTANCE
                        && vec_to_ledgegrab.Y > MT_UPB_CURLED_HEIGHT
                    )
                )
                && (!can_jump || !OptEnabled(OPT_MARTH_JUMP))
                && vel.Y < 0.f
            )
        ) {
            cpu_data->cpu.held |= PAD_BUTTON_B;
            cpu_data->cpu.lstickY = 127;
            
        // WAIT
        } else {
            if (pos.X * dir + 10.f < target_ledgegrab.X * dir)
                cpu_data->cpu.lstickX = 127 * dir;
            else
                cpu_data->cpu.lstickX = -127 * dir;
        }
    
    // DOLPHIN SLASH
    } else if (state == 0x170) {
        // choose curl
        if (cpu_data->TM.state_frame == 3) {
            float curl; // -1 = full curl backwards, 1 = full curl forwards
            
            if (stc_stage->kind == GRKIND_BATTLE && vec_to_ledgegrab.X * dir < 5.f) {
                curl = -1.f;
            } else if (vec_to_ledgegrab.X * dir < MT_UPB_CURLED_DISTANCE && vec_to_ledgegrab.Y < MT_UPB_CURLED_HEIGHT) {
                curl = HSD_Randf() * 0.3f + 0.7f;
            } else if (vec_to_ledgegrab.X * dir > MT_UPB_CURLED_DISTANCE) {
                curl = 1.f;
            } else if (vec_to_ledgegrab.Y > MT_UPB_STRAIGHT_HEIGHT) {
                curl = -1.f;
            } else {
                curl = Progress(vec_to_ledgegrab.X * dir, MT_UPB_STRAIGHT_DISTANCE, MT_UPB_CURLED_DISTANCE);
                curl = fclamp(curl, -1.f, 1.f);
                
                // similar function to sqrt but easier to compute
                curl = (-(curl*curl) + 2.f * fabs(curl)) * sign(curl);
            }
            
            // curl starts from the opposite diagonal notch
            if (curl < 0.f)
                curl /= 2.f;
            
            float ang = curl * M_PI / 2.f;
            cpu_data->cpu.lstickX = (127.f * sin(ang)) * dir;
            cpu_data->cpu.lstickY = 127.f * cos(ang);
        }
        
        else if (cpu_data->TM.state_frame == 5) {
            cpu_data->cpu.lstickX = 127.f * dir;
        }

    } else if (ASID_ATTACKAIRN <= state && state <= ASID_ATTACKAIRLW) {
        cpu_data->cpu.lstickX = 127 * sign(vec_to_ledgegrab.X);

    } else if (ASID_FALLSPECIAL <= state && state <= ASID_FALLSPECIALB) {
        cpu_data->cpu.lstickX = 127 * sign(vec_to_ledgegrab.X);
    }
}

// Newer recoveries ---------------------------------------------------------
//
// Yoshi, Peach, Luigi, Dr. Mario, the Ice Climbers, Jigglypuff and Samus.
// They set every button and stick themselves each frame (Recovery_Think
// clears them first). Heights of animated moves come from each move's TransN
// root motion in the ISO's PlXxAJ.dat (the game moves the fighter by that
// bone's change every frame); the rest follow the decompiled move code and
// each fighter's attributes. See notes/EDGEGUARD-NEW-CHARACTERS.md.

// How far below the ledge-grab point the top of a rise still grabs the ledge.
// Falcon's 48 and Marth's 60 above, against their 37.6 and 47.3 rises, imply
// 10 to 13.
#define LEDGE_GRAB_REACH 10.f

// Mashing: one fresh B press every this many frames, give or take one.
#define MASH_INTERVAL 6      // about 10 presses a second
#define MASH_INTERVAL_FAST 4 // about 15, a very fast masher

typedef struct RecoveryInfo {
    Vec2 pos, vel;
    float dir;      // 1 when the stage is to the right
    Vec2 ledge;     // the ledge on the CPU's side
    Vec2 to_ledge;  // ledge - pos
    Vec2 to_grab;   // ledge grab point - pos
    float out;      // how far outside the grab point (negative under or over the stage)
    bool can_jump;
    bool tumble;    // knocked into tumble: no air dodge or float
} RecoveryInfo;

static int mash_timer = 0;
static int lg_missile_charge = 0;
static bool pe_float_after_jump = false;
static int pr_high = -1; // Jigglypuff's plan this attempt: 1 high, 0 low, -1 undecided

static bool RecoveryAssignsInputs(int kind) {
    return kind == FTKIND_YOSHI || kind == FTKIND_PEACH || kind == FTKIND_LUIGI
        || kind == FTKIND_DRMARIO || kind == FTKIND_POPO || kind == FTKIND_JIGGLYPUFF
        || kind == FTKIND_SAMUS;
}

// Every attempt starts in hitlag; forget the last one's plans.
static void ResetRecoveryPlans(void) {
    mash_timer = 0;
    lg_missile_charge = 0;
    pe_float_after_jump = false;
    pr_high = -1;
}

static RecoveryInfo GetRecoveryInfo(FighterData *ft, float grab_x, float grab_y) {
    RecoveryInfo r;
    r.pos = (Vec2) { ft->phys.pos.X, ft->phys.pos.Y };
    r.vel = (Vec2) { ft->phys.self_vel.X, ft->phys.self_vel.Y };
    r.dir = r.pos.X > 0.f ? -1.f : 1.f;
    r.ledge = ledge_positions[r.pos.X > 0.f];
    r.to_ledge = (Vec2) { r.ledge.X - r.pos.X, r.ledge.Y - r.pos.Y };
    r.to_grab = (Vec2) { r.to_ledge.X - grab_x * r.dir, r.to_ledge.Y - grab_y };
    r.out = r.to_grab.X * r.dir;
    r.can_jump = ft->jump.jumps_used < ft->attr.max_jumps;
    r.tumble = ft->state_id == ASID_DAMAGEFALL || InHitstunAnim(ft->state_id);
    return r;
}

static bool OneIn(int n) { return HSD_Randi(n) == 0; }

// A fresh press. The game only acts on presses, so a button already down
// last frame is let go for a frame first; asking every frame then presses
// every other frame until the game takes it.
static void Press(FighterData *ft, int buttons) {
    ft->cpu.held |= buttons & ~ft->input.held;
}

static void SetStick(FighterData *ft, float x, float y) {
    ft->cpu.lstickX = (s8)(fclamp(x, -1.f, 1.f) * 127.f);
    ft->cpu.lstickY = (s8)(fclamp(y, -1.f, 1.f) * 127.f);
}

// Full tilt toward a point.
static void AimStick(FighterData *ft, float dx, float dy) {
    Vec2 v = { dx, dy };
    if (Vec2_Length(&v) < 0.01f)
        return;
    Vec2_Normalize(&v);
    SetStick(ft, v.X, v.Y);
}

// Above the grab point, drift onto the stage. Below it, line up `lineup`
// units outside the grab point (where the up special starts) rather than
// drifting under the stage.
static void DriftHome(FighterData *ft, RecoveryInfo *r, float lineup) {
    float x = 0.f;
    if (r->to_grab.Y < 0.f || r->out > lineup + 3.f)
        x = r->dir;
    else if (r->out < lineup - 3.f)
        x = -r->dir;
    SetStick(ft, x, 0.f);
}

// One fresh B press every MASH_INTERVAL frames, give or take one, and never
// on two frames in a row: a person mashing, not a turbo button.
static bool MashB(bool fast) {
    if (mash_timer > 0) {
        --mash_timer;
        return false;
    }
    mash_timer = (fast ? MASH_INTERVAL_FAST : MASH_INTERVAL) - 2 + HSD_Randi(3);
    return true;
}

// The player is close enough to hit with an aerial a few frames from now.
static bool PlayerInReach(FighterData *ft, float size, int frames) {
    FighterData *hmn = Fighter_GetGObj(0)->userdata;
    return hmn->hurt.intang_frames.ledge < 4 && ProjectedDistance(hmn, ft, frames) < size;
}

// Yoshi ----------------------------------------------------------------

#define YS_GRAB_X 14.f
#define YS_GRAB_Y 8.f
// The double jump's root motion dips for 8 frames, then rises 52.5 by frame
// 52, the highest in the game.
#define YS_JUMP_PEAK_FRAME 52
#define YS_JUMP_LATEST 38.f      // never wait lower than this under the grab point
#define YS_AIRDODGE_REACH 28.f   // 3.1 speed, x0.9 a frame: 31 units, less a margin

#define YS_JUMP_HIGH_CHANCE 15
#define YS_JUMP_LOW_CHANCE 20
#define YS_AIRDODGE_LEDGE_CHANCE 6
#define YS_AIRDODGE_STAGE_CHANCE 8
#define YS_EGG_CHANCE 45

static void Think_Yoshi(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    RecoveryInfo r = GetRecoveryInfo(ft, YS_GRAB_X, YS_GRAB_Y);
    int state = ft->state_id;
    bool high = OptEnabled(OPT_YOSHI_JUMP_HIGH);
    bool low = OptEnabled(OPT_YOSHI_JUMP_LOW);
    // The double jump keeps rising until frame 52; a dodge or an attack
    // would cut it short.
    bool rising = (state == ASID_JUMPAERIALF || state == ASID_JUMPAERIALB)
        && ft->TM.state_frame < YS_JUMP_PEAK_FRAME;
    Vec2 to_stage = { r.to_ledge.X + 15.f * r.dir, r.to_ledge.Y + 1.f };

    if (ft->flags.hitlag) {
        ResetRecoveryPlans();
        DIIn(ft);

    } else if (IsAirActionable(cpu)) {
        // JUMP
        if (
            r.can_jump && r.vel.Y < 0.5f
            && (
                r.to_grab.Y > YS_JUMP_LATEST
                || (high && r.to_grab.Y > -30.f && r.to_grab.Y < 10.f && r.out > 10.f && OneIn(YS_JUMP_HIGH_CHANCE))
                || (low && r.to_grab.Y >= 10.f && OneIn(YS_JUMP_LOW_CHANCE))
                || (!high && !low && r.to_grab.Y > 20.f)
            )
        ) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, r.dir, 0.f);

        // EGG TOSS while there is height to spare
        } else if (
            OptEnabled(OPT_YOSHI_EGG) && !rising && r.to_grab.Y < -10.f
            && PlayerInReach(ft, 70.f, 10) && OneIn(YS_EGG_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // AIR DODGE TO LEDGE: the dodge can't grab, the helpless fall after it can
        } else if (
            OptEnabled(OPT_YOSHI_AIRDODGE_LEDGE) && !r.tumble && !rising && !r.can_jump
            && r.to_grab.Y > 0.f && r.to_grab.Y < YS_AIRDODGE_REACH
            && r.out < YS_AIRDODGE_REACH * 0.7f
            && (OneIn(YS_AIRDODGE_LEDGE_CHANCE) || r.to_grab.Y > YS_AIRDODGE_REACH * 0.6f)
        ) {
            Press(ft, PAD_TRIGGER_R);
            AimStick(ft, r.to_grab.X, r.to_grab.Y + 10.f);

        // AIR DODGE TO STAGE
        } else if (
            OptEnabled(OPT_YOSHI_AIRDODGE_STAGE) && !r.tumble && !rising
            && r.to_ledge.Y < 5.f && Vec2_Length(&to_stage) < YS_AIRDODGE_REACH
            && OneIn(YS_AIRDODGE_STAGE_CHANCE)
        ) {
            Press(ft, PAD_TRIGGER_R);
            AimStick(ft, to_stage.X, to_stage.Y);

        // WAIT
        } else {
            DriftHome(ft, &r, 0.f);
        }

    } else {
        DriftHome(ft, &r, 0.f);
    }
}

// Peach ----------------------------------------------------------------

#define PE_GRAB_X 12.f
#define PE_GRAB_Y 8.f
// The double jump dips for 8 frames, then rises 33.2 by frame 50; holding
// jump floats at its top (the game only checks from frame 30). The parasol
// rises 36.8 by frame 33 and can only grab on the way down.
#define PE_JUMP_PEAK_FRAME 50
#define PE_PARASOL_REACH (36.8f + LEDGE_GRAB_REACH)

#define PE_FLOAT_STAGE_CHANCE 18
#define PE_FLOAT_LOW_CHANCE 6
#define PE_PARASOL_CHANCE 6
#define PE_BOMBER_CHANCE 20
#define PE_FLOAT_AERIAL_CHANCE 3

#define PE_STATE_FLOAT 341

static void Think_Peach(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    RecoveryInfo r = GetRecoveryInfo(ft, PE_GRAB_X, PE_GRAB_Y);
    int state = ft->state_id;
    bool has_float = ft->fighter_var.ft_var1 != 0; // fp+222C, cleared once she floats
    bool jumping = state == ASID_JUMPAERIALF || state == ASID_JUMPAERIALB;
    bool rising = jumping && ft->TM.state_frame < PE_JUMP_PEAK_FRAME;
    // Over the stage and above the ledge: stop floating and land.
    bool over_stage = r.to_ledge.X * r.dir < -10.f && r.to_ledge.Y < 0.f;
    bool parasol_range = OptEnabled(OPT_PEACH_PARASOL)
        && r.to_grab.Y > 2.f && r.to_grab.Y < PE_PARASOL_REACH && r.out < 20.f;
    bool parasol_late = r.to_grab.Y > PE_PARASOL_REACH - 12.f;

    if (ft->flags.hitlag) {
        ResetRecoveryPlans();
        DIIn(ft);

    // FLOATING: holding jump keeps it going for up to 150 frames
    } else if (state == PE_STATE_FLOAT) {
        if (
            OptEnabled(OPT_PEACH_FLOAT_AERIALS) && PlayerInReach(ft, 22.f, 5)
            && OneIn(PE_FLOAT_AERIAL_CHANCE)
        ) {
            FighterData *hmn = Fighter_GetGObj(0)->userdata;
            ft->cpu.held |= PAD_BUTTON_Y;
            Press(ft, PAD_BUTTON_A);
            SetStick(ft, sign(hmn->phys.pos.X - r.pos.X), 0.f);
        } else if (parasol_range && (OneIn(PE_PARASOL_CHANCE) || parasol_late)) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);
        } else if (over_stage) {
            SetStick(ft, r.dir, 0.f);
        } else {
            ft->cpu.held |= PAD_BUTTON_Y;
            SetStick(ft, r.dir, 0.f);
        }

    } else if (IsAirActionable(cpu)) {
        // DOUBLE JUMPING: let it rise, holding jump to float at the top.
        // Down and jump, or the parasol, would cut it short.
        if (jumping && (pe_float_after_jump || rising)) {
            if (pe_float_after_jump)
                ft->cpu.held |= PAD_BUTTON_Y;
            SetStick(ft, r.dir, 0.f);

        // DOUBLE JUMP
        } else if (
            r.can_jump && r.vel.Y < 0.5f
            && (
                r.to_grab.Y > PE_PARASOL_REACH - 14.f
                || (OptEnabled(OPT_PEACH_FLOAT_STAGE) && r.to_grab.Y > -15.f && r.out > 15.f && OneIn(PE_FLOAT_STAGE_CHANCE))
            )
        ) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, r.dir, 0.f);
            pe_float_after_jump = OptEnabled(OPT_PEACH_FLOAT_STAGE) && has_float;

        // FLOAT LOW: down and jump floats on the spot
        } else if (
            OptEnabled(OPT_PEACH_FLOAT_LOW) && has_float && !r.tumble && r.vel.Y < 0.f
            && r.to_grab.Y > -18.f && r.to_grab.Y < 12.f && r.out > 8.f
            && OneIn(PE_FLOAT_LOW_CHANCE)
        ) {
            ft->cpu.held |= PAD_BUTTON_Y; // held, not pressed, with the stick down
            SetStick(ft, 0.f, -1.f);

        // PARASOL
        } else if (
            parasol_range && r.vel.Y < 0.5f && (!r.can_jump || r.to_grab.Y < 25.f)
            && (OneIn(PE_PARASOL_CHANCE) || parasol_late)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // PEACH BOMBER
        } else if (
            OptEnabled(OPT_PEACH_BOMBER) && r.out > 45.f
            && r.to_ledge.Y > -35.f && r.to_ledge.Y < 15.f && OneIn(PE_BOMBER_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, r.dir, 0.f);

        // WAIT
        } else {
            DriftHome(ft, &r, 0.f);
        }

    } else {
        // parasol, bomber, float fall and helpless: steer home
        DriftHome(ft, &r, 0.f);
    }
}

// Luigi ----------------------------------------------------------------

#define LG_GRAB_X 12.f
#define LG_GRAB_Y 8.f
// The up special rises 45.9 straight up by frame 26, so line up first.
#define LG_UPB_REACH (45.9f + LEDGE_GRAB_REACH)
#define LG_UPB_LINEUP 10.f
// A Cyclone mashed 10 times a second can end 12 lower than it started.
#define LG_CYCLONE_SAFE 15.f

#define LG_JUMP_CHANCE 25
#define LG_MISSILE_CHANCE 10
#define LG_CYCLONE_CHANCE 10
#define LG_UPB_CHANCE 6
#define LG_UPB_EARLY_CHANCE 20

#define LG_STATE_MISSILE_START 349
#define LG_STATE_MISSILE_HOLD 350
#define LG_STATE_CYCLONE 358

static void Think_Luigi(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    RecoveryInfo r = GetRecoveryInfo(ft, LG_GRAB_X, LG_GRAB_Y);
    int state = ft->state_id;
    bool cyclone_lifts = ft->fighter_var.ft_var1 == 0; // fp+222C, set when a Cyclone ends its lift
    bool jump = OptEnabled(OPT_LUIGI_JUMP) && r.can_jump;

    if (ft->flags.hitlag) {
        ResetRecoveryPlans();
        DIIn(ft);

    // GREEN MISSILE CHARGE: holding B charges, letting go launches
    } else if (state == LG_STATE_MISSILE_START || state == LG_STATE_MISSILE_HOLD) {
        if (lg_missile_charge > 0) {
            if (state == LG_STATE_MISSILE_HOLD)
                --lg_missile_charge;
            ft->cpu.held |= PAD_BUTTON_B;
        }
        SetStick(ft, r.dir, 0.f);

    // CYCLONE: mash for lift
    } else if (state == LG_STATE_CYCLONE) {
        if (MashB(OptEnabled(OPT_LUIGI_FAST_MASH)))
            Press(ft, PAD_BUTTON_B);
        DriftHome(ft, &r, 0.f);

    } else if (IsAirActionable(cpu)) {
        // JUMP
        if (
            jump && r.vel.Y < 0.5f
            && (
                r.to_grab.Y > LG_UPB_REACH - 8.f
                || (r.to_grab.Y > 5.f && OneIn(LG_JUMP_CHANCE))
                || (r.out > 70.f && r.to_grab.Y > -20.f && OneIn(LG_JUMP_CHANCE))
            )
        ) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, r.dir, 0.f);

        // GREEN MISSILE across a long gap near ledge height
        } else if (
            OptEnabled(OPT_LUIGI_MISSILE) && r.out > 40.f
            && r.to_grab.Y > -30.f && r.to_grab.Y < 25.f && r.vel.Y < 0.5f
            && (OneIn(LG_MISSILE_CHANCE) || (r.out > 90.f && !jump))
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, r.dir, 0.f);
            // longer gaps get a longer charge (a full charge is 90 frames)
            lg_missile_charge = (int)fclamp((r.out - 40.f) * 0.5f, 0.f, 50.f) + HSD_Randi(10);

        // CYCLONE while it still lifts (once, until a Cyclone touches the ground)
        } else if (
            OptEnabled(OPT_LUIGI_CYCLONE) && cyclone_lifts && !jump && r.vel.Y < 0.f && r.out < 60.f
            && (
                (r.to_grab.Y > 12.f && r.to_grab.Y < LG_UPB_REACH - LG_CYCLONE_SAFE && OneIn(LG_CYCLONE_CHANCE))
                || r.to_grab.Y > LG_UPB_REACH - 4.f // too low for the up special alone
            )
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, -1.f);
            mash_timer = 2;

        // UP B to the ledge
        } else if (
            !jump && r.vel.Y < 0.f && r.to_grab.Y > 0.f
            && (
                (r.to_grab.Y < LG_UPB_REACH && fabs(r.out) < LG_UPB_LINEUP && OneIn(LG_UPB_CHANCE))
                || r.to_grab.Y > LG_UPB_REACH - 10.f
            )
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // UP B EARLY onto the stage
        } else if (
            OptEnabled(OPT_LUIGI_UPB_EARLY) && r.to_grab.Y > -20.f && r.to_grab.Y <= 0.f
            && r.out < 15.f && OneIn(LG_UPB_EARLY_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // WAIT
        } else {
            DriftHome(ft, &r, 0.f);
        }

    } else {
        DriftHome(ft, &r, 0.f);
    }
}

// Dr. Mario ------------------------------------------------------------

#define DR_GRAB_X 12.f
#define DR_GRAB_Y 8.f
// The up special rises 40.4 and moves 17.5 forward by frame 24.
#define DR_UPB_REACH (40.4f + LEDGE_GRAB_REACH)
#define DR_UPB_FORWARD 15.f
// A tornado mashed 10 times a second can end 12 lower than it started.
#define DR_TORNADO_SAFE 15.f

#define DR_JUMP_CHANCE 25
#define DR_TORNADO_CHANCE 10
#define DR_CAPE_CHANCE 30
#define DR_UPB_CHANCE 6
#define DR_UPB_EARLY_CHANCE 20

#define DR_STATE_TORNADO 350

static void Think_DrMario(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    RecoveryInfo r = GetRecoveryInfo(ft, DR_GRAB_X, DR_GRAB_Y);
    int state = ft->state_id;
    bool tornado_lifts = ft->fighter_var.ft_var3 == 0; // fp+2234, set when a tornado ends its lift
    bool cape_lifts = ft->fighter_var.ft_var4 == 0;    // fp+2238, set by the first cape each airtime
    bool jump = OptEnabled(OPT_DRMARIO_JUMP) && r.can_jump;
    // how far outside the up special's starting spot
    float upb_out = r.out - DR_UPB_FORWARD;

    if (ft->flags.hitlag) {
        ResetRecoveryPlans();
        DIIn(ft);

    // DR. TORNADO: mash for lift
    } else if (state == DR_STATE_TORNADO) {
        if (MashB(OptEnabled(OPT_DRMARIO_FAST_MASH)))
            Press(ft, PAD_BUTTON_B);
        DriftHome(ft, &r, DR_UPB_FORWARD);

    } else if (IsAirActionable(cpu)) {
        // JUMP
        if (
            jump && r.vel.Y < 0.5f
            && (
                r.to_grab.Y > DR_UPB_REACH - 8.f
                || (r.to_grab.Y > 5.f && OneIn(DR_JUMP_CHANCE))
                || (r.out > 70.f && r.to_grab.Y > -20.f && OneIn(DR_JUMP_CHANCE))
            )
        ) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, r.dir, 0.f);

        // CAPE STALL: the first cape each airtime stops the fall for a moment
        } else if (
            OptEnabled(OPT_DRMARIO_CAPE) && cape_lifts && r.vel.Y < 0.f && r.out > 10.f
            && r.to_grab.Y > -10.f && r.to_grab.Y < DR_UPB_REACH - 15.f && OneIn(DR_CAPE_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, r.dir, 0.f);

        // DR. TORNADO while the first one this airtime still lifts
        } else if (
            OptEnabled(OPT_DRMARIO_TORNADO) && tornado_lifts && !jump && r.vel.Y < 0.f && r.out < 60.f
            && (
                (r.to_grab.Y > 12.f && r.to_grab.Y < DR_UPB_REACH - DR_TORNADO_SAFE && OneIn(DR_TORNADO_CHANCE))
                || r.to_grab.Y > DR_UPB_REACH - 4.f // too low for the up special alone
            )
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, -1.f);
            mash_timer = 1;

        // UP B to the ledge
        } else if (
            !jump && r.vel.Y < 0.f && r.to_grab.Y > 0.f
            && (
                (r.to_grab.Y < DR_UPB_REACH && upb_out > -10.f && upb_out < 8.f && OneIn(DR_UPB_CHANCE))
                || r.to_grab.Y > DR_UPB_REACH - 10.f
            )
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.4f * r.dir, 1.f);

        // UP B EARLY onto the stage
        } else if (
            OptEnabled(OPT_DRMARIO_UPB_EARLY) && r.to_grab.Y > -20.f && r.to_grab.Y <= 0.f
            && r.out < 25.f && OneIn(DR_UPB_EARLY_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.4f * r.dir, 1.f);

        // WAIT
        } else {
            DriftHome(ft, &r, DR_UPB_FORWARD);
        }

    } else {
        DriftHome(ft, &r, DR_UPB_FORWARD);
    }
}

// Ice Climbers ---------------------------------------------------------
//
// Popo is the CPU here; Nana follows him with the game's own AI. Squall
// Hammer and Belay both end helpless, so it's one or the other.

#define IC_GRAB_X 10.f
#define IC_GRAB_Y 6.f
#define IC_JUMP_LATEST 30.f      // the double jump rises about 35
// Belay checks for Nana within 60 on its fifth frame; ask for a little less.
#define IC_BELAY_NANA_RANGE 45.f
#define IC_BELAY_REACH 60.f      // Nana is thrown up and pulls Popo after her
#define IC_BELAY_SOLO_REACH 22.f // alone: 1.8 up against 0.1 gravity, about 16
// Squall Hammer with Nana lifts 45 even at 7 presses a second; alone about 10.
#define IC_SQUALL_SOLO_REACH 18.f

#define IC_JUMP_CHANCE 20
#define IC_BELAY_CHANCE 6
#define IC_SQUALL_CHANCE 10

#define IC_STATE_SQUALL_FIRST 345
#define IC_STATE_SQUALL_LAST 346

static bool NanaCanHelp(FighterData *popo) {
    GOBJ *nana = Fighter_GetSubcharGObj(popo->ply, 1);
    if (!nana)
        return false;
    FighterData *nn = nana->userdata;
    float dx = nn->phys.pos.X - popo->phys.pos.X;
    float dy = nn->phys.pos.Y - popo->phys.pos.Y;
    return !nn->flags.dead && !nn->flags.sleep && !nn->flags.hitlag && !nn->flags.hitstun
        && dx * dx + dy * dy < IC_BELAY_NANA_RANGE * IC_BELAY_NANA_RANGE;
}

static void Think_IceClimbers(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    RecoveryInfo r = GetRecoveryInfo(ft, IC_GRAB_X, IC_GRAB_Y);
    int state = ft->state_id;
    bool nana = NanaCanHelp(ft);
    bool belay = OptEnabled(OPT_ICS_BELAY);
    bool squall = OptEnabled(OPT_ICS_SQUALL);

    if (ft->flags.hitlag) {
        ResetRecoveryPlans();
        DIIn(ft);

    // SQUALL HAMMER: mash for lift
    } else if (IC_STATE_SQUALL_FIRST <= state && state <= IC_STATE_SQUALL_LAST) {
        if (MashB(OptEnabled(OPT_ICS_FAST_MASH)))
            Press(ft, PAD_BUTTON_B);
        SetStick(ft, r.dir, 0.f);

    } else if (IsAirActionable(cpu)) {
        // JUMP
        if (
            r.can_jump && r.vel.Y < 0.5f
            && (
                r.to_grab.Y > IC_JUMP_LATEST
                || (r.to_grab.Y > 5.f && OneIn(IC_JUMP_CHANCE))
                || (r.out > 60.f && r.to_grab.Y > -20.f && OneIn(IC_JUMP_CHANCE))
            )
        ) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, r.dir, 0.f);

        // BELAY while Nana is close
        } else if (
            belay && nana && !r.can_jump && r.vel.Y < 0.5f && r.out < 30.f
            && r.to_grab.Y > 5.f && r.to_grab.Y < IC_BELAY_REACH
            && (OneIn(IC_BELAY_CHANCE) || r.to_grab.Y > IC_BELAY_REACH - 15.f)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // SQUALL HAMMER across a gap, or for height when Belay can't help
        } else if (
            squall && !r.can_jump && r.vel.Y < 0.5f && r.to_grab.Y > -20.f
            && (
                (r.out > 35.f && OneIn(IC_SQUALL_CHANCE))
                || (r.to_grab.Y > 5.f && (!nana || !belay) && (OneIn(IC_SQUALL_CHANCE) || r.to_grab.Y > IC_SQUALL_SOLO_REACH - 8.f))
                || r.to_grab.Y > IC_BELAY_REACH - 10.f
            )
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, r.dir, 0.f);
            mash_timer = 1;

        // SOLO BELAY: little height, the last thing left
        } else if (
            belay && !squall && !nana && !r.can_jump && r.vel.Y < 0.f && r.out < 15.f
            && r.to_grab.Y > 3.f && r.to_grab.Y < IC_BELAY_SOLO_REACH
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // WAIT
        } else {
            DriftHome(ft, &r, 0.f);
        }

    } else {
        DriftHome(ft, &r, 0.f);
    }
}

// Jigglypuff -----------------------------------------------------------
//
// Five midair jumps (each one at least 28 frames after the last), the
// fastest air drift in the game, and Pound to cover distance. Each attempt
// picks a height to hold: above the ledge to drift onto the stage, or level
// with the ledge to grab it.

#define PR_GRAB_X 10.f
#define PR_GRAB_Y 5.f
#define PR_HIGH_ABOVE 25.f
#define PR_AERIAL_SIZE 20.f

#define PR_JUMP_CHANCE 6
#define PR_POUND_CHANCE 25
#define PR_AERIAL_CHANCE 3

#define PR_STATE_JUMP_FIRST 341
#define PR_STATE_JUMP_LAST 345
#define PR_STATE_POUND 364

static void Think_Puff(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    RecoveryInfo r = GetRecoveryInfo(ft, PR_GRAB_X, PR_GRAB_Y);
    int state = ft->state_id;
    bool actionable = IsAirActionable(cpu)
        || (PR_STATE_JUMP_FIRST <= state && state <= PR_STATE_JUMP_LAST);

    if (pr_high < 0) {
        bool high = OptEnabled(OPT_PUFF_JUMP_HIGH), low = OptEnabled(OPT_PUFF_JUMP_LOW);
        pr_high = high && (!low || HSD_Randi(2));
    }
    float hold_y = pr_high ? r.ledge.Y + PR_HIGH_ABOVE : r.ledge.Y - PR_GRAB_Y;

    if (ft->flags.hitlag) {
        ResetRecoveryPlans();
        DIIn(ft);

    // POUND: the angle, up to 20 degrees up, is read on frame 12
    } else if (state == PR_STATE_POUND) {
        SetStick(ft, r.dir, 0.5f);

    } else if (actionable) {
        FighterData *hmn = Fighter_GetGObj(0)->userdata;
        float facing = ft->facing_direction;

        // AERIAL: back air at a player behind, forward or neutral air in front
        if (
            OptEnabled(OPT_PUFF_AERIALS)
            && PlayerInReach(ft, PR_AERIAL_SIZE, 6) && OneIn(PR_AERIAL_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_A);
            if ((hmn->phys.pos.X - r.pos.X) * facing < 0.f)
                SetStick(ft, -facing, 0.f);
            else if (OneIn(2))
                SetStick(ft, facing, 0.f);

        // JUMP back up to the planned height
        } else if (
            r.can_jump && r.vel.Y < -0.3f && r.pos.Y < hold_y
            && (OneIn(PR_JUMP_CHANCE) || r.to_grab.Y > 15.f)
        ) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, r.dir, 0.f);

        // POUND across a gap
        } else if (
            OptEnabled(OPT_PUFF_POUND) && r.out > 25.f
            && r.to_ledge.Y > -40.f && r.to_ledge.Y < 15.f && OneIn(PR_POUND_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, r.dir, 0.5f);

        // WAIT
        } else {
            DriftHome(ft, &r, 0.f);
        }

    } else {
        DriftHome(ft, &r, 0.f);
    }
}

// Samus ----------------------------------------------------------------

#define SS_GRAB_X 14.f
#define SS_GRAB_Y 8.f
// Screw Attack starts at 2.5 up against 0.066 gravity: about 48.
#define SS_SCREW_REACH (48.f + LEDGE_GRAB_REACH)
// The grapple head leaves her hand (9 up, 9 forward) about 8 frames after
// the press and flies straight out, reaching about 40, and only latches onto
// a wall. A miss is helpless, so aim it at the wall just under the ledge: the
// reel then ends with the ledge in her grab box (0 to 11 under it). Too close
// and the hand is already past the wall, which cancels the grapple.
#define SS_GRAPPLE_FRAMES 8.f
#define SS_GRAPPLE_HAND_Y 9.f
#define SS_GRAPPLE_MIN_X 13.f
#define SS_GRAPPLE_MAX_X 40.f
#define SS_GRAPPLE_MIN_DEPTH 2.f
#define SS_GRAPPLE_MAX_DEPTH 10.f
// Once latched, A reels her in; she hangs (and then tumbles) until it's pressed.
#define SS_GRAPPLE_REEL_FRAME 4

#define SS_JUMP_CHANCE 30
#define SS_BOMB_CHANCE 8
#define SS_GRAPPLE_CHANCE 6
#define SS_SCREW_CHANCE 10
#define SS_SCREW_HIGH_CHANCE 25

#define SS_STATE_BOMB_JUMP 342
#define SS_STATE_BOMB 356
#define SS_STATE_GRAPPLE 357
#define SS_STATE_GRAPPLE_HIT 358

static void Think_Samus(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    RecoveryInfo r = GetRecoveryInfo(ft, SS_GRAB_X, SS_GRAB_Y);
    int state = ft->state_id;
    bool jump = OptEnabled(OPT_SAMUS_JUMP) && r.can_jump;
    // A bomb that goes off on Samus launches her up, still actionable.
    bool actionable = IsAirActionable(cpu) || state == SS_STATE_BOMB_JUMP;
    // Where the grapple head would leave from: outside the ledge, and under it.
    float grapple_out = (r.to_ledge.X - SS_GRAPPLE_FRAMES * (r.vel.X + ft->phys.kb_vel.X)) * r.dir;
    float grapple_depth = r.to_ledge.Y - SS_GRAPPLE_HAND_Y - SS_GRAPPLE_FRAMES * (r.vel.Y + ft->phys.kb_vel.Y);

    if (ft->flags.hitlag) {
        ResetRecoveryPlans();
        DIIn(ft);

    // LAYING A BOMB: the drift is fast. Travel when far away; when close,
    // stay over the bomb so it launches her.
    } else if (state == SS_STATE_BOMB) {
        SetStick(ft, r.out > 50.f ? r.dir : 0.f, 0.f);

    } else if (actionable) {
        // JUMP
        if (
            jump && r.vel.Y < 0.5f
            && (r.to_grab.Y > SS_SCREW_REACH - 10.f || (r.to_grab.Y > 5.f && OneIn(SS_JUMP_CHANCE)))
        ) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, r.dir, 0.f);

        // GRAPPLE: Z tethers to the wall under a ledge in front of her.
        // Without a jump, take the last chance before it would hit too deep.
        } else if (
            OptEnabled(OPT_SAMUS_GRAPPLE) && !ft->flags.used_tether
            && ft->facing_direction * r.dir > 0.f && r.vel.Y < 0.f
            && grapple_out > SS_GRAPPLE_MIN_X && grapple_out < SS_GRAPPLE_MAX_X
            && grapple_depth > SS_GRAPPLE_MIN_DEPTH && grapple_depth < SS_GRAPPLE_MAX_DEPTH
            && (OneIn(SS_GRAPPLE_CHANCE) || (!jump && grapple_depth > SS_GRAPPLE_MAX_DEPTH - 3.f))
        ) {
            Press(ft, PAD_TRIGGER_Z);

        // BOMBS to stall and drift in
        } else if (
            OptEnabled(OPT_SAMUS_BOMBS) && r.out > 30.f && r.to_grab.Y < 30.f
            && r.vel.Y < 0.3f && OneIn(SS_BOMB_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, -1.f);

        // SCREW ATTACK to the ledge
        } else if (
            OptEnabled(OPT_SAMUS_SCREW) && !jump && r.vel.Y < 0.f && r.to_grab.Y > 0.f
            && (
                (r.to_grab.Y < SS_SCREW_REACH && r.out < 25.f && OneIn(SS_SCREW_CHANCE))
                || r.to_grab.Y > SS_SCREW_REACH - 10.f
            )
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // SCREW ATTACK onto the stage
        } else if (
            OptEnabled(OPT_SAMUS_SCREW_HIGH) && r.to_grab.Y > -20.f && r.to_grab.Y <= 0.f
            && r.out < 20.f && OneIn(SS_SCREW_HIGH_CHANCE)
        ) {
            Press(ft, PAD_BUTTON_B);
            SetStick(ft, 0.f, 1.f);

        // WAIT
        } else {
            DriftHome(ft, &r, 0.f);
        }

    } else if (state == SS_STATE_GRAPPLE_HIT) {
        // press A to reel in; the reel ends on the ledge or in a hop up the wall
        if (ft->TM.state_frame >= SS_GRAPPLE_REEL_FRAME)
            Press(ft, PAD_BUTTON_A);

    } else if (state == SS_STATE_GRAPPLE) {
        // leave the stick alone while the grapple flies out

    } else {
        // screw attack and helpless falls
        DriftHome(ft, &r, 0.f);
    }
}


