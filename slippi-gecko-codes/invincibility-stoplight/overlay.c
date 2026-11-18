/* Standalone Slippi NTSC 1.02 Invincibility Stoplight. No Training Mode exports or heap use. */
#include "../../MexTK/mex.h"
#include "../../src/action_cues.h"

typedef char script_layout[(sizeof(TMCueScript) == 0x24 && __builtin_offsetof(FighterData, script) == 0x3E4) ? 1 : -1];

/* Training Mode's ActionCues_ProtectionLeft: protected frames left, counting
 * this one. Counters and Yoshi's armor end on no countdown the game keeps, so
 * they never show the stoplight. */
static int protection_left(FighterData *ft) {
    if ((ft->kind == FTKIND_MARTH || ft->kind == FTKIND_ROY) &&
        (ft->state_id == 369 || ft->state_id == 371) &&
        ft->flags.shield_enable && ft->flags.shield_x40) return 0;
    if (ft->kind == FTKIND_YOSHI && ft->dmg.armor > 0) return 0;
    TMCueScript script;
    __builtin_memcpy(&script, &ft->script, sizeof(script));
    TMCueHurt hurt = {.kind = ft->hurt.kind_script, .count = ft->hurt_num};
    for (unsigned i = 0; i < hurt.count && i < countof(ft->hurtbox); ++i) {
        hurt.state[i] = ft->hurtbox[i].state;
        hurt.bone[i] = ft->hurtbox[i].bone_index;
    }
    const u8 *raw = (const u8 *)ft;
    /* 0x221D 0x02 is starman (0x8007B868 reads it); MexTK calls it ground_ignore.
     * Vanilla Melee only sets these timers once and lets them count down, so
     * the code needs no per-frame history: every timer counts (Training Mode's
     * Techchase re-arms one every frame, so it checks). */
    TMCueEngine engine = {
        .kind = ft->hurt.kind_game, .ledge = ft->hurt.intang_frames.ledge,
        .respawn = ft->hurt.intang_frames.respawn, .held = raw[0x2221] & 0x80,
        .star = raw[0x221D] & 0x02, .star_frames = ft->x2004, .counting = TM_CUE_COUNTING_ALL,
    };
    return TMCue_ProtectionLeft(&script, ft->state.rate, ft->flags.freeze, &hurt, &engine);
}

extern void native_draw(GOBJ *, int);
void gecko_draw(GOBJ *obj, int pass) {
    FighterData *ft = obj->userdata;
    int left = ft && !ft->flags.dead && !ft->flags.sleep ? protection_left(ft) : 0;
    if (left < 1 || left > TM_CUE_ENDING_FRAMES) { native_draw(obj, pass); return; }
    /* Same composition as Training Mode: replace the selected tint for this
     * draw only, then restore all three native color structures. Whatever
     * the Invincibility Overlay code set is what gets restored. */
    ColorOverlay saved[3];
    __builtin_memcpy(saved, ft->color, sizeof(saved));
    unsigned selected = ft->color[0].colanim ? 0 : 1;
    for (unsigned i = 0; i < 3; ++i) ft->color[i].color_enable = 0;
    ft->color[selected].hex = left > 2 ? (GXColor)TM_CUE_ENDING_RED_COLOR : (GXColor)TM_CUE_ENDING_YELLOW_COLOR;
    ft->color[selected].color_enable = 1;
    native_draw(obj, pass);
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
