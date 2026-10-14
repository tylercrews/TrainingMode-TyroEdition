#include "events.h"
#include "asmash_osd.h"
#include <stddef.h>

typedef struct ASMashEpisode {
    FighterData *fighter;
    int spawn, family, active;
    unsigned frame, opportunities;
} ASMashEpisode;
static ASMashEpisode episodes[12];

typedef char asmash_native_layout[(offsetof(FighterData, input.held) == 0x65C &&
    offsetof(FighterData, input.down) == 0x668 &&
    offsetof(FighterData, smash.state) == 0x2114) ? 1 : -1];

void ASMash_Clear(void) { memset(episodes, 0, sizeof(episodes)); }

static ASMashEpisode *entry_for(FighterData *fighter) {
    unsigned slot = (unsigned)(u8)fighter->ply * 2 + !!fighter->flags.ms;
    if (slot >= countof(episodes)) return 0;
    ASMashEpisode *entry = &episodes[slot];
    if (entry->fighter != fighter || entry->spawn != fighter->spawn_num)
        *entry = (ASMashEpisode){.fighter = fighter, .spawn = fighter->spawn_num};
    return entry;
}
/* 1=side, 2=up, 3=down. Ness' charge/release states share the move family. */
static int family_for(int kind, int state) {
    if (state >= ASID_ATTACKS4HI && state <= ASID_ATTACKS4LW) return 1;
    if (state == ASID_ATTACKHI4) return 2;
    if (state == ASID_ATTACKLW4) return 3;
    if (kind == FTKIND_GAW && state == 346) return 1;
    if (kind == FTKIND_PEACH && state >= 349 && state <= 351) return 1;
    if (kind == FTKIND_NESS) {
        if (state == 341) return 1;
        if (state >= 342 && state <= 344) return 2;
        if (state >= 345 && state <= 347) return 3;
    }
    return 0;
}
static int ness_continuation(int kind, int state) {
    return kind == FTKIND_NESS && (state == 343 || state == 346);
}
static int ness_release(int kind, int state) {
    return kind == FTKIND_NESS && (state == 344 || state == 347);
}
static int a_origin(FighterData *fighter, int family) {
    if (!(fighter->input.down & HSD_BUTTON_A)) return 0;
    ftCommonData *common = *stc_ftcommon;
    if (!common) return 0;
    /* Native smash checks read 0x670/671. The SDK calls these "tilt" timers;
     * its "smash" timers at 0x673/674 are unconsumed sticky copies. Consumers
     * can stamp the active timers 254 without changing those sticky copies. */
    if (family == 1) {
        float x = fighter->input.lstick.X;
        if (x < 0) x = -x;
        return x >= common->dash_min_stick &&
            (fighter->state_id == ASID_DASH ||
             (u8)fighter->input.timer_lstick_tilt_x < common->dash_min_stick_tilt);
    }
    if (family == 2)
        return fighter->input.lstick.Y >= common->xcc &&
            (fighter->state_id == ASID_KNEEBEND ||
             (u8)fighter->input.timer_lstick_tilt_y < common->xd0);
    return fighter->input.lstick.Y <= common->xd4 &&
        (u8)fighter->input.timer_lstick_tilt_y < common->xd8;
}
void OSD_ASmashEnter(FighterData *fighter, int next_state) {
    if (!fighter) return;
    ASMashEpisode *entry = entry_for(fighter);
    if (!entry) return;
    int family = family_for(fighter->kind, next_state);
    /* Ness' native charge transition preserves the initial A press and count.
     * His automatic release is a separate state entry, so it retires here. */
    if (entry->active && entry->family == family &&
        ness_continuation(fighter->kind, next_state)) return;
    entry->active = 0;
    if (!family || ness_release(fighter->kind, next_state) ||
        ness_continuation(fighter->kind, next_state) || fighter->flags.dead ||
        fighter->flags.ms || !a_origin(fighter, family)) return;
    *entry = (ASMashEpisode){.fighter = fighter, .spawn = fighter->spawn_num,
        .family = family, .active = 1, .frame = stc_match->time_frames};
}
void OSD_ASmashInput(GOBJ *object) {
    if (!object || !object->userdata) return;
    FighterData *fighter = object->userdata;
    ASMashEpisode *entry = entry_for(fighter);
    if (!entry || !entry->active) return;
    unsigned frame = stc_match->time_frames;
    if (fighter->flags.dead || fighter->flags.ms ||
        entry->family != family_for(fighter->kind, fighter->state_id) ||
        ness_release(fighter->kind, fighter->state_id) ||
        fighter->smash.state == 3 || frame < entry->frame) {
        entry->active = 0; return;
    }
    if (frame == entry->frame || fighter->flags.hitlag || fighter->flags.freeze) return;
    entry->frame = frame;
    ++entry->opportunities;
    if (fighter->input.held & HSD_BUTTON_A) return;
    entry->active = 0;
    if (!Settings_Get(TM_SETTING_OSD_ENABLED, TM_ASMASH_OSD_ID)) return;
    /* Earliest post-entry release is 1f. Before-charge releases count too:
     * native PreCharge simply clears without a charge/release state change. */
    Message_Display(OSD_MessageTag(TM_ASMASH_MESSAGE_KIND, TM_ASMASH_OSD_ID, 1, 1, 0),
        fighter->ply, MSGCOLOR_WHITE, "A-Smash Release\n%df", entry->opportunities);
}
