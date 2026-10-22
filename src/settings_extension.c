#include "settings_extension.h"
#include "cpu_ground_actions.h"
#include "cpu_air_attacks.h"
#include "ledgechase_logic.h"
#include "shield_training.h"
#include <string.h>

typedef struct ExtensionPreference { uint8_t shift, width; uint16_t count, initial; } ExtensionPreference;
static const ExtensionPreference tech_preferences[TM_TECHCHASE_PREF_COUNT] = {
    {0,4,9,0}, {4,1,2,0}, {5,1,2,1}, {6,2,3,0},
    {8,2,3,0}, {10,2,3,0}, {12,2,4,0}, {14,1,2,0}, {16,3,8,7},
    {19,2,4,0}, {21,2,4,0},
};
/* The Lava Overlay bit is word 20's former padding: every record that owns
 * word 20 (version 11 on) zeroed it, so no version bump is needed. */
static const ExtensionPreference under_fire_preferences[TM_UNDERFIRE_PREF_COUNT] = {{18,1,2,0}, {19,1,2,0}, {21,1,2,0}};
/* Position and height occupy byte 35 (word at 32, bits 24..30). */
static const ExtensionPreference low_laser_preferences[TM_LOW_LASER_PREF_COUNT] = {
    {20,1,2,1}, {21,1,2,0}, {24,4,10,0}, {28,3,5,0},
};
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
/* Version 18 appends bytes 52..63, three words: 52 the Custom Hit Strength
 * ranges, 56 the percents, 60 Hit Strength and the recovery masks. Version 19
 * appends word 64: the recovery masks of the CPUs added after Marth. Each
 * mask's initial value is its recovery menu's defaults in recovery.c.
 * Player Position takes word 60's spare bits 30..31, which every record that
 * owns word 60 zeroed. It is stored as an offset from Close so those zero bits
 * read as Close, the original spot, with no version bump. */
#define EDGEGUARD_OFFSET 52
#define EDGEGUARD_SIZE 12
#define EDGEGUARD2_OFFSET 64
#define EDGEGUARD2_SIZE 4
static const ExtensionPreference edgeguard_preferences[TM_EG_PREF_COUNT] = {
    {0,9,301,100}, {9,9,301,120}, {18,7,91,45}, {25,7,91,60},
    {0,10,999,0}, {10,10,1000,50}, {20,10,1000,80},
    {0,3,5,1}, {3,6,64,1}, {9,6,64,1}, {15,6,64,7}, {21,5,32,3}, {26,4,16,3},
    /* word 64: Yoshi, Peach, Luigi, Dr. Mario, Ice Climbers, Jigglypuff, Samus */
    {0,5,32,7}, {5,5,32,7}, {10,5,32,7}, {15,5,32,3}, {20,3,8,3}, {23,4,16,15}, {27,5,32,15},
    /* word 60 again: Player Position */
    {30,2,TM_EG_POSITION_COUNT,TM_EG_POSITION_CLOSE},
};
#define EG_POSITION_STORED(value) (((value)+TM_EG_POSITION_COUNT-TM_EG_POSITION_CLOSE)%TM_EG_POSITION_COUNT)
#define EG_POSITION_READ(stored) (((stored)+TM_EG_POSITION_CLOSE)%TM_EG_POSITION_COUNT)
typedef char TMExtension_EdgeguardPosition[(TM_EG_POSITION_COUNT==4) ? 1 : -1]; /* Exactly fills bits 30..31. */
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
static int version12(const uint8_t *r) { return format(r,12,16); }
static int version13(const uint8_t *r) { return format(r,13,16); }
static int version14(const uint8_t *r) { return format(r,14,16); }
static int version15(const uint8_t *r) { return format(r,15,28); }
static int version16(const uint8_t *r) { return format(r,16,42); }
static int version17(const uint8_t *r) { return format(r,17,44); }
static int version18(const uint8_t *r) { return format(r,18,56); }
/* Byte 8: A-Smash Release palette in bits 0..2, Randall Overlay in bit 3,
 * Act OoAttack palette in bits 4..6 (version 14). Bit 7 remains free. */
#define RANDALL_BIT 0x08u
#define ACT_OO_ATTACK_SHIFT 4
#define ACT_OO_ATTACK_BITS (7u<<ACT_OO_ATTACK_SHIFT)
/* Version 15 appends bytes 24..35: one nibble per OSD category slot, low nibble
 * first. 0 keeps the 3-bit palette; 1..8 select Orange..Lavender. Byte 35 holds
 * Low Laser's position and height: every migration to 15 zeroes it, so existing
 * version-15 records already read as the defaults (Center, Grounded). */
