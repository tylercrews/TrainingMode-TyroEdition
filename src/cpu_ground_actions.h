#ifndef TM_CPU_GROUND_ACTIONS_H
#define TM_CPU_GROUND_ACTIONS_H

/* One menu catalogue shared by Training Lab and Ledgechase. */
#define CPU_GROUND_ACTIONS(X) \
    X(CPUACT_NONE, "None") \
    X(CPUACT_SPOTDODGE, "Spotdodge") \
    X(CPUACT_SHIELD, "Shield") \
    X(CPUACT_RUNUPGRAB, "Grab") \
    X(CPUACT_UPB, "Up B") \
    X(CPUACT_SIDEBTOWARD, "Side B Toward") \
    X(CPUACT_SIDEBAWAY, "Side B Away") \
    X(CPUACT_DOWNB, "Down B") \
    X(CPUACT_NEUTRALB, "Neutral B") \
    X(CPUACT_USMASH, "Up Smash") \
    X(CPUACT_DSMASH, "Down Smash") \
    X(CPUACT_FSMASH, "Forward Smash") \
    X(CPUACT_ROLLAWAY, "Roll Away") \
    X(CPUACT_ROLLTOWARDS, "Roll Towards") \
    X(CPUACT_ROLLRDM, "Roll Random") \
    X(CPUACT_NAIR, "Neutral Air") \
    X(CPUACT_FAIR, "Forward Air") \
    X(CPUACT_DAIR, "Down Air") \
    X(CPUACT_BAIR, "Back Air") \
    X(CPUACT_UAIR, "Up Air") \
    X(CPUACT_JAB, "Jab") \
    X(CPUACT_FTILT, "Forward Tilt") \
    X(CPUACT_UTILT, "Up Tilt") \
    X(CPUACT_DTILT, "Down Tilt") \
    X(CPUACT_DASHATTACK, "Dash Attack") \
    X(CPUACT_SHORTHOP, "Short Hop") \
    X(CPUACT_FULLHOP, "Full Hop") \
    X(CPUACT_WAVEDASH_AWAY, "Wavedash Away") \
    X(CPUACT_WAVEDASH_TOWARDS, "Wavedash Towards") \
    X(CPUACT_WAVEDASH_DOWN, "Wavedash Down") \
    X(CPUACT_DASH_AWAY, "Dash Back") \
    X(CPUACT_DASH_TOWARDS, "Dash Through")

#define CPU_GROUND_ACTION_ID(id, name) id,
#define CPU_GROUND_ACTION_NAME(id, name) name,
#define CPU_GROUND_ACTION_ADD(id, name) + 1
enum { CPU_GROUND_ACTION_COUNT = 1 CPU_GROUND_ACTIONS(CPU_GROUND_ACTION_ADD) };
#undef CPU_GROUND_ACTION_ADD
#endif
