#include "events.h"
#include "osds.h"
#include "osd_context.h"
/* Act Oo OSDs whose attempts end in a result or "--" (TMNoAction). */
enum { NO_ACTION_WAIT, NO_ACTION_ATTACK, NO_ACTION_SHIELD, NO_ACTION_HITSTUN, NO_ACTION_JUMP, NO_ACTIONS };
static const struct { u8 id, kind; char *format; } no_action_osds[NO_ACTIONS] = {
    {OSD_ActOoWait, 5, "Act OoWait\n%s\n%df"}, {OSD_ActOoAttack, OSD_ActOoAttack, "Act OoAttack\n%s\n%df"},
    {OSD_ActOoS, 3, "Act OoShield\n%df"}, {OSD_ActOoHitstun, 5, "Act OoHitstun\n%df"},
    {OSD_ActOoAirborne, OSD_ActOoAirborne, "Act OoJump\n%df"},
};
typedef struct OSDContextFighter {
    FighterData *fighter; int spawn; TMOSDContext context; TMShineEpisode shine;
    TMAttackEpisode attack; int throw_iasa_clear; /* past_iasa survives into throws. */
    TMNoAction no_action[NO_ACTIONS];
    uint32_t no_action_frame; /* Last update + 1; 0 = not yet observed. */
    int no_action_state, no_action_hitstun;
} OSDContextFighter;
typedef char osd_context_layout[(sizeof(TMOSDContext) == 64 && sizeof(TMShineEpisode) == 60 &&
    sizeof(TMAttackEpisode) == 36 && sizeof(TMNoAction) == 12 && sizeof(OSDContextFighter) == 244) ? 1 : -1];
static OSDContextFighter contexts[12];
static OSDContextFighter *entry_for(FighterData *fighter) {
    unsigned slot = (unsigned)(u8)fighter->ply * 2 + !!fighter->flags.ms;
    if (slot >= countof(contexts)) return 0;
    OSDContextFighter *entry = &contexts[slot];
    if (entry->fighter != fighter || entry->spawn != fighter->spawn_num)
        *entry = (OSDContextFighter){.fighter = fighter, .spawn = fighter->spawn_num};
    return entry;
}
static TMOSDContext *context_for(FighterData *fighter) {
    OSDContextFighter *entry = entry_for(fighter);
    return entry ? &entry->context : 0;
}
void OSDContext_Clear(void) { memset(contexts, 0, sizeof(contexts)); }
static void no_action_tick(OSDContextFighter *entry, FighterData *data, int frozen);
/* 0x8006AB78 is reached by both frozen and normal animation updates, before
 * native IASA/physics OSD producers. Never rely on a late event callback. */
