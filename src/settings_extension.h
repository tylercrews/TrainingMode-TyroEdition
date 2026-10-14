#ifndef TM_SETTINGS_EXTENSION_H
#define TM_SETTINGS_EXTENSION_H
#include "settings.h"

/* First three of the 23 unused event-score slots. No native score is moved. */
#define TM_EXTENSION_OFFSET 0x1AE0
#define TM_EXTENSION_SIZE 12
#define TM_EXTENSION_REGION_SIZE 92
#define TM_EXTENSION_VERSION 1
typedef char TMExtension_FitsSpareScores[(TM_EXTENSION_SIZE <= TM_EXTENSION_REGION_SIZE) ? 1 : -1];
enum { TM_EXTENSION_ABSENT, TM_EXTENSION_READY, TM_EXTENSION_REPAIRED, TM_EXTENSION_UNSUPPORTED };
void TMExtension_Init(uint8_t *record);
int TMExtension_Prepare(uint8_t *record);
uint32_t TMExtension_Read(const uint8_t *record, unsigned field, unsigned index);
int TMExtension_Write(uint8_t *record, unsigned field, unsigned index, uint32_t value);
#endif
