#include "events.h"
#include "powershield_osd.h"
#include "shield_training.h"
#include "techchase_logic.h"
#include <stddef.h>

/* The original attack routines consume this small menu adapter, never the
 * modern menu's userdata. Keep the function table at its existing 50 slots. */
typedef char shield_bridge_offset[(offsetof(EventVars, shield_training) == 0xAC) ? 1 : -1];
extern EventDesc Reversal, AttackOnShield;
static u8 native_menu[0x50];
static const u8 native_info[] = {3, 0, 0, 0};
static int reversal, pending, hits, hit_tick, shield_di_direction;
enum { AOS_IDLE, AOS_JUMP, AOS_AIR_ATTACK, AOS_JUMP_CANCEL, AOS_DODGE_START, AOS_DODGE_END, AOS_FINISHED };
static int action_phase;
enum { REV_IDLE, REV_JUMP, REV_DODGE_START, REV_DODGE_END, REV_FINISHED };
static int reversal_phase;
static const char *distances[] = {"Right Next To", "Close", "Medium", "Past Tech Roll"};
static const char *attacks[] = {
    "Random Smash Attack", "Forward Smash", "Down Smash", "Up Smash",
    "Random Aerial", "Fair", "Nair", "Dair", "FTilt", "DTilt", "UTilt",
    "Dash Attack", "Getup Attack (Stomach)", "Getup Attack (Back)",
    "Grab", "Run Up JC Grab", "Spotdodge Shine"
};
typedef char rev_action_count[(countof(attacks)==REV_ACTION_COUNT) ? 1 : -1];
static const char *actions[] = {"Grab", "Short Hop Nair", "Up B", "Up Smash", "Shine Out of Shield",
    "Spotdodge", "Roll Away", "Roll Towards", "Wavedash Away", "None",
    "Short Hop Fair", "Short Hop Dair", "Short Hop Bair", "Short Hop Uair", "Spotdodge Shine"};
typedef char aos_action_count[(countof(actions)==AOS_ACTION_COUNT) ? 1 : -1];
static const char *shield_di[] = {"In", "Out"};
static const char *facings[] = {"Towards", "Away"};
static const char *positions[] = {"Ground", "Platform"};
static void Change(GOBJ *menu, int value);
static void Exit(GOBJ *menu) {
    if(stc_match->state==3) return;
    Powershield_SetObserver(0, 0, 0);
    ActionCues_SetInfiniteShields(0);
    if(menu && menu->userdata && ((MenuData *)menu->userdata)->mode==MenuMode_Paused)
        EventMenu_ExitMenu(menu);
    Memcard_SaveIfChanged();
    stc_match->state=3;
    Match_EndVS();
}
static EventOption rev_options[] = {
    {.kind=OPTKIND_STRING, .name="CPU Attack", .values=attacks, .value_num=countof(attacks),
     .OnChange=Change, .desc={"Choose the CPU attack to punish.", "Run Up JC Grab tracks your current position.", "Spotdodge Shine is intended for Fox/Falco.", "Your selection is saved."}},
    {.kind=OPTKIND_STRING, .name="Player Distance", .values=distances, .value_num=TC_DISTANCE_COUNT,
     .OnChange=Change, .desc={"Starting separation: 10, 20, 30 or 45 units.", "D-pad Left/Right fine-tunes the current spacing."}},
    {.kind=OPTKIND_STRING, .name="Player Facing", .values=facings, .value_num=2,
     .OnChange=Change, .desc={"Face towards or away from the CPU."}},
    {.kind=OPTKIND_STRING, .name="CPU Facing", .values=facings, .value_num=2,
     .OnChange=Change, .desc={"Have the CPU face towards or away from you."}},
    {.kind=OPTKIND_STRING, .name="Position", .values=positions, .value_num=2,
     .OnChange=Change, .desc={"Practice on the ground or an available platform."}},
    {.kind=OPTKIND_FUNC, .name="Exit", .OnSelect=Exit, .desc={"Return to the Event Selection Screen."}},
};
static EventOption aos_options[] = {
    {.kind=OPTKIND_STRING, .name="CPU Action", .values=actions, .value_num=countof(actions),
     .OnChange=Change, .desc={"Choose the CPU's response after blocking.", "Aerials use a short hop; Fair/Bair follow facing.", "Shine actions are intended for Fox/Falco."}},
    {.kind=OPTKIND_INT, .name="Hits Before Reaction", .value_min=1, .value_num=5, .val=1,
     .format="%d", .OnChange=Change, .desc={"React after blocking this many hits (1-5).", "Counts each shield hit, including multihits."}},
    {.kind=OPTKIND_TOGGLE, .name="Infinite Shields", .val=1, .OnChange=Change,
     .desc={"Keep both fighters' shields full while On.", "Overrides global Infinite Shields, including Off."}},
    {.kind=OPTKIND_STRING, .name="Player Distance", .values=distances, .value_num=TC_DISTANCE_COUNT,
     .OnChange=Change, .desc={"Starting separation: 10, 20, 30 or 45 units."}},
    {.kind=OPTKIND_STRING, .name="Shield DI", .values=shield_di, .value_num=countof(shield_di),
     .OnChange=Change, .desc={"Hold in toward the player, or out away from them,", "during shieldstun and shield hitlag.", "Direction follows positions, including swapped sides."}},
    {.kind=OPTKIND_FUNC, .name="Exit", .OnSelect=Exit, .desc={"Return to the Event Selection Screen."}},
};
static EventMenu rev_menu = {.name="Reversal Training", .options=rev_options,
    .option_num=countof(rev_options), .on_exit=Exit};