void OSDContext_Tick(FighterData *fighter) {
    TMOSDContext *c = context_for(fighter);
    if (!c) return;
    int state = fighter->state_id;
    int shine = (fighter->kind == FTKIND_FOX || fighter->kind == FTKIND_FALCO) && state >= 360 && state <= 369;
    OSDContextFighter *entry = entry_for(fighter);
    int hitlag = fighter->flags.hitlag || (fighter->flags.freeze && fighter->dmg.hitlag_frames > 0);
    TMShine_Tick(&entry->shine, stc_match->time_frames, shine ? state : -1, hitlag, fighter->flags.dead);
    int common = ActionCues_CommonState(fighter->kind, state);
    TMAttackEpisode *attack = &entry->attack;
    if (TMAttack_Tracked(common) && (!attack->active || attack->state != common ||
        attack->instance != (unsigned)fighter->atk_instance)) entry->throw_iasa_clear = 0;
    TMAttack_Tick(attack, common, fighter->atk_instance, hitlag, fighter->flags.dead);
    int shield = state >= ASID_GUARDON && state <= ASID_GUARDREFLECT;
    int victim = (state >= ASID_DAMAGEHI1 && state <= ASID_DAMAGEFLYROLL) || state == ASID_DAMAGEFALL;
    TMOSDContext_Step(c, stc_match->time_frames, state, fighter->atk_instance,
        fighter->phys.air_state, shine, shield, victim,
        fighter->flags.hitlag || (fighter->flags.freeze && fighter->dmg.hitlag_frames > 0), fighter->flags.dead);
    no_action_tick(entry, fighter, hitlag);
}
void OSDContext_LCancel(GOBJ *fighter) {
    FighterData *data = fighter->userdata;
    TMOSDContext *c = context_for(data);
    if (c) c->cancelled = (u8)data->input.timer_trigger_any_ignore_hitlag < (**stc_ftcommon).lcancel_input_window;
}
unsigned OSDContext_MessageHitlag(int queue, unsigned category) {
    if ((unsigned)queue >= 6) return 0;
    GOBJ *object = Fighter_GetSubcharGObj(queue, 0);
    if (!object || !object->userdata) return 0;
    TMOSDContext *c = context_for(object->userdata);
    return c ? TMOSDContext_Hitlag(c, category) : 0;
}
/* Any result for an Act Oo OSD ends that player's pending attempt. */
void OSDContext_Result(int queue, int settings_id) {
    for (unsigned i = 0; i < NO_ACTIONS; ++i) {
        if (no_action_osds[i].id != settings_id) continue;
        GOBJ *object = (unsigned)queue < 6 ? Fighter_GetSubcharGObj(queue, 0) : 0;
        OSDContextFighter *entry = object && object->userdata ? entry_for(object->userdata) : 0;
        if (entry) TMNoAction_Result(&entry->no_action[i]);
        return;
    }
}
/* Recovery label for state (common IDs), entered from before. */
static const char *wait_label(FighterData *data, int state, int before) {
    if (state == ASID_LANDING) return before >= ASID_ATTACKAIRN && before <= ASID_ATTACKAIRLW ? "Autocancel" : "Landing";
    if (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW) {
        if (data->kind == FTKIND_GAW) return "Aerial landing";
        TMOSDContext *c = context_for(data);
        return c && c->cancelled ? "L-cancel" : "Missed L-cancel";
    }
    if (state == ASID_LANDINGFALLSPECIAL) return before == ASID_ESCAPEAIR ? "Waveland" : "Special landing";
    /* Throws and ledge attacks, including through Wait, belong to Act OoAttack. */
    if (state == ASID_CLIFFCLIMBSLOW || state == ASID_CLIFFCLIMBQUICK) return "Ledge getup";
    if (state == ASID_CLIFFESCAPESLOW || state == ASID_CLIFFESCAPEQUICK) return "Ledge roll";
    if (state == ASID_PASSIVE) return "Tech";
    if (state == ASID_PASSIVESTANDF || state == ASID_PASSIVESTANDB) return "Tech roll";
    if (state == ASID_DOWNATTACKU || state == ASID_DOWNATTACKD) return "Getup attack";
    if (state >= ASID_DOWNBOUNDU && state <= ASID_PASSIVESTANDB) return "Getup";
    if ((data->kind == FTKIND_FOX || data->kind == FTKIND_FALCO) && state >= 341 && state <= 346) return "Laser";
    if (data->kind == FTKIND_SHEIK && state >= 341 && state <= 348) return "Needles";
    if (data->kind == FTKIND_PEACH && state >= 344 && state <= 348) return "Float aerial";
    if (state >= ASID_ATTACKAIRN && state <= ASID_ATTACKAIRLW) return "Aerial";
    return 0;
}
static const char *wait_source(FighterData *data, int index) {
    return wait_label(data, ActionCues_CommonState(data->kind, data->TM.state_prev[index]),
        index < 5 ? ActionCues_CommonState(data->kind, data->TM.state_prev[index + 1]) : -1);
}
/* Normal landing's unavoidable frames, rounded up. */
static unsigned landing_lag(FighterData *data) {
    float lag = data->attr.normal_landing_lag;
    return lag > 0 ? (unsigned)lag + (lag > (unsigned)lag) : 0;
}
void OSD_ActOutWait(GOBJ *object) {
    FighterData *data = object->userdata;
    if (data->flags.ms || data->TM.state_frame > 1) return;
    uint16_t states[6];
    for (unsigned i = 0; i < 6; ++i) states[i] = ActionCues_CommonState(data->kind, data->TM.state_prev[i]);
    int anchor, source;
    int frame = TMOSD_WaitFrames(states, data->TM.state_prev_frames, 6, landing_lag(data), &anchor, &source);
    if (!frame) return;
    const char *label = wait_source(data, source);
    if (!label) return;
    Message_Display(OSD_MessageTag(5, OSD_ActOoWait, 2, 2, 0) | TM_OSD_POINTER_FIRST, data->ply, MSGCOLOR_WHITE,
        "Act OoWait\n%s\n%df", label, frame);
}

