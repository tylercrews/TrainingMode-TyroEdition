#include "../MexTK/mex.h"
#include "events.h"
#include "powershield_osd.h"
#include "stage_points.h"
#include "stage_swap.h"
#include "fighter_swap.h"

/* Final Destination's numbers. On another stage each scales by that side's
 * live edge (stage_points.h's FD_EDGE), so FD keeps them exactly: you start at
 * -60 and the CPU at +60, where FD's spawns put you and Falco, and Falco never
 * moves past +-70. */
#define START_X 60.f
#define LEDGE_X 70.f
#define FULL_FALCO_SHORTHOP_DISTANCE 30.f
#define SLIGHT_FALCO_SHORTHOP_DISTANCE 13.f

/* Samus's super missile, from PlSs.dat (docs/POWERSHIELD-TRAINING.md). It
 * flies flat from her cannon (joint 56), 9.92 above her feet on the frame it
 * spawns: the air move's 16th frame. The event presses one frame before the
 * move starts, so the spawn is SAMUS_MISSILE_DELAY frames after the press. */
#define SAMUS_MISSILE_RISE 9.92f
#define SAMUS_MISSILE_DELAY 16
/* Missile heights above the floor below: Very Low, Low, Mid, High. Each comes
 * out within half a frame's fall (0.7) of it. Very Low spawns with her feet
 * within that of the floor, well before her airborne ECB (3.25 over her feet
 * then) could land her and cancel it. */
static const float samus_heights[] = { 10.f, 13.f, 16.5f, 20.5f };
/* How high above her floor Samus's short hop can catch a platform (its 22.75
 * apex plus her airborne ECB's bottom), and her full hop (34.46 plus it).
 * Falco's short hop (11.58) catches only Fountain's lowest side platforms. */
#define SAMUS_SHORT_HOP_REACH 29.f
#define SAMUS_FULL_HOP_REACH 37.f
#define FALCO_SHORT_HOP_REACH 18.f
/* A start spot keeps this far from a platform it could land on, and from the edge. */
#define SPOT_MARGIN 8.f
#define SPOT_EDGE 10.f
/* Fountain's side platforms rise and sink; this is their lowest surface
 * (notes/LEGAL-STAGE-PLATFORMS.md), so a start spot keeps off them there too. */
#define FOUNTAIN_SIDE_LOWEST 12.375f
/* Frames the down input may take to drop through a platform before it's let go and tried again. */
#define SAMUS_DROP_LIMIT 20
/* Samus's side-B motion states: Homing and Super Missile, grounded and aerial. */
enum { SAMUS_SPECIALS = 349, SAMUS_SPECIALAIRS_SMASH = 352 };

void Exit(GOBJ *menu);
void ChangeFireSpeedOption(GOBJ *event_menu, int value);
void ChangeDirection(GOBJ *event_menu, int value);
void Reset(void);
void ChangeRandomFireDelayMin(GOBJ *event_menu, int value);
void ChangeRandomFireDelayMax(GOBJ *event_menu, int value);
int GetRandomLaserDelay(void);
void ChangeAllowNana(GOBJ *event_menu, int value);
static void ChangePreference(GOBJ *menu, int value);
static void ChangeOverlay(GOBJ *menu, int value);
static void ChangeSamusPattern(GOBJ *menu, int value);
static void Detach(void);

enum menu_options {
    OPT_CHARACTERS,
    OPT_STAGE,
    OPT_FIRE_SPEED,
    OPT_FIRE_DELAY_RANDOM_MIN,
    OPT_FIRE_DELAY_RANDOM_MAX,
    OPT_LASER_HEIGHT,
    OPT_SAMUS_PATTERN,
    OPT_DIRECTION,
    OPT_MOVEMENT,
    OPT_ALLOW_NANA,
    OPT_OSDS,
    OPT_SUPPRESS,
    OPT_OVERLAY,
    OPT_HELP,
    OPT_EXIT,
};

enum fire_speed {
    FIRE_SPEED_RANDOM,
    FIRE_SPEED_SLOW,
    FIRE_SPEED_MEDIUM,
    FIRE_SPEED_FAST,
};

enum laser_height {
    LASER_HEIGHT_RANDOM,
    LASER_HEIGHT_VERY_LOW,
    LASER_HEIGHT_LOW,
    LASER_HEIGHT_MID,
    LASER_HEIGHT_HIGH,
};

enum falco_direction {
    DIRECTION_RIGHT,
    DIRECTION_LEFT,
};

enum movement {
    MOVEMENT_IN_PLACE,
    MOVEMENT_RANDOM,
    MOVEMENT_APPROACHING,
    MOVEMENT_RETREATING,
};

static const char *Options_FireSpeed[] = { "Random", "Slow", "Medium", "Fast" };
static const char *Options_LaserHeight[] = { "Random", "Very Low", "Low", "Mid", "High" };
static const char *Options_Direction[] = { "Right", "Left" };
static const char *Options_Movement[] = { "In-Place", "Random", "Approaching", "Retreating" };
static const char *Options_SamusPattern[TM_PS_SAMUS_PATTERN_COUNT] = { "Short Hop", "Platform", "Random" };
/* Final Destination has no platforms: both patterns with one play short hops. */
static const char *Options_SamusPatternFD[TM_PS_SAMUS_PATTERN_COUNT] = { "Short Hop", "Platform (N/A)", "Random (N/A)" };
static const u8 Powershield_CPUKinds[] = { CKIND_FALCO, CKIND_SAMUS };

