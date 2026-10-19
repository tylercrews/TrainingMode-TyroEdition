#include "events.h"
#include "settings.h"
#include "settings_extension.h"
#include <stddef.h>

typedef char settings_mask_offset[(offsetof(Memcard, TM_OSDEnabled) == TM_SETTINGS_OFFSET) ? 1 : -1];
typedef char settings_extra_offset[(offsetof(Memcard, TM_SettingsExtraFlags) == 0x1F4C) ? 1 : -1];
typedef char settings_tail_offset[(offsetof(Memcard, TM_SettingsReserved) == 0x1F4D) ? 1 : -1];
typedef char settings_boundary[(offsetof(Memcard, unk2004) == TM_SETTINGS_OFFSET + TM_SETTINGS_SIZE) ? 1 : -1];
typedef char card_dirty_word[(sizeof(((MemcardState *)0)->memcard_changed) == 4 && offsetof(MemcardState, memcard_changed) == 0xC) ? 1 : -1];
typedef char card_enable_word[(sizeof(((MemcardState *)0)->enable) == 4 && offsetof(MemcardState, enable) == 0x18 && offsetof(MemcardState, x5C) == 0x5C) ? 1 : -1];

static u8 last_record[TM_SETTINGS_SIZE];
static u8 fallback[TM_SETTINGS_SIZE];
static int seen;
static int last_owns;
static int status;
static unsigned osd_page, editor_page;
/* ASM/Globals.s mirrors this as SettingsField_EditorPage. */
typedef char TMSettings_EditorPageField[TM_SETTING_EDITOR_PAGE == 35 ? 1 : -1];
static int pending_save; /* Survives native archive unload clearing its own xC. */
static int queued_to_native;
static u8 extension_fallback[TM_EXTENSION_SIZE];
static int extension_private_mode;
static int card_ready(void) {
    return stc_memcard_state->enable && stc_memcard_state->x5C &&
        stc_memcard_work->work_area && stc_memcard_work->buffer && stc_memcard_state->x8 == 0;
}
static void mark_changed(void) {
    pending_save = 1;
    queued_to_native = 0;
    if (card_ready()) { stc_memcard_state->memcard_changed = true; queued_to_native = 1; }
}

/* Keep pointers as runtime arguments. Inlining this against low-memory identity
 * can make GCC emit a symbol-minus-0x80000000 relocation; the MEX loader does
 * not preserve that addend and the resulting indexed read wraps into low RAM. */
__attribute__((noinline)) static int same_bytes(const u8 *a, const u8 *b, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        if (a[i] != b[i]) return 0;
    return 1;
}

static u8 *settings_view(void) {
    u8 *record = (u8 *)stc_memcard + TM_SETTINGS_OFFSET;
    int owns = same_bytes((const u8 *)0x80000000, (const u8 *)TM_GAME_ID, 6);
    if (!seen || owns != last_owns || !same_bytes(record, last_record, TM_SETTINGS_SIZE)) {
        status = TMSettings_Prepare(record, owns);
        if (status == TM_SETTINGS_UNSUPPORTED || status == TM_SETTINGS_FOREIGN)
            TMSettings_Init(fallback);
        else if (status == TM_SETTINGS_MIGRATED || status == TM_SETTINGS_REPAIRED) {
            mark_changed();
        }
        memcpy(last_record, record, TM_SETTINGS_SIZE);
        seen = 1;
        last_owns = owns;
    }
    return status == TM_SETTINGS_UNSUPPORTED || status == TM_SETTINGS_FOREIGN ? fallback : record;
}

static const u8 extension_osds[] = { TM_GLOBAL_ASMASH_RELEASE_ID, TM_GLOBAL_ACT_OO_ATTACK_ID };
#define EXTENSION_OSD_BITS ((1u<<TM_GLOBAL_ASMASH_RELEASE_ID)|(1u<<TM_GLOBAL_ACT_OO_ATTACK_ID))
/* New colors keep the nearest original color in the 3-bit palette, so the OSD
 * stays enabled and an older build still draws a close color. */
