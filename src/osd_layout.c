#include "osd_layout.h"
#include <string.h>

int TMOSD_Key(int player, int category) {
    if ((unsigned)player >= TM_OSD_PLAYERS) return -1;
    if (category == 64) return player * TM_OSD_SLOTS + TM_OSD_CATEGORIES;
    for (unsigned i = 0; i < TM_OSD_CATEGORIES; ++i)
        if (TMSettings_OSDCategoryID(i) == (unsigned)category) return player * TM_OSD_SLOTS + i;
    return -1;
}
int TMOSD_Category(int key) {
    if ((unsigned)key >= TM_OSD_KEYS) return -1;
    unsigned slot = key % TM_OSD_SLOTS;
    return slot == TM_OSD_CATEGORIES ? 8 : TMSettings_OSDCategoryID(slot);
}
int TMOSD_MessageKey(int player, int category, int kind) { return TMOSD_Key(player, category == 8 && kind == 64 ? 64 : category); }
void TMOSD_MapSplitOwners(TMOSDMap *map, unsigned owners) { map->split_owners = owners & 63; }
void TMOSD_MapReset(TMOSDMap *map, uint32_t mask) {
    /* Cell choices are saved configuration, not ownership; keep them. */
    uint8_t cells[TM_OSD_SLOTS], placed = map->placed;
    memcpy(cells, map->cells, sizeof(cells));
    memset(map, 0, sizeof(*map)); map->mask = mask;
    memcpy(map->cells, cells, sizeof(cells)); map->placed = placed;
}
void TMOSD_MapCells(TMOSDMap *map, const uint8_t cells[TM_OSD_SLOTS]) {
    /* A damaged save may repeat or exceed a cell; the earlier slot keeps it. */
    uint32_t taken = 0;
    map->placed = 0;
    for (unsigned slot = 0; slot < TM_OSD_SLOTS; ++slot) {
        unsigned cell = cells[slot];
        if (cell > TM_OSD_GRID_CELLS || (taken & (1u << cell))) cell = 0;
        if (cell) { taken |= 1u << cell; map->placed = 1; }
        map->cells[slot] = cell;
    }
}
void TMOSD_MapOwners(TMOSDMap *map, unsigned eligible) {
    for (unsigned player = 0; player < TM_OSD_PLAYERS; ++player) {
        if (!(eligible & (1u << player))) continue;
        unsigned i = 0;
        while (i < map->owner_count && map->owners[i] != player) ++i;
        if (i == map->owner_count && i < TM_OSD_PLAYERS) map->owners[map->owner_count++] = player;
    }
}
static unsigned categories(const TMOSDMap *map) {
    unsigned count = 0;
    for (unsigned i = 0; i < TM_OSD_CATEGORIES; ++i) count += !!(map->mask & (1u << TMSettings_OSDCategoryID(i)));
    return count;
}
static unsigned split(const TMOSDMap *map, unsigned player) { return !!(map->split_owners & (1u << player)) && !!(map->mask & (1u << 8)); }
unsigned TMOSD_Count(const TMOSDMap *map) {
    unsigned count = categories(map) * map->owner_count;
    for (unsigned i = 0; i < map->owner_count; ++i) count += split(map, map->owners[i]);
    return count;
}
/* Cells filled in order per page: Auto OSDs only ever use the grid's top two rows. */
unsigned TMOSD_Capacity(unsigned layout) { return layout == TM_OSD_PANEL ? TM_OSD_PANEL_CELLS : TM_OSD_GRID_AUTO_CELLS; }
static int placed_mode(const TMOSDMap *map, unsigned layout) { return map->placed && layout == TM_OSD_FIXED; }
static unsigned drawn(const TMOSDMap *map, unsigned player, unsigned slot) {
    return slot == TM_OSD_CATEGORIES ? split(map, player) : !!(map->mask & (1u << TMSettings_OSDCategoryID(slot)));
}
/* Auto order is the default order, with JC Shine right after Fighter-specific Tech (slot 4). */
static unsigned auto_order(unsigned i) { return i < 5 ? i : i == 5 ? TM_OSD_CATEGORIES : i - 1; }
/* On a player's first page, Auto OSDs start after the highest top-two-row cell
 * any OSD was given, so blanks left between placed OSDs stay blank. */
static unsigned auto_first(const TMOSDMap *map) {
    unsigned first = 0;
    for (unsigned slot = 0; slot < TM_OSD_SLOTS; ++slot)
        if (map->cells[slot] <= TM_OSD_GRID_AUTO_CELLS && map->cells[slot] > first) first = map->cells[slot];
    return first;
}
/* With cells assigned, each player's block starts on a fresh page: its placed
 * cells on that page, then Auto OSDs overflowing ten per page. */