static EventOption help_options[] = {
    {.kind = OPTKIND_INFO, .name = "Hard Press L / R",
     .desc = {"Click L or R fully just before the shot arrives.",
              "Analog light shield alone does not start reflection.",
              "Release, then press crisply; avoid a slow squeeze."}},
    {.kind = OPTKIND_INFO, .name = "Two Reflect Frames",
     .desc = {"The projectile reflector lasts frames 1 and 2.",
              "The normal shield and reflector are different shapes.",
              "A shield hit during reflection is not proof of early input.",
              "Try a different spacing, posture, or shot height."}},
    {.kind = OPTKIND_INFO, .name = "Read Powershield",
     .desc = {"Success means Melee accepted a reflected projectile.",
              "The frame counts from hard-shield startup.",
              "Reflect active / shield hit: check spacing or height.",
              "No hard press: no eligible click. Reflect ended: late."}},
    {.kind = OPTKIND_INFO, .name = "After a Reflection",
     .desc = {"Reflected lasers and missiles cause no shieldstun.",
              "Green marks recovery; normal shield rules still apply.",
              "Releasing shield still has normal shield-drop lag.",
              "You cannot instantly drop shield into a dash."}},
    {.kind = OPTKIND_INFO, .name = "Choose a Follow-up",
     .desc = {"Jump into an aerial or wavedash to reposition.",
              "Or grab, or jump-cancel into up-smash / up-B.",
              "Wavedashing skips shield-drop lag, but has landing lag.",
              "Choose based on spacing and your character's options."}},
    {.kind = OPTKIND_INFO, .name = "Physical vs Projectile",
     .desc = {"Physical powershields still have shieldstun.",
              "Afterward, attacks can cancel their shield release.",
              "Reflected shots do not grant that attack cancel.",
              "Neither gives an instant shield-drop dash."}},
    {.kind = OPTKIND_INFO, .name = "Read Act out of Laser",
     .desc = {"Frame 1 is your first unfrozen chance after the stun.",
              "Laser / Missile hit, Shield hit or Powershield: the source.",
              "Shielding and walking do not finish the count.",
              "1f Cyan, 2f Green, 3f Yellow, 4+f Red; ROLL or DODGE fails."}},
    {.kind = OPTKIND_INFO, .name = "Recovery Cue",
     .desc = {"Yellow: final two frames of hitstun / shieldstun.",
              "Green: first 2 recovery frames; 4 after a reflection.",
              "Both your character and shield receive the cue.",
              "Green while shielding does not mean an instant dash."}},
    {.kind = OPTKIND_INFO, .name = "Samus's Patterns",
     .desc = {"Short Hop: a super missile on the way down, at its height.",
              "Platform: a rising missile up to a side platform, then",
              "drop through and missile on the way down, at its height.",
              "Final Destination has no platforms: short hops only."}},
};
static EventMenu help_menu = {.name = "Powershield Help", .option_num = countof(help_options), .options = help_options};

static EventMenu Menu_Characters = {
    .name = "Change Characters",
    .option_num = countof(FighterSwap_Options),
    .options = FighterSwap_Options,
    .row_hooks = &FighterSwap_RowHooks,
};

static EventOption Options_Main[] = {
    {
        .kind = OPTKIND_MENU,
        .name = "Change Characters",
        .desc = {"Pick your character and the CPU: Falco or Samus.",
                 "Restarts the match; saved settings are kept."},
        .menu = &Menu_Characters,
    },
    {
        .kind = OPTKIND_MENU,
        .name = "Change Stage",
        .desc = {"Restart on another tournament-legal stage.",
                 "Saved settings are kept."},
        .menu = &StageSwap_Menu,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(Options_FireSpeed) / 4,
        .name = "Fire Speed",
        .desc = {"Change the rate of fire."},
        .values = Options_FireSpeed,
        .OnChange = ChangeFireSpeedOption,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 61,
        .val = 0,
        .name = "Min Fire Delay",
        .desc = {"Adjust the minimum number of frames between shots"},
        .format = "%d",
        .OnChange = ChangeRandomFireDelayMin,
        .disable = false,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 61,
        .val = 20,
        .name = "Max Fire Delay",
        .desc = {"Adjust the maximum number of frames between shots"},
        .format = "%d",
        .OnChange = ChangeRandomFireDelayMax,
        .disable = false,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(Options_LaserHeight) / 4,
        .name = "Laser Height",
        .desc = {"Change the laser height."},
        .values = Options_LaserHeight,
        .OnChange = ChangePreference,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = TM_PS_SAMUS_PATTERN_COUNT,
        .name = "Samus Pattern",
        .desc = {"Short Hop: a super missile on the way down.",
                 "Platform: a rising missile up to a side platform,",
                 "then drop through and missile on the way down.",
                 "Random mixes both. Samus only. This setting is saved."},
        .values = Options_SamusPattern,
        .OnChange = ChangeSamusPattern,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(Options_Direction) / 4,
        .name = "Direction",
        .desc = {"Change which way the CPU shoots."},
        .values = Options_Direction,
        .OnChange = ChangeDirection,
    },
    {
        .kind = OPTKIND_STRING,
        .value_num = sizeof(Options_Movement) / 4,
        .name = "Movement",
        .desc = {"Change how falco lasers around the stage."},
        .values = Options_Movement,
        .OnChange = ChangePreference,
    },
    {
        .kind = OPTKIND_TOGGLE,
        .val = 1,  // Default to both climbers
        .name = "Nana",
        .desc = {"Toggle Nana on or off."},
        .OnChange = ChangeAllowNana,
    },
    {.kind = OPTKIND_TOGGLE, .name = "Event OSDs", .val = 1, .OnChange = ChangePreference,
     .desc = {"Show Act out of Laser and Powershield side by side.", "Defaults On. This setting is saved."}},
    {.kind = OPTKIND_TOGGLE, .name = "Suppress Global OSDs", .OnChange = ChangePreference,
     .desc = {"Hide global OSDs during this event only.", "Keeps the event's two OSDs independent.", "Defaults Off. This setting is saved."}},
    {.kind = OPTKIND_TOGGLE, .name = "Laser Recovery Cue", .val = 1, .OnChange = ChangeOverlay,
     .desc = {"Yellow for the final two hitstun / shieldstun frames.", "Green on recovery, including reflected lasers.", "Independent of global cues. This setting is saved."}},
    {.kind = OPTKIND_MENU, .name = "Help", .menu = &help_menu,
     .desc = {"Learn the native reflection window and read both OSDs."}},
    {
        .kind = OPTKIND_FUNC,
        .name = "Exit",
        .desc = {"Return to the Event Select Screen."},
        .OnSelect = Exit,
    },
};

static EventMenu Menu_Main = {
    .on_exit = Exit,
    .name = "Powershield Training",
    .option_num = sizeof(Options_Main) / sizeof(EventOption),
    .options = Options_Main,
};

static int falco_wait_delay = -1;
static int falco_shoot_delay = -1;
static int falco_fastfall_delay = -1;

static int falco_dash_direction = 0;
static int falco_jump_direction = 0;
static int falco_shoot_direction = 0;