#define COLOR_EXTRA_OFFSET 24
#define COLOR_EXTRA_COUNT (TM_COLOR_COUNT-TM_COLOR_LEGACY_COUNT+1)
typedef char TMExtension_ColorNibbles[(COLOR_EXTRA_OFFSET+(TM_OSD_CATEGORIES+1)/2<=35) ? 1 : -1]; /* Byte 35 is Low Laser's. */
static unsigned color_extra(const uint8_t *r,unsigned slot) {
    return (r[COLOR_EXTRA_OFFSET+slot/2]>>((slot&1)*4))&15;
}
static void set_color_extra(uint8_t *r,unsigned slot,unsigned value) {
    unsigned shift=(slot&1)*4;
    uint8_t *byte=&r[COLOR_EXTRA_OFFSET+slot/2];
    *byte=(*byte&~(15u<<shift))|(value<<shift);
}
/* Version 16 appends bytes 36..49: a 5-bit Fixed Grid cell per OSD slot plus
 * JC Shine (0 Auto, 1..30), packed from bit 0 of byte 36. Every migration to 16
 * zeroes them, so older saves keep the default order. */
#define GRID_OFFSET 36
#define GRID_BITS 5
#define GRID_END (GRID_OFFSET+(TM_OSD_GRID_SLOTS*GRID_BITS+7)/8)
/* Version 17 appends bytes 50..51: the human and CPU costume (0..5) of the
 * saved character. Every migration to 17 zeroes them (Default costume). */
#define COSTUME_OFFSET 50
typedef char TMExtension_GridCells[(TM_OSD_GRID_SLOTS==22 && TM_OSD_GRID_POSITIONS<32 &&
    GRID_END<=COSTUME_OFFSET && COSTUME_OFFSET+2<=EDGEGUARD_OFFSET &&
    EDGEGUARD_OFFSET+EDGEGUARD_SIZE<=EDGEGUARD2_OFFSET && EDGEGUARD2_OFFSET+EDGEGUARD2_SIZE<=TM_EXTENSION_SIZE) ? 1 : -1];
