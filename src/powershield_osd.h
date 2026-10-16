#ifndef TM_POWERSHIELD_OSD_H
#define TM_POWERSHIELD_OSD_H
enum { TM_LASER_HIT, TM_LASER_SHIELD, TM_LASER_POWERSHIELD };
enum { TM_PS_SUCCESS, TM_PS_REFLECT_ACTIVE, TM_PS_REFLECT_ENDED, TM_PS_NO_HARD_PRESS, TM_PS_NO_SHIELD };
/* Native types are declared by events.h/MEX before including this header. */
void Powershield_SetObserver(void (*contact)(FighterData *, int, int, int),
    void (*timing)(FighterData *, int, int), void (*clear)(void));
void Powershield_Contact(ItemData *item, FighterData *fighter, int kind);
void Powershield_Clear(void);
void Powershield_SceneChange(void);
void EventTiming_Observe(FighterData *fighter, int phase, int next);
int Powershield_CommonState(FighterData *fighter, int state);
#endif
