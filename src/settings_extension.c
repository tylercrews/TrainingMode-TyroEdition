#include "settings_extension.h"
#include <string.h>

typedef struct ExtensionPreference { uint8_t shift, width, count, initial; } ExtensionPreference;
static const ExtensionPreference tech_preferences[TM_TECHCHASE_PREF_COUNT] = {
    {0,4,9,0}, {4,1,2,0}, {5,1,2,1}, {6,2,3,0},
    {8,2,3,0}, {10,2,3,0}, {12,2,4,0}, {14,1,2,0}, {16,3,8,7},
    {19,2,4,0}, {21,2,4,0},
};
static const ExtensionPreference under_fire_preferences[TM_UNDERFIRE_PREF_COUNT] = {{18,1,2,0}, {19,1,2,0}};
static const ExtensionPreference ledge_preferences[TM_LEDGECHASE_PREF_COUNT] = {
    {0,2,4,0}, {2,1,2,0}, {3,3,7,0}, {6,3,6,0},
    {9,3,5,1}, {12,3,8,7}, {15,3,5,0},
};
static int signature(const uint8_t *r) { return r[0]=='T' && r[1]=='Y' && r[2]=='E' && r[3]=='X'; }
static int format(const uint8_t *r, unsigned version, unsigned payload_size) {
    return signature(r) && r[4]==version && r[5]==payload_size && r[6]==0xA5 && r[7]==0x5A;
}
static int supported(const uint8_t *r) { return format(r,TM_EXTENSION_VERSION,TM_EXTENSION_SIZE-8); }
static int legacy(const uint8_t *r) { return format(r,1,TM_EXTENSION_LEGACY_SIZE-8); }
static int version2(const uint8_t *r) { return format(r,2,TM_EXTENSION_SIZE-8); }
static int version3(const uint8_t *r) { return format(r,3,TM_EXTENSION_SIZE-8); }
static int version4(const uint8_t *r) { return format(r,4,TM_EXTENSION_SIZE-8); }
static unsigned preference_count(unsigned field) {
    return field==TM_SETTING_TECHCHASE ? TM_TECHCHASE_PREF_COUNT :
        field==TM_SETTING_LEDGECHASE ? TM_LEDGECHASE_PREF_COUNT : TM_UNDERFIRE_PREF_COUNT;
}
static const ExtensionPreference *preference(unsigned field,unsigned index) {
    if(field==TM_SETTING_TECHCHASE && index<TM_TECHCHASE_PREF_COUNT) return &tech_preferences[index];
    if(field==TM_SETTING_LEDGECHASE && index<TM_LEDGECHASE_PREF_COUNT) return &ledge_preferences[index];
    if(field==TM_SETTING_UNDERFIRE && index<TM_UNDERFIRE_PREF_COUNT) return &under_fire_preferences[index];
    return 0;
}
static unsigned preference_offset(unsigned field) { return field==TM_SETTING_TECHCHASE?9:12; }
static unsigned read_bits(const uint8_t *r,unsigned offset) {
    return r[offset] | ((unsigned)r[offset+1]<<8) | ((unsigned)r[offset+2]<<16);
}
static void set_preference(uint8_t *r,unsigned field,unsigned index,unsigned value) {
    const ExtensionPreference *p=preference(field,index);
    unsigned offset=preference_offset(field), mask=((1u<<p->width)-1)<<p->shift;
    unsigned bits=(read_bits(r,offset)&~mask)|(value<<p->shift);
    for(unsigned byte=0;byte<3;++byte) r[offset+byte]=bits>>(byte*8);
}
void TMExtension_Init(uint8_t *r) {
    memset(r,0,TM_EXTENSION_SIZE);
    r[0]='T'; r[1]='Y'; r[2]='E'; r[3]='X';
    r[4]=TM_EXTENSION_VERSION; r[5]=TM_EXTENSION_SIZE-8; r[6]=0xA5; r[7]=0x5A;
    for(unsigned i=0;i<TM_TECHCHASE_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_TECHCHASE,i,tech_preferences[i].initial);
    for(unsigned i=0;i<TM_LEDGECHASE_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_LEDGECHASE,i,ledge_preferences[i].initial);
    for(unsigned i=0;i<TM_UNDERFIRE_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_UNDERFIRE,i,under_fire_preferences[i].initial);
}
int TMExtension_Prepare(uint8_t *r) {
    if(!signature(r)) return TM_EXTENSION_ABSENT;
    int migrated=legacy(r);
    if(migrated) {
        uint8_t old[TM_EXTENSION_LEGACY_SIZE]; memcpy(old,r,sizeof(old));
        TMExtension_Init(r);
        /* Existing palette/Tech fields and still-unclaimed padding retain
         * their bits. Initialize CPU color to Gray and the two new selectors
         * to zero in byte 11, preserving its remaining high padding bit. */
        memcpy(r+8,old+8,3);
        r[11]=(old[11]&0x80)|(r[11]&7);
    } else if(version2(r) || version3(r) || version4(r)) {
        // None is the new zero/default. Preserve all four previous actions;
        // the additional bit comes from padding in the same 16-byte record.
        int v2=version2(r), v4=version4(r);
        unsigned action=(read_bits(r,12)>>15)&3;
        r[4]=TM_EXTENSION_VERSION;
        if(v2) set_preference(r,TM_SETTING_LEDGECHASE,TM_LEDGECHASE_CPU_ACTION,action+1);
        if(!v4) {
            set_preference(r,TM_SETTING_TECHCHASE,TM_TECHCHASE_HEIGHT,0);
            set_preference(r,TM_SETTING_TECHCHASE,TM_TECHCHASE_DISTANCE,0);
            set_preference(r,TM_SETTING_UNDERFIRE,TM_UNDERFIRE_ATTACK_PARTICLES,0);
        }
        set_preference(r,TM_SETTING_UNDERFIRE,TM_UNDERFIRE_COUNTER_SCORE,0);
        migrated=1;
    } else if(!supported(r)) return TM_EXTENSION_UNSUPPORTED;
    int changed=0;
    for(unsigned field=TM_SETTING_TECHCHASE;field<=TM_SETTING_UNDERFIRE;++field) {
        unsigned count=preference_count(field);
        for(unsigned i=0;i<count;++i) {
            const ExtensionPreference *p=preference(field,i);
            unsigned value=(read_bits(r,preference_offset(field))>>p->shift)&((1u<<p->width)-1);
            if(value>=p->count) { set_preference(r,field,i,p->initial); changed=1; }
        }
    }
    return migrated?TM_EXTENSION_MIGRATED:changed?TM_EXTENSION_REPAIRED:TM_EXTENSION_READY;
}
uint32_t TMExtension_Read(const uint8_t *r,unsigned field,unsigned index) {
    int v2=version2(r), older=v2||version3(r), v4=version4(r), current=supported(r)||older||v4, old=legacy(r);
    if(field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ASMASH_RELEASE_ID)
        return current||old?r[8]&7:0;
    const ExtensionPreference *p=preference(field,index);
    if(!p) return 0;
    if(v4 && field==TM_SETTING_UNDERFIRE && index==TM_UNDERFIRE_COUNTER_SCORE) return 0;
    if(older && ((field==TM_SETTING_TECHCHASE && index>=TM_TECHCHASE_HEIGHT) || field==TM_SETTING_UNDERFIRE)) return p->initial;
    unsigned value=p->initial;
    if(v2 && field==TM_SETTING_LEDGECHASE && index==TM_LEDGECHASE_CPU_ACTION)
        return ((read_bits(r,12)>>15)&3)+1;
    if(current || (old && field==TM_SETTING_TECHCHASE && index<TM_TECHCHASE_CPU_COLOR))
        value=(read_bits(r,preference_offset(field))>>p->shift)&((1u<<p->width)-1);
    return value<p->count?value:p->initial;
}
int TMExtension_Write(uint8_t *r,unsigned field,unsigned index,uint32_t value) {
    int color=field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ASMASH_RELEASE_ID;
    const ExtensionPreference *p=preference(field,index);
    if(!color && !p) return 0;
    if(value >= (color?8:p->count)) return 0;
    if(signature(r) && !supported(r) && !legacy(r) && !version2(r) && !version3(r) && !version4(r)) return 0;
    uint8_t before[TM_EXTENSION_SIZE]; memcpy(before,r,sizeof(before));
    if(legacy(r) || version2(r) || version3(r) || version4(r)) TMExtension_Prepare(r);
    else if(!supported(r)) TMExtension_Init(r);
    if(color) r[8]=(r[8]&~7u)|value;
    else set_preference(r,field,index,value);
    for(unsigned i=0;i<TM_EXTENSION_SIZE;++i) if(before[i]!=r[i]) return 1;
    return 0;
}
