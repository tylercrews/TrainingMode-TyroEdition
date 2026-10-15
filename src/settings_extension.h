#ifndef TM_SETTINGS_EXTENSION_H
#define TM_SETTINGS_EXTENSION_H
#include "settings.h"

/* Legacy event scores retain slots 0..27; Ledgechase owns slot 50. */
#define TM_EVENT_SCORE_OFFSET 0x1A70
#define TM_EVENT_LEGACY_SCORE_SLOTS 28
#define TM_EVENT_LEDGECHASE_SCORE_SLOT 50
#define TM_EVENT_UNDERFIRE_SCORE_SLOT 4
#define TM_EVENT_NATIVE_SCORE_SLOTS 51
#define TM_EXTENSION_OFFSET 0x1AE0
#define TM_EXTENSION_REGION_END (TM_EVENT_SCORE_OFFSET + TM_EVENT_LEDGECHASE_SCORE_SLOT * 4)
#define TM_EXTENSION_SIZE 20
#define TM_EXTENSION_REGION_SIZE (TM_EXTENSION_REGION_END - TM_EXTENSION_OFFSET)
#define TM_EXTENSION_VERSION 10
#define TM_EXTENSION_LEGACY_SIZE 12
typedef char TMExtension_FitsSpareScores[(TM_EXTENSION_SIZE <= TM_EXTENSION_REGION_SIZE) ? 1 : -1];
enum { TM_EXTENSION_ABSENT, TM_EXTENSION_READY, TM_EXTENSION_REPAIRED, TM_EXTENSION_UNSUPPORTED,
       TM_EXTENSION_MIGRATED };
void TMExtension_Init(uint8_t *record);
int TMExtension_Prepare(uint8_t *record);
uint32_t TMExtension_Read(const uint8_t *record, unsigned field, unsigned index);
int TMExtension_Write(uint8_t *record, unsigned field, unsigned index, uint32_t value);
#endif
