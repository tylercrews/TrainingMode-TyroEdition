#ifndef TM_SHIELD_TRAINING_H
#define TM_SHIELD_TRAINING_H
/* Commands are mirrored by the native assembly adapter. */
enum { SHIELD_SYNC, SHIELD_DISTANCE, SHIELD_REACT, SHIELD_INIT_AOS, SHIELD_INIT_REVERSAL, SHIELD_AOS_INPUT, SHIELD_REV_INPUT, SHIELD_RESET };
/* Stable action IDs: the first ten retain version-11 saved selections. */
enum {
    AOS_GRAB, AOS_NAIR, AOS_UP_B, AOS_UP_SMASH, AOS_SHINE,
    AOS_SPOTDODGE, AOS_ROLL_AWAY, AOS_ROLL_TOWARDS, AOS_WAVEDASH_AWAY, AOS_NONE,
    AOS_FAIR, AOS_DAIR, AOS_BAIR, AOS_UAIR, AOS_SPOTDODGE_SHINE, AOS_ACTION_COUNT
};
enum { SHIELD_INPUT_CONTINUE, SHIELD_INPUT_START, SHIELD_INPUT_FROZEN };
enum { REV_GRAB=14, REV_RUN_JC_GRAB, REV_SPOTDODGE_SHINE, REV_ACTION_COUNT };
void ShieldTraining_Init(int reversal);
void *ShieldTraining_Control(int command, void *native_data);
#endif
