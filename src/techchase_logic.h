#ifndef TM_TECHCHASE_LOGIC_H
#define TM_TECHCHASE_LOGIC_H

enum TCTechPreset {
    TC_PRESET_ORIGINAL, TC_PRESET_EVEN, TC_PRESET_PLACE, TC_PRESET_AWAY,
    TC_PRESET_TOWARD, TC_PRESET_MISS, TC_PRESET_PLACE_TOWARD,
    TC_PRESET_AWAY_TOWARD, TC_PRESET_TYRO, TC_PRESET_COUNT
};
enum TCLocation { TC_CENTER, TC_LEFT_PLATFORM, TC_RIGHT_PLATFORM, TC_TOP_PLATFORM, TC_LOCATION_COUNT };
enum { TC_HEIGHT_COUNT = 4, TC_DISTANCE_COUNT = 4 };

static float TC_DropHeight(int option) {
    static const float heights[TC_HEIGHT_COUNT] = {27, 15, 40, 54};
    return heights[option >= 0 && option < TC_HEIGHT_COUNT ? option : 0];
}

static float TC_PlayerDistance(int option) {
    static const float distances[TC_DISTANCE_COUNT] = {10, 20, 30, 45};
    return distances[option >= 0 && option < TC_DISTANCE_COUNT ? option : 0];
}
enum TCPunishPhase { TC_PUNISH_START, TC_PUNISH_TECH, TC_PUNISH_DODGING, TC_PUNISH_FINISHED };
enum TCPunishState { TC_STATE_OTHER, TC_STATE_OPTION, TC_STATE_WAIT, TC_STATE_DODGE };
enum TCPunishAction { TC_ACTION_NONE, TC_ACTION_DODGE, TC_ACTION_RESET };

static int TC_PresetChance(int preset, int option)
{
    static const unsigned char chances[TC_PRESET_COUNT][4] = {
        {40, 30, 30, 0}, {25, 25, 25, 25},
        {100, 0, 0, 0}, {0, 100, 0, 0}, {0, 0, 100, 0}, {0, 0, 0, 100},
        {50, 0, 50, 0}, {0, 50, 50, 0}, {60, 15, 15, 10}
    };
    if (preset < 0 || preset >= TC_PRESET_COUNT) preset = TC_PRESET_ORIGINAL;
    return option >= 0 && option < 4 ? chances[preset][option] : 0;
}

/* Only arm after a tech/getup. The timer starts after the one spotdodge ends,
 * and never consumes its startup, intangibility, or punishable recovery. */
static int TC_PunishStep(int *phase, int enabled, int state)
{
    if (!enabled) return state == TC_STATE_WAIT ? TC_ACTION_RESET : TC_ACTION_NONE;
    if (state == TC_STATE_OPTION && *phase < TC_PUNISH_DODGING) *phase = TC_PUNISH_TECH;
    if (state == TC_STATE_DODGE) *phase = TC_PUNISH_DODGING;
    if (state != TC_STATE_WAIT) return TC_ACTION_NONE;
    if (*phase == TC_PUNISH_TECH) return TC_ACTION_DODGE;
    if (*phase == TC_PUNISH_DODGING) *phase = TC_PUNISH_FINISHED;
    return TC_ACTION_RESET;
}

/* Evaluate a live collision line. Caller excludes disabled/non-floor lines.
 * Center uses solid ground; the three platform targets use one-way floors.
 * A 5-unit margin avoids spawning near a platform edge. */
static int TC_SurfaceTarget(float ax, float ay, float bx, float by, int drop,
                            int location, float *x, float *y)
{
    float left = ax < bx ? ax : bx, right = ax > bx ? ax : bx;
    if (right - left < 10) return 0;
    float target = (left + right) * 0.5f;
    if (location == TC_CENTER) {
        if (drop || left > 5 || right < 5) return 0;
        target = 5;
    } else {
        if (!drop) return 0;
        if (location == TC_LEFT_PLATFORM && target >= -10) return 0;
        if (location == TC_RIGHT_PLATFORM && target <= 10) return 0;
        if (location == TC_TOP_PLATFORM) {
            if (left > 0 || right < 0) return 0;
            target = right >= 10 ? 5 : 0;
        }
    }
    float minimum = left + 5, maximum = right - 5;
    if (target < minimum) target = minimum;
    if (target > maximum) target = maximum;
    *x = target;
    *y = ay + (by - ay) * (target - ax) / (bx - ax);
    return 1;
}

#endif