static void no_action_display(FighterData *data, const TMNoAction *attempt, unsigned which) {
    unsigned id = no_action_osds[which].id;
    if (!(Settings_Get(TM_SETTING_OSD_MASK, 0) & (1u << id))) return;
    /* Recent pop-ups expire anyway: clear out the stale one rather than add an empty one. */
    if (!Settings_Get(TM_SETTING_OSD_LAYOUT, 0)) { Message_Retire(data->ply, id); return; }
    const char *label = attempt->label;
    uint32_t tag = OSD_MessageTag(no_action_osds[which].kind, id, label ? 2 : 1, label ? 2 : 1, 0) | TM_OSD_DEFER_FORMAT;
    GOBJ *message = label ?
        Message_Display(tag | TM_OSD_POINTER_FIRST, data->ply, MSGCOLOR_WHITE, no_action_osds[which].format, label, 0) :
        Message_Display(tag, data->ply, MSGCOLOR_WHITE, no_action_osds[which].format, 0);
    MsgData *msg = message->userdata;
    msg->timing_blank = 1; msg->timing_hitlag = 0;
    OSD_FormatTiming(msg, 0, label ? -MSGTEXT_YOFFSET : -MSGTEXT_YOFFSET / 2);
    OSD_ApplyMessageStyle(msg);
}
static int jump_state(FighterData *data, int state) {
    return (state >= ASID_JUMPF && state <= ASID_JUMPAERIALB) ||
        (data->attr.max_jumps > 2 && state >= 0x155 && state <= 0x159); /* Kirby and Jigglypuff's later jumps. */
}
/* A Fall that Act OoJump measures on its own: not the end of a jump, drop, aerial or special. */
static int own_fall(FighterData *data) {
    int before = ActionCues_CommonState(data->kind, data->TM.state_prev[0]);
    return !jump_state(data, before) && before != ASID_PASS &&
        !(before >= ASID_ATTACKAIRN && before <= ASID_ATTACKAIRLW) && before <= ASID_BARRELCANNONWAIT;
}
/* Start each attempt where its producer starts counting; Act OoAttack starts
 * in attack_before, on its first actionable update. */
static void no_action_tick(OSDContextFighter *entry, FighterData *data, int frozen) {
    uint32_t frame = stc_match->time_frames;
    if (data->flags.ms || entry->no_action_frame == frame + 1) return;
    TMNoAction *n = entry->no_action;
    int state = data->state_id, last = entry->no_action_state;
    if (entry->no_action_frame && !data->flags.dead) {
        if (state != last) {
            int common = ActionCues_CommonState(data->kind, state);
            const char *label;
            if (common == ASID_WAIT && (label = wait_source(data, 0)))
                TMNoAction_Start(&n[NO_ACTION_WAIT], 0, label);
            else if (common == ASID_LANDING)
                TMNoAction_Start(&n[NO_ACTION_WAIT], landing_lag(data), wait_label(data, ASID_LANDING,
                    ActionCues_CommonState(data->kind, data->TM.state_prev[0])));
            if (last == ASID_GUARDSETOFF && (state == ASID_GUARD || state == ASID_GUARDOFF))
                TMNoAction_Start(&n[NO_ACTION_SHIELD], 0, 0);
            if (jump_state(data, state) || state == ASID_PASS ||
                (state == ASID_FALL && !n[NO_ACTION_JUMP].pending && own_fall(data)))
                TMNoAction_Start(&n[NO_ACTION_JUMP], 0, 0);
        }
        int victim = (state >= ASID_DAMAGEHI1 && state <= ASID_DAMAGEFLYROLL) || state == ASID_DAMAGEFALL;
        if (entry->no_action_hitstun && !data->flags.hitstun && victim)
            TMNoAction_Start(&n[NO_ACTION_HITSTUN], 0, 0);
    }
    entry->no_action_frame = frame + 1;
    entry->no_action_state = state; entry->no_action_hitstun = data->flags.hitstun;
    for (unsigned i = 0; i < NO_ACTIONS; ++i)
        if (TMNoAction_Tick(&n[i], frozen, data->flags.dead)) no_action_display(data, &n[i], i);
}

static const char *attack_label(int state) {
    static const char *names[] = {
        "Jab", "Jab", "Jab", "Rapid jab", "Rapid jab", "Rapid jab", "Dash attack",
        "Ftilt", "Ftilt", "Ftilt", "Ftilt", "Ftilt", "Utilt", "Dtilt",
        "Fsmash", "Fsmash", "Fsmash", "Fsmash", "Fsmash", "Usmash", "Dsmash",
    };
    static const char *throws[] = { "Fthrow", "Bthrow", "Uthrow", "Dthrow" };
    if (state >= ASID_ATTACK11 && state <= ASID_ATTACKLW4) return names[state - ASID_ATTACK11];
    if (state >= ASID_THROWF && state <= ASID_THROWLW) return throws[state - ASID_THROWF];
    if (state == ASID_CLIFFATTACKSLOW || state == ASID_CLIFFATTACKQUICK) return "Ledge attack";
    return "Attack";
}
typedef char attack_label_layout[(ASID_ATTACKLW4 - ASID_ATTACK11 == 20 && ASID_THROWLW - ASID_THROWF == 3) ? 1 : -1];
/* Count each native IASA call that could have accepted an action: an attack or
 * throw past its interrupt point, or its Wait/crouch once the animation ends. */
