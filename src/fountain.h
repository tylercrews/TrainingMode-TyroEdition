#ifndef TM_FOUNTAIN_H
#define TM_FOUNTAIN_H

/* Fountain of Dreams side platforms. Each Lab row picks Random (the game's own
 * movement), Hidden (sunk, as when the game hides it) or one of
 * FOUNTAIN_HEIGHT_COUNT held heights, lowest first. */
enum { FOUNTAIN_RANDOM, FOUNTAIN_HIDDEN, FOUNTAIN_FIRST_HEIGHT };

/* GrIz.dat's yakumono_param clamps every move to joint heights 15 to 35, and
 * the platforms start at 20 (left) and 28 (right) and resurface at 25. Steps of
 * 0.5 land on all five. The surface is at 0.75 * (joint height + 1.5) stage
 * units: 12.375 to 27.375 in steps of 0.375, which is what the menu shows. */
#define FOUNTAIN_LOWEST 15.0f
#define FOUNTAIN_STEP 0.5f
#define FOUNTAIN_HEIGHT_COUNT 41
#define FOUNTAIN_CHOICE_COUNT (FOUNTAIN_FIRST_HEIGHT + FOUNTAIN_HEIGHT_COUNT)

extern const char *Fountain_Labels[FOUNTAIN_CHOICE_COUNT];

/* Hold the left and right platforms at their choices, or leave a Random one to
 * the game. Call every frame after the stage's think. Does nothing off Fountain. */
void Fountain_Apply(int left_choice, int right_choice);

#endif
