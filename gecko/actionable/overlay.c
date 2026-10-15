/* Standalone Slippi NTSC 1.02 adapter. No Training Mode exports or heap use. */
#include "../../MexTK/mex.h"
#include "../../src/action_cues.h"

enum { HISTORY = 16, SLOTS = 12 };
typedef struct Sample {
    GOBJ *object;
    int spawn;
    unsigned tick, color;
    TMCueState cue;
} Sample;
static Sample history[SLOTS][HISTORY];

/* This counter is maintained by Slippi Recording, including offline VS.
 * Its containing data region is restored during online rollback. */
static unsigned tick(void) {
    unsigned result;
    __asm__("lwz %0,-0x49ac(13)" : "=r"(result));
    return result;
}
static unsigned slot(FighterData *ft) {
    return (unsigned)(u8)ft->ply * 2 + !!ft->flags.ms;
}
static int common_state(int kind, int state) {
    if (kind == FTKIND_GAW && state >= 341 && state <= 352) {
        static const short common[] = {
            ASID_ATTACK11, ASID_ATTACK100START, ASID_ATTACK100LOOP,
            ASID_ATTACK100END, ASID_ATTACKLW3, ASID_ATTACKS4S,
            ASID_ATTACKAIRN, ASID_ATTACKAIRB, ASID_ATTACKAIRHI,
            ASID_LANDINGAIRN, ASID_LANDINGAIRB, ASID_LANDINGAIRHI,
        };
        return common[state - 341];
    }
    if (kind == FTKIND_KIRBY && (state == 351 || state == 352)) return ASID_ATTACKDASH;
    return state;
}
static int remaining(GOBJ *obj) {
    FighterData *ft = obj->userdata;
    int state = common_state(ft->kind, ft->state_id);
    if ((state >= ASID_WAIT && state <= ASID_RUN) ||
        (state >= ASID_JUMPF && state <= ASID_FALLAERIALB) ||
        state == ASID_SQUATWAIT || state == ASID_OTTOTTOWAIT ||
        state == ASID_GUARD || state == ASID_GUARDREFLECT || state == ASID_PASS) return 0;
    int landing = state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL ||
        (state >= ASID_LANDINGAIRN && state <= ASID_LANDINGAIRLW);
    int attack = state >= ASID_ATTACK11 && state <= ASID_ATTACKAIRLW;
    if ((!landing && !attack) || state == ASID_ATTACK100START ||
        state == ASID_ATTACK100LOOP || ft->action_flags.loop_anim || !(ft->state.rate > 0)) return -1;
    float end = ((float (*)(GOBJ *))0x8006F484)(obj);
    if (!(end > 0)) return -1;
    if ((state == ASID_LANDING || state == ASID_LANDINGFALLSPECIAL) &&
        ft->state_var.state_var1 && ft->attr.normal_landing_lag < end) end = ft->attr.normal_landing_lag;
    int result = TMCue_Remaining(ft->state.frame, ft->state.rate, end);
    if (attack && !(ft->kind == FTKIND_KIRBY && ft->state_id == 352)) {
        if (ft->flags.past_iasa) return 0;
        TMCueScript script;
        __builtin_memcpy(&script, &ft->script, sizeof(script));
        int iasa = TMCue_ScriptIASA(&script, ft->state.rate);
        if (iasa >= 0 && iasa < result) result = iasa;
    }
    return result;
}

void gecko_tick(GOBJ *obj) {
    if (!obj || !obj->userdata) return;
    FighterData *ft = obj->userdata;
    unsigned index = slot(ft), now = tick();
    if (index >= SLOTS) return;
    Sample *out = &history[index][now % HISTORY];
    /* Always rebuild from the previous frame, even when resimulating a tick
     * already in the ring. Never consume state left by a discarded future. */
    Sample *before = &history[index][(now - 1) % HISTORY];
    Sample next = {.object = obj, .spawn = ft->spawn_num, .tick = now};
    if (now != (unsigned)-123 && before->object == obj &&
        before->spawn == ft->spawn_num && before->tick == now - 1) next.cue = before->cue;
    if (!ft->flags.dead && !ft->flags.sleep)
        next.color = TMCue_Update(&next.cue, ft->state_id, ft->state.frame,
            ft->atk_instance, remaining(obj), ft->flags.hitlag || ft->flags.freeze, 1, 0);
    *out = next;
}

extern void native_draw(GOBJ *, int, int);
void gecko_draw(GOBJ *obj, int pass, int arg) {
    FighterData *ft = obj->userdata;
    unsigned index = slot(ft), color = 0;
    if (index < SLOTS && !ft->flags.dead && !ft->flags.sleep) {
        Sample *sample = &history[index][tick() % HISTORY];
        if (sample->object == obj && sample->spawn == ft->spawn_num && sample->tick == tick())
            color = sample->color;
    }
    if (color != TM_CUE_YELLOW && color != TM_CUE_GREEN) { native_draw(obj, pass, arg); return; }
    ColorOverlay saved[3];
    __builtin_memcpy(saved, ft->color, sizeof(saved));
    unsigned selected = ft->color[0].colanim ? 0 : 1;
    for (unsigned i = 0; i < 3; ++i) ft->color[i].color_enable = 0;
    ft->color[selected].hex = color == TM_CUE_YELLOW ?
        (GXColor){255,240,0,220} : (GXColor){80,255,90,220};
    ft->color[selected].color_enable = 1;
    native_draw(obj, pass, arg);
    __builtin_memcpy(ft->color, saved, sizeof(saved));
}

/* Freestanding GCC may emit these for structure copies. */
void memcpy(void *to, const void *from, int size) {
    unsigned char *d = to; const unsigned char *s = from;
    while (size--) *d++ = *s++;
}
void memset(void *to, int value, int size) {
    unsigned char *d = to;
    while (size--) *d++ = value;
}
