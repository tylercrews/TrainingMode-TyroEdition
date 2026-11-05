/* Production savestate code, with fixtures only at native engine boundaries. */
#include "../src/savestate_v1.c"

enum { FIRST, SECOND, FORMS };
static EventVars vars;
static GOBJ event, gobjs[FORMS];
static FighterData ft[FORMS];
static JOBJ joints[FORMS];
static CmSubject subjects[FORMS];
static Playerblock block;
static int stale[64];
static Savestate_v1 *state = (void *)0x80100000;
static GOBJ *lookup[64];
static u8 event_data[sizeof(state->event_data)];

void memset(void *dest, int value, int size) { u8 *p = dest; while (size--) *p++ = value; }
void memcpy(void *dest, const void *src, int size) { u8 *d = dest; const u8 *s = src; while (size--) *d++ = *s++; }

/* The game's own lookups: both go through isTransformed. */
GOBJ *Fighter_GetSubcharGObj(int ply, int ms) { return ply == 0 ? block.gobj[block.isTransformed[ms]] : 0; }
GOBJ *Fighter_GetGObj(int ply) { return Fighter_GetSubcharGObj(ply, 0); }
Playerblock *Fighter_GetPlayerblock(int ply) { return &block; }
int *Fighter_GetStaleMoveTable(int ply) { return stale; }
int Fighter_GetStocks(int ply) { return 4; }

void ActionStateChange(float frame, float speed, float blend, GOBJ *f, int state_id, int flags, GOBJ *source) {
    ((FighterData *)f->userdata)->state_id = state_id;
}
void Fighter_EnterSleep(GOBJ *f, int ms) { }
void Fighter_ColAnim_Remove(FighterData *f, int id) { }
void Fighter_ProcDynamics(GOBJ *f) { }
void Fighter_SetAllHurtboxesNotUpdated(GOBJ *f) { }
void Fighter_UpdateHurtboxes(FighterData *f) { }
void Fighter_UpdateIK(GOBJ *f) { }
int Fighter_GetCharacterID(void) { return 0; }
void JOBJ_SetMtxDirtySub(JOBJ *joint) { }
void Match_CorrectCamera(void) { }
int Match_CheckIfStock(void) { return 0; }
void Match_CreateHUD(int ply) { }
void SFX_StopCrowd(void) { }
void SFX_StopAllFighterSFX(FighterData *f) { }
int SFX_PlayCommon(int id) { return 0; }
void Trails_Clear(void) { }
void ActionCues_Clear(void) { }
void Events_NotifyRestore(void) { }
uint32_t Settings_Get(unsigned field, unsigned index) { return 0; }
int Stage_GetExternalID(void) { return GRKINDEXT_FD; }
int Stage_ExternalToInternal(int id) { return id; }
void Animation_GuardAgain(GOBJ *f) { }
void GuardOnInitIDK(GOBJ *f) { }
s8 Fighter_BoneLookup(FighterData *f, int bone) { return -1; }
int Pause_CheckStatus(int type) { return 0; }
void ScreenFlash_Create(int kind, int unk) { }
void psDeletePntJObjwithParticle(Particle *ptcl) { }
int psRemoveParticleAppSRT(Particle *ptcl) { return 0; }
void Item_Destroy(GOBJ *item) { }
void GObj_Destroy(GOBJ *gobj) { }
void HSD_ObjFree(HSD_ObjAllocData *table, void *object) { }

/* Fighter pointers saved as ids. Nothing in this fixture holds one. */
GOBJ *GOBJToID(GOBJ *gobj) { return (GOBJ *)-1; }
GOBJ *IDToGOBJ(GOBJ *id) { return 0; }
FighterData *FtDataToID(FighterData *data) { return (FighterData *)-1; }
FighterData *IDToFtData(FighterData *id) { return 0; }
JOBJ *BoneToID(FighterData *data, JOBJ *bone) { return (JOBJ *)-1; }
JOBJ *IDToBone(FighterData *data, JOBJ *id) { return 0; }

void TestStateLoaded(void) { }

/* Zelda's slot: the form you picked awake at (x, 0), the other asleep. */
int TestStateInit(void) {
    *event_vars_ptr_loc = &vars;
    vars.event_gobj = &event;
    event.userdata = event_data;
    *stc_gobj_lookup = lookup;
    for (int i = 0; i < FORMS; ++i) {
        gobjs[i].userdata = &ft[i];
        gobjs[i].hsd_object = &joints[i];
        ft[i].fighter = &gobjs[i];
        ft[i].camera_subject = &subjects[i];
        block.gobj[i] = &gobjs[i];
        ft[i].flags.ms = i;
    }
    block.isTransformed[FIRST] = 0;
    block.isTransformed[SECOND] = 1;
    ft[FIRST].phys.pos.X = 10;
    ft[FIRST].state_id = ASID_WAIT;
    ft[SECOND].phys.pos.X = 20;
    ft[SECOND].state_id = ASID_SLEEP;
    ft[SECOND].flags.sleep = 1;
    return 0;
}
int TestStateSave(void) { return Savestate_Save_v1(state, Savestate_Silent); }
int TestStateLoad(void) { return Savestate_Load_v1(state, Savestate_Silent); }

/* What Fighter_Transform leaves behind: the pair and their ms bits traded. */
void TestStateTransform(void) {
    u8 first = block.isTransformed[FIRST];
    block.isTransformed[FIRST] = block.isTransformed[SECOND];
    block.isTransformed[SECOND] = first;
    for (int i = 0; i < FORMS; ++i)
        ft[i].flags.ms = !ft[i].flags.ms;
    ft[FIRST].flags.sleep = 1;
    ft[SECOND].flags.sleep = 0;
    ft[FIRST].state_id = ASID_SLEEP;
    ft[SECOND].state_id = ASID_WAIT;
    ft[FIRST].phys.pos.X = ft[SECOND].phys.pos.X = 99;
}
void TestStateMove(int x) { ((FighterData *)Fighter_GetGObj(0)->userdata)->phys.pos.X = x; }

int TestStateInfo(int key) {
    GOBJ *active = Fighter_GetGObj(0);
    switch (key) {
    case 0: return active == &gobjs[FIRST] ? FIRST : SECOND;
    case 1: return ft[FIRST].phys.pos.X;
    case 2: return ft[SECOND].phys.pos.X;
    case 3: return ft[FIRST].state_id;
    case 4: return ft[SECOND].state_id;
    case 5: return ft[FIRST].flags.ms;
    case 6: return ft[SECOND].flags.ms;
    case 7: return ft[FIRST].flags.sleep;
    case 8: return ft[SECOND].flags.sleep;
    case 9: return ASID_WAIT;
    case 10: return ASID_SLEEP;
    case 11: return sizeof(Savestate_v1);
    }
    return -1;
}
