#ifndef TM_LEDGECHASE_LOGIC_H
#define TM_LEDGECHASE_LOGIC_H

enum LCSide { LC_SIDE_LEFT, LC_SIDE_RIGHT, LC_SIDE_SWAP, LC_SIDE_RANDOM, LC_SIDE_COUNT };
enum LCOption { LC_GETUP, LC_JUMP, LC_ROLL, LC_ATTACK, LC_OPTION_COUNT };
enum LCPreset { LC_PRESET_EVEN, LC_PRESET_GETUP, LC_PRESET_JUMP, LC_PRESET_ROLL, LC_PRESET_ATTACK, LC_PRESET_TYRO, LC_PRESET_GETUP_ROLL, LC_PRESET_COUNT };
enum { LC_DISTANCE_COUNT = 5, LC_PUNISH_WINDOW = 30, LC_HIT_RESET_DELAY = 15, LC_ATTEMPT_LIMIT = 300 };

static int LC_PresetChance(int preset, int option) {
    static const unsigned char chances[LC_PRESET_COUNT][LC_OPTION_COUNT] = {
        {25, 25, 25, 25}, {100, 0, 0, 0}, {0, 100, 0, 0},
        {0, 0, 100, 0}, {0, 0, 0, 100}, {60, 10, 15, 15}, {50, 0, 50, 0}
    };
    if (preset < 0 || preset >= LC_PRESET_COUNT) preset = LC_PRESET_EVEN;
    return option >= 0 && option < LC_OPTION_COUNT ? chances[preset][option] : 0;
}

static int LC_SelectOption(const int chances[LC_OPTION_COUNT], unsigned roll) {
    int value = roll % 100;
    for (int i = 0; i < LC_OPTION_COUNT - 1; ++i) {
        value -= chances[i];
        if (value < 0) return i;
    }
    return LC_ATTACK;
}

static void LC_Rebound(int chances[LC_OPTION_COUNT], int changed) {
    int delta = 100;
    for (int i = 0; i < LC_OPTION_COUNT; ++i) delta -= chances[i];
    while (delta) {
        changed = (changed + 1) % LC_OPTION_COUNT;
        int value = chances[changed] + delta;
        if (value < 0) { chances[changed] = 0; delta = value; }
        else if (value > 100) { chances[changed] = 100; delta = value - 100; }
        else { chances[changed] = value; delta = 0; }
    }
}

/* 70% choose the first legal action frame. The other 30% span the remaining
 * ledge protection and at most five vulnerable frames. Native readiness and
 * the independent vulnerability cap still apply if protection expires early. */
static int LC_ActFrame(unsigned preference_roll, unsigned delay_roll, int invuln_frames) {
    if (preference_roll % 100 < 70) return 1;
    unsigned span = invuln_frames > 0 ? invuln_frames + 4 : 4;
    return 2 + delay_roll % span;
}
static int LC_ReadyToAct(int *legal_frames, int *vulnerable_frames, int protected, int act_frame) {
    ++*legal_frames;
    if (protected) *vulnerable_frames = 0;
    else if (*vulnerable_frames < 5) ++*vulnerable_frames;
    return *legal_frames >= act_frame || *vulnerable_frames >= 5;
}

static int LC_NextSide(int setting, int current, int success, unsigned random) {
    if (setting == LC_SIDE_LEFT) return 0;
    if (setting == LC_SIDE_RIGHT) return 1;
    if (setting == LC_SIDE_RANDOM) return random & 1;
    return success ? !current : !!current;
}

static float LC_Distance(int option) {
    static const unsigned char distances[LC_DISTANCE_COUNT] = {4, 12, 24, 40, 80};
    if (option < 0 || option >= LC_DISTANCE_COUNT) option = 1;
    return distances[option];
}

#endif