/* Samus's plan, frame to frame. A cycle starts once she can act on the ground
 * or a platform and her fire delay runs out: on a platform she drops through,
 * and on the ground she short hops or, for Platform, full hops toward the
 * platform above with a rising missile. Each airtime fires one missile. */
typedef struct SamusPlan {
    int wait;      /* Fire delay frames left; -1 until set. */
    int platform;  /* This cycle full hops for the platform above. */
    int full_hop;  /* Holding the jump through jumpsquat. */
    int rising;    /* The rising missile is next. */
    int dropping;  /* Frames the drop's down input has been held; 0 when not dropping. */
    int fired;     /* This airtime's missile is out. */
    int press;     /* Frames since an unconfirmed press; 0 for none. */
    float target;  /* This airtime's missile height above the floor below. */
} SamusPlan;
static SamusPlan samus;
static int restarting;

typedef struct PowershieldData {
    EventDesc *desc;
    FighterData *fighter;
    int spawn, kind, armed, ready, opportunities, opportunity_tick, input_tick, input_state, in_iasa;
    int tick, seen_tick, cue_green, cue_color;
    int act_frame, act_kind, act_fail, ps_valid, ps_kind, ps_frame, ps_reason;
    Text *text[2];
} PowershieldData;
typedef char powershield_data_fits[(sizeof(PowershieldData) <= EVENT_DATASIZE) ? 1 : -1];
static PowershieldData *Data(void) { return event_vars->event_gobj->userdata; }
/* Rolling or spotdodging out of laser stun fails the attempt, whatever its frame. */
enum { ACT_OK, ACT_FAIL_ROLL, ACT_FAIL_DODGE };
static const unsigned preference_rows[TM_PS_PREF_COUNT] = {
    OPT_FIRE_SPEED, OPT_FIRE_DELAY_RANDOM_MIN, OPT_FIRE_DELAY_RANDOM_MAX, OPT_LASER_HEIGHT,
    OPT_DIRECTION, OPT_MOVEMENT, OPT_ALLOW_NANA, OPT_OSDS, OPT_SUPPRESS, OPT_OVERLAY,
    OPT_SAMUS_PATTERN,
};
static int IsSamus(FighterData *ft) { return ft && ft->kind == FTKIND_SAMUS; }
static int CPUIsSamus(void) {
    GOBJ *cpu = Fighter_GetGObj(1);
    return cpu && IsSamus(cpu->userdata);
}
static int CommonState(FighterData *ft, int state) {
    return ft->kind == FTKIND_YOSHI && state >= 341 && state <= 345 ? state - 341 + ASID_GUARDON : state;
}
static int IgnoreLaserAction(FighterData *ft, int state) {
    state = CommonState(ft, state);
    /* Include walking turns/stops and shield release so these cannot finish
     * an attempt indirectly after the initial shield/walk was ignored. */
    return (state >= ASID_WAIT && state <= ASID_TURN) ||
           (state >= ASID_GUARDON && state <= ASID_GUARDREFLECT);
}
static void ApplyCue(void) {
    PowershieldData *data = Data();
    GOBJ *fighter = Fighter_GetGObj(0);
    if (!fighter) return;
    static const GXColor yellow = {255,240,0,220}, green = {80,255,90,220};
    const GXColor *color = !Options_Main[OPT_OVERLAY].val ? 0 :
        data->cue_color == TM_CUE_YELLOW ? &yellow : data->cue_color == TM_CUE_GREEN ? &green : 0;
    event_vars->set_timing_overlay(fighter, color);
}
static void ClearTiming(void) {
    PowershieldData *data = Data();
    data->fighter = 0;
    data->armed = data->ready = data->opportunities = data->in_iasa = 0;
    data->seen_tick = data->cue_green = data->cue_color = data->ps_valid = 0;
    data->opportunity_tick = data->input_tick = -1;
    data->act_frame = -1; data->act_fail = ACT_OK;
    ApplyCue();
}
static void LaserContact(FighterData *ft, int kind, int frame, int reason) {
    if (ft->ply != 0 || ft->flags.ms) return;
    PowershieldData *data = Data();
    data->fighter = ft; data->spawn = ft->spawn_num;
    data->kind = kind; data->armed = 1; data->ready = data->opportunities = data->in_iasa = 0;
    data->opportunity_tick = data->input_tick = -1;
    data->act_frame = -1; data->act_kind = kind; data->act_fail = ACT_OK;
    data->ps_valid = 1; data->ps_kind = kind; data->ps_frame = frame; data->ps_reason = reason;
    data->cue_green = data->cue_color = 0;
    ApplyCue();
}
static void LaserTiming(FighterData *ft, int phase, int next) {
    PowershieldData *data = Data();
    if (ft != data->fighter || ft->ply != 0 || ft->flags.ms) return;
    if (ft->spawn_num != data->spawn || ft->flags.dead || ft->flags.sleep) { ClearTiming(); return; }
    if (phase == 1) { data->in_iasa = 0; return; }
    if (ft->flags.hitlag || ft->flags.freeze || ft->dmg.hitlag_frames > 0) {
        if (phase == 4) { data->cue_color = 0; ApplyCue(); }
        return;
    }
    int tick = stc_match->time_frames;
    if (phase == 4) {
        if (data->seen_tick && tick < data->tick) { ClearTiming(); return; }
        if (data->seen_tick && tick == data->tick) return;
        data->seen_tick = 1; data->tick = tick;
        int state = CommonState(ft, ft->state_id);
        int ready = data->kind == TM_LASER_HIT ? !ft->flags.hitstun : state != ASID_GUARDSETOFF;
        if (ready && !data->ready) {
            data->ready = 1;
            data->cue_green = data->kind == TM_LASER_POWERSHIELD ? 4 : 2;
        } else if (data->cue_green) --data->cue_green;
        data->cue_color = 0;
        if (data->kind == TM_LASER_HIT) {
            float stun;
            memcpy(&stun, &ft->state_var.state_var1, sizeof(stun));
            if (ft->flags.hitstun && stun > 0 && stun <= 2) data->cue_color = TM_CUE_YELLOW;
        } else if (state == ASID_GUARDSETOFF && ft->state.rate > 0) {
            /* Shieldstun is a rate-scaled animation, not a hitstun timer.
             * Read its native boundary (including animation blending) and
             * wait for the real state exit before showing recovery green. */
            float end = Fighter_GetCurrentAnimLength(ft->fighter);
            if (end > 0 && ft->state.frame >= 0 && ft->state.frame < end &&
                ft->state.frame + 2 * ft->state.rate >= end)
                data->cue_color = TM_CUE_YELLOW;
        }
        if (!data->cue_color && data->cue_green) data->cue_color = TM_CUE_GREEN;
        ApplyCue();
    } else if (phase == 0 && data->armed && data->ready) {
        data->in_iasa = 1; data->input_tick = tick; data->input_state = ft->state_id;
        if (data->opportunity_tick != tick) {
            data->opportunity_tick = tick;
            if (data->opportunities < 32767) ++data->opportunities;
        }
    } else if (phase == 2 && data->armed && data->ready && data->in_iasa &&
        data->input_tick == tick && data->input_state == ft->state_id && next != ft->state_id &&
        !(next >= ASID_DAMAGEHI1 && next <= ASID_DAMAGEFLYROLL) &&
        next > ASID_REBIRTHWAIT && !IgnoreLaserAction(ft, next)) {
        data->act_frame = data->opportunities; data->act_kind = data->kind;
        data->act_fail = next == ASID_ESCAPE ? ACT_FAIL_DODGE :
            next == ASID_ESCAPEF || next == ASID_ESCAPEB ? ACT_FAIL_ROLL : ACT_OK;
        data->armed = data->in_iasa = 0;
    }
}
/* The rows that name the CPU's shot, and the ones only one CPU uses. Final
 * Destination has no side platform, so Samus's patterns with one are N/A there. */
