#ifndef TM_TECHCHASE_LOGIC_H
#define TM_TECHCHASE_LOGIC_H

/* Menu order. Getup Attack Only (T10) is saved last, see TC_PresetToSaved. */
enum TCTechPreset {
    TC_PRESET_ORIGINAL, TC_PRESET_EVEN, TC_PRESET_PLACE, TC_PRESET_AWAY,
    TC_PRESET_TOWARD, TC_PRESET_MISS, TC_PRESET_GETUP_ATTACK, TC_PRESET_PLACE_TOWARD,
    TC_PRESET_AWAY_TOWARD, TC_PRESET_TYRO, TC_PRESET_COUNT
};
enum TCLocation { TC_CENTER, TC_LEFT_PLATFORM, TC_RIGHT_PLATFORM, TC_TOP_PLATFORM, TC_LOCATION_COUNT };
enum TCDistance { TC_DISTANCE_NEXT_TO, TC_DISTANCE_CLOSE, TC_DISTANCE_MEDIUM, TC_DISTANCE_PAST_ROLL,
                  TC_DISTANCE_RANDOM, TC_DISTANCE_COUNT };
/* Reversal and Attack on Shield share the four fixed spacings, not Random:
 * their saved fields hold four choices. */
enum { TC_FIXED_DISTANCE_COUNT = TC_DISTANCE_RANDOM };
enum TCSide { TC_SIDE_LEFT, TC_SIDE_RIGHT, TC_SIDE_RANDOM, TC_SIDE_COUNT };
enum { TC_HEIGHT_COUNT = 4 };
/* Random spacing runs from Right Next To through Medium. */
#define TC_RANDOM_DISTANCE_MIN 10.f
#define TC_RANDOM_DISTANCE_MAX 30.f
/* A player spot past the stage's end stops this far in from the ledge, as
 * Edgeguard's At Ledge does. Yoshi's side platforms overhang its ledges. */
#define TC_LEDGE_MARGIN 5.f

static float TC_DropHeight(int option) {
    static const float heights[TC_HEIGHT_COUNT] = {27, 15, 40, 54};
    return heights[option >= 0 && option < TC_HEIGHT_COUNT ? option : 0];
}

static float TC_FixedDistance(int option) {
    static const float distances[TC_FIXED_DISTANCE_COUNT] = {10, 20, 30, 45};
    return distances[option >= 0 && option < TC_FIXED_DISTANCE_COUNT ? option : 0];
}

/* `roll` in [0, 1) places Random anywhere from Right Next To to Medium. */
static float TC_PlayerDistance(int option, float roll) {
    if (option == TC_DISTANCE_RANDOM)
        return TC_RANDOM_DISTANCE_MIN + (TC_RANDOM_DISTANCE_MAX - TC_RANDOM_DISTANCE_MIN) * roll;
    return TC_FixedDistance(option);
}

/* From the CPU toward the player: -1 when the player is on the left. */
static int TC_PlayerDirection(int side, int coin) {
    if (side == TC_SIDE_RANDOM) side = coin ? TC_SIDE_RIGHT : TC_SIDE_LEFT;
    return side == TC_SIDE_RIGHT ? 1 : -1;
}

/* The player stands `distance` to its side of the CPU, but no closer than
 * TC_LEDGE_MARGIN to either end of the solid ground [left, right]. */
static float TC_PlayerX(float cpu_x, int direction, float distance, float left, float right) {
    float x = cpu_x + direction * distance;
    if (right - left > 2 * TC_LEDGE_MARGIN) {
        if (x < left + TC_LEDGE_MARGIN) x = left + TC_LEDGE_MARGIN;
        if (x > right - TC_LEDGE_MARGIN) x = right - TC_LEDGE_MARGIN;
    }
    return x;
}
/* CPU Followup Action (T10), in menu and saved order. It was Spotdodge
 * Punish, Off/On, so a saved Off or On reads as None or Spotdodge. */
enum TCFollowup {
    TC_FOLLOWUP_NONE, TC_FOLLOWUP_SPOTDODGE, TC_FOLLOWUP_ROLL_AWAY, TC_FOLLOWUP_ROLL_IN,
    TC_FOLLOWUP_SHINE, TC_FOLLOWUP_RANDOM, TC_FOLLOWUP_COUNT
};
/* The shine's waveland heads away from the player unless its floor ends
 * within this far that way: a waveland slides, and it would slide off. */
#define TC_WAVELAND_ROOM 30.f

/* Shine is Fox's and Falco's alone. */
static int TC_FollowupAvailable(int followup, int can_shine) {
    return followup >= 0 && followup < TC_FOLLOWUP_COUNT && (followup != TC_FOLLOWUP_SHINE || can_shine);
}

/* Random picks one of Spotdodge, Roll Away, Roll In and, for Fox and Falco,
 * Shine: `pick` is a roll in [0, TC_RandomFollowupCount). Anything this CPU
 * can't do is None. */
static int TC_RandomFollowupCount(int can_shine) { return can_shine ? 4 : 3; }
static int TC_ResolveFollowup(int followup, int can_shine, int pick) {
    if (!TC_FollowupAvailable(followup, can_shine)) return TC_FOLLOWUP_NONE;
    if (followup != TC_FOLLOWUP_RANDOM) return followup;
    if (pick < 0 || pick >= TC_RandomFollowupCount(can_shine)) pick = 0;
    return TC_FOLLOWUP_SPOTDODGE + pick;
}

