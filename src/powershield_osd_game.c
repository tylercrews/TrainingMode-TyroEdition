#include "events.h"
#include "osds.h"
#include "powershield_osd.h"

typedef struct ShieldAttempt {
    FighterData *fighter;
    int spawn, age, tick, seen_tick;
} ShieldAttempt;
static ShieldAttempt attempts[12];
static void (*event_contact)(FighterData *, int, int, int);
static void (*event_timing)(FighterData *, int, int);
static void (*event_clear)(void);

int Powershield_CommonState(FighterData *ft, int state) {
    /* Yoshi's five egg-shield states mirror GuardOn through GuardReflect. */
    return ft->kind == FTKIND_YOSHI && state >= 341 && state <= 345 ? state - 341 + ASID_GUARDON : state;
}
static ShieldAttempt *Attempt(FighterData *ft) {
    unsigned slot = (unsigned)(u8)ft->ply * 2 + !!ft->flags.ms;
    if (slot >= countof(attempts)) return 0;
    ShieldAttempt *a = &attempts[slot];
    if (a->fighter != ft || a->spawn != ft->spawn_num)
        *a = (ShieldAttempt){.fighter = ft, .spawn = ft->spawn_num};
    return a;
}
void Powershield_SetObserver(void (*contact)(FighterData *, int, int, int),
    void (*timing)(FighterData *, int, int), void (*clear)(void)) {
    event_contact = contact; event_timing = timing; event_clear = clear;
}
void Powershield_Clear(void) {
    memset(attempts, 0, sizeof(attempts));
    if (event_clear) event_clear();
}
void Powershield_SceneChange(void) {
    event_contact = 0; event_timing = 0; event_clear = 0;
    memset(attempts, 0, sizeof(attempts));
}
void EventTiming_Observe(FighterData *ft, int phase, int next) {
    /* Reuse the final shared export instead of overflowing its 50-slot table.
     * Phases 0..4 are timing, 5..7 carry an accepted contact's ItemData pointer. */
    if (phase >= 5 && phase <= 7) {
        Powershield_Contact((ItemData *)next, ft, phase - 5);
        return;
    }
    ShieldAttempt *a = Attempt(ft);
    if (a) {
        int state = Powershield_CommonState(ft, ft->state_id);
        if (phase == 2) {
            int target = Powershield_CommonState(ft, next);
            if (target == ASID_GUARDREFLECT) {
                a->age = 1; a->tick = stc_match->time_frames; a->seen_tick = 1;
            } else if (target < ASID_GUARDON || target > ASID_GUARDREFLECT) a->age = 0;
        } else if (phase == 4 && !ft->flags.hitlag && !ft->flags.freeze && ft->dmg.hitlag_frames <= 0) {
            int tick = stc_match->time_frames;
            if (a->seen_tick && tick < a->tick) a->age = 0;
            if (a->seen_tick && tick != a->tick && a->age && a->age < 32767) ++a->age;
            a->tick = tick; a->seen_tick = 1;
            if (ft->flags.dead || ft->flags.sleep || state < ASID_GUARDON || state > ASID_GUARDREFLECT) a->age = 0;
        }
    }
    if (event_timing) event_timing(ft, phase, next);
    UnderFire_Timing(ft, phase, next);
}
void Powershield_Contact(ItemData *item, FighterData *ft, int kind) {
    if (!item || !ft || ft->flags.dead || ft->flags.sleep) return;
    int state = Powershield_CommonState(ft, ft->state_id);
    /* The reflection hook is inside the accepted-damage branch, not the
     * tentative collision test. Shine and other reflectors are not powershields. */
    if (kind == TM_LASER_POWERSHIELD && state != ASID_GUARDREFLECT) return;
    ShieldAttempt *a = Attempt(ft);
    int frame = a ? a->age : 0;
    if (!frame && state == ASID_GUARDREFLECT && ft->flags.reflect_enable) {
        float left;
        memcpy(&left, &ft->state_var.state_var6, sizeof(left)); /* guard + 0x14 */
        frame = (int)((**stc_ftcommon).x2a4 - left) + 1;
        if (frame < 1) frame = 1;
    }
    int reason = kind == TM_LASER_POWERSHIELD ? TM_PS_SUCCESS :
        kind == TM_LASER_HIT ? TM_PS_NO_SHIELD :
        ft->flags.reflect_enable && state == ASID_GUARDREFLECT ? TM_PS_REFLECT_ACTIVE :
        frame ? TM_PS_REFLECT_ENDED : TM_PS_NO_HARD_PRESS;
    if (event_contact && item->kind == ITEM_FALCOLASER) event_contact(ft, kind, frame, reason);
    if (kind == TM_LASER_HIT || ft->flags.ms ||
        !(Settings_Get(TM_SETTING_OSD_MASK, 0) & (1u << OSD_Powershield))) return;
    /* This is a contact outcome, not a best-frame contest: both native reflect
     * frames succeed. Do not apply the generic Cyan/Green timing ranking. */
    if (kind == TM_LASER_POWERSHIELD)
        Message_Display(OSD_MessageTag(OSD_Powershield, OSD_Powershield, 0, 1, 0),
            ft->ply, MSGCOLOR_GREEN, "Powershield\nSuccess: %df", frame);
    else if (!frame)
        Message_Display(OSD_MessageTag(OSD_Powershield, OSD_Powershield, 0, 1, 0),
            ft->ply, MSGCOLOR_RED, "Missed Powershield\nNo hard press");
    else
        Message_Display(OSD_MessageTag(OSD_Powershield, OSD_Powershield, 0, 1, 0) | TM_OSD_POINTER_FIRST,
            ft->ply, MSGCOLOR_RED, "Missed Powershield\n%s\nShield frame: %df",
            reason == TM_PS_REFLECT_ACTIVE ? "Reflect active / shield hit" : "Reflect ended", frame);
}