static void LabelRows(void) {
    int samus = CPUIsSamus();
    Options_Main[OPT_LASER_HEIGHT].name = samus ? "Missile Height" : "Laser Height";
    Options_Main[OPT_LASER_HEIGHT].desc[0] = samus ? "Change the missile height." : "Change the laser height.";
    Options_Main[OPT_LASER_HEIGHT].desc[1] = samus ? "Short hop and falling missiles both use it." : 0;
    Options_Main[OPT_OVERLAY].name = samus ? "Missile Recovery Cue" : "Laser Recovery Cue";
    Options_Main[OPT_OVERLAY].desc[1] = samus ? "Green on recovery, including reflected missiles." :
        "Green on recovery, including reflected lasers.";
    Options_Main[OPT_OSDS].desc[0] = samus ? "Show Act out of Missile and Powershield side by side." :
        "Show Act out of Laser and Powershield side by side.";
    Options_Main[OPT_SAMUS_PATTERN].disable = !samus;
    Options_Main[OPT_SAMUS_PATTERN].values = Stage_GetExternalID() == GRKINDEXT_FD ?
        Options_SamusPatternFD : Options_SamusPattern;
    Options_Main[OPT_MOVEMENT].disable = samus;
    Options_Main[OPT_MOVEMENT].desc[1] = samus ? "Falco only: Samus shoots from where she stands." : 0;
}
void Event_Init(GOBJ *event) {
    PowershieldData *data = event->userdata;
    EventDesc *desc = data->desc;
    memset(data, 0, sizeof(*data)); data->desc = desc;
    restarting = 0;
    for (unsigned i = 0; i < TM_PS_PREF_COUNT; ++i) {
        EventOption *option = &Options_Main[preference_rows[i]];
        option->val = option->val_prev = TM_GetSetting(TM_SETTING_POWERSHIELD, i);
    }
    if (Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val > Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val)
        Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val_prev = Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val;
    Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].disable = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].disable =
        Options_Main[OPT_FIRE_SPEED].val != FIRE_SPEED_RANDOM;
    Menu_Main.cursor = Menu_Main.scroll = help_menu.cursor = help_menu.scroll = 0;
    Menu_Characters.cursor = Menu_Characters.scroll = 0;
    StageSwap_Init();
    FighterSwap_Restrict(1, Powershield_CPUKinds, countof(Powershield_CPUKinds));
    FighterSwap_Init();
    FighterSwap_Options[1].desc[2] = "Left/Right: Falco or Samus. X/Y: costume color. A: restart the match.";
    LabelRows();
    event_vars->set_osd_suppression(&Options_Main[OPT_SUPPRESS].val);
    event_vars->set_powershield_observer(LaserContact, LaserTiming, ClearTiming);
    ClearTiming();
    int canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 81, 19);
    GXColor white = {255,255,255,255};
    for (int i = 0; i < 2; ++i) {
        Text *text = data->text[i] = Text_CreateText(2, canvas);
        if (!text) continue;
        text->kerning = text->align = 1;
        text->trans = (Vec3){i ? 385 : 255, 100, 0};
        text->viewport_scale.X = text->viewport_scale.Y = 1;
        text->scale.X = text->scale.Y = .45f;
        Text_AddSubtext(text, 0, 0, i ? "Powershield" : CPUIsSamus() ? "Act out of Missile" : "Act out of Laser");
        Text_AddSubtext(text, 0, 22, "-");
        Text_AddSubtext(text, 0, 44, "-");
        for (int row = 0; row < 3; ++row) Text_SetColor(text, row, &white);
    }
}
void Event_Update(void) {
    PowershieldData *data = Data();
    int paused = ((MenuData *)event_vars->menu_gobj->userdata)->mode == MenuMode_Paused;
    static const char *laser_sources[] = {"Laser hit", "Shield hit", "Powershield"};
    static const char *missile_sources[] = {"Missile hit", "Shield hit", "Powershield"};
    const char **sources = CPUIsSamus() ? missile_sources : laser_sources;
    static const char *reasons[] = {"Reflected laser", "Reflect active", "Reflect ended", "No hard press", "No shield"};
    static const char *fails[] = {"", "ROLL", "DODGE"};
    GXColor white = {255,255,255,255}, cyan = {0,255,255,255}, green = {141,255,110,255}, yellow = {255,240,0,255}, red = {255,80,80,255};
    GXColor fail = {255,162,186,255}; /* Jump Out Of Shine's FAIL color. */
    for (int i = 0; i < 2; ++i) if (data->text[i]) data->text[i]->hidden = paused || !Options_Main[OPT_OSDS].val;
    if (data->text[0]) {
        if (data->act_frame < 0) Text_SetText(data->text[0], 1, "-");
        else if (data->act_fail) Text_SetText(data->text[0], 1, "%s", fails[data->act_fail]);
        else Text_SetText(data->text[0], 1, "%df", data->act_frame);
        Text_SetText(data->text[0], 2, "%s", data->ps_valid ? sources[data->act_kind] : "-");
        Text_SetColor(data->text[0], 1, data->act_frame < 0 ? &white : data->act_fail ? &fail : data->act_frame == 1 ? &cyan :
            data->act_frame == 2 ? &green : data->act_frame == 3 ? &yellow : &red);
    }
    if (data->text[1]) {
        if (!data->ps_valid || !data->ps_frame || data->ps_kind == TM_LASER_HIT) Text_SetText(data->text[1], 1, "-");
        else Text_SetText(data->text[1], 1, data->ps_kind == TM_LASER_POWERSHIELD ? "Success: %df" : "Shield: %df", data->ps_frame);
        Text_SetText(data->text[1], 2, "%s", data->ps_valid ? (data->ps_reason == TM_PS_SUCCESS && CPUIsSamus() ?
            "Reflected missile" : reasons[data->ps_reason]) : "-");
        Text_SetColor(data->text[1], 1, !data->ps_valid ? &white : data->ps_kind == TM_LASER_POWERSHIELD ? &green : &red);
    }
}
static void ChangePreference(GOBJ *menu, int value) {
    for (unsigned i = 0; i < TM_PS_PREF_COUNT; ++i)
        TM_SetSetting(TM_SETTING_POWERSHIELD, i, Options_Main[preference_rows[i]].val);
}
static void ChangeOverlay(GOBJ *menu, int value) { ChangePreference(menu, value); ApplyCue(); }

