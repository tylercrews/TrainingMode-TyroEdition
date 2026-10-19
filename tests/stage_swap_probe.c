/* Production in-event stage change and EventInit, with native boundaries stubbed. */
#include "../src/events.h"

extern EventVars stc_event_vars;
int GetPageNum(void);
int GetPageEventNum(int page);
static Preload preload;
static MatchInit match;
static int updates, saves, ends, invalidates;

Preload *Preload_GetTable(void) { return &preload; }
void Preload_Update(void) { ++updates; }
/* As native: the queued table returns to its empty defaults. */
void Preload_Invalidate(void) {
    ++invalidates;
    preload.queued.stage = -1;
    for (int i = 0; i < 8; ++i) preload.queued.fighters[i] = (PreloadChar){33, 0};
}
void Memcard_SaveIfChanged(void) { ++saves; }
void Match_EndVS(void) { ++ends; }
void CSS_InitPlayerData(PlayerData *data) { }
int CSS_GetNametagRumble(int player, u8 tag) { return 0; }

int TestStageSwapInit(void) { *event_vars_ptr_loc = &stc_event_vars; return 1; }
int TestStageSwapLocate(EventDesc *target) {
    for (int page = 0; page <= GetPageNum(); ++page)
        for (int event = 0; event <= GetPageEventNum(page); ++event)
            if (GetEventDesc(page, event) == target) return page << 8 | event;
    return -1;
}
/* EventLoad records the running event; the SSS records the chosen stage. */
void TestStageSwapEnter(EventDesc *desc) { stc_event_vars.event_desc = desc; }
void TestStageSwapQueue(int stage) { preload.queued.stage = stage; }
void TestStageSwapQueueFighter(int slot, int kind, int costume) {
    preload.queued.fighters[slot].kind = kind;
    preload.queued.fighters[slot].costume = costume;
}
void TestStageSwapSlotByte(int slot, int value) { ((u8 *)&preload.queued.fighters[slot])[5] = value; }
int TestStageSwapFighters(int hmn_kind, int hmn_costume, int cpu_kind, int cpu_costume) {
    return stc_event_vars.restart_on_fighters(hmn_kind, hmn_costume, cpu_kind, cpu_costume);
}
void TestStageSwapRestart(int stage) { stc_event_vars.restart_on_stage(stage); }
void TestStageSwapPrep(int located) { EventInit(located >> 8, located & 0xFF, &match); }
unsigned TestStageSwapStore(unsigned tag, unsigned size) { return (unsigned)stc_event_vars.carry_store(tag, size); }
unsigned TestStageSwapTake(unsigned tag, unsigned size) { return (unsigned)stc_event_vars.carry_take(tag, size); }
int TestStageSwapInfo(int key) {
    switch (key) {
    case 0: return preload.queued.stage;
    case 1: return match.stage;
    case 2: return updates;
    case 3: return saves;
    case 4: return ends;
    case 5: return stc_match->end_kind;
    case 6: return stc_match->state;
    case 7: return preload.queued.fighters[0].kind;
    case 8: return preload.queued.fighters[0].costume;
    case 9: return preload.queued.fighters[1].kind;
    case 10: return preload.queued.fighters[1].costume;
    case 11: return match.playerData[0].c_kind;
    case 12: return match.playerData[0].costume;
    case 13: return match.playerData[1].c_kind;
    case 14: return match.playerData[1].costume;
    case 15: return invalidates;
    case 16: return preload.queued.fighters[2].kind;
    case 17: return ((u8 *)&preload.queued.fighters[1])[5];
    }
    return -999;
}
