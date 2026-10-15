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
static int reversal, pending, hits, hit_tick;
static const char *distances[] = {"Right Next To", "Close", "Medium", "Past Tech Roll"};
static const char *attacks[] = {
    "Random Smash Attack", "Forward Smash", "Down Smash", "Up Smash",
    "Random Aerial", "Fair", "Nair", "Dair", "FTilt", "DTilt", "UTilt",
    "Dash Attack", "Getup Attack (Stomach)", "Getup Attack (Back)"
};
static const char *actions[] = {"Grab", "Nair", "Up B", "Up Smash", "Shine",
    "Spotdodge", "Roll Away", "Roll Towards", "Wavedash Away", "None"};
static const char *facings[] = {"Towards", "Away"};
static const char *positions[] = {"Ground", "Platform"};
static void Change(GOBJ *menu, int value);
static void Exit(GOBJ *menu) {
    Powershield_SetObserver(0, 0, 0);
    ActionCues_SetInfiniteShields(0);
    Memcard_SaveIfChanged();
    Match_EndVS();
}
static EventOption rev_options[] = {
    {.kind=OPTKIND_STRING, .name="CPU Attack", .values=attacks, .value_num=countof(attacks),
     .OnChange=Change, .desc={"Choose the CPU attack to punish.", "Your selection is saved."}},
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
    {.kind=OPTKIND_STRING, .name="OoS Option", .values=actions, .value_num=countof(actions),
     .OnChange=Change, .desc={"Choose the CPU's response after blocking."}},
    {.kind=OPTKIND_INT, .name="Hits Before Reaction", .value_min=1, .value_num=5, .val=1,
     .format="%d", .OnChange=Change, .desc={"React after blocking this many hits (1-5).", "Counts each shield hit, including multihits."}},
    {.kind=OPTKIND_TOGGLE, .name="Infinite Shields", .val=1, .OnChange=Change,
     .desc={"Keep both fighters' shields full while On.", "Overrides global Infinite Shields, including Off."}},
    {.kind=OPTKIND_STRING, .name="Player Distance", .values=distances, .value_num=TC_DISTANCE_COUNT,
     .OnChange=Change, .desc={"Starting separation: 10, 20, 30 or 45 units."}},
    {.kind=OPTKIND_FUNC, .name="Exit", .OnSelect=Exit, .desc={"Return to the Event Selection Screen."}},
};
static EventMenu rev_menu = {.name="Reversal Training", .options=rev_options,
    .option_num=countof(rev_options), .on_exit=Exit};
static EventMenu aos_menu = {.name="Attack on Shield", .options=aos_options,
    .option_num=countof(aos_options), .on_exit=Exit};
static void ClearHits(void) { hits=0; hit_tick=-1; }
static void Timing(FighterData *ft, int phase, int next) {
    /* Native state-entry notifications see every accepted shieldstun entry,
     * including a re-entry during a multihit and Yoshi's egg-shield states. */
    if (reversal || phase!=2 || ft->ply!=1 || ft->flags.ms || ft->flags.dead ||
        Powershield_CommonState(ft,next)!=ASID_GUARDSETOFF) return;
    int tick=stc_match->time_frames;
    if (tick==hit_tick) return;
    hit_tick=tick;
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
    ClearHits();
}
static void Update(void) {
    event_vars->game_timer=stc_match->time_frames;
    EventMenu_Update(event_vars->menu_gobj);
    Develop_UpdateMatchHotkeys();
}
void ShieldTraining_Init(int is_reversal) {
    event_vars->event_desc=is_reversal?&Reversal:&AttackOnShield;
    reversal=is_reversal; pending=0; ClearHits();
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
    Powershield_SetObserver(0,Timing,ClearHits);
    stc_hsd_update->onFrame=Update;
}
void *ShieldTraining_Control(int command, void *native_data) {
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
        ClearHits();
    }
    return 0;
}