/* Live stage geometry. */
static float SideScale(int side) {
    float left, right;
    GroundEdges(&left, &right);
    return (side < 0 ? -left : right) / FD_EDGE;
}
static float FloorBelow(float x) {
    Vec3 ground;
    int line;
    return FindSolidGround(x, &ground, &line) ? ground.Y : 0;
}
static int OnPlatform(FighterData *ft) {
    Vec2 a, b;
    int platform;
    return FloorLine(ft->coll_data.ground_index, &a, &b, &platform) && platform;
}
/* A platform over x that a jump reaching `reach` above the floor could land on. */
static int PlatformOver(float x, float margin, float reach, Vec2 *left, Vec2 *right) {
    float floor = FloorBelow(x);
    int fountain = Stage_GetExternalID() == GRKINDEXT_IZUMI;
    for (int i = 0; i < LineCount(); ++i) {
        Vec2 a, b;
        int platform;
        if (!FloorLine(i, &a, &b, &platform) || !platform) continue;
        float y = a.Y > b.Y ? a.Y : b.Y;
        if (fountain && fabs(a.X + b.X) > 20 && y > FOUNTAIN_SIDE_LOWEST) y = FOUNTAIN_SIDE_LOWEST;
        if (x > a.X - margin && x < b.X + margin && y > floor && y - floor < reach) {
            if (left) *left = a;
            if (right) *right = b;
            return 1;
        }
    }
    return 0;
}
/* The nearest spot to x, on the right half, with no platform a short hop
 * reaching `reach` could land on: outward if there's room, else inward. */
static float FreeSpot(float x, float reach) {
    float left, right;
    GroundEdges(&left, &right);
    for (int tries = 0; tries < 4; ++tries) {
        Vec2 a, b;
        if (!PlatformOver(x, SPOT_MARGIN, reach, &a, &b)) break;
        x = b.X + SPOT_MARGIN <= right - SPOT_EDGE ? b.X + SPOT_MARGIN : a.X - SPOT_MARGIN;
    }
    return x;
}
/* The CPU's start on the right half (Direction Right; Reset mirrors it for
 * Left): FD's +60 scaled to the stage, off any platform its short hop could
 * catch. Samus's Platform and Random patterns stand her under the right side
 * platform instead, when there is one her full hop reaches. */
static float CPUStartX(FighterData *cpu) {
    float x = START_X * SideScale(1);
    if (!IsSamus(cpu)) return FreeSpot(x, FALCO_SHORT_HOP_REACH);
    Vec3 platform;
    int line;
    if (Options_Main[OPT_SAMUS_PATTERN].val != TM_PS_SAMUS_SHORT_HOP && FindPlatform(1, &platform, &line) &&
        platform.Y - FloorBelow(platform.X) < SAMUS_FULL_HOP_REACH)
        return platform.X;
    return FreeSpot(x, SAMUS_SHORT_HOP_REACH);
}
/* Standing on the live floor at x, as Low Laser places its fighters. */
static void GroundFighter(GOBJ *fighter, float x) {
    FighterData *ft = fighter->userdata;
    Vec3 ground;
    int line;
    GroundPoint(x, &ground, &line);
    GroundAt(fighter, ground, line);
    Fighter_EnterWait(fighter);
    Fighter_UpdateCameraBox(fighter);
    CmSubject *subject = ft->camera_subject;
    subject->boundtop_curr = subject->boundtop_proj;
    subject->boundbottom_curr = subject->boundbottom_proj;
    subject->boundleft_curr = subject->boundleft_proj;
    subject->boundright_curr = subject->boundright_proj;
}
static void ClearPlans(void) {
    falco_wait_delay = -1;
    falco_shoot_delay = -1;
    falco_fastfall_delay = -1;
    falco_dash_direction = 0;
    falco_jump_direction = 0;
    falco_shoot_direction = 0;
    samus = (SamusPlan){ .wait = -1 };
}
/* Place both fighters as Direction Right has them and keep it as the event's
 * savestate: you on the left facing left, as before, and the CPU on the right
 * facing you. */
static void SaveStart(void) {
    GOBJ *player = Fighter_GetGObj(0), *cpu = Fighter_GetGObj(1);
    FighterData *hmn = player->userdata, *ft = cpu->userdata;
    hmn->facing_direction = -1;
    GroundFighter(player, -START_X * SideScale(-1));
    ft->facing_direction = -1;
    GroundFighter(cpu, CPUStartX(ft));
    Match_CorrectCamera();
    event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
}
static void ChangeSamusPattern(GOBJ *menu, int value) {
    ChangePreference(menu, value);
    /* Platform stands her under a platform and Short Hop away from one. */
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);
    SaveStart();
    Reset();
}