static unsigned placed_pages(const TMOSDMap *map, unsigned player) {
    unsigned autos = 0, room = TM_OSD_GRID_AUTO_CELLS - auto_first(map);
    for (unsigned slot = 0; slot < TM_OSD_SLOTS; ++slot) autos += !map->cells[slot] && drawn(map, player, slot);
    return autos <= room ? 1 : 1 + (autos - room + TM_OSD_GRID_AUTO_CELLS - 1) / TM_OSD_GRID_AUTO_CELLS;
}
unsigned TMOSD_PageCount(const TMOSDMap *map, unsigned layout) {
    unsigned count = TMOSD_Count(map), capacity = TMOSD_Capacity(layout);
    if (count && placed_mode(map, layout)) {
        unsigned pages = 0;
        for (unsigned i = 0; i < map->owner_count; ++i) pages += placed_pages(map, map->owners[i]);
        return pages;
    }
    return count ? (count + capacity - 1) / capacity : 1;
}
int TMOSD_PageOwner(const TMOSDMap *map, unsigned layout, unsigned page) {
    if (!TMOSD_Count(map) || !placed_mode(map, layout)) return -1;
    for (unsigned i = 0, first = 0; i < map->owner_count; ++i) {
        first += placed_pages(map, map->owners[i]);
        if (page < first) return map->owners[i];
    }
    return -1;
}
TMOSDCell TMOSD_Cell(const TMOSDMap *map, int key, unsigned layout) {
    TMOSDCell out = {.key = -1, .page = -1, .cell = -1};
    int category = TMOSD_Category(key);
    if (category < 0 || !(map->mask & (1u << category))) return out;
    unsigned owner = 0;
    unsigned player = key / TM_OSD_SLOTS, slot = key % TM_OSD_SLOTS;
    if (slot == TM_OSD_CATEGORIES && !split(map, player)) return out;
    while (owner < map->owner_count && map->owners[owner] != player) ++owner;
    if (owner == map->owner_count) return out;
    out.key = key;
    if (placed_mode(map, layout)) {
        unsigned page = 0, cell;
        for (unsigned i = 0; i < owner; ++i) page += placed_pages(map, map->owners[i]);
        if (map->cells[slot]) cell = map->cells[slot] - 1;
        else {
            unsigned rank = 0, first = auto_first(map), room = TM_OSD_GRID_AUTO_CELLS - first;
            for (unsigned i = 0; auto_order(i) != slot; ++i)
                rank += !map->cells[auto_order(i)] && drawn(map, player, auto_order(i));
            if (rank < room) cell = first + rank;
            else {
                rank -= room;
                page += 1 + rank / TM_OSD_GRID_AUTO_CELLS; cell = rank % TM_OSD_GRID_AUTO_CELLS;
            }
        }
        out.page = page; out.cell = cell;
    } else {
        unsigned index = owner * categories(map);
        for (unsigned i = 0; i < owner; ++i) index += split(map, map->owners[i]);
        unsigned before = slot == TM_OSD_CATEGORIES ? 5 : slot;
        for (unsigned i = 0; i < before; ++i)
            index += !!(map->mask & (1u << TMSettings_OSDCategoryID(i)));
        if (slot != TM_OSD_CATEGORIES && slot > 4) index += split(map, player);
        unsigned capacity = TMOSD_Capacity(layout);
        out.page = index / capacity; out.cell = index % capacity;
    }
    /* Six 5.15-unit grid boxes fit between the screen top (y 24.27) and the
     * OSD camera's clip at y -16.18, with ~10 px margins and ~15 px gaps. */
    out.x = layout == TM_OSD_PANEL ? -26.f : -23.2f + (out.cell % TM_OSD_GRID_COLUMNS) * 11.6f;
    out.y = layout == TM_OSD_PANEL ? 18.f - out.cell * 3.4f : 20.7f - (out.cell / TM_OSD_GRID_COLUMNS) * 6.65f;
    return out;
}
void TMOSD_HistoryPush(TMOSDHistory *h, int kind, int frame, uint32_t color, int turn, uint32_t native_frame) {
    if (frame < 0) return;
    if (h->seen && h->kind == kind && h->native_frame == native_frame && h->count &&
        h->frame[h->count - 1] == frame && h->turn[h->count - 1] == turn) return;
    if (h->seen && h->kind != kind) h->count = 0; /* Do not mix unrelated category sub-techniques. */
    if (h->count == TM_OSD_HISTORY) {
        for (unsigned i = 0; i < TM_OSD_HISTORY - 1; ++i) {
            h->frame[i] = h->frame[i + 1]; h->color[i] = h->color[i + 1]; h->turn[i] = h->turn[i + 1];
        }
        --h->count;
    }
    unsigned i = h->count++;
    h->frame[i] = frame; h->color[i] = color; h->turn[i] = turn;
    h->kind = kind; h->native_frame = native_frame; h->seen = 1;
}
