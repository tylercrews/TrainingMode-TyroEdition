#ifndef TM_ASMASH_OSD_H
#define TM_ASMASH_OSD_H

/* Additive settings category; existing OSD category and message IDs stay put. */
#define TM_ASMASH_OSD_ID TM_GLOBAL_ASMASH_RELEASE_ID
#define TM_ASMASH_MESSAGE_KIND 70

void ASMash_Clear(void);
/* Native state entry, before state_id is written: fp=r26, next_state=r24. */
void OSD_ASmashEnter(FighterData *fighter, int next_state);
/* Native input has been updated; called before the engine checks A release. */
void OSD_ASmashInput(GOBJ *object);

#endif