/* Falco's lasers. Distances that keep him on the stage scale like START_X. */
static void FalcoThink(FighterData *player_data, FighterData *falco_data) {
    int state = falco_data->state_id;
    int state_frame = falco_data->TM.state_frame;

    bool ground_actionable = (state == ASID_LANDING && state_frame > 4) || state == ASID_WAIT;
    bool in_turn = state == ASID_TURN;
    bool in_dash = state == ASID_DASH;
    bool in_jumpsquat = state == ASID_KNEEBEND;
    bool air_actionable = state == ASID_JUMPF || state == ASID_JUMPB || (state == ASID_KNEEBEND && state_frame == 5);
    bool can_fastfall = falco_data->phys.air_state == 1 && falco_data->phys.self_vel.Y <= 0.f;

    if (ground_actionable && falco_wait_delay > 0)
        falco_wait_delay--;

    if (air_actionable && falco_shoot_delay > 0)
        falco_shoot_delay--;

    if (can_fastfall && falco_fastfall_delay > 0)
        falco_fastfall_delay--;


    if (ground_actionable && falco_wait_delay == -1) {
        // set wait timer

        int delay_option = Options_Main[OPT_FIRE_SPEED].val;

        if (delay_option == FIRE_SPEED_RANDOM)
            falco_wait_delay = GetRandomLaserDelay();
        else if (delay_option == FIRE_SPEED_SLOW)
            falco_wait_delay = 20;
        else if (delay_option == FIRE_SPEED_MEDIUM)
            falco_wait_delay = 10;
        else if (delay_option == FIRE_SPEED_FAST)
            falco_wait_delay = 0;
    }

    if (ground_actionable && falco_wait_delay == 0) {
        // choose shoot type
        if (falco_shoot_direction == 0) {
            float falco_x = falco_data->phys.pos.X;
            float player_x = player_data->phys.pos.X;
            float left_x = -LEDGE_X * SideScale(-1), right_x = LEDGE_X * SideScale(1);

            float approaching_dir = player_x < falco_x ? -1.f : 1.f;
            float retreating_dir = -approaching_dir;
            #define ON_STAGE(x) ((x) > left_x && (x) < right_x)
            bool can_in_place = true;
            bool can_full_approach = ON_STAGE(falco_x + FULL_FALCO_SHORTHOP_DISTANCE*approaching_dir);
            bool can_slight_approach = ON_STAGE(falco_x + SLIGHT_FALCO_SHORTHOP_DISTANCE*approaching_dir);
            bool can_full_retreat = ON_STAGE(falco_x + FULL_FALCO_SHORTHOP_DISTANCE*retreating_dir);
            bool can_slight_retreat = ON_STAGE(falco_x + SLIGHT_FALCO_SHORTHOP_DISTANCE*retreating_dir);
            #undef ON_STAGE

            typedef struct {
                int in_place_odds;
                int full_approaching_odds;
                int slight_approaching_odds;
                int full_retreating_odds;
                int slight_retreating_odds;
            } ShootOdds;

            static ShootOdds odds_table[] = {
                { 1, 0, 0, 0, 0 }, // MOVEMENT_IN_PLACE
                { 4, 3, 3, 1, 3 }, // MOVEMENT_RANDOM
                { 2, 4, 3, 0, 2 }, // MOVEMENT_APPROACHING
                { 4, 0, 2, 2, 4 }, // MOVEMENT_RETREATING
            };

            int movement = Options_Main[OPT_MOVEMENT].val;
            ShootOdds *odds = &odds_table[movement];

            int odds_total = 0;
            if (can_in_place) odds_total += odds->in_place_odds;
            if (can_full_approach) odds_total += odds->full_approaching_odds;
            if (can_slight_approach) odds_total += odds->slight_approaching_odds;
            if (can_full_retreat) odds_total += odds->full_retreating_odds;
            if (can_slight_retreat) odds_total += odds->slight_retreating_odds;

            int dir_to_player = falco_data->phys.pos.X < player_data->phys.pos.X ? 1 : -1;

            int rng = HSD_Randi(odds_total);
            if (can_in_place && (rng -= odds->in_place_odds) < 0) {
                falco_shoot_direction = dir_to_player;
                falco_dash_direction = 0;
                falco_jump_direction = 0;
            }
            else if (can_full_approach && (rng -= odds->full_approaching_odds) < 0) {
                if (fabs(falco_data->phys.pos.X - player_data->phys.pos.X) > FULL_FALCO_SHORTHOP_DISTANCE) {
                    falco_shoot_direction = dir_to_player;
                } else {
                    falco_shoot_direction = -dir_to_player;
                }
                falco_dash_direction = dir_to_player;
                falco_jump_direction = dir_to_player;
            }
            else if (can_slight_approach && (rng -= odds->slight_approaching_odds) < 0) {
                if (fabs(falco_data->phys.pos.X - player_data->phys.pos.X) > SLIGHT_FALCO_SHORTHOP_DISTANCE) {
                    falco_shoot_direction = dir_to_player;
                } else {
                    falco_shoot_direction = -dir_to_player;
                }
                falco_dash_direction = dir_to_player;
                falco_jump_direction = 0;
            }
            else if (can_full_retreat && (rng -= odds->full_retreating_odds) < 0) {
                falco_shoot_direction = dir_to_player;
                falco_dash_direction = -dir_to_player;
                falco_jump_direction = -dir_to_player;
            }
            else if (can_slight_retreat && (rng -= odds->slight_retreating_odds) < 0) {
                falco_shoot_direction = dir_to_player;
                falco_dash_direction = -dir_to_player;
                falco_jump_direction = 0;
            }
            else {
                assert("no shoot type available!");
            }
        }

        // start movement
        falco_wait_delay = -1;

        if (falco_dash_direction == 0 || state == ASID_DASH) {
            // start jump

            falco_data->cpu.held |= PAD_BUTTON_Y;
        } else {
            // start dash

            falco_data->cpu.lstickX = 127 * falco_dash_direction;
        }
    }

    if (in_turn) {
        // finish dashback

        falco_data->cpu.lstickX = 127 * falco_dash_direction;
    }

    if (in_dash) {
        // start jump

        falco_data->cpu.held |= PAD_BUTTON_Y;
    }

    if (in_jumpsquat) {
        // maintain jump momentum

        falco_data->cpu.lstickX = 127 * falco_jump_direction;
    }

    if (air_actionable && falco_shoot_delay == -1) {
        // set shoot timer

        int delay_option = Options_Main[OPT_LASER_HEIGHT].val;

        if (delay_option == LASER_HEIGHT_RANDOM) {
            falco_shoot_delay = HSD_Randi(4) + 2;
            falco_fastfall_delay = 1;
        } else if (delay_option == LASER_HEIGHT_VERY_LOW) {
            falco_shoot_delay = 11;
            falco_fastfall_delay = 8;
        } else if (delay_option == LASER_HEIGHT_LOW) {
            falco_shoot_delay = 5;
            falco_fastfall_delay = 1;
        } else if (delay_option == LASER_HEIGHT_MID) {
            falco_shoot_delay = 4;
            falco_fastfall_delay = 1;
        } else if (delay_option == LASER_HEIGHT_HIGH) {
            falco_shoot_delay = 2;
            falco_fastfall_delay = 0;
        }
    }

    if (air_actionable && falco_shoot_delay > 1) {
        // drift before shooting

        falco_data->cpu.lstickX = 127 * falco_jump_direction;
    }

    if (air_actionable && falco_shoot_delay == 1) {
        // set laser direction 1f before shooting

        falco_data->cpu.lstickX = 80 * falco_shoot_direction;
        falco_shoot_direction = 0;
    }

    if (air_actionable && falco_shoot_delay == 0) {
        // start laser

        falco_shoot_delay = -1;
        falco_data->cpu.held |= PAD_BUTTON_B;
    }

    if (can_fastfall && falco_fastfall_delay == 0) {
        falco_data->cpu.lstickY = -127;
        falco_fastfall_delay = -1;
    }
}