static void attack_before(GOBJ *object, FighterData *data, OSDContextFighter *entry) {
    int state = ActionCues_CommonState(data->kind, data->state_id);
    int actionable = 0;
    if (TMAttack_Tracked(state)) {
        actionable = ActionCues_Remaining(object) == 0;
        if (state >= ASID_THROWF && state <= ASID_THROWLW) {
            /* Throw entry keeps an earlier attack's IASA bit; trust it only
             * after this throw has been seen without it. */
            if (!data->flags.past_iasa) entry->throw_iasa_clear = 1;
            else if (entry->throw_iasa_clear) actionable = 1;
        }
    }
    TMAttack_Before(&entry->attack, stc_match->time_frames, state, data->atk_instance, actionable,
        data->flags.hitlag || data->flags.freeze || data->flags.dead);
    if (entry->attack.prepared && entry->attack.opportunity == 1 && !data->flags.ms)
        TMNoAction_Start(&entry->no_action[NO_ACTION_ATTACK], 0, attack_label(entry->attack.state));
}
static void attack_after(FighterData *data, OSDContextFighter *entry) {
    TMAttackEpisode *attack = &entry->attack;
    unsigned frame = TMAttack_After(attack, stc_match->time_frames,
        ActionCues_CommonState(data->kind, data->state_id), data->atk_instance,
        data->flags.hitlag || data->flags.freeze || data->flags.dead);
    if (!frame || data->flags.ms || !(Settings_Get(TM_SETTING_OSD_MASK, 0) & (1u << OSD_ActOoAttack))) return;
    GOBJ *message = Message_Display(OSD_MessageTag(OSD_ActOoAttack, OSD_ActOoAttack, 2, 2, 0) |
        TM_OSD_POINTER_FIRST | TM_OSD_DEFER_FORMAT, data->ply, MSGCOLOR_WHITE,
        "Act OoAttack\n%s\n%df", attack_label(attack->state), frame);
    MsgData *msg = message->userdata;
    msg->timing_hitlag = attack->hitlag; /* The attack's own hitlag, not the new state's. */
    OSD_FormatTiming(msg, 0, -MSGTEXT_YOFFSET);
    OSD_ApplyMessageStyle(msg);
}

void OSD_ShineBeforeIASA(GOBJ *object) {
    if (!object || !object->userdata) return;
    FighterData *data = object->userdata;
    OSDContextFighter *entry = entry_for(data);
    if (!entry) return;
    attack_before(object, data, entry);
    if (data->kind != FTKIND_FOX && data->kind != FTKIND_FALCO) return;
    TMShine_Before(&entry->shine, stc_match->time_frames, data->state_id,
        data->flags.freeze || data->flags.dead,
        data->state_id == 361 || (u8)data->jump.jumps_used < data->attr.max_jumps);
}
void OSD_ShineAfterIASA(GOBJ *object) {
    if (!object || !object->userdata) return;
    FighterData *data = object->userdata;
    OSDContextFighter *entry = entry_for(data);
    if (!entry) return;
    attack_after(data, entry);
    if (data->kind != FTKIND_FOX && data->kind != FTKIND_FALCO) return;
    TMShineEpisode *shine = &entry->shine;
    int result = TMShine_After(shine, stc_match->time_frames, data->state_id,
        data->flags.freeze || data->flags.dead);
    if (!result || data->flags.ms || !(Settings_Get(TM_SETTING_OSD_MASK, 0) & (1u << OSD_FighterSpecificTech))) return;
    unsigned frame = result == TM_SHINE_JUMP ? shine->jump_opportunity : shine->opportunity;
    GOBJ *message = Message_Display(OSD_MessageTag(OSD_FighterSpecificTech, OSD_FighterSpecificTech, 1, 1, 0) | TM_OSD_DEFER_FORMAT,
        data->ply, MSGCOLOR_WHITE, "Jump Out Of Shine\n%df", frame);
    MsgData *msg = message->userdata;
    msg->timing_hitlag = shine->hitlag;
    msg->timing_turn = shine->first_turn;
    msg->timing_second_turn = result == TM_SHINE_DOUBLE_TURN;
    msg->timing_failed = result == TM_SHINE_FAIL;
    OSD_FormatTiming(msg, 0, -MSGTEXT_YOFFSET / 2);
    OSD_ApplyMessageStyle(msg);
}
