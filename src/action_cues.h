#ifndef TM_ACTION_CUES_H
#define TM_ACTION_CUES_H
#include <stdint.h>

enum { TM_CUE_NONE, TM_CUE_YELLOW, TM_CUE_GREEN, TM_CUE_RED };
#define TM_CUE_PULSE_FRAMES 8
/* The cues' colors, for events that show them through the timing overlay. */
#define TM_CUE_YELLOW_COLOR {255,240,0,220}
#define TM_CUE_GREEN_COLOR {80,255,90,220}
#define TM_CUE_RED_COLOR {255,40,40,180}
#define TM_CUE_RED_PULSE {180,140,100,60,20,60,100,140}
/* Invincibility Stoplight: the last four protected frames, red then yellow,
 * the same for everyone. A little more opaque than the protection tint (112). */
#define TM_CUE_ENDING_FRAMES 4
#define TM_CUE_ENDING_RED_COLOR {255,40,40,150}
#define TM_CUE_ENDING_YELLOW_COLOR {255,240,0,150}
#define TM_CPU_INVINCIBILITY_NAMES "Off", "Red", "Blue", "Yellow", "Green", "Cyan", "Magenta", "Lavender"
enum TMCPUInvincibilityColor {
    TM_CPU_INVINCIBILITY_OFF, TM_CPU_INVINCIBILITY_RED, TM_CPU_INVINCIBILITY_BLUE,
    TM_CPU_INVINCIBILITY_YELLOW, TM_CPU_INVINCIBILITY_GREEN, TM_CPU_INVINCIBILITY_CYAN,
    TM_CPU_INVINCIBILITY_MAGENTA, TM_CPU_INVINCIBILITY_LAVENDER, TM_CPU_INVINCIBILITY_COUNT,
};
#define TM_CPU_INVINCIBILITY_DEFAULT TM_CPU_INVINCIBILITY_LAVENDER
uint32_t TMCue_CPUInvincibilityColor(unsigned choice);
/* Invincibility Overlay color. FFA: the port color (accent), Lavender for CPUs.
 * Teams: teams[ply] is each port's team (Red 0, Blue 1, Green 2), or
 * TM_TEAM_NONE for an empty port. Teammates are numbered in port order and
 * each gets its own shade of the team color. Pass NULL teams outside teams. */
#define TM_TEAM_NONE 0xFF
uint32_t TMCue_InvincibilityColor(const uint8_t *teams, unsigned ply, unsigned accent, int cpu);
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
/* The body state the action script sets: hurt.kind_script and the hurtboxes. */
typedef struct TMCueHurt {
    int kind;
    unsigned count;
    int state[15], bone[15];
} TMCueHurt;
/* Protection the engine counts down itself (PlayerThink_Animation, 0x8006A360):
 * hurt.kind_game with its ledge and respawn timers, the 0x2221 0x80 hold that
 * no timer ends, and the starman flag (0x221D 0x02) with its timer (0x2004).
 * counting: the timers seen going down by one since the last simulated frame.
 * Only those predict an end; code can hold a timer instead, as Techchase
 * re-arms a 1-frame ledge timer every frame. */
#define TM_CUE_COUNTING_LEDGE 1u
#define TM_CUE_COUNTING_RESPAWN 2u
#define TM_CUE_COUNTING_STAR 4u
#define TM_CUE_COUNTING_ALL 7u
typedef struct TMCueEngine {
    int kind, ledge, respawn, held, star, star_frames;
    unsigned counting;
} TMCueEngine;
/* Protected frames left, counting this one: 0 not protected, 1..4, or 5 for
 * five or more. -1 when the script can't be followed far enough to tell.
 * Hitlag (frozen) holds the script but not the engine's timers. */
int TMCue_ProtectionLeft(const TMCueScript *script, float rate, int frozen,
                         const TMCueHurt *hurt, const TMCueEngine *engine);
unsigned TMCue_Update(TMCueState *cue, int state, float frame, unsigned instance,
                      int remaining, int frozen, int timing, int red_active);
unsigned TMCue_RedAlpha(unsigned phase);
void ActionCues_Clear(void);
void ActionCues_SceneChange(void);
void ActionCues_MatchStart(void);
#endif