/* Samus's super missiles, from raw inputs the game reads the next frame, like
 * Falco's. A super missile is B with the stick flicked fully sideways: it must
 * cross the smash threshold within 3 frames (PlSs special attribute 0x28), so
 * every press comes after a frame with the stick neutral, and never with the
 * stick up or down, which would be up-B or a bomb. She never fastfalls. */
static int SamusFireDelay(void) {
    int speed = Options_Main[OPT_FIRE_SPEED].val;
    return speed == FIRE_SPEED_RANDOM ? GetRandomLaserDelay() : speed == FIRE_SPEED_SLOW ? 20 :
        speed == FIRE_SPEED_MEDIUM ? 10 : 0;
}
static float SamusTarget(void) {
    int height = Options_Main[OPT_LASER_HEIGHT].val;
    if (height < LASER_HEIGHT_VERY_LOW || height > LASER_HEIGHT_HIGH) height = LASER_HEIGHT_VERY_LOW + HSD_Randi(4);
    return samus_heights[height - LASER_HEIGHT_VERY_LOW];
}
/* Where a missile pressed `later` frames from now would fly, above the floor
 * below her: her height SAMUS_MISSILE_DELAY frames after the press, falling as
 * the move falls (gravity to terminal speed, the same as a jump's or a
 * fall's), plus the cannon. */
static float SamusMissileHeight(FighterData *ft, int later, float *speed) {
    float y = ft->phys.pos.Y, v = ft->phys.self_vel.Y;
    float gravity = ft->attr.gravity, terminal = ft->attr.terminal_velocity;
    for (int i = 0; i < SAMUS_MISSILE_DELAY + later; ++i) {
        v -= gravity;
        if (v < -terminal) v = -terminal;
        y += v;
    }
    *speed = v;
    return y + SAMUS_MISSILE_RISE - FloorBelow(ft->phys.pos.X);
}
static int SamusCanShoot(int state) {
    return state == ASID_JUMPF || state == ASID_JUMPB || (state >= ASID_FALL && state <= ASID_FALLB) ||
        state == ASID_PASS;
}
static void SamusPress(FighterData *ft, int dir) {
    ft->cpu.lstickX = 127 * dir;
    ft->cpu.held |= PAD_BUTTON_B;
    samus.press = 1;
}
static void SamusThink(FighterData *player_data, FighterData *ft) {
    int state = ft->state_id;
    int dir = player_data->phys.pos.X < ft->phys.pos.X ? -1 : 1;
    int airborne = ft->phys.air_state == 1;
    if (samus.dropping) {
        if (airborne) samus.dropping = 0;
        /* The native drop didn't take: let go, and try again once she can act. */
        else if (++samus.dropping > SAMUS_DROP_LIMIT) { samus.dropping = 0; return; }
        else { ft->cpu.lstickY = -127; return; }
    }
    if (!airborne) {
        samus.fired = samus.press = 0;
        if (state == ASID_KNEEBEND) {
            if (samus.full_hop) ft->cpu.held |= PAD_BUTTON_Y;
            return;
        }
        samus.full_hop = samus.rising = 0;
        if (!((state == ASID_LANDING && ft->TM.state_frame > 4) || state == ASID_WAIT)) return;
        if (samus.wait < 0) samus.wait = SamusFireDelay();
        if (samus.wait > 0) { --samus.wait; return; }
        samus.wait = -1;
        samus.target = SamusTarget();
        if (OnPlatform(ft)) {
            /* Drop through: the stick from neutral to fully down. */
            samus.dropping = 1;
            ft->cpu.lstickY = -127;
            return;
        }
        int pattern = Options_Main[OPT_SAMUS_PATTERN].val;
        samus.platform = pattern != TM_PS_SAMUS_SHORT_HOP &&
            PlatformOver(ft->phys.pos.X, 0, SAMUS_FULL_HOP_REACH, 0, 0) &&
            (pattern == TM_PS_SAMUS_PLATFORM || HSD_Randi(2));
        samus.full_hop = samus.rising = samus.platform;
        ft->cpu.held |= PAD_BUTTON_Y;
        return;
    }
    if (state >= SAMUS_SPECIALS && state <= SAMUS_SPECIALAIRS_SMASH) { samus.fired = 1; samus.press = 0; return; }
    if (samus.fired || !SamusCanShoot(state)) return;
    /* An unconfirmed press gets a frame to start the move; then the stick goes
     * back to neutral for a frame before the next try. */
    if (samus.press) {
        if (++samus.press > 2) samus.press = 0;
        return;
    }
    /* The full hop's rising missile, on its first airborne frame. */
    if (samus.rising) {
        samus.rising = 0;
        SamusPress(ft, dir);
        return;
    }
    /* On the way down, on whichever frame comes closest to the height: now, if
     * it's already there or waiting a frame would miss by more. */
    float speed, later_speed, now = SamusMissileHeight(ft, 0, &speed);
    float next = SamusMissileHeight(ft, 1, &later_speed);
    if (speed < 0 && (now <= samus.target || (next < samus.target && samus.target - next > now - samus.target)))
        SamusPress(ft, dir);
}

