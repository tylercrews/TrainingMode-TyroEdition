#include "../MexTK/mex.h"
#include "events.h"
#include "powershield_osd.h"

#define LEDGE_X 70.f
#define FULL_FALCO_SHORTHOP_DISTANCE 30.f
#define SLIGHT_FALCO_SHORTHOP_DISTANCE 13.f

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

enum menu_options {
    OPT_FIRE_SPEED,
    OPT_FIRE_DELAY_RANDOM_MIN,
    OPT_FIRE_DELAY_RANDOM_MAX,
    OPT_LASER_HEIGHT,
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

static EventOption help_options[] = {
    {.kind = OPTKIND_INFO, .name = "Hard Press L / R",
     .desc = {"Click L or R fully just before the laser arrives.",
              "Analog light shield alone does not start reflection.",
              "Release, then press crisply; avoid a slow squeeze."}},
    {.kind = OPTKIND_INFO, .name = "Two Reflect Frames",
     .desc = {"The projectile reflector lasts frames 1 and 2.",
              "The normal shield and reflector are different shapes.",
              "A shield hit during reflection is not proof of early input.",
              "Try a different spacing, posture, or laser height."}},
    {.kind = OPTKIND_INFO, .name = "Read Powershield",
     .desc = {"Success means Melee accepted a reflected projectile.",
              "The frame counts from hard-shield startup.",
              "Reflect active / shield hit: check spacing or height.",
              "No hard press: no eligible click. Reflect ended: late."}},
    {.kind = OPTKIND_INFO, .name = "After Laser Reflection",
     .desc = {"Reflected lasers cause no shieldstun.",
              "Green marks recovery; normal shield rules still apply.",
              "Releasing shield still has normal shield-drop lag.",
              "You cannot instantly drop shield into a dash."}},
    {.kind = OPTKIND_INFO, .name = "Choose a Follow-up",
     .desc = {"Jump into an aerial or wavedash to reposition.",
              "Or grab, dodge, or jump-cancel into up-smash / up-B.",
              "Wavedashing skips shield-drop lag, but has landing lag.",
              "Choose based on spacing and your character's options."}},
    {.kind = OPTKIND_INFO, .name = "Physical vs Projectile",
     .desc = {"Physical powershields still have shieldstun.",
              "Afterward, attacks can cancel their shield release.",
              "Reflected lasers do not grant that attack cancel.",
              "Neither gives an instant shield-drop dash."}},
    {.kind = OPTKIND_INFO, .name = "Read Act out of Laser",
     .desc = {"Frame 1 is your first unfrozen chance after laser stun.",
              "Laser hit, Shield hit, or Powershield identifies the source.",
              "Shielding and walking do not finish the count.",
              "1f Cyan, 2f Green, 3f Yellow, 4+f Red."}},
    {.kind = OPTKIND_INFO, .name = "Laser Recovery Cue",
     .desc = {"Yellow: final two frames of laser hitstun / shieldstun.",
              "Green: first 2 recovery frames; 4 for reflected lasers.",
              "Both your character and shield receive the cue.",
              "Green while shielding does not mean an instant dash."}},
};
static EventMenu help_menu = {.name = "Powershield Help", .option_num = countof(help_options), .options = help_options};

static EventOption Options_Main[] = {
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
        .desc = {"Adjust the minimum number of frames between lasers"},
        .format = "%d",
        .OnChange = ChangeRandomFireDelayMin,
        .disable = false,
    },
    {
        .kind = OPTKIND_INT,
        .value_num = 61,
        .val = 20,
        .name = "Max Fire Delay",
        .desc = {"Adjust the maximum number of frames between lasers"},
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
        .value_num = sizeof(Options_Direction) / 4,
        .name = "Direction",
        .desc = {"Change which way falco shoots a laser."},
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

static void PutOnGround(GOBJ *ft);

typedef struct PowershieldData {
    EventDesc *desc;
    FighterData *fighter;
    int spawn, kind, armed, ready, opportunities, opportunity_tick, input_tick, input_state, in_iasa;
    int tick, seen_tick, cue_green, cue_color;
    int act_frame, act_kind, ps_valid, ps_kind, ps_frame, ps_reason;
    Text *text[2];
} PowershieldData;
typedef char powershield_data_fits[(sizeof(PowershieldData) <= EVENT_DATASIZE) ? 1 : -1];
static PowershieldData *Data(void) { return event_vars->event_gobj->userdata; }
static const unsigned preference_rows[TM_PS_PREF_COUNT] = {
    OPT_FIRE_SPEED, OPT_FIRE_DELAY_RANDOM_MIN, OPT_FIRE_DELAY_RANDOM_MAX, OPT_LASER_HEIGHT,
    OPT_DIRECTION, OPT_MOVEMENT, OPT_ALLOW_NANA, OPT_OSDS, OPT_SUPPRESS, OPT_OVERLAY,
};
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
    data->act_frame = -1;
    ApplyCue();
}
static void LaserContact(FighterData *ft, int kind, int frame, int reason) {
    if (ft->ply != 0 || ft->flags.ms) return;
    PowershieldData *data = Data();
    data->fighter = ft; data->spawn = ft->spawn_num;
    data->kind = kind; data->armed = 1; data->ready = data->opportunities = data->in_iasa = 0;
    data->opportunity_tick = data->input_tick = -1;
    data->act_frame = -1; data->act_kind = kind;
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
        data->armed = data->in_iasa = 0;
    }
}
void Event_Init(GOBJ *event) {
    PowershieldData *data = event->userdata;
    EventDesc *desc = data->desc;
    memset(data, 0, sizeof(*data)); data->desc = desc;
    for (unsigned i = 0; i < TM_PS_PREF_COUNT; ++i) {
        EventOption *option = &Options_Main[preference_rows[i]];
        option->val = option->val_prev = TM_GetSetting(TM_SETTING_POWERSHIELD, i);
    }
    if (Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val > Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val)
        Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].val_prev = Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].val;
    Options_Main[OPT_FIRE_DELAY_RANDOM_MIN].disable = Options_Main[OPT_FIRE_DELAY_RANDOM_MAX].disable =
        Options_Main[OPT_FIRE_SPEED].val != FIRE_SPEED_RANDOM;
    Menu_Main.cursor = Menu_Main.scroll = help_menu.cursor = help_menu.scroll = 0;
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
        Text_AddSubtext(text, 0, 0, i ? "Powershield" : "Act out of Laser");
        Text_AddSubtext(text, 0, 22, "-");
        Text_AddSubtext(text, 0, 44, "-");
        for (int row = 0; row < 3; ++row) Text_SetColor(text, row, &white);
    }
}
void Event_Update(void) {
    PowershieldData *data = Data();
    int paused = ((MenuData *)event_vars->menu_gobj->userdata)->mode == MenuMode_Paused;
    static const char *sources[] = {"Laser hit", "Shield hit", "Powershield"};
    static const char *reasons[] = {"Reflected laser", "Reflect active", "Reflect ended", "No hard press", "No shield"};
    GXColor white = {255,255,255,255}, cyan = {0,255,255,255}, green = {141,255,110,255}, yellow = {255,240,0,255}, red = {255,80,80,255};
    for (int i = 0; i < 2; ++i) if (data->text[i]) data->text[i]->hidden = paused || !Options_Main[OPT_OSDS].val;
    if (data->text[0]) {
        if (data->act_frame < 0) Text_SetText(data->text[0], 1, "-");
        else Text_SetText(data->text[0], 1, "%df", data->act_frame);
        Text_SetText(data->text[0], 2, "%s", data->ps_valid ? sources[data->act_kind] : "-");
        Text_SetColor(data->text[0], 1, data->act_frame < 0 ? &white : data->act_frame == 1 ? &cyan :
            data->act_frame == 2 ? &green : data->act_frame == 3 ? &yellow : &red);
    }
    if (data->text[1]) {
        if (!data->ps_valid || !data->ps_frame || data->ps_kind == TM_LASER_HIT) Text_SetText(data->text[1], 1, "-");
        else Text_SetText(data->text[1], 1, data->ps_kind == TM_LASER_POWERSHIELD ? "Success: %df" : "Shield: %df", data->ps_frame);
        Text_SetText(data->text[1], 2, "%s", data->ps_valid ? reasons[data->ps_reason] : "-");
        Text_SetColor(data->text[1], 1, !data->ps_valid ? &white : data->ps_kind == TM_LASER_POWERSHIELD ? &green : &red);
    }
}
static void ChangePreference(GOBJ *menu, int value) {
    for (unsigned i = 0; i < TM_PS_PREF_COUNT; ++i)
        TM_SetSetting(TM_SETTING_POWERSHIELD, i, Options_Main[preference_rows[i]].val);
}
static void ChangeOverlay(GOBJ *menu, int value) { ChangePreference(menu, value); ApplyCue(); }