static const u8 legacy_color[TM_COLOR_COUNT-TM_COLOR_LEGACY_COUNT] = {
    TM_COLOR_YELLOW, TM_COLOR_MAGENTA, TM_COLOR_MAGENTA, TM_COLOR_CYAN,
    TM_COLOR_GREEN, TM_COLOR_YELLOW, TM_COLOR_BLUE, TM_COLOR_MAGENTA,
};
static u8 *extension_view(void) {
    u8 *core=settings_view();
    u8 *extension=(u8 *)stc_memcard+TM_EXTENSION_OFFSET;
    int mode=core==fallback?1:0;
    int ext_status=mode?TM_EXTENSION_UNSUPPORTED:TMExtension_Prepare(extension);
    if(!mode && ext_status==TM_EXTENSION_UNSUPPORTED) mode=2;
    if(mode) {
        if(extension_private_mode!=mode) TMExtension_Init(extension_fallback);
        extension_private_mode=mode;
        return extension_fallback;
    }
    extension_private_mode=0;
    if(ext_status==TM_EXTENSION_REPAIRED || ext_status==TM_EXTENSION_MIGRATED) mark_changed();
    return extension;
}
static void extension_set(unsigned field,unsigned index,u32 value) {
    u8 *view=extension_view();
    if(TMExtension_Write(view,field,index,value) && view!=extension_fallback) mark_changed();
}
uint32_t Settings_Get(unsigned field, unsigned index) {
    if (field == TM_SETTING_OSD_PAGE) return osd_page;
    if (field == TM_SETTING_EDITOR_PAGE) return editor_page;
    if(field==TM_SETTING_EDITOR_ROW) { index=TMSettings_EditorIDFor(editor_page,index); field=TM_SETTING_NATIVE_ROW; }
    if(field==TM_SETTING_NATIVE_ROW && index==TM_GLOBAL_RANDALL_ROW) { field=TM_SETTING_FLAG; index=TM_FLAG_RANDALL; }
    if(field==TM_SETTING_OSD_COLOR) {
        int slot=TMSettings_OSDCategorySlot(index);
        u32 base=TM_EXTENSION_OSD(index)?TMExtension_Read(extension_view(),field,index):
            TMSettings_Read(settings_view(),field,index);
        if(slot<0 || !base) return base; /* Off wins over a retained new color. */
        u32 extra=TMExtension_Read(extension_view(),TM_SETTING_OSD_COLOR_EXTRA,slot);
        return extra?TM_COLOR_LEGACY_COUNT-1+extra:base;
    }
    if(field==TM_SETTING_TECHCHASE || field==TM_SETTING_LEDGECHASE || field==TM_SETTING_UNDERFIRE || field==TM_SETTING_LOW_LASER || field==TM_SETTING_POWERSHIELD || field==TM_SETTING_REVERSAL || field==TM_SETTING_ATTACK_SHIELD || (field==TM_SETTING_OSD_COLOR && TM_EXTENSION_OSD(index)) || (field==TM_SETTING_FLAG && index==TM_FLAG_RANDALL) || field==TM_SETTING_OSD_COLOR_EXTRA || field==TM_SETTING_OSD_GRID_CELL || field==TM_SETTING_COSTUME)
        return TMExtension_Read(extension_view(),field,index);
    if((field==TM_SETTING_OSD_ENABLED || field==TM_SETTING_NATIVE_ROW) && TM_EXTENSION_OSD(index))
        return !!Settings_Get(TM_SETTING_OSD_COLOR,index);
    if(field==TM_SETTING_OSD_MASK) {
        u32 mask=TMSettings_Read(settings_view(),field,index)&~EXTENSION_OSD_BITS;
        for(unsigned i=0;i<countof(extension_osds);++i)
            if(Settings_Get(TM_SETTING_OSD_COLOR,extension_osds[i])) mask|=1u<<extension_osds[i];
        return mask;
    }
    return TMSettings_Read(settings_view(), field, index);
}