void Event_Think(GOBJ *menu) {
    if (restarting) return;
    /* Only the six legal stages. The stage select's others, which Random can
     * pick, restart on Final Destination, the event's original stage. */
    if (!LegalStage()) {
        Detach();
        event_vars->restart_on_stage(GRKINDEXT_FD);
        return;
    }
    GOBJ *player = Fighter_GetGObj(0);
    FighterData *player_data = player->userdata;
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;

    if (event_vars->game_timer == 1) {
        // Disable Nana option for non-Ice Climbers
        Options_Main[OPT_ALLOW_NANA].disable = (player_data->kind != FTKIND_POPO);
        LabelRows();
        SaveStart();
        Reset();
    }

    Fighter_ZeroCPUInputs(cpu_data);
    cpu_data->flags.no_reaction_always = true;
    cpu_data->grab.vuln = 0x1FF;
    player_data->shield.health = 60;

    // Manage Nana based on option
    if (player_data->kind == FTKIND_POPO) {
        GOBJ *nana = Fighter_GetSubcharGObj(0, 1);
        int nana_option = Options_Main[OPT_ALLOW_NANA].val;

        if (nana_option == 0 && nana) {
            FighterData *nana_data = nana->userdata;
            if (nana_data->state_id != ASID_SLEEP) {
                Fighter_EnterSleep(nana, 1);
            }
            nana_data->phys.pos.X = 999.f;
            nana_data->phys.pos.Y = -999.f;
        }
    }

    int new_direction = -1;
    if (player_data->input.down & HSD_BUTTON_DPAD_LEFT) {
        new_direction = DIRECTION_RIGHT;
    } else if (player_data->input.down & HSD_BUTTON_DPAD_RIGHT) {
        new_direction = DIRECTION_LEFT;
    }
    if (new_direction != -1) {
        Options_Main[OPT_DIRECTION].val = Options_Main[OPT_DIRECTION].val_prev = new_direction;
        ChangePreference(menu, new_direction);
        Reset();
    }

    int state = cpu_data->state_id;
    if (ASID_DEADDOWN <= state && state <= ASID_REBIRTHWAIT)
        Reset();

    if (IsSamus(cpu_data)) SamusThink(player_data, cpu_data);
    else FalcoThink(player_data, cpu_data);
}

/* Restarting for another stage or fighters: let go of what Exit lets go of.
 * The restart service saves and ends the match. */
static void Detach(void) {
    restarting = 1;
    event_vars->set_powershield_observer(0, 0, 0);
    event_vars->set_osd_suppression(0);
    GOBJ *player = Fighter_GetGObj(0);
    if (player) event_vars->set_timing_overlay(player, 0);
}
static void StageSwap_BeforeRestart(void) { Detach(); }
static void FighterSwap_BeforeRestart(void) { Detach(); }

void Exit(GOBJ *menu) {
    event_vars->set_powershield_observer(0, 0, 0);
    event_vars->set_osd_suppression(0);
    GOBJ *player = Fighter_GetGObj(0);
    if (player) event_vars->set_timing_overlay(player, 0);
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

void ChangeFireSpeedOption(GOBJ *event_menu, int value) {
    ChangePreference(event_menu, value);
    bool disable_random_bounds = value != FIRE_SPEED_RANDOM;
    Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].disable = disable_random_bounds;
    Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].disable = disable_random_bounds;
}

/* Back to the start: the savestate, mirrored for Direction Left, with both
 * fighters standing on the live floor where it puts them. */
void Reset(void) {
    int direction = Options_Main[OPT_DIRECTION].val;
    int mirror = direction == DIRECTION_LEFT ? Savestate_Mirror : 0;
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent | mirror);

    ClearPlans();

    GOBJ *player = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);

    GroundFighter(player, ((FighterData *)player->userdata)->phys.pos.X);
    GroundFighter(cpu, ((FighterData *)cpu->userdata)->phys.pos.X);
    Match_CorrectCamera();
}

void ChangeDirection(GOBJ *event_menu, int value) {
    ChangePreference(event_menu, value);
    Reset();
}

void ChangeRandomFireDelayMin(GOBJ *event_menu, int value) {
    int random_fire_delay_min = Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val;
    int random_fire_delay_max = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val;

    // Ensure the min is never greater than the max
    if (random_fire_delay_min > random_fire_delay_max) {
        Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val_prev = random_fire_delay_min;
    }
    ChangePreference(event_menu, value);
}

void ChangeRandomFireDelayMax(GOBJ *event_menu, int value) {
    int random_fire_delay_min = Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val;
    int random_fire_delay_max = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val;

    // Ensure the max is never less than the min
    if (random_fire_delay_max < random_fire_delay_min) {
        Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val = Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val_prev = random_fire_delay_max;
    }
    ChangePreference(event_menu, value);
}

int GetRandomLaserDelay(void) {
    int random_fire_delay_min = Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val;
    int random_fire_delay_max = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val;

    return HSD_Randi(random_fire_delay_max - random_fire_delay_min + 1) + random_fire_delay_min;
}

void ChangeAllowNana(GOBJ *event_menu, int value) {
    ChangePreference(event_menu, value);
    GOBJ *player = Fighter_GetGObj(0);
    if (!player) return;

    FighterData *player_data = player->userdata;
    if (player_data->kind != FTKIND_POPO) return;

    GOBJ *nana = Fighter_GetSubcharGObj(0, 1);

    if (value == 1 && nana) {
        FighterData *nana_data = nana->userdata;
        if (nana_data->state_id == ASID_SLEEP) {
            Fighter_EnterDeadDown(player);
        }
    }
}

EventMenu *Event_Menu = &Menu_Main;
