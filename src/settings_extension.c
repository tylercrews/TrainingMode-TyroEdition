#include "settings_extension.h"
#include <string.h>

typedef struct ExtensionPreference { uint8_t shift, width, count, initial; } ExtensionPreference;
static const ExtensionPreference tech_preferences[TM_TECHCHASE_PREF_COUNT] = {
    {0,4,9,0}, {4,1,2,0}, {5,1,2,1}, {6,2,3,0},
    {8,2,3,0}, {10,2,3,0}, {12,2,4,0}, {14,1,2,0},
};
static int signature(const uint8_t *r) { return r[0]=='T' && r[1]=='Y' && r[2]=='E' && r[3]=='X'; }
static int supported(const uint8_t *r) {
    return signature(r) && r[4]==TM_EXTENSION_VERSION && r[5]==4 && r[6]==0xA5 && r[7]==0x5A;
}
static unsigned tech_bits(const uint8_t *r) { return r[9] | ((unsigned)r[10] << 8); }
static void set_tech(uint8_t *r, unsigned index, unsigned value) {
    const ExtensionPreference *p=&tech_preferences[index];
    unsigned mask=((1u<<p->width)-1)<<p->shift;
    unsigned bits=(tech_bits(r)&~mask)|(value<<p->shift);
    r[9]=bits; r[10]=bits>>8;
}
void TMExtension_Init(uint8_t *r) {
    memset(r,0,TM_EXTENSION_SIZE);
    r[0]='T'; r[1]='Y'; r[2]='E'; r[3]='X';
    r[4]=TM_EXTENSION_VERSION; r[5]=4; r[6]=0xA5; r[7]=0x5A;
    for(unsigned i=0;i<TM_TECHCHASE_PREF_COUNT;++i) set_tech(r,i,tech_preferences[i].initial);
}
int TMExtension_Prepare(uint8_t *r) {
    if(!signature(r)) return TM_EXTENSION_ABSENT;
    /* Unknown versions/sizes retain every byte, including reserved payload. */
    if(!supported(r)) return TM_EXTENSION_UNSUPPORTED;
    int changed=0;
    for(unsigned i=0;i<TM_TECHCHASE_PREF_COUNT;++i) {
        const ExtensionPreference *p=&tech_preferences[i];
        unsigned v=(tech_bits(r)>>p->shift)&((1u<<p->width)-1);
        if(v>=p->count) { set_tech(r,i,p->initial); changed=1; }
    }
    return changed?TM_EXTENSION_REPAIRED:TM_EXTENSION_READY;
}
uint32_t TMExtension_Read(const uint8_t *r,unsigned field,unsigned index) {
    if(field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ASMASH_RELEASE_ID) return supported(r)?r[8]&7:0;
    if(field!=TM_SETTING_TECHCHASE || index>=TM_TECHCHASE_PREF_COUNT) return 0;
    const ExtensionPreference *p=&tech_preferences[index];
    unsigned v=supported(r)?(tech_bits(r)>>p->shift)&((1u<<p->width)-1):p->initial;
    return v<p->count?v:p->initial;
}
int TMExtension_Write(uint8_t *r,unsigned field,unsigned index,uint32_t value) {
    int color=field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ASMASH_RELEASE_ID;
    if(!color && (field!=TM_SETTING_TECHCHASE || index>=TM_TECHCHASE_PREF_COUNT)) return 0;
    if(value >= (color?8:tech_preferences[index].count)) return 0;
    if(signature(r) && !supported(r)) return 0;
    uint8_t before[TM_EXTENSION_SIZE]; memcpy(before,r,sizeof(before));
    if(!supported(r)) TMExtension_Init(r);
    if(color) r[8]=(r[8]&~7u)|value;
    else set_tech(r,index,value);
    for(unsigned i=0;i<TM_EXTENSION_SIZE;++i) if(before[i]!=r[i]) return 1;
    return 0;
}
