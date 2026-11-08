/* Standalone Slippi NTSC 1.02 Invincibility Overlay. No Training Mode exports or heap use. */
#include "../../MexTK/mex.h"
#include "../../src/action_cues.h"

/* Native player blocks and the match's teams flag (MatchInfo + 0x24D0). */
static Playerblock *player(unsigned ply) { return (Playerblock *)(0x80453080 + ply * 0xE90); }
static int teams_match(void) { return *(volatile u8 *)0x8046DB70; }

/* Training Mode's ActionCues_IsProtected: whole-body invincibility or
 * intangibility, Yoshi's double-jump armor and Marth/Roy's counter stance. */
static int protected(GOBJ *obj) {
    FighterData *ft = obj->userdata;
    if ((ft->kind == FTKIND_MARTH || ft->kind == FTKIND_ROY) &&
        (ft->state_id == 369 || ft->state_id == 371) &&
        ft->flags.shield_enable && ft->flags.shield_x40) return 1;
    /* 0x8007B868 is the effective hurt status, from both script and game code. */
    if (((int (*)(GOBJ *))0x8007B868)(obj) > 0 || (ft->kind == FTKIND_YOSHI && ft->dmg.armor > 0)) return 1;
    /* SetAllHurtCapsules does not set the status: count it when no capsule is vulnerable. */
    if (!ft->hurt_num || ft->hurt_num > countof(ft->hurtbox)) return 0;
    for (unsigned i = 0; i < ft->hurt_num; ++i)
        if (ft->hurtbox[i].state <= 0) return 0;
    return 1;
}

extern void native_draw(GOBJ *, int);
void gecko_draw(GOBJ *obj, int pass) {
    FighterData *ft = obj->userdata;
    unsigned own = ft ? (u8)ft->ply : 6;
    if (own >= 6 || ft->flags.dead || ft->flags.sleep || !protected(obj)) { native_draw(obj, pass); return; }
    uint8_t teams[6];
    for (unsigned ply = 0; ply < 6; ++ply)
        teams[ply] = player(ply)->state || ply == own ? player(ply)->team : TM_TEAM_NONE;
    Playerblock *self = player(own);
    uint32_t rgba = TMCue_InvincibilityColor(teams_match() ? teams : 0, own,
        self->color_accent, self->p_kind == 1);
    /* Same composition as Training Mode: replace the selected tint for this
     * draw only, then restore all three native color structures. */
    ColorOverlay saved[3];
    __builtin_memcpy(saved, ft->color, sizeof(saved));
    unsigned selected = ft->color[0].colanim ? 0 : 1;
    for (unsigned i = 0; i < 3; ++i) ft->color[i].color_enable = 0;
    ft->color[selected].hex = (GXColor){rgba >> 24, rgba >> 16, rgba >> 8, 112};
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
