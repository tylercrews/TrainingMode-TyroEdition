#ifndef TM_STAGE_SWAP_H
#define TM_STAGE_SWAP_H
/* "Change Stage" submenu for stage-select events: the six tournament-legal
 * stages. Choosing one restarts the event's match on that stage, skipping the
 * CSS/SSS (event_vars->restart_on_stage). Each including event defines
 * StageSwap_BeforeRestart for its own cleanup and calls StageSwap_Init from
 * Event_Init. See notes/MID-EVENT-CHARACTER-STAGE-SWAP.md. */
#include "events.h"

#define STAGE_SWAP_COUNT 6
static const s16 StageSwap_Stages[STAGE_SWAP_COUNT] = {
    GRKINDEXT_FD, GRKINDEXT_BATTLE, GRKINDEXT_OLDPU, GRKINDEXT_STORY, GRKINDEXT_IZUMI, GRKINDEXT_PSTAD,
};

static void StageSwap_BeforeRestart(void);
static EventOption StageSwap_Options[STAGE_SWAP_COUNT];
static void StageSwap_Choose(int index)
{
    if (index < 0 || index >= STAGE_SWAP_COUNT || StageSwap_Options[index].disable) return;
    StageSwap_BeforeRestart();
    event_vars->restart_on_stage(StageSwap_Stages[index]);
}
/* One callback per row, so the choice never depends on the menu cursor. */
#define STAGE_SWAP_SELECT(i) static void StageSwap_Select##i(GOBJ *menu) { StageSwap_Choose(i); }
STAGE_SWAP_SELECT(0) STAGE_SWAP_SELECT(1) STAGE_SWAP_SELECT(2)
STAGE_SWAP_SELECT(3) STAGE_SWAP_SELECT(4) STAGE_SWAP_SELECT(5)
#undef STAGE_SWAP_SELECT
#define STAGE_SWAP_ROW(i, label) { .kind = OPTKIND_FUNC, .name = label, \
    .desc = {"Restart the match on " label ".", "Skips the character and stage select."}, \
    .OnSelect = StageSwap_Select##i }
static EventOption StageSwap_Options[STAGE_SWAP_COUNT] = {
    STAGE_SWAP_ROW(0, "Final Destination"), STAGE_SWAP_ROW(1, "Battlefield"),
    STAGE_SWAP_ROW(2, "Dream Land"), STAGE_SWAP_ROW(3, "Yoshi's Story"),
    STAGE_SWAP_ROW(4, "Fountain of Dreams"), STAGE_SWAP_ROW(5, "Pokemon Stadium"),
};
#undef STAGE_SWAP_ROW
static EventMenu StageSwap_Menu = {
    .name = "Change Stage",
    .option_num = STAGE_SWAP_COUNT,
    .options = StageSwap_Options,
    .purpose = "Restart on one of the six tournament-legal stages.",
};

/* Grey out the current stage and start the cursor on the first other one. */
static void StageSwap_Init(void)
{
    int current = Stage_GetExternalID(), first = -1;
    for (int i = 0; i < STAGE_SWAP_COUNT; ++i) {
        int here = StageSwap_Stages[i] == current;
        StageSwap_Options[i].disable = here;
        StageSwap_Options[i].value_string = here ? "Current" : 0;
        if (!here && first < 0) first = i;
    }
    StageSwap_Menu.cursor = first;
    StageSwap_Menu.scroll = 0;
}

#endif