/* A menu step onto a choice this CPU can't do keeps going the same way, as
 * Starting Location skips missing platforms. */
static int TC_StepFollowup(int value, int previous, int can_shine) {
    int direction = (value - previous + TC_FOLLOWUP_COUNT) % TC_FOLLOWUP_COUNT == 1 ? 1 : -1;
    for (int i = 0; i < TC_FOLLOWUP_COUNT && !TC_FollowupAvailable(value, can_shine); ++i)
        value = (value + direction + TC_FOLLOWUP_COUNT) % TC_FOLLOWUP_COUNT;
    return value;
}

/* Roll Away rolls away from stage center (x = 0), Roll In toward it. Right
 * at the center, `fallback` counts as away. */
static int TC_RollDirection(int followup, float x, int fallback) {
    int away = x > 0 ? 1 : x < 0 ? -1 : fallback;
    return followup == TC_FOLLOWUP_ROLL_IN ? -away : away;
}

/* The waveland's direction: `away` (from the player) if its floor has the
 * room, else back toward the player, else straight down (0). */
static int TC_WavelandDirection(int away, float room_away, float room_toward) {
    if (room_away >= TC_WAVELAND_ROOM) return away;
    if (room_toward >= TC_WAVELAND_ROOM) return -away;
    return 0;
}

/* DODGING is the followup under way, from its first state until the CPU
 * stands again. TC_STATE_DODGE is a state only the followup enters. */
enum TCPunishPhase { TC_PUNISH_START, TC_PUNISH_TECH, TC_PUNISH_DODGING, TC_PUNISH_FINISHED };
enum TCPunishState { TC_STATE_OTHER, TC_STATE_OPTION, TC_STATE_WAIT, TC_STATE_DODGE };
enum TCPunishAction { TC_ACTION_NONE, TC_ACTION_DODGE, TC_ACTION_RESET };

static int TC_PresetChance(int preset, int option)
{
    static const unsigned char chances[TC_PRESET_COUNT][4] = {
        {40, 30, 30, 0}, {25, 25, 25, 25},
        {100, 0, 0, 0}, {0, 100, 0, 0}, {0, 0, 100, 0}, {0, 0, 0, 100}, {0, 0, 0, 100},
        {50, 0, 50, 0}, {0, 50, 50, 0}, {60, 15, 15, 10}
    };
    if (preset < 0 || preset >= TC_PRESET_COUNT) preset = TC_PRESET_ORIGINAL;
    return option >= 0 && option < 4 ? chances[preset][option] : 0;
}

/* Getup Attack Only is the one preset that also picks the getup: stand, roll
 * away, roll toward, attack. -1 means the preset leaves the getups alone. */
static int TC_PresetGetupChance(int preset, int option)
{
    static const unsigned char attack[4] = {0, 0, 0, 100};
    if (preset != TC_PRESET_GETUP_ATTACK) return -1;
    return option >= 0 && option < 4 ? attack[option] : 0;
}

/* Saves from before Getup Attack Only keep their numbers: it is saved as 9,
 * after Tyro setting, though the menu lists it after Miss Tech Only. */
static int TC_PresetToSaved(int preset)
{
    if (preset == TC_PRESET_GETUP_ATTACK) return TC_PRESET_COUNT - 1;
    return preset > TC_PRESET_GETUP_ATTACK ? preset - 1 : preset;
}

static int TC_PresetFromSaved(int saved)
{
    if (saved < 0 || saved >= TC_PRESET_COUNT) return TC_PRESET_ORIGINAL;
    if (saved == TC_PRESET_COUNT - 1) return TC_PRESET_GETUP_ATTACK;
    return saved >= TC_PRESET_GETUP_ATTACK ? saved + 1 : saved;
}

/* Only arm after a tech/getup. TC_ACTION_DODGE starts the followup on the
 * first Wait frame. The timer starts once the one followup ends, and never
 * consumes its startup, intangibility, or punishable recovery. */
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
 * A 5-unit margin avoids spawning near a platform edge. `direction` is
 * TC_PlayerDirection's: with the player on the right, Center and Top mirror
 * the CPU to x = -5, so both sides keep the same room. */
static int TC_SurfaceTarget(float ax, float ay, float bx, float by, int drop,
                            int location, int direction, float *x, float *y)
{
    float left = ax < bx ? ax : bx, right = ax > bx ? ax : bx;
    if (right - left < 10) return 0;
    float target = (left + right) * 0.5f;
    if (location == TC_CENTER) {
        target = -5.f * direction;
        if (drop || left > target || right < target) return 0;
    } else {
        if (!drop) return 0;
        if (location == TC_LEFT_PLATFORM && target >= -10) return 0;
        if (location == TC_RIGHT_PLATFORM && target <= 10) return 0;
        if (location == TC_TOP_PLATFORM) {
            if (left > 0 || right < 0) return 0;
            target = (direction < 0 ? right >= 10 : left <= -10) ? -5.f * direction : 0;
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
