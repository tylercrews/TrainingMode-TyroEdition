#ifndef TM_SHIELD_TRAINING_H
#define TM_SHIELD_TRAINING_H
/* Commands are mirrored by the native assembly adapter. */
enum { SHIELD_SYNC, SHIELD_DISTANCE, SHIELD_REACT, SHIELD_INIT_AOS, SHIELD_INIT_REVERSAL };
void ShieldTraining_Init(int reversal);
void *ShieldTraining_Control(int command, void *native_data);
#endif
