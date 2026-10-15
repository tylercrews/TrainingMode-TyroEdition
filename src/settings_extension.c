#include "settings_extension.h"
#include "cpu_ground_actions.h"
#include "cpu_air_attacks.h"
#include "ledgechase_logic.h"
#include "shield_training.h"
#include <string.h>

typedef struct ExtensionPreference { uint8_t shift, width, count, initial; } ExtensionPreference;
static const ExtensionPreference tech_preferences[TM_TECHCHASE_PREF_COUNT] = {
    {0,4,9,0}, {4,1,2,0}, {5,1,2,1}, {6,2,3,0},
    {8,2,3,0}, {10,2,3,0}, {12,2,4,0}, {14,1,2,0}, {16,3,8,7},
    {19,2,4,0}, {21,2,4,0},
};
static const ExtensionPreference under_fire_preferences[TM_UNDERFIRE_PREF_COUNT] = {{18,1,2,0}, {19,1,2,0}};
static const ExtensionPreference low_laser_preferences[TM_LOW_LASER_PREF_COUNT] = {{20,1,2,1}, {21,1,2,0}};
static const ExtensionPreference powershield_preferences[TM_PS_PREF_COUNT] = {
    {0,2,4,0}, {2,6,61,0}, {8,6,61,20}, {14,3,5,0},
    {17,1,2,0}, {18,2,4,0}, {20,1,2,1}, {21,1,2,1},
    {22,1,2,0}, {23,1,2,1},
};
/* Appended word at bytes 20..23; older settings retain all of their bits. */
static const ExtensionPreference reversal_preferences[TM_REV_PREF_COUNT] = {
    {0,4,REV_ACTION_COUNT,0}, {4,2,4,0}, {6,1,2,0}, {7,1,2,0}, {8,1,2,0},
};
static const ExtensionPreference attack_shield_preferences[TM_AOS_PREF_COUNT] = {
    {9,4,AOS_ACTION_COUNT,0}, {13,3,5,0}, {16,1,2,1}, {17,2,4,0}, {19,1,2,0},
};
static const ExtensionPreference ledge_preferences[TM_LEDGECHASE_PREF_COUNT] = {
    {0,2,4,0}, {2,1,2,0}, {3,3,7,0}, {24,4,LC_PRESET_COUNT,0},
    {9,3,5,1}, {12,3,8,7}, {24,6,CPU_GROUND_ACTION_COUNT,0},
    {28,4,CPU_AIR_ATTACK_COUNT,0},
    {22,2,LC_ENTRY_COUNT,LC_ENTRY_JUMP}, {30,2,LC_HOP_HEIGHT_COUNT,LC_HOP_LOW},
};
static int signature(const uint8_t *r) { return r[0]=='T' && r[1]=='Y' && r[2]=='E' && r[3]=='X'; }
static int format(const uint8_t *r, unsigned version, unsigned payload_size) {
    return signature(r) && r[4]==version && r[5]==payload_size && r[6]==0xA5 && r[7]==0x5A;
}
static int supported(const uint8_t *r) { return format(r,TM_EXTENSION_VERSION,TM_EXTENSION_SIZE-8); }
static int legacy(const uint8_t *r) { return format(r,1,TM_EXTENSION_LEGACY_SIZE-8); }
static int version2(const uint8_t *r) { return format(r,2,8); }
static int version3(const uint8_t *r) { return format(r,3,8); }
static int version4(const uint8_t *r) { return format(r,4,8); }
static int version5(const uint8_t *r) { return format(r,5,8); }
static int version6(const uint8_t *r) { return format(r,6,8); }
static int version7(const uint8_t *r) { return format(r,7,8); }
static int version8(const uint8_t *r) { return format(r,8,12); }
static int version9(const uint8_t *r) { return format(r,9,12); }
static int version10(const uint8_t *r) { return format(r,10,12); }
static int version11(const uint8_t *r) { return format(r,11,16); }
/* Old None, Spotdodge, inward Roll, Dash, Smash menu choices. */
static unsigned migrate_ledge_action(unsigned action) {
    static const uint8_t choices[] = {0,1,13,31,11};
    return action < sizeof(choices) ? choices[action] : 0;
}
static unsigned preference_count(unsigned field) {
    return field==TM_SETTING_TECHCHASE ? TM_TECHCHASE_PREF_COUNT :
        field==TM_SETTING_LEDGECHASE ? TM_LEDGECHASE_PREF_COUNT :
        field==TM_SETTING_UNDERFIRE ? TM_UNDERFIRE_PREF_COUNT :
        field==TM_SETTING_LOW_LASER ? TM_LOW_LASER_PREF_COUNT :
        field==TM_SETTING_POWERSHIELD ? TM_PS_PREF_COUNT :
        field==TM_SETTING_REVERSAL ? TM_REV_PREF_COUNT :
        field==TM_SETTING_ATTACK_SHIELD ? TM_AOS_PREF_COUNT : 0;
}
static const ExtensionPreference *preference(unsigned field,unsigned index) {
    if(field==TM_SETTING_TECHCHASE && index<TM_TECHCHASE_PREF_COUNT) return &tech_preferences[index];
    if(field==TM_SETTING_LEDGECHASE && index<TM_LEDGECHASE_PREF_COUNT) return &ledge_preferences[index];
    if(field==TM_SETTING_UNDERFIRE && index<TM_UNDERFIRE_PREF_COUNT) return &under_fire_preferences[index];
    if(field==TM_SETTING_LOW_LASER && index<TM_LOW_LASER_PREF_COUNT) return &low_laser_preferences[index];
    if(field==TM_SETTING_POWERSHIELD && index<TM_PS_PREF_COUNT) return &powershield_preferences[index];
    if(field==TM_SETTING_REVERSAL && index<TM_REV_PREF_COUNT) return &reversal_preferences[index];
    if(field==TM_SETTING_ATTACK_SHIELD && index<TM_AOS_PREF_COUNT) return &attack_shield_preferences[index];
    return 0;
}
static unsigned preference_offset(unsigned field, unsigned index) {
    if(field==TM_SETTING_REVERSAL || field==TM_SETTING_ATTACK_SHIELD) return 20;
    if(field==TM_SETTING_LEDGECHASE && (index==TM_LEDGECHASE_PRESET || index==TM_LEDGECHASE_AIR_ATTACK)) return 16;
    return field==TM_SETTING_TECHCHASE?9:field==TM_SETTING_POWERSHIELD?16:12;
}
static unsigned read_bits(const uint8_t *r,unsigned offset) {
    return r[offset] | ((unsigned)r[offset+1]<<8) | ((unsigned)r[offset+2]<<16) | ((unsigned)r[offset+3]<<24);
}
static unsigned preference_value(const uint8_t *r,unsigned field,unsigned index) {
    const ExtensionPreference *p=preference(field,index);
    unsigned bits=read_bits(r,preference_offset(field,index));
    unsigned value=(bits>>p->shift)&((1u<<p->width)-1);
    /* Retain the original four attack bits; the fifth occupies new padding. */
    if(field==TM_SETTING_REVERSAL && index==TM_REV_ATTACK) value|=((bits>>20)&1)<<4;
    return value;
}
static void set_preference(uint8_t *r,unsigned field,unsigned index,unsigned value) {
    const ExtensionPreference *p=preference(field,index);
    unsigned offset=preference_offset(field,index), mask=((1u<<p->width)-1)<<p->shift;
    unsigned bits=(read_bits(r,offset)&~mask)|((value<<p->shift)&mask);
    if(field==TM_SETTING_REVERSAL && index==TM_REV_ATTACK) bits=(bits&~(1u<<20))|((value>>4)<<20);
    for(unsigned byte=0;byte<4;++byte) r[offset+byte]=bits>>(byte*8);
}
static void init_shield_preferences(uint8_t *r) {
    memset(r+20,0,4);
    for(unsigned field=TM_SETTING_REVERSAL;field<=TM_SETTING_ATTACK_SHIELD;++field)
        for(unsigned i=0;i<preference_count(field);++i)
            set_preference(r,field,i,preference(field,i)->initial);
}
void TMExtension_Init(uint8_t *r) {
    memset(r,0,TM_EXTENSION_SIZE);
    init_shield_preferences(r);
    r[0]='T'; r[1]='Y'; r[2]='E'; r[3]='X';
    r[4]=TM_EXTENSION_VERSION; r[5]=TM_EXTENSION_SIZE-8; r[6]=0xA5; r[7]=0x5A;
    for(unsigned i=0;i<TM_TECHCHASE_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_TECHCHASE,i,tech_preferences[i].initial);
    for(unsigned i=0;i<TM_LEDGECHASE_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_LEDGECHASE,i,ledge_preferences[i].initial);
    for(unsigned i=0;i<TM_UNDERFIRE_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_UNDERFIRE,i,under_fire_preferences[i].initial);
    for(unsigned i=0;i<TM_LOW_LASER_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_LOW_LASER,i,low_laser_preferences[i].initial);
    for(unsigned i=0;i<TM_PS_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_POWERSHIELD,i,powershield_preferences[i].initial);
}
int TMExtension_Prepare(uint8_t *r) {
    if(!signature(r)) return TM_EXTENSION_ABSENT;
    int migrated=legacy(r), keep_shield=version11(r);
    if(migrated) {
        uint8_t old[TM_EXTENSION_LEGACY_SIZE]; memcpy(old,r,sizeof(old));
        TMExtension_Init(r);
        /* Existing palette/Tech fields and still-unclaimed padding retain
         * their bits. Initialize CPU color to Gray and the two new selectors
         * to zero in byte 11, preserving its remaining high padding bit. */
        memcpy(r+8,old+8,3);
        r[11]=(old[11]&0x80)|(r[11]&7);
    } else if(version11(r)) {
        /* Keep every previous preference/padding bit except the newly claimed
         * Shield DI bit. Old invalid actions must not become new choices. */
        unsigned action=(read_bits(r,20)>>9)&15;
        if(action>=10) set_preference(r,TM_SETTING_ATTACK_SHIELD,TM_AOS_ACTION,0);
        set_preference(r,TM_SETTING_ATTACK_SHIELD,TM_AOS_SHIELD_DI,0);
        unsigned attack=read_bits(r,20)&15;
        set_preference(r,TM_SETTING_REVERSAL,TM_REV_ATTACK,attack<14?attack:0);
        r[4]=TM_EXTENSION_VERSION;
        migrated=1;
    } else if(version10(r)) {
        r[4]=TM_EXTENSION_VERSION; r[5]=TM_EXTENSION_SIZE-8;
        migrated=1;
    } else if(version2(r) || version3(r) || version4(r) || version5(r) || version6(r) || version7(r) || version8(r) || version9(r)) {
        // Move the expanded action index to the spare final byte. Keep all
        // older choices and the intervening event preferences intact.
        int v2=version2(r), v4=version4(r), v5=version5(r), v6=version6(r), v7=version7(r), v8=version8(r), v9=version9(r);
        unsigned action=(read_bits(r,12)>>15)&(v2?3:7);
        unsigned preset=v9?r[19]&15:(read_bits(r,12)>>6)&7;
        r[4]=TM_EXTENSION_VERSION;
        r[5]=TM_EXTENSION_SIZE-8;
        if(!v7 && !v8 && !v9) set_preference(r,TM_SETTING_LEDGECHASE,TM_LEDGECHASE_CPU_ACTION,migrate_ledge_action(action+v2));
        if(!v4 && !v5 && !v6 && !v7 && !v8 && !v9) {
            set_preference(r,TM_SETTING_TECHCHASE,TM_TECHCHASE_HEIGHT,0);
            set_preference(r,TM_SETTING_TECHCHASE,TM_TECHCHASE_DISTANCE,0);
            set_preference(r,TM_SETTING_UNDERFIRE,TM_UNDERFIRE_ATTACK_PARTICLES,0);
        }
        if(!v5 && !v6 && !v7 && !v8 && !v9) set_preference(r,TM_SETTING_UNDERFIRE,TM_UNDERFIRE_COUNTER_SCORE,0);
        if(!v6 && !v7 && !v8 && !v9) for(unsigned i=0;i<TM_LOW_LASER_PREF_COUNT;++i)
            set_preference(r,TM_SETTING_LOW_LASER,i,low_laser_preferences[i].initial);
        /* Claim only the four newly owned bytes, never inherited score padding. */
        if(!v8 && !v9) {
            memset(r+16,0,4);
            for(unsigned i=0;i<TM_PS_PREF_COUNT;++i)
                set_preference(r,TM_SETTING_POWERSHIELD,i,powershield_preferences[i].initial);
        }
        set_preference(r,TM_SETTING_LEDGECHASE,TM_LEDGECHASE_PRESET,preset<(v9?14:7)?preset:0);
        if(!v9) set_preference(r,TM_SETTING_LEDGECHASE,TM_LEDGECHASE_AIR_ATTACK,0);
        set_preference(r,TM_SETTING_LEDGECHASE,TM_LEDGECHASE_ENTRY,LC_ENTRY_JUMP);
        set_preference(r,TM_SETTING_LEDGECHASE,TM_LEDGECHASE_HOP_HEIGHT,LC_HOP_LOW);
        migrated=1;
    } else if(!supported(r)) return TM_EXTENSION_UNSUPPORTED;
    if(migrated && !keep_shield) init_shield_preferences(r);
    int changed=0;
    for(unsigned field=TM_SETTING_TECHCHASE;field<=TM_SETTING_ATTACK_SHIELD;++field) {
        unsigned count=preference_count(field);
        for(unsigned i=0;i<count;++i) {
            const ExtensionPreference *p=preference(field,i);
            unsigned value=preference_value(r,field,i);
            if(value>=p->count) { set_preference(r,field,i,p->initial); changed=1; }
        }
    }
    return migrated?TM_EXTENSION_MIGRATED:changed?TM_EXTENSION_REPAIRED:TM_EXTENSION_READY;
}
uint32_t TMExtension_Read(const uint8_t *r,unsigned field,unsigned index) {
    int v2=version2(r), older=v2||version3(r), v4=version4(r), v5=version5(r), v6=version6(r), v7=version7(r), v8=version8(r), v9=version9(r), v10=version10(r), v11=version11(r),
        current=supported(r)||older||v4||v5||v6||v7||v8||v9||v10||v11, old=legacy(r);
    if(field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ASMASH_RELEASE_ID)
        return current||old?r[8]&7:0;
    const ExtensionPreference *p=preference(field,index);
    if(!p) return 0;
    if((field==TM_SETTING_REVERSAL || field==TM_SETTING_ATTACK_SHIELD) && !supported(r) && !v11) return p->initial;
    if(v11 && field==TM_SETTING_ATTACK_SHIELD && index==TM_AOS_SHIELD_DI) return p->initial;
    if(v11 && field==TM_SETTING_ATTACK_SHIELD && index==TM_AOS_ACTION) {
        unsigned action=(read_bits(r,20)>>9)&15;
        return action<10?action:0;
    }
    if(v11 && field==TM_SETTING_REVERSAL && index==TM_REV_ATTACK) return (r[20]&15)<14?r[20]&15:0;
    if(field==TM_SETTING_POWERSHIELD && !supported(r) && !v8 && !v9 && !v10 && !v11) return p->initial;
    if(field==TM_SETTING_LOW_LASER && !supported(r) && !v6 && !v7 && !v8 && !v9 && !v10 && !v11) return p->initial;
    if(field==TM_SETTING_LEDGECHASE && !supported(r) && !v10 && !v11) {
        if(index==TM_LEDGECHASE_ENTRY || index==TM_LEDGECHASE_HOP_HEIGHT) return p->initial;
        if(v9 && index==TM_LEDGECHASE_PRESET) return (r[19]&15)<14?r[19]&15:0;
    }
    if(field==TM_SETTING_LEDGECHASE && !supported(r) && !v10 && !v11 && !v9) {
        if(index==TM_LEDGECHASE_AIR_ATTACK) return p->initial;
        if(index==TM_LEDGECHASE_PRESET) {
            unsigned preset=current?(read_bits(r,12)>>6)&7:0;
            return preset<7?preset:0;
        }
    }
    if(v4 && field==TM_SETTING_UNDERFIRE && index==TM_UNDERFIRE_COUNTER_SCORE) return 0;
    if(older && ((field==TM_SETTING_TECHCHASE && index>=TM_TECHCHASE_HEIGHT) || field==TM_SETTING_UNDERFIRE)) return p->initial;
    unsigned value=p->initial;
    if((older||v4||v5||v6) && field==TM_SETTING_LEDGECHASE && index==TM_LEDGECHASE_CPU_ACTION)
        return migrate_ledge_action(((read_bits(r,12)>>15)&(v2?3:7))+v2);
    if(current || (old && field==TM_SETTING_TECHCHASE && index<TM_TECHCHASE_CPU_COLOR))
        value=preference_value(r,field,index);
    return value<p->count?value:p->initial;
}
int TMExtension_Write(uint8_t *r,unsigned field,unsigned index,uint32_t value) {
    int color=field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ASMASH_RELEASE_ID;
    const ExtensionPreference *p=preference(field,index);
    if(!color && !p) return 0;
    if(value >= (color?8:p->count)) return 0;
    if(signature(r) && !supported(r) && !legacy(r) && !version2(r) && !version3(r) && !version4(r) && !version5(r) && !version6(r) && !version7(r) && !version8(r) && !version9(r) && !version10(r) && !version11(r)) return 0;
    uint8_t before[TM_EXTENSION_SIZE]; memcpy(before,r,sizeof(before));
    if(legacy(r) || version2(r) || version3(r) || version4(r) || version5(r) || version6(r) || version7(r) || version8(r) || version9(r) || version10(r) || version11(r)) TMExtension_Prepare(r);
    else if(!supported(r)) TMExtension_Init(r);
    if(color) r[8]=(r[8]&~7u)|value;
    else set_preference(r,field,index,value);
    for(unsigned i=0;i<TM_EXTENSION_SIZE;++i) if(before[i]!=r[i]) return 1;
    return 0;
}