void Event_Think(GOBJ *menu) {
    GOBJ *player = Fighter_GetGObj(0);
    FighterData *player_data = player->userdata;
    GOBJ *falco = Fighter_GetGObj(1);
    FighterData *falco_data = falco->userdata;

    if (event_vars->game_timer == 1) {
        // Disable Nana option for non-Ice Climbers
        Options_Main[OPT_ALLOW_NANA].disable = (player_data->kind != FTKIND_POPO);
        
        player_data->facing_direction = -1;
        PutOnGround(player);
        PutOnGround(falco);
        Match_CorrectCamera();

        event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
        Reset();
    }

    Fighter_ZeroCPUInputs(falco_data);
    falco_data->flags.no_reaction_always = true;
    falco_data->grab.vuln = 0x1FF;
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

    int state = falco_data->state_id;
    int state_frame = falco_data->TM.state_frame;
    
    if (ASID_DEADDOWN <= state && state <= ASID_REBIRTHWAIT)
        Reset();

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
        
            float approaching_dir = player_x < falco_x ? -1.f : 1.f;
            float retreating_dir = -approaching_dir;
            bool can_in_place = true;
            bool can_full_approach = fabs(falco_x + FULL_FALCO_SHORTHOP_DISTANCE*approaching_dir) < LEDGE_X;
            bool can_slight_approach = fabs(falco_x + SLIGHT_FALCO_SHORTHOP_DISTANCE*approaching_dir) < LEDGE_X;
            bool can_full_retreat = fabs(falco_x + FULL_FALCO_SHORTHOP_DISTANCE*retreating_dir) < LEDGE_X;
            bool can_slight_retreat = fabs(falco_x + SLIGHT_FALCO_SHORTHOP_DISTANCE*retreating_dir) < LEDGE_X;

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

static void PutOnGround(GOBJ *ft) {
    FighterData *ft_data = ft->userdata;
    ft_data->coll_data.ground_index = 1;

    Vec3 pos = { ft_data->phys.pos.X, 0, 0 };
    ft_data->phys.pos = pos;
    ft_data->coll_data.topN_Curr = pos;
    ft_data->coll_data.topN_CurrCorrect = pos;
    ft_data->coll_data.topN_Prev = pos;
    ft_data->coll_data.topN_Proj = pos;
    ft_data->coll_data.coll_test = *stc_colltest;

    JOBJ *jobj = ft->hsd_object;
    jobj->trans = pos;
    JOBJ_SetMtxDirtySub(jobj);
    
    Fighter_SetPosition(ft_data->ply, ft_data->flags.ms, &ft_data->phys.pos);

    EnvironmentCollision_WaitLanding(ft);
    Fighter_SetGrounded(ft_data);
    Fighter_EnterWait(ft);

    Fighter_UpdateCameraBox(ft);
    CmSubject *subject = ft_data->camera_subject;
    subject->boundtop_curr = subject->boundtop_proj;
    subject->boundbottom_curr = subject->boundbottom_proj;
    subject->boundleft_curr = subject->boundleft_proj;
    subject->boundright_curr = subject->boundright_proj;
}

void Reset(void) {
    int direction = Options_Main[OPT_DIRECTION].val;
    int mirror = direction == DIRECTION_LEFT ? Savestate_Mirror : 0;
    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent | mirror);

    falco_wait_delay = -1;
    falco_shoot_delay = -1;
    falco_fastfall_delay = -1;
    falco_dash_direction = 0;
    falco_jump_direction = 0;
    falco_shoot_direction = 0;
    
    GOBJ *player = Fighter_GetGObj(0);
    GOBJ *falco = Fighter_GetGObj(1);
    
    PutOnGround(player);
    PutOnGround(falco);
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
