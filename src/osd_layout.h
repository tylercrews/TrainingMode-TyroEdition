#ifndef TM_OSD_LAYOUT_H
#define TM_OSD_LAYOUT_H
#include <stdint.h>
#include "settings.h"

#define TM_OSD_PLAYERS 6
#define TM_OSD_SLOTS (TM_OSD_CATEGORIES + 1) /* Runtime-only JC Shine child slot, sharing setting 8. */
#define TM_OSD_KEYS (TM_OSD_PLAYERS * TM_OSD_SLOTS)
#define TM_OSD_GRID_COLUMNS 5
#define TM_OSD_GRID_ROWS 6
#define TM_OSD_GRID_CELLS (TM_OSD_GRID_COLUMNS * TM_OSD_GRID_ROWS) /* One page; a saved cell is 1 + its index. */
#define TM_OSD_GRID_AUTO_CELLS 10 /* Auto OSDs fill the top two rows of each page. */
#define TM_OSD_PANEL_CELLS 9
typedef char TMOSD_GridPositions[(TM_OSD_GRID_CELLS == TM_OSD_GRID_POSITIONS && TM_OSD_SLOTS == TM_OSD_GRID_SLOTS) ? 1 : -1];
#define TM_OSD_HISTORY 4 /* Latest + three previous scores. */

typedef struct TMOSDHistory {
    int frame[TM_OSD_HISTORY];
    uint32_t color[TM_OSD_HISTORY];
    uint32_t native_frame;
    int kind;
    uint8_t count, turn[TM_OSD_HISTORY], seen;
} TMOSDHistory;
typedef struct TMOSDMap {
    uint32_t mask;
    uint8_t owners[TM_OSD_PLAYERS], owner_count;
    uint32_t split_owners;
    uint8_t cells[TM_OSD_SLOTS]; /* Fixed Grid only: 0 Auto, else 1 + cell, by slot. */
    uint8_t placed; /* Any cell assigned; otherwise Fixed Grid keeps the original order. */
} TMOSDMap;
typedef struct TMOSDCell {
    int key, page, cell;
    float x, y;
} TMOSDCell;

int TMOSD_Key(int player, int category);
int TMOSD_Category(int key);
int TMOSD_MessageKey(int player, int category, int kind);
void TMOSD_MapSplitOwners(TMOSDMap *map, unsigned owners);
void TMOSD_MapReset(TMOSDMap *map, uint32_t mask);
void TMOSD_MapOwners(TMOSDMap *map, unsigned eligible);
void TMOSD_MapCells(TMOSDMap *map, const uint8_t cells[TM_OSD_SLOTS]);
int TMOSD_PageOwner(const TMOSDMap *map, unsigned layout, unsigned page);
unsigned TMOSD_Count(const TMOSDMap *map);
unsigned TMOSD_Capacity(unsigned layout);
unsigned TMOSD_PageCount(const TMOSDMap *map, unsigned layout);
TMOSDCell TMOSD_Cell(const TMOSDMap *map, int key, unsigned layout);
void TMOSD_HistoryPush(TMOSDHistory *history, int kind, int frame, uint32_t color,
                       int turn, uint32_t native_frame);
#endif