void Settings_Set(unsigned field, unsigned index, uint32_t value) {
    if (field == TM_SETTING_OSD_PAGE) { if (value < 19) osd_page = value; return; }
    if (field == TM_SETTING_EDITOR_PAGE) { if (value < TM_EDITOR_PAGES) editor_page = value; return; }
    if(field==TM_SETTING_EDITOR_ROW) { index=TMSettings_EditorIDFor(editor_page,index); field=TM_SETTING_NATIVE_ROW; }
    if(field==TM_SETTING_NATIVE_ROW && index==TM_GLOBAL_RANDALL_ROW) { field=TM_SETTING_FLAG; index=TM_FLAG_RANDALL; }
    if(field==TM_SETTING_OSD_COLOR_EXTRA) return; /* Written only through OSD_COLOR. */
    if(field==TM_SETTING_OSD_COLOR) {
        int slot=TMSettings_OSDCategorySlot(index);
        if(slot<0 || value>=TM_COLOR_COUNT) return;
        int extended=value>=TM_COLOR_LEGACY_COUNT;
        extension_set(TM_SETTING_OSD_COLOR_EXTRA,slot,extended?value-TM_COLOR_LEGACY_COUNT+1:0);
        if(extended) value=legacy_color[value-TM_COLOR_LEGACY_COUNT];
    }
    if(field==TM_SETTING_TECHCHASE || field==TM_SETTING_LEDGECHASE || field==TM_SETTING_UNDERFIRE || field==TM_SETTING_LOW_LASER || field==TM_SETTING_POWERSHIELD || field==TM_SETTING_REVERSAL || field==TM_SETTING_ATTACK_SHIELD || (field==TM_SETTING_OSD_COLOR && TM_EXTENSION_OSD(index)) || (field==TM_SETTING_FLAG && index==TM_FLAG_RANDALL) || field==TM_SETTING_OSD_GRID_CELL || field==TM_SETTING_COSTUME) {
        extension_set(field,index,value); return;
    }
    if((field==TM_SETTING_OSD_ENABLED || field==TM_SETTING_NATIVE_ROW) && TM_EXTENSION_OSD(index)) {
        if(value>1) return;
        unsigned old=Settings_Get(TM_SETTING_OSD_COLOR,index);
        extension_set(TM_SETTING_OSD_COLOR,index,value?(old?old:TM_COLOR_WHITE):TM_COLOR_OFF); return;
    }
    u8 *view = settings_view();
    if(field==TM_SETTING_OSD_MASK) {
        for(unsigned i=0;i<countof(extension_osds);++i) {
            unsigned id=extension_osds[i], old=Settings_Get(TM_SETTING_OSD_COLOR,id);
            extension_set(TM_SETTING_OSD_COLOR,id,value&(1u<<id)?(old?old:TM_COLOR_WHITE):TM_COLOR_OFF);
        }
        /* Existing core bytes and palette mappings stay compatible. The new
         * enables are synthesized; these formerly unused bits are never repurposed. */
        value=(value&~EXTENSION_OSD_BITS)|(TMSettings_Read(view,TM_SETTING_OSD_MASK,0)&EXTENSION_OSD_BITS);
    }
    if (TMSettings_Write(view, field, index, value) && view != fallback) {
        mark_changed();
        memcpy(last_record, view, TM_SETTINGS_SIZE);
    }
}

int Settings_Status(void) {
    settings_view();
    return status;
}
void *Settings_EventScoreSlot(unsigned event) {
    unsigned slot = Events_ScoreIndex(Settings_Get(TM_SETTING_EVENT_PAGE, 0), event);
    /* Older opaque extensions were allowed to occupy all 92 spare bytes.
     * Claim slot 50 only when this build understands the owning save format. */
    if(slot==TM_EVENT_LEDGECHASE_SCORE_SLOT && extension_view()==extension_fallback) return 0;
    /* Under Fire now records perfect-refresh counts. Keep old time records
     * untouched and invisible until the first counter record replaces one.
     * Never claim an opaque future/foreign record for the new score format. */
    if(slot==TM_EVENT_UNDERFIRE_SCORE_SLOT && (extension_view()==extension_fallback ||
        !Settings_Get(TM_SETTING_UNDERFIRE,TM_UNDERFIRE_COUNTER_SCORE))) return 0;
    return slot<TM_EVENT_LEGACY_SCORE_SLOTS || slot==TM_EVENT_LEDGECHASE_SCORE_SLOT
        ? (u8 *)stc_memcard+TM_EVENT_SCORE_OFFSET+slot*4 : 0;
}
void Settings_ResetEventRecords(void *card) {
    u8 *bytes = card;
    if (!bytes) return;
    if (same_bytes((const u8 *)0x80000000, (const u8 *)TM_GAME_ID, 6)) {
        /* Validate private copies: records erasure must neither migrate the
         * preferences nor claim the final four bytes of an opaque old record. */
        u8 core[TM_SETTINGS_SIZE], extension[TM_EXTENSION_SIZE];
        memcpy(core,bytes+TM_SETTINGS_OFFSET,sizeof(core));
        memcpy(extension,bytes+TM_EXTENSION_OFFSET,sizeof(extension));
        int understood=TMSettings_Prepare(core,1)!=TM_SETTINGS_UNSUPPORTED &&
            TMExtension_Prepare(extension)!=TM_EXTENSION_UNSUPPORTED;
        memset(bytes+0x1A68,0,TM_EXTENSION_OFFSET-0x1A68);
        if(understood) memset(bytes+TM_EXTENSION_REGION_END,0,0x1B40-TM_EXTENSION_REGION_END);
        else memset(bytes+0x1B3C,0,4);
    } else memset(bytes + 0x1A68, 0, 216);
}

