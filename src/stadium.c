#include "../MexTK/include/stage.h"
#include "../MexTK/include/obj.h"
#include "../MexTK/include/dynamics.h"
#include "stadium.h"

/* NTSC 1.02 Ground.u.stadium, from grStadium_801D13E0/801D4548.
 * Only the transformation manager (map 2) has this layout. */
typedef struct StadiumData {
    unsigned char ground[0xC4];
    unsigned char flags;
    unsigned char padding[3];
    unsigned int file_size;
    void *file_buffer;
    void *archive;
    float unused;
    int timer;
    short state;
    short kind;
    short previous;
    short previous_previous;
    GOBJ *terrain;
    GOBJ *next_terrain;
} StadiumData;

typedef char stadium_layout[(__builtin_offsetof(StadiumData, state) == 0xDC &&
                            __builtin_offsetof(StadiumData, next_terrain) == 0xE8) ? 1 : -1];

/* Retain the game's asynchronous loading and terrain/collision lifecycle. */
extern void Stage_StadiumUpdate(GOBJ *gobj);
extern void Stage_UnloadSecondaryArchive(void *archive);
extern void File_ReadAsync(char *name, void *buffer, unsigned int *size,
                           void *callback, int argument);

static const short kinds[] = {5, 3, 4, 6, 9};
static char *const files[] = {0, "GrPs1.dat", "GrPs2.dat", "GrPs4.dat", "GrPs3.dat"};
static int requested;
static int update_wind;

void Stadium_Select(int value)
{
    if (value >= 0 && value < (int)(sizeof(kinds) / sizeof(*kinds)))
        requested = value;
}

static void Stadium_Think(GOBJ *gobj)
{
    StadiumData *data = gobj->userdata;
    /* Match the native pre-GO gate. The on-GO callback clears this bit. */
    if (!(data->flags & 0x80)) {
        if (data->state == 0 && data->kind != kinds[requested]) {
            /* Native Stadium returns to Normal before reusing its buffer.
             * Never load over a live terrain's assets. */
            int target = data->kind == 5 ? requested : 0;
            data->previous_previous = data->previous;
            data->previous = data->kind;
            data->kind = kinds[target];
            if (target == 0) {
                data->state = 2;
            } else {
                Stage_UnloadSecondaryArchive(data->archive);
                data->archive = 0;
                data->flags |= 0x40;
                File_ReadAsync(files[target], data->file_buffer, &data->file_size,
                               (void *)0x801D4220, 0);
                data->state = 1;
            }
        }
        /* Calling at idle would run the random timer. Queue changed selections
         * until the active native transition finishes. */
        if (data->state != 0)
            Stage_StadiumUpdate(gobj);
    }
    Stage_UpdateMovingColl(gobj);
    /* Hazards_Disable already schedules this when it removes Stadium's proc. */
    if (update_wind)
        Dynamics_DecayWind();
}

void Stadium_Init(int hazards_disabled)
{
    requested = 0;
    update_wind = !hazards_disabled;
    if (Stage_GetExternalID() != GRKINDEXT_PSTAD)
        return;
    GOBJ *gobj = Stage_GetMapGObj(2);
    if (!gobj || !gobj->userdata)
        return;
    GObj_RemoveProc(gobj);
    GObj_AddProc(gobj, Stadium_Think, 4);
}
