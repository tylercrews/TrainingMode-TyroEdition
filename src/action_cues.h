#ifndef TM_ACTION_CUES_H
#define TM_ACTION_CUES_H
#include <stdint.h>

enum { TM_CUE_NONE, TM_CUE_YELLOW, TM_CUE_GREEN, TM_CUE_RED };
#define TM_CUE_PULSE_FRAMES 8
#define TM_CPU_INVINCIBILITY_NAMES "Off", "Red", "Blue", "Yellow", "Green", "Cyan", "Magenta", "Gray"
enum TMCPUInvincibilityColor {
    TM_CPU_INVINCIBILITY_OFF, TM_CPU_INVINCIBILITY_RED, TM_CPU_INVINCIBILITY_BLUE,
    TM_CPU_INVINCIBILITY_YELLOW, TM_CPU_INVINCIBILITY_GREEN, TM_CPU_INVINCIBILITY_CYAN,
    TM_CPU_INVINCIBILITY_MAGENTA, TM_CPU_INVINCIBILITY_GRAY, TM_CPU_INVINCIBILITY_COUNT,
};
#define TM_CPU_INVINCIBILITY_DEFAULT TM_CPU_INVINCIBILITY_GRAY
uint32_t TMCue_CPUInvincibilityColor(unsigned choice);
/* Borrow an event's palette option. NULL returns CPUs to the global setting.
 * Off is an explicit CPU override; other choices enable natural protection.
 * Draw-time reads include paused edits; restore clears retain the binding. */
void ActionCues_SetCPUInvincibilityOverlay(const int16_t *palette);
/* NULL follows the global preference; a local Boolean overrides On and Off. */
void ActionCues_SetInfiniteShields(const int16_t *option);
/* Native command interpreter snapshot at Fighter + 0x3E4, including its stack. */
typedef struct TMCueScript {
    float timer, frame;
    const uint32_t *current;
    unsigned depth;
    uintptr_t stack[5];
} TMCueScript;
typedef struct TMCueState {
    int seen, state, remaining, green, red, red_source;
    float frame;
    unsigned instance;
} TMCueState;
/* -1 unknown; 0 actionable; 1/2 within the yellow window; 3 further away. */
int TMCue_Remaining(float frame, float rate, float boundary);
int TMCue_ScriptIASA(const TMCueScript *source, float rate);
unsigned TMCue_Update(TMCueState *cue, int state, float frame, unsigned instance,
                      int remaining, int frozen, int timing, int red_active);
unsigned TMCue_RedAlpha(unsigned phase);
void ActionCues_Clear(void);
void ActionCues_SceneChange(void);
void ActionCues_MatchStart(void);
#endif
