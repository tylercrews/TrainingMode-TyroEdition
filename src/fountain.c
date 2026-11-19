#include "../MexTK/include/stage.h"
#include "../MexTK/include/obj.h"
#include "../MexTK/include/match.h"
#include "fountain.h"

/* NTSC 1.02 Ground data of a side platform (map gobj 4), from
 * Stages_Fountain_SidePlatformSpawn (801CCBDC) and Stages_FoDPlatformThink
 * (801CC358). The left one spawns first, on line group 0. */
typedef struct FountainPlatform {
    int x0;
    GOBJ *gobj;                 // 0x04
    unsigned char x8[0xC];
    int map_id;                 // 0x14
    unsigned char x18[0xC4 - 0x18];
    short state;                // 0xC4, FountainState
    short timer;                // 0xC6
    short line_group;           // 0xC8: 0 left, 1 right
    JOBJ *joint;                // 0xCC, the platform's joint in the fountain model
    float height;               // 0xD0, joint height
    float target;               // 0xD4
    float column_height;        // 0xD8, the water column's height at scale 1
    float resurface;            // 0xDC
} FountainPlatform;

typedef char fountain_layout[(__builtin_offsetof(FountainPlatform, map_id) == 0x14 &&
                              __builtin_offsetof(FountainPlatform, state) == 0xC4 &&
                              __builtin_offsetof(FountainPlatform, joint) == 0xCC &&
                              __builtin_offsetof(FountainPlatform, resurface) == 0xDC) ? 1 : -1];

enum FountainState {
    FOUNTAIN_PICK_WAIT,
    FOUNTAIN_WAIT,  // then sinks, moves or waits again
    FOUNTAIN_MOVE,
    FOUNTAIN_SUNK,  // hides on the next frame
    FOUNTAIN_GONE,  // hidden, then resurfaces
};

#define FOUNTAIN_SIDE_MAP 4
/* The game sinks a platform to -1 and leaves its joint 1 below the column's. */
#define FOUNTAIN_SINK -1.0f
/* A held platform keeps this timer, so Random picks up from where it was held
 * about 4 seconds later: it moves, sinks or resurfaces as in a match. */
#define FOUNTAIN_RELEASE_WAIT 0xFF

const char *Fountain_Labels[FOUNTAIN_CHOICE_COUNT] = {
    "Random", "Hidden",
    "12.375", "12.750", "13.125", "13.500", "13.875", "14.250", "14.625", "15.000",
    "15.375", "15.750", "16.125", "16.500", "16.875", "17.250", "17.625", "18.000",
    "18.375", "18.750", "19.125", "19.500", "19.875", "20.250", "20.625", "21.000",
    "21.375", "21.750", "22.125", "22.500", "22.875", "23.250", "23.625", "24.000",
    "24.375", "24.750", "25.125", "25.500", "25.875", "26.250", "26.625", "27.000",
    "27.375",
};

/* Write the state the game would be in, waiting at this height or hidden. The
 * game's think still runs, updating the collision from the joint, and the held
 * timer never runs out. */
static void Fountain_Hold(GOBJ *gobj, FountainPlatform *platform, int choice)
{
    JOBJ *column = gobj->hsd_object;
    int hide = choice == FOUNTAIN_HIDDEN;
    float height = hide ? FOUNTAIN_SINK : FOUNTAIN_LOWEST + (choice - FOUNTAIN_FIRST_HEIGHT) * FOUNTAIN_STEP;
    if (column && hide != (platform->state == FOUNTAIN_GONE)) {
        if (hide) {
            JOBJ_SetFlagsAll(column, JOBJ_HIDDEN);  // as the game hides it (801CC748)
            JOBJ_RemoveAnimAll(column);
        } else {
            JOBJ_ClearFlagsAll(column, JOBJ_HIDDEN);  // as the game resurfaces it (801CC828)
            Stage_SetMapJOBJAnim(gobj, 0, 7, 0, 0.0f, 1.0f);
        }
    }
    platform->state = hide ? FOUNTAIN_GONE : FOUNTAIN_WAIT;
    platform->timer = FOUNTAIN_RELEASE_WAIT;
    platform->height = platform->target = height;

    /* The game's update after a change (801CC870): the column scales with the height. */
    JOBJ *water = column ? column->child : 0;
    if (water) {
        float scale = height / platform->column_height;
        if (scale < 0.01f)
            scale = 0.01f;
        water->scale.X = 0.5f * scale + 0.5f;
        water->scale.Y = scale;
        JOBJ_SetMtxDirtySub(water);
    }
    platform->joint->trans.Y = hide && column ? column->trans.Y + FOUNTAIN_SINK : height;
    JOBJ_SetMtxDirtySub(platform->joint);
}

/* Match on the back pointer as well as the map ID, so nothing else's data is
 * taken for a platform's. */
static FountainPlatform *Fountain_Platform(GOBJ *gobj)
{
    FountainPlatform *platform = gobj->userdata;
    if (!platform || platform->gobj != gobj || platform->map_id != FOUNTAIN_SIDE_MAP || !platform->joint)
        return 0;
    return platform;
}

void Fountain_Apply(int left_choice, int right_choice)
{
    if (Stage_GetExternalID() != GRKINDEXT_IZUMI || !*stc_gobj_lookup)
        return;
    for (GOBJ *gobj = (*stc_gobj_lookup)[MATCHPLINK_MAP]; gobj; gobj = gobj->next) {
        FountainPlatform *platform = Fountain_Platform(gobj);
        if (!platform)
            continue;
        int choice = platform->line_group == 0 ? left_choice
                   : platform->line_group == 1 ? right_choice : FOUNTAIN_RANDOM;
        if (choice > FOUNTAIN_RANDOM && choice < FOUNTAIN_CHOICE_COUNT)
            Fountain_Hold(gobj, platform, choice);
    }
}