void Settings_CommitPending(void) {
    if (!pending_save) return;
    if (!same_bytes((const u8 *)0x80000000, (const u8 *)TM_GAME_ID, 6)) return;
    if (!card_ready()) return;
    /* Native per-frame autosave owns requests, polling, checksums and errors.
     * Requeue after returning to ESS, even if archive teardown reset dirty. */
    if (queued_to_native && stc_memcard_state->x10 == 1 && !stc_memcard_state->memcard_changed) {
        pending_save = 0; /* Native request has accepted the current data. */
        return;
    }
    stc_memcard_state->memcard_changed = true;
    queued_to_native = 1;
}

/* These native CSS backups are RAM-only (outside the card manifest). Keep
 * costumes during a session when the character matches; otherwise restore the
 * saved costume of the saved character, or use default.
 * The whitelist uses CSS icon IDs, not external fighter IDs. Sheik shares Zelda. */
static const u8 character_css_id[26] = {
    7,6,10,22,13,3,16,2,1,23,21,11,4,19,12,20,14,5,15,15,9,17,0,24,18,8,
};
/* Costumes per external ID; matches the game's table (CSS_GetCostumeNum). */
static const u8 character_costumes[26] = {
    6,5,4,4,6,4,5,4,5,5,4,4,5,4,4,5,5,6,5,5,4,5,5,5,4,5,
};
static int character_allowed(unsigned kind, int mask) {
    return kind < 26 && (mask == -1 || ((u32)mask & (1u << character_css_id[kind])));
}
void Settings_Characters(unsigned save, int hmn_mask, int cpu_mask, int cpu_selectable) {
    u8 *backup = (u8 *)stc_memcard + 3344;
    for (unsigned player = 0; player < 2; ++player) {
        unsigned offset = player ? 140 : 104, costume = player ? 143 : 107;
        int mask = player ? cpu_mask : hmn_mask;
        if (save) {
            /* Fixed/restricted events are exercises, not a new general choice.
             * Never persist None/Random/nonplayable IDs from a closed door. */
            if (mask == -1 && (!player || cpu_selectable) && backup[offset] < 26) {
                Settings_Set(TM_SETTING_CHARACTER, player, backup[offset]);
                Settings_Set(TM_SETTING_COSTUME, player,
                    backup[costume] < character_costumes[backup[offset]] ? backup[costume] : 0);
            }
            continue;
        }
        unsigned saved = Settings_Get(TM_SETTING_CHARACTER, player), kind = saved;
        if (kind >= 26) kind = backup[offset];
        if (!character_allowed(kind, mask)) {
            kind = 2; /* Fox for an uninitialized unrestricted backup. */
            if (mask != -1) {
                for (kind = 0; kind < 26; ++kind)
                    if (character_allowed(kind, mask)) break;
                if (kind == 26) continue; /* Empty whitelist: leave native behavior. */
            }
        }
        if (kind != backup[offset]) {
            unsigned color = kind == saved ? Settings_Get(TM_SETTING_COSTUME, player) : 0;
            backup[costume] = color < character_costumes[kind] ? color : 0;
        }
        backup[offset] = kind;
    }
}
