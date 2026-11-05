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
static void Press(FighterData *ft, int buttons);
static void SetStick(FighterData *ft, float x, float y);

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

/* Edgeguard saves the Firefox, Double Jump, Fast Fall and Instant rows in their
 * pre-T9 bits (Instant has the old Illusion row's bit 4, Fast Fall bit 5), the
 * other side-B rows in a second mask, and Firefox To Ledge on its own. */
enum options_spacies {
    OPT_SPACIES_FF_LOW,
    OPT_SPACIES_FF_MID,
    OPT_SPACIES_FF_HIGH,
    OPT_SPACIES_FF_LEDGE,
    OPT_SPACIES_JUMP,
    OPT_SPACIES_FASTFALL,
    OPT_SPACIES_SIDEB_INSTANT,
    OPT_SPACIES_SIDEB_SWEETSPOT,
    OPT_SPACIES_SIDEB_TO_LEDGE,
    OPT_SPACIES_SIDEB_TO_STAGE,
    OPT_SPACIES_SIDEB_TO_PLAT,
    OPT_SPACIES_SIDEB_SHORTEN,

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
    OPT_LUIGI_UPB_SWEETSPOT,
    OPT_LUIGI_BACKFLIP,
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
        .desc = {"Allow Fox to aim his up special at the ledge",
                 "from anywhere in range. Up close, it flies",
                 "past the ledge."},
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
        .name = "Firefox To Ledge",
        .desc = {"Allow Fox to wait for the distance where his",
                 "up special ends right at the ledge, and grab",
                 "it. He double jumps first if he needs to."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Fox to double jump.",
                 "The Illusion rows jump on their own."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Fall",
        .desc = {"Allow Fox to fast fall."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Illusion Instant",
        .desc = {"Allow Fox to side special as soon as",
                 "hitstun ends, without jumping."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Illusion Ledge Sweetspot",
        .desc = {"Allow Fox to jump up and side special",
                 "right into the ledge, grabbing it the moment",
                 "the dash ends."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Illusion To Ledge",
        .desc = {"Allow Fox to jump up and side special",
                 "short of the ledge, then drift into it,",
                 "lingering at the edge for a few frames."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Illusion To Main Stage",
        .desc = {"Allow Fox to jump up and side special right",
                 "above the ledge, onto the main stage and",
                 "through you if you're near the ledge."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Illusion To Side Plat",
        .desc = {"Allow Fox to jump high and side special onto",
                 "the side platform. Without one, like on Final",
                 "Destination, he goes to the stage instead."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Illusion Shortest Shorten",
        .desc = {"Allow Fox to jump up close to the ledge and",
                 "side special with the earliest shorten,",
                 "grabbing the ledge out of it."},
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
        .desc = {"Allow Falco to aim his up special at the ledge",
                 "from anywhere in range. Up close, it flies",
                 "past the ledge."},
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
        .name = "Firebird To Ledge",
        .desc = {"Allow Falco to wait for the distance where his",
                 "up special ends right at the ledge, and grab",
                 "it. He double jumps first if he needs to."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Double Jump",
        .desc = {"Allow Falco to double jump.",
                 "The Phantasm rows jump on their own."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Fall",
        .desc = {"Allow Falco to fast fall."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Phantasm Instant",
        .desc = {"Allow Falco to side special as soon as",
                 "hitstun ends, without jumping."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Phantasm Ledge Sweetspot",
        .desc = {"Allow Falco to jump up and side special",
                 "right into the ledge, grabbing it the moment",
                 "the dash ends."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Phantasm To Ledge",
        .desc = {"Allow Falco to jump up and side special",
                 "short of the ledge, then drift into it,",
                 "lingering at the edge for a few frames."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Phantasm To Main Stage",
        .desc = {"Allow Falco to jump up and side special right",
                 "above the ledge, onto the main stage and",
                 "through you if you're near the ledge."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Phantasm To Side Plat",
        .desc = {"Allow Falco to jump high and side special onto",
                 "the side platform. Without one, like on Final",
                 "Destination, he goes to the stage instead."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Phantasm Shortest Shorten",
        .desc = {"Allow Falco to jump up close to the ledge and",
                 "side special with the earliest shorten,",
                 "grabbing the ledge out of it."},
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
                 "height and distance before he up specials.",
                 "It lifts once each attempt."},
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
        .name = "Up B Sweetspot",
        .desc = {"Allow Luigi to line up by the ledge and up",
                 "special from so low that its top only just",
                 "reaches the ledge."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Backflip Jump Sweetspot",
        .desc = {"Allow Luigi to double jump backward, drift",
                 "back in and grab the ledge on the way down,",
                 "with no up special."},
    },
    {
        .kind = OPTKIND_TOGGLE,
        .name = "Fast Mashing",
        .desc = {"Mash B about 20 times a second instead",
                 "of about 15, for more Cyclone height."},
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
#define SP_FASTFALL_CHANCE 8

#define SP_FIREFOX_DISTANCE 80.f
#define SP_FIREBIRD_DISTANCE 70.f

#define SP_SWEETSPOT_OFFSET_X 16
#define SP_SWEETSPOT_OFFSET_Y 5

// Side-B (Illusion / Phantasm). From the decompiled move (ftfoxspecials.c) and
// the NTSC 1.02 special attributes, which Fox and Falco share but for the dash:
// - Startup: Y speed 0, X speed divided by 1.5 (0x28), then 0.05 (0x2C) slower
//   a frame. No fall for 15 frames (0x24), then 1/60 (0x30) a frame. Fox's
//   runs 19 physics frames (a 1/6 drop), Falco's 15 (none).
// - Dash: three steps of 78/4 (Fox) or 60/4 (Falco) times the model scale, at
//   a fixed height. A fresh B on its first two frames ends it before the first
//   step (see sweetspot.c).
// - Endlag: 2.0 (0x3C) forward, 0.07 (0x40) slower a frame. No fall for 5
//   frames (0x44), then 0.08 (0x48) a frame. Then special fall.
// - The grab box (mpcoll.c) spans cliffgrab_y_offset +/- half of cliffgrab_height
//   above the feet (8.5 to 17.5) and reaches cliffgrab_width past the front of
//   the ECB, whose bottom must still be outside the ledge and under it. Holding
//   down refuses the grab (ftCliffCommon_80081298).
// - The game only looks for a ledge on a frame the fighter moved down (mpcoll.c).
//   The dash and the endlag's first five frames are level, so the dash never
//   grabs: the earliest grab out of one is the endlag's sixth frame. And all
//   three steps always run: one that reaches the box keeps going, into the wall
//   if the ECB's front point finds one, and under the stage if it doesn't.
#define SP_FOX_STARTUP_FRAMES 19
#define SP_FALCO_STARTUP_FRAMES 15
#define SP_ENDLAG_FRAMES 40
#define SP_ATTR(ft, offset) (((float *)(ft)->special_attributes)[(offset) / sizeof(float)])

#define ASID_SPECIALAIRS 0x15F // the dash

// The ECB's front point in the dash, posed from the ISO's skeletons and
// SpecialAirS: how high over the feet and how far ahead of them. It's what meets
// the wall, and at that height Battlefield's wall is only 6 tall.
#define SP_FOX_DASH_ECB_Y 5.5f
#define SP_FOX_DASH_ECB_FRONT 3.f
#define SP_FALCO_DASH_ECB_Y 6.05f
#define SP_FALCO_DASH_ECB_FRONT 3.8f
// The game wants the feet outside the ledge to grab it. A wall that stops them
// closer than this doesn't count: it slopes inward, and the fall slides them
// under. With no wall, a dash ending this close is under the ledge before it
// can grab it.
#define SP_WALL_MARGIN 0.5f
#define SP_LIP_MARGIN 2.f

// The endlag's first grab can come on frame 6. A sweetspot grabs right there or
// a frame later. To Ledge's endlag drifts in for longer: 9 to 14 frames where it
// aims, and never fewer than 8 when it side-Bs.
#define SP_LINGER_FEWEST 8
#define SP_LINGER_AIM_MIN 9
#define SP_LINGER_AIM_MAX 14
#define SP_ENDLAG_FIRST_GRAB 6

// Ledge Sweetspot's dash height. High enough for the front point to find the
// wall on every legal stage, or, from the band's low end, for the simulation to
// see the dash stop short of a wall that isn't there.
#define SP_SWEETSPOT_LO -13.5f
#define SP_SWEETSPOT_HI -6.5f

enum spacie_plan {
    SP_PLAN_NONE,        // not chosen yet this attempt
    SP_PLAN_UPB,         // the up special logic, also used with nothing checked
    SP_PLAN_FF_LEDGE,    // Firefox To Ledge, and the last resort: a side-B from
                         // too low only goes under the stage
    // one per side-B row, in row order
    SP_PLAN_INSTANT,
    SP_PLAN_SWEETSPOT,
    SP_PLAN_TO_LEDGE,
    SP_PLAN_TO_STAGE,
    SP_PLAN_TO_PLAT,
    SP_PLAN_SHORTEN,
};

typedef struct SpaciePlan {
    int kind;
    float lo, hi;    // side-B with the feet this high above the ledge (for
                     // To Main Stage, the height of the dash itself)
    float aim;       // and this far outside it when the dash starts
    float jump_at;   // double jump on the way down past this height
    bool shorten;    // B again on the dash's first frame
} SpaciePlan;

static SpaciePlan sp_plan = { SP_PLAN_NONE };

// Positions are relative to the CPU's ledge: `out` is how far outside it
// (negative over the stage), `h` how far above it. X speeds are toward the stage.
typedef struct SpacieGeo {
    Vec2 ledge;
    float dir;            // 1 when the stage is to the right
    float out, h;
    float vx, vy;         // own speed
    float kbx, kby;       // knockback still carrying the CPU
    float reach;          // how far past the feet the ledge-grab box reaches
    bool has_plat;
    float plat_h;         // the side platform's height
    float plat_near;      // and the outs of its ends
    float plat_far;
} SpacieGeo;

enum sideb_result {
    SIDEB_NOTHING = -1,
    SIDEB_MISS,
    SIDEB_GRAB, // only from SideBStep; the simulation names the part
    SIDEB_GRAB_STARTUP,
    SIDEB_GRAB_DASH,
    SIDEB_GRAB_ENDLAG,
    SIDEB_LAND_STAGE,
    SIDEB_LAND_PLAT,
};

typedef struct SideBSim {
    const SpacieGeo *g;
    float out, h, kbx, kby;
    float box_lo, box_hi, bottom, decay;
    float stop; // the wall: it can't get closer than this. -1: none, under the lip
} SideBSim;

static float SpacieRand(float lo, float hi) { return lo + (hi - lo) * HSD_Randf(); }

static int SpacieStartupFrames(FighterData *ft) {
    return ft->kind == FTKIND_FOX ? SP_FOX_STARTUP_FRAMES : SP_FALCO_STARTUP_FRAMES;
}

static float SpacieDashStep(FighterData *ft) {
    float step = (ft->kind == FTKIND_FOX ? 78.f : 60.f) / 4.f;
    return step * ft->attr.model_scaling * ft->scale.Y;
}

static float SpacieSlow(float speed, float by) {
    return speed > 0.f ? fmax(speed - by, 0.f) : fmin(speed + by, 0.f);
}

// How far the startup carries the CPU toward the stage from X speed vx.
static float SpacieStartupDrift(FighterData *ft, float vx) {
    float drift = 0.f;
    vx /= SP_ATTR(ft, 0x28);
    for (int i = SpacieStartupFrames(ft); i > 0; --i) {
        vx = SpacieSlow(vx, SP_ATTR(ft, 0x2C));
        drift += vx;
    }
    return drift;
}

// How far the endlag carries the CPU in its first `frames` frames.
static float SpacieEndlagDrift(FighterData *ft, int frames) {
    float speed = SP_ATTR(ft, 0x3C), drift = 0.f;
    while (frames-- > 0 && (speed -= SP_ATTR(ft, 0x40)) > 0.f)
        drift += speed;
    return drift;
}

// Endlag frames until the CPU has fallen `drop`.
static int SpacieEndlagFallFrames(FighterData *ft, float drop) {
    float vy = 0.f, fallen = 0.f;
    int delay = SP_ATTR(ft, 0x44), n = 0;
    while (fallen < drop && n < SP_ENDLAG_FRAMES) {
        if (delay > 0)
            --delay;
        else
            vy += SP_ATTR(ft, 0x48);
        fallen += vy;
        ++n;
    }
    return n;
}

// The double jump's rise, and how many frames it rises for. Gravity applies
// on its first frame (ftCo_JumpAerial_Enter_Basic).
static float SpacieJumpRise(FighterData *ft, int *frames) {
    float speed = ft->attr.jump_v_initial_velocity * ft->attr.air_jump_v_multiplier;
    float rise = 0.f;
    int n = 0;
    while ((speed -= ft->attr.gravity) > 0.f && n < 120) {
        rise += speed;
        ++n;
    }
    *frames = n;
    return rise;
}

// Frames for the feet to fall from h to `to`, from Y speed vy.
static int SpacieFallFrames(FighterData *ft, float h, float vy, float to) {
    int n = 0;
    while (h > to && n < 300) {
        vy = fmax(vy - ft->attr.gravity, -ft->attr.terminal_velocity);
        h += vy;
        ++n;
    }
    return n;
}

// The one-way platform on this ledge's half of the stage nearest the ledge,
// like Battlefield's side platforms. Live vertices, so it follows Fountain of
// Dreams' platforms; Final Destination has none.
static void SpacieFindPlatform(SpacieGeo *g) {
    g->has_plat = false;
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts)
        return;
    for (int i = 0; i < stage->line_num; ++i) {
        CollLineDesc *line = lines[i].desc;
        // native MapLine.lo_flags LINE_FLAG_PLATFORM is 0x0100
        if (!line || !line->is_floor || line->disabled || !(((u8 *)line)[0xE] & 1)
            || !GrColl_CheckIfLineEnabled(i)
            || line->vert_prev < 0 || line->vert_next < 0
            || line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num)
            continue;
        Vec2 a = verts[line->vert_prev].pos_curr;
        Vec2 b = verts[line->vert_next].pos_curr;
        float out_a = (g->ledge.X - a.X) * g->dir;
        float out_b = (g->ledge.X - b.X) * g->dir;
        float near = fmax(out_a, out_b), far = fmin(out_a, out_b);
        float h = (a.Y + b.Y) * 0.5f - g->ledge.Y;
        bool this_half = (a.X + b.X) * 0.5f * g->dir < -10.f;
        if (!this_half || near - far < 10.f || h < 5.f || h > 80.f)
            continue;
        if (!g->has_plat || near > g->plat_near) {
            g->has_plat = true;
            g->plat_h = h;
            g->plat_near = near;
            g->plat_far = far;
        }
    }
}

static SpacieGeo SpacieGetGeo(FighterData *ft) {
    SpacieGeo g;
    float x = ft->phys.pos.X;
    g.dir = x > 0.f ? -1.f : 1.f;
    g.ledge = ledge_positions[x > 0.f];
    g.out = (g.ledge.X - x) * g.dir;
    g.h = ft->phys.pos.Y - g.ledge.Y;
    g.vx = ft->phys.self_vel.X * g.dir;
    g.vy = ft->phys.self_vel.Y;
    g.kbx = ft->phys.kb_vel.X * g.dir;
    g.kby = ft->phys.kb_vel.Y;
    CollData *coll = &ft->coll_data;
    float front = g.dir > 0.f ? coll->ecbCurr_right.X : -coll->ecbCurr_left.X;
    g.reach = front + coll->cliffgrab_width;
    SpacieFindPlatform(&g);
    return g;
}

// How far inland of the ledge the side of the stage is at height `depth` (under
// the ledge, so negative). Walls and undersides both count: under Battlefield's
// lip that's far inland, and a dash there goes under the stage.
static float SpacieWallInset(const SpacieGeo *g, float depth) {
    CollDataStage *stage = *stc_colldata;
    CollLine *lines = *stc_collline;
    CollVert *verts = *stc_collvert;
    if (!stage || !lines || !verts)
        return 0.f;
    float inset = 1000.f;
    for (int i = 0; i < stage->line_num; ++i) {
        CollLineDesc *line = lines[i].desc;
        // skip one-way platforms, as SpacieFindPlatform finds them
        if (!line || line->is_floor || line->disabled || (((u8 *)line)[0xE] & 1)
            || !GrColl_CheckIfLineEnabled(i)
            || line->vert_prev < 0 || line->vert_next < 0
            || line->vert_prev >= stage->vert_num || line->vert_next >= stage->vert_num)
            continue;
        Vec2 a = verts[line->vert_prev].pos_curr;
        Vec2 b = verts[line->vert_next].pos_curr;
        float ya = a.Y - g->ledge.Y, yb = b.Y - g->ledge.Y;
        if (fmin(ya, yb) > depth || fmax(ya, yb) < depth)
            continue;
        float t = ya == yb ? 0.f : (depth - ya) / (yb - ya);
        float in = (a.X + (b.X - a.X) * t - g->ledge.X) * g->dir;
        if (in > -5.f && in < inset)
            inset = in;
    }
    return inset;
}

static void SpacieKnockbackStep(float *kbx, float *kby, float decay) {
    float kb = sqrtf(*kbx * *kbx + *kby * *kby);
    if (kb <= decay) {
        *kbx = *kby = 0.f;
    } else {
        *kbx -= decay * *kbx / kb;
        *kby -= decay * *kby / kb;
    }
}

// Move one frame by (vx, vy) plus the knockback left, which decays like the
// game's, and say what the frame ended in.
static int SideBStep(SideBSim *s, float vx, float vy) {
    float out0 = s->out, h0 = s->h;
    SpacieKnockbackStep(&s->kbx, &s->kby, s->decay);
    s->out = fmax(s->out - vx - s->kbx, s->stop);
    s->h += vy + s->kby;

    // Only a frame that moved down can grab. The box covers that frame's sweep,
    // so a fast step can't skip the ledge.
    if (s->h < h0 && s->out > 0.f && fmin(s->out, out0) <= s->g->reach
        && fmin(s->h, h0) + s->box_lo <= 0.f && 0.f <= fmax(s->h, h0) + s->box_hi
        && s->h + s->bottom < 0.f)
        return SIDEB_GRAB;

    const SpacieGeo *g = s->g;
    if (g->has_plat && h0 + s->bottom >= g->plat_h && s->h + s->bottom < g->plat_h
        && g->plat_far <= s->out && s->out <= g->plat_near)
        return SIDEB_LAND_PLAT;

    // Inside the ledge and under it: onto the stage from above, or into the wall.
    if (s->out <= 0.f && s->h + s->bottom < 0.f)
        return h0 + s->bottom >= 0.f ? SIDEB_LAND_STAGE : SIDEB_MISS;
    // No wall here, so nothing stops it going under the stage.
    if (s->stop == -1.f && s->out < SP_LIP_MARGIN && s->h + s->bottom < 0.f)
        return SIDEB_MISS;
    return SIDEB_NOTHING;
}

// Where a side-B started this frame ends: a ledge grab (in the startup, dash
// or endlag, with the endlag frame), a landing, or a miss, and the height the
// dash runs at. `shorten` presses B again on the dash's first frame. The press
// takes effect this frame: Event_Think runs before Fighter_procInput.
static int SimulateSideB(FighterData *ft, const SpacieGeo *g, bool shorten, int *frame, float *dash_h) {
    CollData *coll = &ft->coll_data;
    SideBSim s = {
        .g = g, .out = g->out, .h = g->h, .kbx = g->kbx, .kby = g->kby,
        .box_lo = coll->cliffgrab_y_offset - coll->cliffgrab_height * 0.5f,
        .box_hi = coll->cliffgrab_y_offset + coll->cliffgrab_height * 0.5f,
        .bottom = coll->ecbCurr_bot.Y,
        .decay = (*stc_ftcommon)->kb_frameDecay,
        .stop = -1000.f,
    };
    float terminal = ft->attr.terminal_velocity;
    int result;
    *frame = 0;
    *dash_h = g->h;

    float vx = g->vx / SP_ATTR(ft, 0x28), vy = 0.f;
    int delay = SP_ATTR(ft, 0x24);
    for (int i = SpacieStartupFrames(ft); i > 0; --i) {
        if (delay > 0)
            --delay;
        else
            vy = fmax(vy - SP_ATTR(ft, 0x30), -terminal);
        vx = SpacieSlow(vx, SP_ATTR(ft, 0x2C));
        if ((result = SideBStep(&s, vx, vy)) != SIDEB_NOTHING)
            return result == SIDEB_GRAB ? SIDEB_GRAB_STARTUP : result;
    }

    *dash_h = s.h;

    // The wall at the height of the ECB's front point. Over the ledge there's
    // no wall to meet.
    bool fox = ft->kind == FTKIND_FOX;
    float tip = s.h + (fox ? SP_FOX_DASH_ECB_Y : SP_FALCO_DASH_ECB_Y);
    if (tip < 0.f)
        s.stop = (fox ? SP_FOX_DASH_ECB_FRONT : SP_FALCO_DASH_ECB_FRONT) - SpacieWallInset(g, tip);
    if (tip < 0.f && s.stop < SP_WALL_MARGIN)
        s.stop = -1.f;

    // A frame before the steps, or only that frame when shortened.
    float step = SpacieDashStep(ft);
    for (int i = 0; i <= (shorten ? 0 : 3); ++i) {
        if ((result = SideBStep(&s, i ? step : 0.f, 0.f)) != SIDEB_NOTHING)
            return result == SIDEB_GRAB ? SIDEB_GRAB_DASH : result;
    }

    vx = SP_ATTR(ft, 0x3C);
    vy = 0.f;
    delay = SP_ATTR(ft, 0x44);
    for (int i = 1; i <= SP_ENDLAG_FRAMES; ++i) {
        if (delay > 0)
            --delay;
        else
            vy = fmax(vy - SP_ATTR(ft, 0x48), -terminal);
        vx = SpacieSlow(vx, SP_ATTR(ft, 0x40));
        if ((result = SideBStep(&s, vx, vy)) != SIDEB_NOTHING) {
            *frame = i;
            return result == SIDEB_GRAB ? SIDEB_GRAB_ENDLAG : result;
        }
    }
    return SIDEB_MISS;
}

// Whether a side-B started this frame ends the way the plan wants.
static bool SpacieSideBWorks(FighterData *ft, const SpacieGeo *g, const SpaciePlan *p) {
    int frame;
    float dash_h;
    int result = SimulateSideB(ft, g, p->shorten, &frame, &dash_h);
    switch (p->kind) {
        case SP_PLAN_SWEETSPOT: return result == SIDEB_GRAB_ENDLAG && frame < SP_LINGER_FEWEST;
        case SP_PLAN_TO_LEDGE: return result == SIDEB_GRAB_ENDLAG && frame >= SP_LINGER_FEWEST;
        case SP_PLAN_SHORTEN: return result == SIDEB_GRAB_ENDLAG;
        case SP_PLAN_TO_STAGE:
            return result == SIDEB_LAND_STAGE && p->lo <= dash_h && dash_h <= p->hi;
        case SP_PLAN_TO_PLAT: return result == SIDEB_LAND_PLAT;
    }
    return false;
}

// Out at the dash's start for a side-B started from here, past where the
// knockback left will still carry the CPU.
static float SpacieDashStartOut(FighterData *ft, const SpacieGeo *g) {
    float kb = sqrtf(g->kbx * g->kbx + g->kby * g->kby);
    float carry = g->kbx * kb / (2.f * (*stc_ftcommon)->kb_frameDecay);
    return g->out - SpacieStartupDrift(ft, g->vx) - carry;
}

// The height the knockback left will still add.
static float SpacieCarryUp(const SpacieGeo *g) {
    if (g->kby <= 0.f)
        return 0.f;
    float kb = sqrtf(g->kbx * g->kbx + g->kby * g->kby);
    return g->kby * kb / (2.f * (*stc_ftcommon)->kb_frameDecay);
}

// How far the CPU drifts in `frames` frames holding one way from X speed v
// that way (ftCommon_CalcSelfAccel_DriftFrom).
static float SpacieDriftReach(FighterData *ft, float v, int frames) {
    float accel = ft->attr.aerial_drift_stick_mult + ft->attr.aerial_drift_base;
    float max = ft->attr.aerial_drift_max, drift = 0.f;
    while (frames-- > 0) {
        v = v + accel > max ? fmax(v - ft->attr.aerial_friction, max) : v + accel;
        drift += v;
    }
    return drift;
}

// Whether the CPU can still drift to the plan's aim by its last pass through
// the height band, saving the double jump for as late as it still reaches.
static bool SpacieCanReach(FighterData *ft, const SpacieGeo *g, const SpaciePlan *p) {
    if (p->kind == SP_PLAN_INSTANT)
        return true;
    bool can_jump = ft->jump.jumps_used < ft->attr.max_jumps;
    int rise_frames;
    float rise = SpacieJumpRise(ft, &rise_frames);
    float latest_jump = p->lo - rise + 3.f;
    float vy = g->vy + g->kby;
    // The top of a rise already underway; jumping there goes highest.
    float peak = g->h + SpacieCarryUp(g) + (vy > 0.f ? vy * vy / (2.f * ft->attr.gravity) : 0.f);
    int frames;
    if (can_jump) {
        if (peak + rise < p->lo)
            return false;
        frames = SpacieFallFrames(ft, g->h, vy, latest_jump) + rise_frames;
    } else if (g->h >= p->lo) {
        frames = SpacieFallFrames(ft, g->h, vy, p->lo);
    } else {
        if (peak < p->lo)
            return false;
        frames = vy / ft->attr.gravity;
    }
    float need = SpacieDashStartOut(ft, g) - p->aim; // > 0: must drift in
    return fabs(need) <= SpacieDriftReach(ft, need > 0.f ? g->vx : -g->vx, frames) + 3.f;
}

// Of a plan's aims (each with or without the earliest shorten), a random one
// the CPU can reach, or else the nearest.
static void SpaciePickAim(FighterData *ft, const SpacieGeo *g, SpaciePlan *p,
                          const float *aims, const bool *shortens, int n) {
    float here = SpacieDashStartOut(ft, g);
    int first = HSD_Randi(n), nearest = 0;
    for (int i = 0; i < n; ++i) {
        int k = (first + i) % n;
        p->aim = aims[k];
        p->shorten = shortens[k];
        if (SpacieCanReach(ft, g, p))
            return;
        if (fabs(aims[i] - here) < fabs(aims[nearest] - here))
            nearest = i;
    }
    p->aim = aims[nearest];
    p->shorten = shortens[nearest];
}

// Where and how high each plan side-Bs from, a little different every attempt.
static SpaciePlan SpacieMakePlan(FighterData *ft, const SpacieGeo *g, int kind) {
    SpaciePlan p = { .kind = kind };
    float step = SpacieDashStep(ft);
    float dash = 3.f * step;
    int rise_frames;
    float rise = SpacieJumpRise(ft, &rise_frames);
    switch (kind) {
        case SP_PLAN_SWEETSPOT: {
            // One, two or three steps from where the endlag's first grab has
            // the ledge in the box. A wall stops the steps left over.
            float first_grab = SpacieEndlagDrift(ft, SP_ENDLAG_FIRST_GRAB);
            p.lo = SP_SWEETSPOT_LO;
            p.hi = SP_SWEETSPOT_HI;
            float inside = first_grab + SpacieRand(SP_LIP_MARGIN + 1.f, g->reach - 3.f);
            float aims[] = { step + inside, 2.f * step + inside, dash + inside };
            bool shortens[] = { false, false, false };
            SpaciePickAim(ft, g, &p, aims, shortens, countof(aims));
            break;
        }
        case SP_PLAN_TO_LEDGE: {
            // The dash (whole, or shortened at once when close) ends short of
            // the ledge, and the endlag drifts in for a few frames.
            int linger = SP_LINGER_AIM_MIN + HSD_Randi(SP_LINGER_AIM_MAX - SP_LINGER_AIM_MIN + 1);
            float endlag = g->reach + SpacieEndlagDrift(ft, linger);
            p.lo = -13.5f;
            p.hi = -9.f;
            float aims[] = { dash + endlag, endlag };
            bool shortens[] = { false, true };
            SpaciePickAim(ft, g, &p, aims, shortens, countof(aims));
            break;
        }
        case SP_PLAN_SHORTEN:
            // Close enough that the endlag alone reaches the ledge.
            p.lo = -15.f;
            p.hi = -6.f;
            p.aim = g->reach + SpacieRand(3.f, 8.f);
            p.shorten = true;
            break;
        case SP_PLAN_TO_STAGE: {
            // The dash runs right above the ledge, from near where the CPU is.
            // It can start a little past a full dash: the endlag carries it
            // the rest of the way.
            float here = SpacieDashStartOut(ft, g) + SpacieRand(-8.f, 8.f);
            p.lo = 1.f;
            p.hi = 5.f;
            p.aim = fclamp(here, 4.f, dash + 6.f);
            // Through a player standing near the ledge, when it can get there.
            FighterData *hmn = Fighter_GetGObj(0)->userdata;
            float hmn_out = (g->ledge.X - hmn->phys.pos.X) * g->dir;
            if (hmn->phys.air_state == 0 && hmn_out <= 0.f && hmn_out > 10.f - dash) {
                SpaciePlan through = p;
                through.aim = fclamp(here, fmax(4.f, hmn_out + 6.f), hmn_out + dash - 6.f);
                if (SpacieCanReach(ft, g, &through))
                    p.aim = through.aim;
            }
            break;
        }
        case SP_PLAN_TO_PLAT: {
            // Land in the platform's middle from the band's middle: the endlag
            // drifts on until the fall reaches the platform.
            p.lo = g->plat_h + 3.f;
            p.hi = g->plat_h + 9.f;
            float drop = (p.lo + p.hi) * 0.5f - g->plat_h + ft->coll_data.ecbCurr_bot.Y;
            float drift = SpacieEndlagDrift(ft, SpacieEndlagFallFrames(ft, drop));
            p.aim = (g->plat_near + g->plat_far) * 0.5f + drift + dash;
            break;
        }
    }
    p.jump_at = SpacieRand(p.lo - rise + 4.f, p.lo - 6.f);
    return p;
}

static bool SpacieUpBChecked(u32 opt_flags) {
    return OptEnabled(OPT_SPACIES_FF_LOW) || OptEnabled(OPT_SPACIES_FF_MID)
        || OptEnabled(OPT_SPACIES_FF_HIGH);
}

// When the checked side-B rows can't make it and no Firefox row is checked:
// the first side-B that still can, or Firefox To Ledge.
static SpaciePlan SpacieBestEffortPlan(FighterData *ft, const SpacieGeo *g) {
    static const u8 order[] = { SP_PLAN_SWEETSPOT, SP_PLAN_TO_STAGE, SP_PLAN_SHORTEN, SP_PLAN_TO_LEDGE };
    for (u32 i = 0; i < countof(order); ++i) {
        SpaciePlan p = SpacieMakePlan(ft, g, order[i]);
        if (SpacieCanReach(ft, g, &p))
            return p;
    }
    return (SpaciePlan) { .kind = SP_PLAN_FF_LEDGE };
}

// Each attempt picks one checked side-B row the CPU can still reach, or the
// up special logic as one more choice when a Firefox row is checked.
static void SpacieChoosePlan(FighterData *ft, u32 opt_flags, const SpacieGeo *g) {
    SpaciePlan choices[OPT_SPACIES_COUNT];
    int n = 0;
    bool any_sideb = false;
    for (int opt = OPT_SPACIES_SIDEB_INSTANT; opt <= OPT_SPACIES_SIDEB_SHORTEN; ++opt) {
        if (!OptEnabled(opt))
            continue;
        any_sideb = true;
        int kind = SP_PLAN_INSTANT + opt - OPT_SPACIES_SIDEB_INSTANT;
        if (kind == SP_PLAN_TO_PLAT && !g->has_plat)
            kind = SP_PLAN_TO_STAGE;
        SpaciePlan plan = SpacieMakePlan(ft, g, kind);
        if (SpacieCanReach(ft, g, &plan))
            choices[n++] = plan;
    }
    if (OptEnabled(OPT_SPACIES_FF_LEDGE))
        choices[n++] = (SpaciePlan) { .kind = SP_PLAN_FF_LEDGE };
    if (SpacieUpBChecked(opt_flags) || (n == 0 && !any_sideb))
        choices[n++] = (SpaciePlan) { .kind = SP_PLAN_UPB };
    if (n == 0)
        choices[n++] = SpacieBestEffortPlan(ft, g);
    sp_plan = choices[HSD_Randi(n)];
}

static void SpacieFallBack(FighterData *ft, u32 opt_flags, const SpacieGeo *g) {
    sp_plan = OptEnabled(OPT_SPACIES_FF_LEDGE) ? (SpaciePlan) { .kind = SP_PLAN_FF_LEDGE }
        : SpacieUpBChecked(opt_flags) ? (SpaciePlan) { .kind = SP_PLAN_UPB }
        : SpacieBestEffortPlan(ft, g);
}

static void SpacieSideB(FighterData *ft, const SpacieGeo *g) {
    SetStick(ft, g->dir, 0.f);
    Press(ft, PAD_BUTTON_B);
}

// One actionable frame of a side-B plan: side-B once the simulation agrees,
// else drift toward the aim and double jump to get back up to the band.
// False when the plan fell through to another this frame.
static bool SpacieSideBThink(FighterData *ft, u32 opt_flags, const SpacieGeo *g) {
    SpaciePlan *p = &sp_plan;
    if (p->kind == SP_PLAN_INSTANT) {
        SpacieSideB(ft, g);
        return true;
    }
    // Knockback left lifts the CPU through the startup, so To Main Stage goes
    // by where the dash will run, not by where the press is.
    bool in_band = p->kind == SP_PLAN_TO_STAGE || (p->lo <= g->h && g->h <= p->hi);
    if (in_band && SpacieSideBWorks(ft, g, p)) {
        SpacieSideB(ft, g);
        return true;
    }

    bool can_jump = ft->jump.jumps_used < ft->attr.max_jumps;
    bool rising = g->vy + g->kby > 0.f;
    int rise_frames;
    float rise = SpacieJumpRise(ft, &rise_frames);
    float off = SpacieDashStartOut(ft, g) - p->aim; // > 0: too far out
    float stick_x = off > 2.f ? g->dir : off < -2.f ? -g->dir : 0.f;

    if (g->h < p->lo && !rising) {
        if (!can_jump || g->h + rise + SpacieCarryUp(g) < p->lo) {
            SpacieFallBack(ft, opt_flags, g);
            return false;
        }
        // Jump at the planned height if the rise is time enough to drift the
        // rest of the way, or at the last height it still reaches the band.
        bool drift_in_time = fabs(off) <= ft->attr.aerial_drift_max * rise_frames * 0.8f;
        if (g->h <= p->lo - rise + 4.f || (g->h <= p->jump_at && drift_in_time)) {
            Press(ft, PAD_BUTTON_Y);
            SetStick(ft, fclamp(off / 8.f, -1.f, 1.f) * g->dir, 0.f);
            return true;
        }
    }
    if (can_jump && !SpacieCanReach(ft, g, p)) {
        SpacieFallBack(ft, opt_flags, g);
        return false;
    }

    float stick_y = 0.f;
    if (
        OptEnabled(OPT_SPACIES_FASTFALL)
        && !ft->flags.is_fastfall
        && g->vy < 0.f
        && g->h > p->hi + 15.f
    )
        stick_y = -1.f;
    SetStick(ft, stick_x, stick_y);
    return true;
}

// Firefox To Ledge: the travel goes a fixed distance, so it only ends at the
// ledge from a ring around the spot. Attributes: no fall for 0x54 frames, then
// 0x60 a frame, and X speed divided by 0x58 then 0x5C slower a frame, for the
// charge (no drift). The travel is 0x68 frames at 0x74, slowing 0x78 a frame from
// frame 0x70.
#define SP_FF_CHARGE_FRAMES 42
// The travel's end. The last of its speed carries it in and up a little more, so
// it starts falling with the ledge in the box.
#define SP_FF_END_OUT 9.f
#define SP_FF_END_LO -15.5f
#define SP_FF_END_HI -10.5f
// Up special this far over the bottom blast zone whatever the distance.
#define SP_FF_BLAST_MARGIN 20.f
// The smallest stick X past the deadzone. A shot is straight up or at least this
// far over, so a launch a little outside the end spot can't be aimed at it.
#define SP_FF_TILT_MIN 0.29f

static float SpacieFirefoxTravel(FighterData *ft) {
    float speed = SP_ATTR(ft, 0x74), travel = 0.f;
    int slow_from = SP_ATTR(ft, 0x70), frames = SP_ATTR(ft, 0x68);
    for (int i = 1; i <= frames; ++i) {
        if (i >= slow_from)
            speed -= SP_ATTR(ft, 0x78);
        travel += speed;
    }
    return travel;
}

// Where a Firefox pressed now launches from.
static void SpacieChargeEnd(FighterData *ft, const SpacieGeo *g, float *out, float *h) {
    float vx = g->vx / SP_ATTR(ft, 0x58), vy = 0.f, kbx = g->kbx, kby = g->kby;
    int delay = SP_ATTR(ft, 0x54);
    *out = g->out;
    *h = g->h;
    for (int i = SP_FF_CHARGE_FRAMES; i > 0; --i) {
        if (delay > 0)
            --delay;
        else
            vy = fmax(vy - SP_ATTR(ft, 0x60), -ft->attr.terminal_velocity);
        vx = SpacieSlow(vx, SP_ATTR(ft, 0x5C));
        SpacieKnockbackStep(&kbx, &kby, (*stc_ftcommon)->kb_frameDecay);
        *out -= vx + kbx;
        *h += vy + kby;
    }
}

// How high the travel from (out, h) ends straight over the end spot, or far
// under it when the end spot is out of range sideways.
static float SpacieFirefoxEndHeight(FighterData *ft, float out, float h) {
    float travel = SpacieFirefoxTravel(ft);
    float dx = out - SP_FF_END_OUT;
    float left = travel * travel - dx * dx;
    return left > 0.f ? h + sqrtf(left) : -1000.f;
}

// Toward a launch point that can be aimed: right under the end spot, or far
// enough out for the smallest tilt.
static float SpacieFirefoxDrift(FighterData *ft, const SpacieGeo *g, float off) {
    float tilt = SpacieFirefoxTravel(ft) * SP_FF_TILT_MIN + 1.f;
    if (off >= tilt)
        return off > tilt + 8.f ? g->dir : 0.f;
    if (off > tilt * 0.5f)
        return -g->dir;
    return off > 2.f ? g->dir : off < -2.f ? -g->dir : 0.f;
}

// One actionable frame: up special from the ring, falling to it from inside and
// double jumping back into it from outside.
static void SpacieFirefoxThink(FighterData *ft, const SpacieGeo *g) {
    float out, h;
    SpacieChargeEnd(ft, g, &out, &h);
    float end_h = SpacieFirefoxEndHeight(ft, out, h);
    bool rising = g->vy + g->kby > 0.f;
    bool can_jump = ft->jump.jumps_used < ft->attr.max_jumps;
    bool low = ft->phys.pos.Y < Stage_GetBlastzoneBottom() + SP_FF_BLAST_MARGIN;
    bool too_far = end_h < SP_FF_END_LO && h <= SP_FF_END_LO; // falling only makes it farther
    bool ready = end_h >= SP_FF_END_LO && end_h <= SP_FF_END_HI;

    if (!rising && too_far && can_jump) {
        Press(ft, PAD_BUTTON_Y);
        SetStick(ft, g->dir, 0.f);
    } else if (!rising && (too_far || ready || low)) {
        Press(ft, PAD_BUTTON_B);
        SetStick(ft, 0.f, 1.f);
    } else {
        SetStick(ft, SpacieFirefoxDrift(ft, g, out - SP_FF_END_OUT), 0.f);
    }
}

// The charge: hold the angle to the end spot, read on its last frame. Stick X
// under the deadzone reads 0, so a shot close to straight up is straight up or
// takes the deadzone's edge.
static void SpacieFirefoxAim(FighterData *ft) {
    SpacieGeo g = SpacieGetGeo(ft);
    float end_h = fclamp(SpacieFirefoxEndHeight(ft, g.out, g.h), SP_FF_END_LO, SP_FF_END_HI);
    Vec2 aim = { (g.out - SP_FF_END_OUT) * g.dir, end_h - g.h };
    if (Vec2_Length(&aim) < 0.01f)
        return;
    Vec2_Normalize(&aim);
    if (fabs(aim.X) < SP_FF_TILT_MIN * 0.5f)
        aim.X = 0.f;
    else if (fabs(aim.X) < SP_FF_TILT_MIN)
        aim.X = aim.X < 0.f ? -SP_FF_TILT_MIN : SP_FF_TILT_MIN;
    SetStick(ft, aim.X, aim.Y);
}

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

    int dj_chance;
    if (cpu_data->phys.pos.Y > target_ledge.Y) {
        dj_chance = SP_JUMP_CHANCE_ABOVE_LEDGE;
    } else {
        dj_chance = SP_JUMP_CHANCE_BELOW_LEDGE;
    }
    
    bool is_past_ledge = fabs(target_ledgegrab.X) > fabs(pos.X);
    bool can_fall_to_ledge = is_past_ledge
        | SimulatePhys_CanReachPoint(cpu_data, target_ledgegrab);
    
    bool can_upb = OptEnabled(OPT_SPACIES_FF_LOW)
        | OptEnabled(OPT_SPACIES_FF_MID)
        | OptEnabled(OPT_SPACIES_FF_HIGH);
    
    float upb_distance = cpu_data->kind == FTKIND_FOX ?
        SP_FIREFOX_DISTANCE : SP_FIREBIRD_DISTANCE;

    // A new plan every attempt (each starts in hitlag) and after landing.
    if (cpu_data->flags.hitlag || cpu_data->phys.air_state == 0)
        sp_plan.kind = SP_PLAN_NONE;

    // HITSTUN
    if (cpu_data->flags.hitlag) {
        DIIn(cpu_data);

    // ACTIONABLE
    } else if (IsAirActionable(cpu)) {
        SpacieGeo geo = SpacieGetGeo(cpu_data);
        if (sp_plan.kind == SP_PLAN_NONE)
            SpacieChoosePlan(cpu_data, opt_flags, &geo);
        bool sideb = sp_plan.kind >= SP_PLAN_INSTANT && SpacieSideBThink(cpu_data, opt_flags, &geo);
        // A plan that fell through to another side-B starts it right away.
        if (!sideb && sp_plan.kind >= SP_PLAN_INSTANT)
            sideb = SpacieSideBThink(cpu_data, opt_flags, &geo);

        // SIDE-B PLANS
        if (sideb) {

        // FIREFOX TO LEDGE
        } else if (sp_plan.kind == SP_PLAN_FF_LEDGE) {
            SpacieFirefoxThink(cpu_data, &geo);

        // JUMP
        } else if (
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

    // ILLUSION DASH: the earliest shorten
    } else if (state == ASID_SPECIALAIRS) {
        if (sp_plan.kind >= SP_PLAN_INSTANT && sp_plan.shorten)
            Press(cpu_data, PAD_BUTTON_B);

    // FIREFOX TO LEDGE STARTUP
    } else if (0x161 <= state && state <= 0x162 && sp_plan.kind == SP_PLAN_FF_LEDGE) {
        SpacieFirefoxAim(cpu_data);

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
        // drift towards stage, but not under it
        SpacieGeo geo = SpacieGetGeo(cpu_data);
        if (sp_plan.kind != SP_PLAN_FF_LEDGE || geo.h >= 0.f || geo.out > 6.f)
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

enum luigi_plan {
    LG_PLAN_NONE,      // not chosen yet this attempt
    LG_PLAN_UPB,       // up special to about the ledge, also what the others fall to
    LG_PLAN_EARLY,     // up special from above the ledge
    LG_PLAN_SWEETSPOT, // up special whose top only just reaches the ledge
    LG_PLAN_BACKFLIP,  // double jump back, drift in, grab on the way down
};

typedef struct LuigiPlan {
    int kind;
    float lo, hi;         // up special (or backflip) with the feet this high
    float out_lo, out_hi; // and this far outside the ledge
    float last;           // up special from wherever at this height
    float jump_at;        // a double jump that's needed waits for this height
    bool cyclone;         // Cyclone on the way even with no need of one
    bool missile;         // Green Missile when it's a long way
} LuigiPlan;

static int mash_timer = 0;
static int lg_missile_charge = 0;
static LuigiPlan lg_plan = { LG_PLAN_NONE };
static bool pe_float_after_jump = false;
static int pr_high = -1; // Jigglypuff's plan this attempt: 1 high, 0 low, -1 undecided

static bool RecoveryAssignsInputs(int kind) {
    // The spacies' side-B shorten needs a fresh B press mid-move.
    return kind == FTKIND_FOX || kind == FTKIND_FALCO
        || kind == FTKIND_YOSHI || kind == FTKIND_PEACH || kind == FTKIND_LUIGI
        || kind == FTKIND_DRMARIO || kind == FTKIND_POPO || kind == FTKIND_JIGGLYPUFF
        || kind == FTKIND_SAMUS;
}

// Every attempt starts in hitlag; forget the last one's plans.
static void ResetRecoveryPlans(void) {
    mash_timer = 0;
    lg_missile_charge = 0;
    lg_plan.kind = LG_PLAN_NONE;
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

// One fresh B press every `interval` frames, give or take one, and never on two
// frames in a row: a person mashing, not a turbo button.
static bool MashEvery(int interval) {
    if (mash_timer > 0) {
        --mash_timer;
        return false;
    }
    mash_timer = interval - 2 + HSD_Randi(3);
    return true;
}

static bool MashB(bool fast) {
    return MashEvery(fast ? MASH_INTERVAL_FAST : MASH_INTERVAL);
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

// The up special's real rise: it falls for the 8 frames before the root motion
// counts, then rises at 0.95 of it, straight up, 41.8 by frame 26. It comes
// back down from there, and that's when it can grab (facing the ledge), so its
// top should be right at the box or fall into it.
#define LG_UPB_RISE 41.8f
#define LG_UPB_TOP_MAX 8.f // the plain up special's highest top over the ledge
// The stick past which the game reverses the up special, and short of where it
// tilts it. Held after the press, so Luigi ends facing the ledge.
#define LG_UPB_FACE_STICK 0.5f
// The Cyclone starts falling (0.7 down), and B only counts on frames 6 to 44
// (cmd var 2), at +0.8 up to 1.4 a press. Against 0.069 gravity that's a net
// +8 height at 10 presses a second, +25 at 15 and +33 at 20. It's out of control
// until frame 80. Starting one replaces his Y speed, so it's only for the way down.
#define LG_MASH_INTERVAL 4      // about 15 presses a second
#define LG_MASH_INTERVAL_FAST 3 // about 20
// A double jump counts as a backflip when the stick is 0.125 away from where he
// faces (ftCo_JumpAerial_Enter_Basic). Past that and the 0.28 deadzone, but no
// more: this is also how fast he leaves the stage.
#define LG_BACKFLIP_STICK 0.4f
// The Green Missile hovers through its startup and flies flat for 40 frames,
// then slows, about this far with no charge and this much farther a frame of
// charge. It rises about 8 on the way and falls about 30 at its end, so it's for
// high enough to up special after it, and not so low it goes under a lip.
#define LG_MISSILE_BASE 65.f
#define LG_MISSILE_PER_CHARGE 0.9f
#define LG_MISSILE_MIN_OUT 25.f
#define LG_MISSILE_MIN_H -12.f
// How much farther out than the box an up special can start for each unit its
// top is over the bottom of the box: it can't drift until it's falling.
#define LG_UPB_DRIFT_PER_RISE 0.25f

#define LG_STATE_MISSILE_START 349
#define LG_STATE_MISSILE_HOLD 350
#define LG_STATE_UPB_GROUND 355
#define LG_STATE_UPB 356
#define LG_STATE_CYCLONE 358

// Only a Cyclone touching down refreshes its lift (ftLuigi_SpecialAirLw_AirToGround).
// A real Luigi does that on the stage. A recovering CPU never does, and nothing
// else clears the flag: not his load, not his death, and fighter data isn't
// zeroed when it's allocated, so he can start the match with whatever the last
// fighter left there, and a savestate only brings that back. Every attempt, and
// every landing, is treated as having done it.
static void Recovery_RefreshCyclone(FighterData *ft) {
    if (ft->kind == FTKIND_LUIGI)
        ft->fighter_var.ft_var1 = 0; // fp+222C, set when a Cyclone ends its lift
}

static void LuigiUpB(FighterData *ft) {
    Press(ft, PAD_BUTTON_B);
    SetStick(ft, 0.f, 1.f);
}

// Hold between two outs, going by where he'd stop: letting go of the stick
// barely slows Luigi in the air.
static void LuigiSteer(FighterData *ft, const SpacieGeo *g, float lo, float hi) {
    float accel = ft->attr.aerial_drift_stick_mult + ft->attr.aerial_drift_base;
    float out = g->out - g->vx * fabs(g->vx) / (2.f * accel);
    SetStick(ft, out > hi ? g->dir : out < lo ? -g->dir : 0.f, 0.f);
}

// How far in a backflip from here gets by the time it's fallen back to the box:
// the whole of the jump, not just its rise. Luigi floats, and that's most of it.
static float LuigiBackflipReach(FighterData *ft, const SpacieGeo *g, float highest) {
    CollData *coll = &ft->coll_data;
    int rise_frames;
    float peak = fmin(g->h, highest) + SpacieJumpRise(ft, &rise_frames);
    float box_lo = coll->cliffgrab_y_offset - coll->cliffgrab_height * 0.5f;
    int fall_frames = SpacieFallFrames(ft, peak, 0.f, -box_lo);
    float back = -LG_BACKFLIP_STICK * ft->attr.air_jump_h_multiplier;
    return SpacieDriftReach(ft, back, rise_frames + fall_frames);
}

// A fall through the box grabs the ledge, so a plan that finishes from under it
// waits just outside the box until it's past, then lines up.
static void LuigiWait(FighterData *ft, const SpacieGeo *g, const LuigiPlan *p, float box_hi) {
    bool from_under = p->kind == LG_PLAN_SWEETSPOT || p->kind == LG_PLAN_BACKFLIP;
    if (from_under && g->h > -box_hi - 2.f && g->out > p->out_lo)
        LuigiSteer(ft, g, g->reach + 2.f, g->reach + 6.f);
    else
        LuigiSteer(ft, g, p->out_lo + 1.f, p->out_hi - 1.f);
}

static LuigiPlan LuigiMakePlan(FighterData *ft, const SpacieGeo *g, int kind) {
    CollData *coll = &ft->coll_data;
    float box_lo = coll->cliffgrab_y_offset - coll->cliffgrab_height * 0.5f;
    float box_hi = coll->cliffgrab_y_offset + coll->cliffgrab_height * 0.5f;
    LuigiPlan p = {
        .kind = kind,
        .out_lo = g->reach - coll->cliffgrab_width + 1.f,
        .out_hi = g->reach - 1.f,
        .last = 3.f - box_hi - LG_UPB_RISE,
        .jump_at = SpacieRand(-15.f, 5.f),
        .cyclone = HSD_Randi(3) != 0,
        .missile = HSD_Randi(2) == 0,
    };
    switch (kind) {
        case LG_PLAN_UPB:
            p.lo = -box_lo - LG_UPB_RISE;
            p.hi = SpacieRand(2.f - box_lo, LG_UPB_TOP_MAX) - LG_UPB_RISE;
            break;
        case LG_PLAN_EARLY:
            p.lo = -8.f;
            p.hi = 12.f;
            p.out_hi = g->reach - 2.f;
            break;
        case LG_PLAN_SWEETSPOT:
            p.lo = p.last;
            p.hi = -box_lo - LG_UPB_RISE;
            p.out_hi = g->reach - 2.f;
            p.cyclone = false; // only if it's needed: this one wants to be low
            break;
        case LG_PLAN_BACKFLIP: {
            // From low enough that the top of the jump is around the box.
            int rise_frames;
            float rise = SpacieJumpRise(ft, &rise_frames);
            p.lo = 2.f - box_hi - rise;
            p.hi = 6.f - rise;
            p.cyclone = false;
            break;
        }
    }
    return p;
}

// Each attempt picks one checked way to finish, or the plain up special with
// none checked. A backflip that won't reach the ledge falls to the up special.
static void LuigiChoosePlan(FighterData *ft, u32 opt_flags, const SpacieGeo *g) {
    int kinds[3], n = 0;
    if (OptEnabled(OPT_LUIGI_UPB_EARLY))
        kinds[n++] = LG_PLAN_EARLY;
    if (OptEnabled(OPT_LUIGI_UPB_SWEETSPOT))
        kinds[n++] = LG_PLAN_SWEETSPOT;
    // Falls and jumps only grab the ledge they face, and a jump doesn't turn him.
    if (OptEnabled(OPT_LUIGI_BACKFLIP) && ft->jump.jumps_used < ft->attr.max_jumps
        && ft->facing_direction == g->dir)
        kinds[n++] = LG_PLAN_BACKFLIP;
    lg_plan = LuigiMakePlan(ft, g, n ? kinds[HSD_Randi(n)] : LG_PLAN_UPB);
}

// One actionable frame: get over the spot the plan finishes from, with the
// moves it needs, and finish.
static void LuigiThink(FighterData *ft, u32 opt_flags, const SpacieGeo *g) {
    if (lg_plan.kind == LG_PLAN_NONE)
        LuigiChoosePlan(ft, opt_flags, g);
    LuigiPlan *p = &lg_plan;
    CollData *coll = &ft->coll_data;
    bool airjump = ft->jump.jumps_used < ft->attr.max_jumps;
    bool backflip = p->kind == LG_PLAN_BACKFLIP;
    bool rising = g->vy + g->kby > 0.f;
    float box_hi = coll->cliffgrab_y_offset + coll->cliffgrab_height * 0.5f;
    float h = g->h + SpacieCarryUp(g); // where the knockback left will take it
    if (backflip && airjump)
        p->out_hi = g->reach - 2.f + LuigiBackflipReach(ft, g, p->hi);

    // The backflip's jump is spent: fall into the box, or up special if it's gone.
    if (backflip && !airjump) {
        if (g->h >= -box_hi - 2.f) {
            LuigiSteer(ft, g, p->out_lo, g->reach - 3.f);
            return;
        }
        *p = LuigiMakePlan(ft, g, LG_PLAN_UPB);
        backflip = false;
    }

    // Stage in drift reach before it falls past the ledge: just land.
    if (p->kind == LG_PLAN_UPB && h > 2.f
        && SpacieDriftReach(ft, g->vx, SpacieFallFrames(ft, g->h, g->vy, 0.f)) >= g->out + 4.f) {
        SetStick(ft, g->dir, 0.f);
        return;
    }

    // Whether drifting alone gets over the spot by the time it's fallen to the
    // lowest it can finish from. Higher up, the fall from the top adds some.
    float box_lo = coll->cliffgrab_y_offset - coll->cliffgrab_height * 0.5f;
    float out_hi = p->out_hi;
    if (p->kind == LG_PLAN_UPB)
        out_hi += fmax(h + LG_UPB_RISE + box_lo, 0.f) * LG_UPB_DRIFT_PER_RISE;
    int frames = h > p->lo ? SpacieFallFrames(ft, g->h, g->vy, p->lo) : 0;
    bool short_of_it = g->out - out_hi > SpacieDriftReach(ft, g->vx, frames);
    bool too_low = h < p->lo;
    // A backflip row that fell through can still use the jump it saved.
    bool jump = (OptEnabled(OPT_LUIGI_JUMP) || OptEnabled(OPT_LUIGI_BACKFLIP)) && airjump && !backflip;
    bool cyclone = OptEnabled(OPT_LUIGI_CYCLONE) && ft->fighter_var.ft_var1 == 0;

    // GREEN MISSILE across a long gap, from high enough to up special after it:
    // some attempts, and whenever it's all that's left
    if (
        OptEnabled(OPT_LUIGI_MISSILE) && (p->missile || (!jump && !cyclone)) && short_of_it && !rising
        && g->out > LG_MISSILE_MIN_OUT && g->h > LG_MISSILE_MIN_H && g->h < 38.f
    ) {
        Press(ft, PAD_BUTTON_B);
        SetStick(ft, g->dir, 0.f);
        // longer gaps get a longer charge (a full charge is 90 frames)
        lg_missile_charge = (int)fclamp((g->out - p->out_hi - LG_MISSILE_BASE) / LG_MISSILE_PER_CHARGE, 0.f, 70.f);

    // CYCLONE on the way down, needed or not: it's what the row is for. Before
    // the double jump, since its end has no drift: that's height better spent
    // with the jump still to come.
    } else if (
        cyclone && !rising && g->h < 6.f
        && (too_low || short_of_it || (p->cyclone && g->h < -6.f))
    ) {
        Press(ft, PAD_BUTTON_B);
        SetStick(ft, 0.f, -1.f);
        mash_timer = 0;

    // JUMP when it's needed, toward the stage
    } else if (jump && !rising && (too_low || (short_of_it && g->h < p->jump_at))) {
        Press(ft, PAD_BUTTON_Y);
        SetStick(ft, g->dir, 0.f);

    // BACKFLIP: the stick behind him makes it one
    } else if (backflip && !rising && g->h <= p->hi && g->out <= p->out_hi && g->out >= p->out_lo) {
        Press(ft, PAD_BUTTON_Y);
        SetStick(ft, -LG_BACKFLIP_STICK * ft->facing_direction, 0.f);

    // A backflip that's out of reach
    } else if (backflip && (too_low || short_of_it)) {
        *p = LuigiMakePlan(ft, g, LG_PLAN_UPB);
        LuigiSteer(ft, g, p->out_lo, p->out_hi);

    // UP SPECIAL from the spot, or from wherever at the last moment
    } else if (
        !backflip && !rising && h <= p->hi
        && ((g->out >= p->out_lo && g->out <= out_hi) || h < p->last)
    ) {
        LuigiUpB(ft);

    // WAIT
    } else {
        LuigiWait(ft, g, p, box_hi);
    }
}

static void Think_Luigi(GOBJ *cpu, u32 opt_flags) {
    FighterData *ft = cpu->userdata;
    SpacieGeo g = SpacieGetGeo(ft);
    int state = ft->state_id;

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
        SetStick(ft, g.dir, 0.f);

    // CYCLONE: mash in the window that counts, steering for the ledge or the stage
    } else if (state == LG_STATE_CYCLONE) {
        if (ft->ftcmd_var.flag2 && MashEvery(OptEnabled(OPT_LUIGI_FAST_MASH) ? LG_MASH_INTERVAL_FAST : LG_MASH_INTERVAL))
            Press(ft, PAD_BUTTON_B);
        if (g.h > 0.f)
            SetStick(ft, g.dir, 0.f);
        else
            LuigiSteer(ft, &g, g.reach - ft->coll_data.cliffgrab_width + 2.f, g.reach - 2.f);

    // UP SPECIAL: end facing the ledge
    } else if (state == LG_STATE_UPB || state == LG_STATE_UPB_GROUND) {
        SetStick(ft, g.dir * LG_UPB_FACE_STICK, 0.f);

    } else if (IsAirActionable(cpu)) {
        LuigiThink(ft, opt_flags, &g);

    // Helpless, or the end of a Green Missile: over the stage if it's there,
    // and otherwise into the box without going under it.
    } else if (g.h > 0.f) {
        SetStick(ft, g.dir, 0.f);
    } else {
        LuigiSteer(ft, &g, g.reach - ft->coll_data.cliffgrab_width + 2.f, g.reach - 2.f);
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