static unsigned grid_cell(const uint8_t *r,unsigned slot) {
    unsigned bit=slot*GRID_BITS, at=GRID_OFFSET+bit/8;
    unsigned pair=r[at]|(at+1<GRID_END?(unsigned)r[at+1]<<8:0);
    return (pair>>(bit&7))&31;
}
static void set_grid_cell(uint8_t *r,unsigned slot,unsigned value) {
    unsigned bit=slot*GRID_BITS, at=GRID_OFFSET+bit/8, shift=bit&7;
    unsigned mask=31u<<shift, pair=r[at]|(at+1<GRID_END?(unsigned)r[at+1]<<8:0);
    pair=(pair&~mask)|((value<<shift)&mask);
    r[at]=pair;
    if(at+1<GRID_END) r[at+1]=pair>>8;
}
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
        field==TM_SETTING_ATTACK_SHIELD ? TM_AOS_PREF_COUNT :
        field==TM_SETTING_EDGEGUARD ? TM_EG_PREF_COUNT : 0;
}
static const ExtensionPreference *preference(unsigned field,unsigned index) {
    if(field==TM_SETTING_TECHCHASE && index<TM_TECHCHASE_PREF_COUNT) return &tech_preferences[index];
    if(field==TM_SETTING_LEDGECHASE && index<TM_LEDGECHASE_PREF_COUNT) return &ledge_preferences[index];
    if(field==TM_SETTING_UNDERFIRE && index<TM_UNDERFIRE_PREF_COUNT) return &under_fire_preferences[index];
    if(field==TM_SETTING_LOW_LASER && index<TM_LOW_LASER_PREF_COUNT) return &low_laser_preferences[index];
    if(field==TM_SETTING_POWERSHIELD && index<TM_PS_PREF_COUNT) return &powershield_preferences[index];
    if(field==TM_SETTING_REVERSAL && index<TM_REV_PREF_COUNT) return &reversal_preferences[index];
    if(field==TM_SETTING_ATTACK_SHIELD && index<TM_AOS_PREF_COUNT) return &attack_shield_preferences[index];
    if(field==TM_SETTING_EDGEGUARD && index<TM_EG_PREF_COUNT) return &edgeguard_preferences[index];
    return 0;
}
static unsigned preference_offset(unsigned field, unsigned index) {
    if(field==TM_SETTING_EDGEGUARD)
        return index==TM_EG_PLAYER_POSITION?EDGEGUARD_OFFSET+8:
            index>=TM_EG_RECOVERY_YOSHI?EDGEGUARD2_OFFSET:
            EDGEGUARD_OFFSET+(index>=TM_EG_HIT_STRENGTH?8:index>=TM_EG_PLAYER_PERCENT?4:0);
    if(field==TM_SETTING_REVERSAL || field==TM_SETTING_ATTACK_SHIELD ||
        (field==TM_SETTING_UNDERFIRE && index==TM_UNDERFIRE_LAVA_OVERLAY)) return 20;
    if(field==TM_SETTING_LEDGECHASE && (index==TM_LEDGECHASE_PRESET || index==TM_LEDGECHASE_AIR_ATTACK)) return 16;
    if(field==TM_SETTING_LOW_LASER && index>=TM_LOW_LASER_POSITION) return 32;
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
    if(field==TM_SETTING_EDGEGUARD && index==TM_EG_PLAYER_POSITION) value=EG_POSITION_READ(value);
    return value;
}
static void set_preference(uint8_t *r,unsigned field,unsigned index,unsigned value) {
    const ExtensionPreference *p=preference(field,index);
    if(field==TM_SETTING_EDGEGUARD && index==TM_EG_PLAYER_POSITION) value=EG_POSITION_STORED(value);
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
/* Initialize the Edgeguard preferences from index `first`, whose bytes the
 * caller has just claimed. */
static void init_edgeguard_preferences(uint8_t *r, unsigned first) {
    for(unsigned i=first;i<TM_EG_PREF_COUNT;++i)
        set_preference(r,TM_SETTING_EDGEGUARD,i,edgeguard_preferences[i].initial);
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
    init_edgeguard_preferences(r,0);
}
int TMExtension_Prepare(uint8_t *r) {
    if(!signature(r)) return TM_EXTENSION_ABSENT;
    int v14=version14(r), v15=version15(r), v16=version16(r), v17=version17(r), v18=version18(r), migrated=legacy(r),
        keep_shield=version11(r)||version12(r)||version13(r)||v14||v15||v16||v17||v18, keep_randall=version13(r)||v14||v15||v16||v17||v18;
    if(migrated) {
        uint8_t old[TM_EXTENSION_LEGACY_SIZE]; memcpy(old,r,sizeof(old));
        TMExtension_Init(r);
        /* Existing palette/Tech fields and still-unclaimed padding retain
         * their bits. Initialize CPU color to Lavender and the two new selectors
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
    } else if(version12(r) || version13(r) || v14 || v15 || v16 || v17 || v18) {
        r[4]=TM_EXTENSION_VERSION; /* Only the byte-8 bits below and appended bytes are newly owned. */
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
    /* Older records never owned these padding bits; they must not turn anything on. */
    if(migrated && !keep_randall) r[8]&=~RANDALL_BIT;
    if(migrated && !v14 && !v15 && !v16 && !v17 && !v18) r[8]&=~ACT_OO_ATTACK_BITS;
    /* Version 18 ends at byte 63: claim only the second Edgeguard word.
     * Version 17 ends at byte 51: claim all the Edgeguard words. Version 16
     * ends at byte 49: claim the costumes too. Version 15 ends at byte 35: keep
     * its colors and Low Laser byte and claim the grid cells too. Every older
     * record ends at byte 23: claim the color bytes as all-legacy too. */
    if(migrated) {
        unsigned from=v18?EDGEGUARD2_OFFSET:v17?EDGEGUARD_OFFSET:v16?COSTUME_OFFSET:v15?GRID_OFFSET:COLOR_EXTRA_OFFSET;
        r[5]=TM_EXTENSION_SIZE-8; memset(r+from,0,TM_EXTENSION_SIZE-from);
        init_edgeguard_preferences(r,v18?TM_EG_RECOVERY_YOSHI:0);
    }
    int changed=0;
    for(unsigned slot=0;slot<TM_OSD_CATEGORIES;++slot)
        if(color_extra(r,slot)>=COLOR_EXTRA_COUNT) { set_color_extra(r,slot,0); changed=1; }
    for(unsigned slot=0;slot<TM_OSD_GRID_SLOTS;++slot)
        if(grid_cell(r,slot)>TM_OSD_GRID_POSITIONS) { set_grid_cell(r,slot,0); changed=1; }
    for(unsigned player=0;player<2;++player)
        if(r[COSTUME_OFFSET+player]>=TM_EXTENSION_COSTUMES) { r[COSTUME_OFFSET+player]=0; changed=1; }
    static const uint8_t fields[]={TM_SETTING_TECHCHASE,TM_SETTING_LEDGECHASE,TM_SETTING_UNDERFIRE,
        TM_SETTING_LOW_LASER,TM_SETTING_POWERSHIELD,TM_SETTING_REVERSAL,TM_SETTING_ATTACK_SHIELD,TM_SETTING_EDGEGUARD};
    for(unsigned f=0;f<sizeof(fields);++f) {
        unsigned field=fields[f], count=preference_count(field);
        for(unsigned i=0;i<count;++i) {
            const ExtensionPreference *p=preference(field,i);
            /* Every value of a field this wide is valid (the toggle masks);
             * Reversal's attack has a fifth bit elsewhere. */
            if(p->count==(1u<<p->width) && !(field==TM_SETTING_REVERSAL && i==TM_REV_ATTACK)) continue;
            unsigned value=preference_value(r,field,i);
            if(value>=p->count) { set_preference(r,field,i,p->initial); changed=1; }
        }
    }
    return migrated?TM_EXTENSION_MIGRATED:changed?TM_EXTENSION_REPAIRED:TM_EXTENSION_READY;
}
uint32_t TMExtension_Read(const uint8_t *r,unsigned field,unsigned index) {
    int v2=version2(r), older=v2||version3(r), v4=version4(r), v5=version5(r), v6=version6(r), v7=version7(r), v8=version8(r), v9=version9(r), v10=version10(r), v11=version11(r), v12=version12(r), v13=version13(r), v14=version14(r), v15=version15(r), v16=version16(r), v17=version17(r), v18=version18(r),
        current=supported(r)||older||v4||v5||v6||v7||v8||v9||v10||v11||v12||v13||v14||v15||v16||v17||v18, old=legacy(r);
    if(field==TM_SETTING_OSD_COLOR_EXTRA) {
        unsigned value=(supported(r)||v15||v16||v17||v18) && index<TM_OSD_CATEGORIES?color_extra(r,index):0;
        return value<COLOR_EXTRA_COUNT?value:0;
    }
    if(field==TM_SETTING_OSD_GRID_CELL) {
        unsigned value=(supported(r)||v16||v17||v18) && index<TM_OSD_GRID_SLOTS?grid_cell(r,index):0;
        return value<=TM_OSD_GRID_POSITIONS?value:0;
    }
    if(field==TM_SETTING_COSTUME) {
        unsigned value=(supported(r)||v17||v18) && index<2?r[COSTUME_OFFSET+index]:0;
        return value<TM_EXTENSION_COSTUMES?value:0;
    }
    if(field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ASMASH_RELEASE_ID)
        return current||old?r[8]&7:0;
    if(field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ACT_OO_ATTACK_ID)
        return supported(r)||v14||v15||v16||v17||v18?(r[8]&ACT_OO_ATTACK_BITS)>>ACT_OO_ATTACK_SHIFT:0;
    if(field==TM_SETTING_FLAG && index==TM_FLAG_RANDALL)
        return supported(r)||v13||v14||v15||v16||v17||v18?!!(r[8]&RANDALL_BIT):0;
    const ExtensionPreference *p=preference(field,index);
    if(!p) return 0;
    /* Version 18 records own the first Edgeguard words, version 19 all of them. */
    if(field==TM_SETTING_EDGEGUARD) {
        int owned=supported(r)||(v18 && index<TM_EG_RECOVERY_YOSHI);
        unsigned value=owned?preference_value(r,field,index):p->initial;
        return value<p->count?value:p->initial;
    }
    if((field==TM_SETTING_REVERSAL || field==TM_SETTING_ATTACK_SHIELD) && !supported(r) && !v11 && !v12 && !v13 && !v14 && !v15 && !v16 && !v17 && !v18) return p->initial;
    /* Records before version 11 end before word 20. */
    if(field==TM_SETTING_UNDERFIRE && index==TM_UNDERFIRE_LAVA_OVERLAY && !supported(r) && !v11 && !v12 && !v13 && !v14 && !v15 && !v16 && !v17 && !v18) return p->initial;
    if(v11 && field==TM_SETTING_ATTACK_SHIELD && index==TM_AOS_SHIELD_DI) return p->initial;
    if(v11 && field==TM_SETTING_ATTACK_SHIELD && index==TM_AOS_ACTION) {
        unsigned action=(read_bits(r,20)>>9)&15;
        return action<10?action:0;
    }
    if(v11 && field==TM_SETTING_REVERSAL && index==TM_REV_ATTACK) return (r[20]&15)<14?r[20]&15:0;
    if(field==TM_SETTING_POWERSHIELD && !supported(r) && !v8 && !v9 && !v10 && !v11 && !v12 && !v13 && !v14 && !v15 && !v16 && !v17 && !v18) return p->initial;
    /* Older records end before byte 35. */
    if(field==TM_SETTING_LOW_LASER && index>=TM_LOW_LASER_POSITION && !supported(r) && !v15 && !v16 && !v17 && !v18) return p->initial;
    if(field==TM_SETTING_LOW_LASER && !supported(r) && !v6 && !v7 && !v8 && !v9 && !v10 && !v11 && !v12 && !v13 && !v14 && !v15 && !v16 && !v17 && !v18) return p->initial;
    if(field==TM_SETTING_LEDGECHASE && !supported(r) && !v10 && !v11 && !v12 && !v13 && !v14 && !v15 && !v16 && !v17 && !v18) {
        if(index==TM_LEDGECHASE_ENTRY || index==TM_LEDGECHASE_HOP_HEIGHT) return p->initial;
        if(v9 && index==TM_LEDGECHASE_PRESET) return (r[19]&15)<14?r[19]&15:0;
    }
    if(field==TM_SETTING_LEDGECHASE && !supported(r) && !v10 && !v11 && !v12 && !v13 && !v14 && !v15 && !v16 && !v17 && !v18 && !v9) {
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
    int attack=field==TM_SETTING_OSD_COLOR && index==TM_GLOBAL_ACT_OO_ATTACK_ID;
    int randall=field==TM_SETTING_FLAG && index==TM_FLAG_RANDALL;
    int extra=field==TM_SETTING_OSD_COLOR_EXTRA && index<TM_OSD_CATEGORIES;
    int grid=field==TM_SETTING_OSD_GRID_CELL && index<TM_OSD_GRID_SLOTS;
    int costume=field==TM_SETTING_COSTUME && index<2;
    const ExtensionPreference *p=preference(field,index);
    if(!color && !attack && !randall && !extra && !grid && !costume && !p) return 0;
    if(value >= (color||attack?8:randall?2:extra?COLOR_EXTRA_COUNT:grid?TM_OSD_GRID_POSITIONS+1:costume?TM_EXTENSION_COSTUMES:p->count)) return 0;
    if(signature(r) && !supported(r) && !legacy(r) && !version2(r) && !version3(r) && !version4(r) && !version5(r) && !version6(r) && !version7(r) && !version8(r) && !version9(r) && !version10(r) && !version11(r) && !version12(r) && !version13(r) && !version14(r) && !version15(r) && !version16(r) && !version17(r) && !version18(r)) return 0;
    uint8_t before[TM_EXTENSION_SIZE]; memcpy(before,r,sizeof(before));
    if(legacy(r) || version2(r) || version3(r) || version4(r) || version5(r) || version6(r) || version7(r) || version8(r) || version9(r) || version10(r) || version11(r) || version12(r) || version13(r) || version14(r) || version15(r) || version16(r) || version17(r) || version18(r)) TMExtension_Prepare(r);
    else if(!supported(r)) TMExtension_Init(r);
    if(color) r[8]=(r[8]&~7u)|value;
    else if(attack) r[8]=(r[8]&~ACT_OO_ATTACK_BITS)|(value<<ACT_OO_ATTACK_SHIFT);
    else if(randall) r[8]=(r[8]&~RANDALL_BIT)|(value?RANDALL_BIT:0);
    else if(extra) set_color_extra(r,index,value);
    else if(grid) set_grid_cell(r,index,value);
    else if(costume) r[COSTUME_OFFSET+index]=value;
    else set_preference(r,field,index,value);
    for(unsigned i=0;i<TM_EXTENSION_SIZE;++i) if(before[i]!=r[i]) return 1;
    return 0;
}