static EventMenu aos_menu = {.name="Attack on Shield", .options=aos_options,
    .option_num=countof(aos_options), .on_exit=Exit};
static void ClearHits(void) { hits=0; hit_tick=-1; }
static void ClearAttempt(void) { ClearHits(); action_phase=AOS_IDLE; reversal_phase=REV_IDLE; shield_di_direction=0; }
static int TowardPlayer(FighterData *ft) {
    GOBJ *human=Fighter_GetSubcharGObj(0,0);
    if(human && human->userdata) {
        float delta=((FighterData *)human->userdata)->phys.pos.X-ft->phys.pos.X;
        if(delta) return delta>0?1:-1;
    }
    return ft->facing_direction<0?-1:1;
}
static void HoldShieldDI(FighterData *ft) {
    if(!shield_di_direction)
        shield_di_direction=TowardPlayer(ft)*(aos_options[TM_AOS_SHIELD_DI].val?-1:1);
    ft->cpu.held=PAD_TRIGGER_L|PAD_TRIGGER_R;
    ft->cpu.lstickX=shield_di_direction*127;
    ft->cpu.lstickY=ft->cpu.cstickX=ft->cpu.cstickY=0;
    ft->input.held=PAD_TRIGGER_L|PAD_TRIGGER_R|0x80000000u;
    ft->input.down=0;
    ft->input.lstick=(Vec2){shield_di_direction,0};
    ft->input.cstick=(Vec2){0,0};
    ft->input.trigger=1;
    /* A held direction is not a new tilt. In particular, retain the native
     * 254 "SDI consumed" marker instead of reopening its window every frame. */
}
static void QueueInput(FighterData *ft, int buttons, int x, int y, int cx, int cy) {
    ft->cpu.held=buttons;
    ft->cpu.lstickX=x; ft->cpu.lstickY=y;
    ft->cpu.cstickX=cx; ft->cpu.cstickY=cy;
    /* The event runs after input decoding. Match Ledgechase's native-input
     * adapter so IASA sees this command now, including grounded special caches. */
    unsigned processed=buttons;
    if(buttons&PAD_TRIGGER_Z) processed|=PAD_BUTTON_A|0x80000000u;
    if(buttons&(PAD_TRIGGER_L|PAD_TRIGGER_R)) processed|=0x80000000u;
    ft->input.down=processed&~ft->input.held;
    ft->input.held=processed;
    ft->input.lstick=(Vec2){x/127.f,y/127.f};
    ft->input.cstick=(Vec2){cx/127.f,cy/127.f};
    ft->input.trigger=processed&0x80000000u?1:0;
    ft->input.timer_specialhi=ft->input.timer_speciallw=ft->input.timer_specials=ft->input.timer_specialn=255;
    if(buttons&PAD_BUTTON_B) {
        ftCommonData *common=*stc_ftcommon;
        if(ft->input.lstick.Y>=common->x21c) ft->input.timer_specialhi=0;
        if(ft->input.lstick.Y< -common->x21c) ft->input.timer_speciallw=0;
        if(fabs(ft->input.lstick.X)>=common->x218) ft->input.timer_specials=0;
        if(fabs(ft->input.lstick.X)<common->x218 && fabs(ft->input.lstick.Y)<common->x21c) ft->input.timer_specialn=0;
    }
    if(x) ft->input.timer_lstick_tilt_x=0;
    if(y) ft->input.timer_lstick_tilt_y=0;
    if(processed&0x80000000u) ft->input.timer_trigger_analog=ft->input.timer_trigger_digital=0;
}
static void RunIASA(FighterData *ft) {
    if(ft && ft->fighter && ft->cb.IASA && !ft->flags.dead && !ft->flags.sleep &&
        !ft->flags.freeze && !ft->flags.hitlag && !ft->flags.hitstun && ft->dmg.hitlag_frames<=0)
        ft->cb.IASA(ft->fighter);
}
static int TickCPU(FighterData *ft) {
    if(!ft || reversal || ft->flags.dead || ft->flags.sleep) return SHIELD_INPUT_CONTINUE;
    int state=Powershield_CommonState(ft,ft->state_id);
    int action=aos_options[TM_AOS_ACTION].val;
    if(state==ASID_GUARDSETOFF) {
        HoldShieldDI(ft);
        return ft->flags.freeze || ft->flags.hitlag || ft->dmg.hitlag_frames>0?
            SHIELD_INPUT_FROZEN:SHIELD_INPUT_CONTINUE;
    }
    shield_di_direction=0;
    /* Neutralize the old direction before selecting a response. A horizontal
     * shield-DI input must never leak into GuardWait and trigger a roll. */
    QueueInput(ft,action_phase==AOS_IDLE?PAD_TRIGGER_L|PAD_TRIGGER_R:0,0,0,0,0);
    if(ft->flags.freeze || ft->flags.hitlag || ft->dmg.hitlag_frames>0) return SHIELD_INPUT_FROZEN;
    if(ft->flags.hitstun || (state>=ASID_DAMAGEHI1 && state<=ASID_DAMAGEFLYROLL)) {
        if(action_phase==AOS_IDLE) { ClearHits(); action_phase=AOS_FINISHED; return SHIELD_INPUT_START; }
        return SHIELD_INPUT_CONTINUE;
    }
    if(action_phase==AOS_IDLE) {
        if(state!=ASID_GUARD || hits<aos_options[TM_AOS_HITS].val) return SHIELD_INPUT_CONTINUE;
        ClearHits();
        if(action==AOS_NONE) return SHIELD_INPUT_CONTINUE;
        action_phase=AOS_FINISHED;
        int away=-TowardPlayer(ft);
        switch(action) {
            case AOS_GRAB: QueueInput(ft,PAD_TRIGGER_L|PAD_TRIGGER_R|PAD_BUTTON_A,0,0,0,0); break;
            case AOS_SPOTDODGE:
            case AOS_SPOTDODGE_SHINE:
                QueueInput(ft,PAD_TRIGGER_L|PAD_TRIGGER_R,0,-127,0,0);
                if(action==AOS_SPOTDODGE_SHINE) action_phase=AOS_DODGE_START;
                break;
            case AOS_ROLL_AWAY:
            case AOS_ROLL_TOWARDS:
                QueueInput(ft,PAD_TRIGGER_L|PAD_TRIGGER_R,away*(action==AOS_ROLL_AWAY?127:-127),0,0,0); break;
            default:
                // Release jump on the first observed jumpsquat update. This
                // makes every aerial a short hop, regardless of jumpsquat length.
                QueueInput(ft,PAD_TRIGGER_L|PAD_TRIGGER_R|PAD_BUTTON_X,0,0,0,0);
                action_phase=(action==AOS_UP_B || action==AOS_UP_SMASH)?AOS_JUMP_CANCEL:AOS_JUMP;
                break;
        }
        return SHIELD_INPUT_START;
    }
    if(action_phase==AOS_DODGE_START) {
        if(state==ASID_ESCAPE) action_phase=AOS_DODGE_END;
        else if(state==ASID_GUARD || state==ASID_GUARDON)
            QueueInput(ft,PAD_TRIGGER_L|PAD_TRIGGER_R,0,-127,0,0);
        return SHIELD_INPUT_CONTINUE;
    }
    if(action_phase==AOS_DODGE_END) {
        // Observe a real spotdodge before waiting for its grounded recovery.
        if(state==ASID_WAIT) {
            QueueInput(ft,PAD_BUTTON_B,0,-127,0,0);
            action_phase=AOS_FINISHED;
        }
        return SHIELD_INPUT_CONTINUE;
    }
    if(action_phase==AOS_JUMP_CANCEL) {
        if(state==ASID_KNEEBEND || (action==AOS_UP_B && (state==ASID_JUMPF || state==ASID_JUMPB))) {
            QueueInput(ft,action==AOS_UP_B?PAD_BUTTON_B:PAD_BUTTON_A,0,127,0,0);
            if(action==AOS_UP_SMASH) action_phase=AOS_FINISHED;
        } else if(state!=ASID_GUARD && state!=ASID_GUARDON) action_phase=AOS_FINISHED;
        return SHIELD_INPUT_CONTINUE;
    }
    if(action_phase==AOS_JUMP) {
        if(state==ASID_JUMPF || state==ASID_JUMPB) action_phase=AOS_AIR_ATTACK;
        else return SHIELD_INPUT_CONTINUE;
    }
    if(action_phase==AOS_AIR_ATTACK) {
        if(state!=ASID_JUMPF && state!=ASID_JUMPB) { action_phase=AOS_FINISHED; return SHIELD_INPUT_CONTINUE; }
        int facing=ft->facing_direction<0?-1:1;
        switch(action) {
            case AOS_NAIR: QueueInput(ft,PAD_BUTTON_A,0,0,0,0); break;
            case AOS_FAIR: QueueInput(ft,0,0,0,facing*127,0); break;
            case AOS_DAIR: QueueInput(ft,0,0,0,0,-127); break;
            case AOS_BAIR: QueueInput(ft,0,0,0,-facing*127,0); break;
            case AOS_UAIR: QueueInput(ft,0,0,0,0,127); break;
            case AOS_SHINE: QueueInput(ft,PAD_BUTTON_B,0,-127,0,0); break;
            case AOS_WAVEDASH_AWAY: QueueInput(ft,PAD_TRIGGER_L|PAD_TRIGGER_R,-TowardPlayer(ft)*90,-90,0,0); break;
        }
    }
    return SHIELD_INPUT_CONTINUE;
}
static void TickReversal(FighterData *ft) {
    if(!reversal || !ft) return;
    QueueInput(ft,0,0,0,0,0);
    if(ft->flags.dead || ft->flags.sleep || ft->flags.freeze || ft->flags.hitlag ||
        ft->dmg.hitlag_frames>0 || ft->flags.hitstun) return;
    int state=Powershield_CommonState(ft,ft->state_id);
    int attack=rev_options[TM_REV_ATTACK].val;
    if(attack==REV_GRAB) {
        if(reversal_phase==REV_IDLE && state==ASID_WAIT) {
            QueueInput(ft,PAD_TRIGGER_Z,0,0,0,0);
            reversal_phase=REV_FINISHED;
        }
    } else if(attack==REV_RUN_JC_GRAB) {
        if(reversal_phase==REV_IDLE) {
            GOBJ *human=Fighter_GetSubcharGObj(0,0);
            if(!human || !human->userdata) return;
            float gap=((FighterData *)human->userdata)->phys.pos.X-ft->phys.pos.X;
            if(state==ASID_WAIT || state==ASID_DASH || state==ASID_RUN || state==ASID_TURN ||
                state==ASID_WALKSLOW || state==ASID_WALKMIDDLE || state==ASID_WALKFAST) {
                int toward=gap>0?1:gap<0?-1:ft->facing_direction<0?-1:1;
                int facing=ft->facing_direction<0?-1:1;
                if(gap>=-12 && gap<=12 && toward==facing) {
                    QueueInput(ft,PAD_BUTTON_X,0,0,0,0);
                    reversal_phase=REV_JUMP;
                } else {
                    // Recompute direction every frame, so movement or an
                    // overshoot changes the run target instead of a stale X.
                    QueueInput(ft,0,gap>0?127:-127,0,0,0);
                }
            }
        } else if(reversal_phase==REV_JUMP && state==ASID_KNEEBEND) {
            QueueInput(ft,PAD_TRIGGER_Z,0,0,0,0);
            reversal_phase=REV_FINISHED;
        }
    } else if(attack==REV_SPOTDODGE_SHINE) {
        if(reversal_phase==REV_IDLE || reversal_phase==REV_DODGE_START) {
            if(state==ASID_ESCAPE) reversal_phase=REV_DODGE_END;
            else if(state==ASID_WAIT || state==ASID_GUARDON || state==ASID_GUARD) {
                QueueInput(ft,PAD_TRIGGER_L|PAD_TRIGGER_R,0,-127,0,0);
                reversal_phase=REV_DODGE_START;
            }
        } else if(reversal_phase==REV_DODGE_END && state==ASID_WAIT) {
            QueueInput(ft,PAD_BUTTON_B,0,-127,0,0);
            reversal_phase=REV_FINISHED;
        }
    }
}
static void Timing(FighterData *ft, int phase, int next) {
    /* Native state-entry notifications see every accepted shieldstun entry,
     * including a re-entry during a multihit and Yoshi's egg-shield states. */
    if (reversal || action_phase!=AOS_IDLE || phase!=2 || ft->ply!=1 || ft->flags.ms || ft->flags.dead ||
        Powershield_CommonState(ft,next)!=ASID_GUARDSETOFF) return;
    int tick=stc_match->time_frames;
    if (tick==hit_tick) return;
    hit_tick=tick;
    shield_di_direction=TowardPlayer(ft)*(aos_options[TM_AOS_SHIELD_DI].val?-1:1);
    if(hits<5) ++hits;
}
static void Change(GOBJ *menu, int value) {
    EventOption *options=reversal?rev_options:aos_options;
    unsigned field=reversal?TM_SETTING_REVERSAL:TM_SETTING_ATTACK_SHIELD;
    unsigned count=reversal?TM_REV_PREF_COUNT:TM_AOS_PREF_COUNT;
    for(unsigned i=0;i<count;++i) {
        /* INT rows use the displayed 1..5; persistence uses the stable 0..4. */
        Settings_Set(field,i,options[i].val-(!reversal && i==TM_AOS_HITS));
        if(options[i].val!=options[i].val_prev) pending|=1<<i;
        options[i].val_prev=options[i].val;
    }
    ClearAttempt();
}
static void Update(void) {
    if(stc_match->state==3) return;
    event_vars->game_timer=stc_match->time_frames;
    EventMenu_Update(event_vars->menu_gobj);
    if(stc_match->state==3) return;
    /* Menu navigation must not also drive native debug/pause camera controls. */
    if(((MenuData *)event_vars->menu_gobj->userdata)->mode!=MenuMode_Paused)
        Develop_UpdateMatchHotkeys();
}
void ShieldTraining_Init(int is_reversal) {
    event_vars->event_desc=is_reversal?&Reversal:&AttackOnShield;
    reversal=is_reversal; pending=0; ClearAttempt();
    memset(native_menu,0,sizeof(native_menu));
    *(const u8 **)native_menu=native_info;
    EventMenu *menu=reversal?&rev_menu:&aos_menu;
    unsigned count=reversal?TM_REV_PREF_COUNT:TM_AOS_PREF_COUNT;
    for(unsigned i=0;i<count;++i) {
        int value=Settings_Get(reversal?TM_SETTING_REVERSAL:TM_SETTING_ATTACK_SHIELD,i);
        value+=!reversal && i==TM_AOS_HITS;
        menu->options[i].val=menu->options[i].val_prev=value;
    }
    menu->scroll=menu->cursor=0;
    event_vars->menu_gobj=EventMenu_Init(menu);
    ActionCues_SetInfiniteShields(reversal?0:&aos_options[TM_AOS_INFINITE].val);
    Powershield_SetObserver(0,Timing,ClearAttempt);
    stc_hsd_update->onFrame=Update;
}
void *ShieldTraining_Control(int command, void *native_data) {
    if(command==SHIELD_RESET) { ClearAttempt(); return 0; }
    if(command==SHIELD_AOS_INPUT) {
        int result=TickCPU(native_data);
        RunIASA(native_data);
        return (void *)(uintptr_t)result;
    }
    if(command==SHIELD_REV_INPUT) { TickReversal(native_data); RunIASA(native_data); return 0; }
    if(command==SHIELD_INIT_AOS || command==SHIELD_INIT_REVERSAL) {
        ShieldTraining_Init(command==SHIELD_INIT_REVERSAL);
        return 0;
    }
    if(command==SHIELD_SYNC) {
        if(reversal) {
            native_menu[0xA]=rev_options[TM_REV_ATTACK].val;
            native_menu[0xB]=rev_options[TM_REV_PLAYER_FACING].val;
            native_menu[0xC]=rev_options[TM_REV_CPU_FACING].val;
            native_menu[0xD]=rev_options[TM_REV_POSITION].val;
            native_menu[0x28]=!!(pending&(1<<TM_REV_ATTACK));
            native_menu[0x29]=!!(pending&(1<<TM_REV_PLAYER_FACING));
            native_menu[0x2A]=!!(pending&(1<<TM_REV_CPU_FACING));
            native_menu[0x2B]=!!(pending&((1<<TM_REV_POSITION)|(1<<TM_REV_DISTANCE)));
        } else {
            native_menu[0xA]=aos_options[TM_AOS_ACTION].val;
            native_menu[0x28]=!!pending;
        }
        pending=0;
        return native_menu;
    }
    if(command==SHIELD_REACT) {
        int ready=hits>=aos_options[TM_AOS_HITS].val;
        if(ready) ClearHits();
        return (void *)(uintptr_t)ready;
    }
    if(command==SHIELD_DISTANCE) {
        /* Native SaveState_Save stores six pairs of main/follower pointers.
         * Only the two participating fighters are positioned by this event. */
        FighterData **saved=native_data;
        if(!saved || !saved[0] || !saved[2]) return 0;
        float distance=TC_PlayerDistance(reversal?rev_options[TM_REV_DISTANCE].val:aos_options[TM_AOS_DISTANCE].val);
        float center=reversal?(saved[0]->phys.pos.X+saved[2]->phys.pos.X)*0.5f:0;
        float side=saved[0]->phys.pos.X<saved[2]->phys.pos.X?-1:1;
        for(int ply=0;ply<2;++ply) {
            float x=center+(ply?-side:side)*distance*0.5f;
            float delta=x-saved[ply*2]->phys.pos.X;
            for(int sub=0;sub<2;++sub) if(saved[ply*2+sub]) saved[ply*2+sub]->phys.pos.X+=delta;
        }
        ClearAttempt();
    }
    return 0;
}
