#ifndef TM_CPU_AIR_ATTACKS_H
#define TM_CPU_AIR_ATTACKS_H

/* Menu identities; commands are borrowed from Training Lab's action tables. */
#define CPU_AIR_ATTACKS(X) \
    X(CPUACT_NONE, "None") \
    X(CPUACT_NAIR, "Neutral Air") \
    X(CPUACT_FAIR, "Forward Air") \
    X(CPUACT_DAIR, "Down Air") \
    X(CPUACT_BAIR, "Back Air") \
    X(CPUACT_UAIR, "Up Air") \
    X(CPUACT_NEUTRALB, "Neutral B (Repeat)") \
    X(CPUACT_UPB, "Up B") \
    X(CPUACT_SIDEBTOWARD, "Side B Toward") \
    X(CPUACT_SIDEBAWAY, "Side B Away") \
    X(CPUACT_DOWNB, "Down B")

#define CPU_AIR_ATTACK_ADD(id, name) + 1
enum { CPU_AIR_ATTACK_COUNT = 1 CPU_AIR_ATTACKS(CPU_AIR_ATTACK_ADD) }; // Includes Random.
#undef CPU_AIR_ATTACK_ADD
#endif
