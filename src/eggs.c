#include "../MexTK/mex.h"
#include "eggs.h"
#include "stage_points.h"

static int egg_counter = 0;
static int high_score = 0;
static int accumulated_damage = 0;
static int reset_clock = 0;
static Vec3 coll_pos, last_coll_pos;
static int egg_falls_slowly = 0;
static GOBJ *egg_gobj;
static CmSubject *cam;
static GOBJ *hud_score_gobj, *hud_best_gobj;
static JOBJ *hud_score_jobj, *hud_best_jobj;
static int canvas;
static Text *hud_score_text, *hud_best_text;

/* Where the score goes for each Egg Counter Position. trans.X is the edge or
 * center the text lines up on, given the alignment. Text fills a 32 tall line
 * from trans.Y, and a smaller label sits at the bottom of its line, so with
 * the label's line COUNTER_LABEL_Y lower the pair runs from y to y + 47: the
 * bottom corners keep the top ones' 25 margin. The timer's digits run from
 * about 54 to 74 down the screen and the score's start 4 into its line, so
 * Under Timer clears it by 5. */
#define COUNTER_LABEL_Y 15
static const struct { float x, y; u8 align; } EggCounter_Spots[TM_EGGS_COUNTER_COUNT] = {
    [TM_EGGS_COUNTER_TOP_LEFT] = {50, 25, 0},
    [TM_EGGS_COUNTER_TOP_RIGHT] = {590, 25, 2},
    [TM_EGGS_COUNTER_BOTTOM_LEFT] = {50, 408, 0},
    [TM_EGGS_COUNTER_BOTTOM_RIGHT] = {590, 408, 2},
    [TM_EGGS_COUNTER_UNDER_TIMER] = {320, 75, 1},
};

/* Eggs In The Air start at least this far over the ground and this far under
 * the top of the camera, and fall no faster than this. */
#define EGG_AIR_HEIGHT 70.f
#define EGG_AIR_CEILING 15.f
#define EGG_AIR_FALL_SPEED .6f
// Eggs Under Platforms stay this far in from a platform's ends.
#define EGG_PLATFORM_INSET 5.f
#define EGG_PLATFORM_TRIES 20

enum egg_spot { EGG_SPOT_NORMAL, EGG_SPOT_UNDER_PLATFORM, EGG_SPOT_IN_AIR };

static const u8 saved_eggs_rows[] = {OPT_DAMAGETHRESHOLD, OPT_SCALE, OPT_VELOCITY, OPT_COLLISION};
static const u8 spawn_eggs_rows[TM_EGGS_SPAWN_PREF_COUNT] = {OPT_UNDERPLATFORMS, OPT_INAIR};
static void Eggs_LoadSettings(void) {
    for (unsigned i = 0; i < countof(saved_eggs_rows); ++i) {
        EventOption *option = &Options_Main[saved_eggs_rows[i]];
        option->val = option->val_prev = TM_GetSetting(TM_SETTING_EGGS, i);
    }
    for (unsigned i = 0; i < countof(spawn_eggs_rows); ++i) {
        EventOption *option = &Options_Main[spawn_eggs_rows[i]];
        option->val = option->val_prev = TM_GetSetting(TM_SETTING_EGGS_SPAWN, i);
    }
    Options_Main[OPT_COUNTERPOSITION].val = Options_Main[OPT_COUNTERPOSITION].val_prev =
        TM_GetSetting(TM_SETTING_EGGS_COUNTER, 0);
}
static void Eggs_PlaceCounter(int position) {
    hud_score_text->trans.X = hud_best_text->trans.X = EggCounter_Spots[position].x;
    hud_score_text->trans.Y = EggCounter_Spots[position].y;
    hud_best_text->trans.Y = EggCounter_Spots[position].y + COUNTER_LABEL_Y;
    hud_score_text->align = hud_best_text->align = EggCounter_Spots[position].align;
}
void Eggs_ChangeCounterPosition(GOBJ *menu, int value) {
    TM_SetSetting(TM_SETTING_EGGS_COUNTER, 0, value);
    Eggs_PlaceCounter(value);
}
void Eggs_ChangeSavedSetting(GOBJ *menu, int value) {
    MenuData *data = menu->userdata;
    if (data->curr_menu != &Menu_Main) return;
    unsigned row = data->curr_menu->scroll + data->curr_menu->cursor;
    for (unsigned i = 0; i < countof(saved_eggs_rows); ++i)
        if (saved_eggs_rows[i] == row) TM_SetSetting(TM_SETTING_EGGS, i, value);
}
void Eggs_ChangeSpawnSetting(GOBJ *menu, int value) {
    for (unsigned i = 0; i < countof(spawn_eggs_rows); ++i)
        TM_SetSetting(TM_SETTING_EGGS_SPAWN, i, Options_Main[spawn_eggs_rows[i]].val);
}
void Eggs_ResetSettings(GOBJ *menu) {
    TM_SetSetting(TM_SETTING_EVENT_RESET, TM_EVENT_EGGS, 1);
    TM_SetSetting(TM_SETTING_EGGS_COUNTER, 0, TM_EGGS_COUNTER_TOP_LEFT);
    for (unsigned i = 0; i < countof(spawn_eggs_rows); ++i)
        TM_SetSetting(TM_SETTING_EGGS_SPAWN, i, 0);
    Eggs_LoadSettings();
    Options_HitboxTrails[OPT_HITBOXTRAILS_ENABLED].val =
        Options_HitboxTrails[OPT_HITBOXTRAILS_ENABLED].val_prev = 0;
    Options_HitboxTrails[OPT_HITBOXTRAILS_DECAY].val =
        Options_HitboxTrails[OPT_HITBOXTRAILS_DECAY].val_prev = 0;
    /* Restart to reset the challenge clock and Free Practice's disabled rows.
     * No global trail preferences or scores are changed. */
    Retry(menu);
}

void Exit(GOBJ *menu) 
{
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

/* Free Practice and the egg settings are saved, so a restart keeps them. */
static void StageSwap_BeforeRestart(void) { }

void Retry(GOBJ *menu) 
{
    Memcard_SaveIfChanged();
    stc_match->end_kind = MATCHENDKIND_RETRY;
    stc_match->state = 3;
    Match_EndVS();
}

float RandomRange(float low, float high) {
    return low + (high - low) * HSD_Randf();
}

void ChangeHitDisplay(GOBJ *menu_gobj, int value) {
    if (menu_gobj) TM_SetSetting(TM_SETTING_EGGS, TM_EGGS_COLLISION, value);
    // loop through all fighters
    GOBJ *this_fighter = (*stc_gobj_lookup)[MATCHPLINK_FIGHTER];
    while (this_fighter != 0)
    {
        // get data
        FighterData *this_fighter_data = this_fighter->userdata;

        // toggle
        this_fighter_data->show_hit = value;

        // get next fighter
        this_fighter = this_fighter->next;
    }
}

void StartFreePractice(GOBJ *gobj) {
    TM_SetSetting(TM_SETTING_EGGS, TM_EGGS_FREE_PRACTICE, 1);
    /* Switching mid-challenge restarts so the HUD clock drops its final 5-4-3-2-1
     * display. Event_Init re-enters Free Practice from the saved setting. */
    if (gobj) {
        Retry(gobj);
        return;
    }
    reset_clock = 1;
    Options_Main[OPT_FREEPRACTICE].disable = 1;
    Options_Main[OPT_DAMAGETHRESHOLD].disable = 0;
    Options_Main[OPT_SCALE].disable = 0;
    Options_Main[OPT_VELOCITY].disable = 0;
    Options_Main[OPT_UNDERPLATFORMS].disable = 0;
    Options_Main[OPT_INAIR].disable = 0;
    Options_Main[OPT_COLLISION].disable = 0;
    stc_match->match.timer = MATCH_TIMER_COUNTUP;
    ChangeHitDisplay(0, Options_Main[OPT_COLLISION].val);
}

void Egg_OnChangeSize(GOBJ *menu, int value)
{
    TM_SetSetting(TM_SETTING_EGGS, TM_EGGS_SCALE, value);
    if (egg_gobj != 0)
    {
        Item_Destroy(egg_gobj);
        egg_gobj = Egg_Spawn();
    }
}

/* The ground below (x, from_y) goes in coll_pos. Returns 0 unless it's a good
 * spot for an egg. */
static int Egg_TrySpot(float x, float from_y)
{
    float min_ground_width = 8;
    float to_y = from_y - 1000;
    Vec3  line_unk;
    int line_index, line_kind;

    // check main ground collision
    int is_ground = GrColl_RaycastGround(&coll_pos, &line_index, &line_kind, &line_unk, -1, -1, -1, 0,
        x, from_y, x, to_y, 0);
    if (is_ground == 0)
        return 0; // try a new random point

    // check distance from last collision
    float distance = Math_Vec3Distance(&coll_pos, &last_coll_pos);
    if (distance < 25.f)
        return 0;

    // check if too close to right end of the stage
    Vec3 near_pos;
    float near_x = x + min_ground_width;
    is_ground = GrColl_RaycastGround(&near_pos, &line_index, &line_kind, &line_unk, -1, -1, -1, 0,
        near_x, from_y, near_x, to_y, 0);
    if (is_ground == 0)
        return 0;

    // check if too close to left end of the stage
    near_x = x - min_ground_width;
    is_ground = GrColl_RaycastGround(&near_pos, &line_index, &line_kind, &line_unk, -1, -1, -1, 0,
        near_x, from_y, near_x, to_y, 0);
    return is_ground != 0;
}

/* Free Practice's options each join the normal spot in an even draw. */
static int Egg_ChooseSpot(void)
{
    if (stc_match->match.timer != MATCH_TIMER_COUNTUP)
        return EGG_SPOT_NORMAL;
    int under = Options_Main[OPT_UNDERPLATFORMS].val;
    int in_air = Options_Main[OPT_INAIR].val;
    int pick = HSD_Randi(1 + under + in_air);
    if (pick == 0)
        return EGG_SPOT_NORMAL;
    return under && pick == 1 ? EGG_SPOT_UNDER_PLATFORM : EGG_SPOT_IN_AIR;
}

/* A random point's raycast rarely starts below a side platform, so aim for one:
 * a raycast from just under the platform can only land on the ground below it.
 * Returns 0 if this stage has no room under a side platform. */
static int Egg_TryUnderPlatform(void)
{
    for (int i = 0; i < EGG_PLATFORM_TRIES; i++)
    {
        Vec3 platform;
        Vec2 left, right;
        int line, is_platform;
        float y;
        if (!FindPlatform(HSD_Randi(2) ? 1 : -1, &platform, &line) ||
            !FloorLine(line, &left, &right, &is_platform))
            continue;
        float x = RandomRange(left.X + EGG_PLATFORM_INSET, right.X - EGG_PLATFORM_INSET);
        if (LineY(line, x, &y) && Egg_TrySpot(x, y - 1))
            return 1;
    }
    return 0;
}

GOBJ *Egg_Spawn(void)
{
    float camera_left = Stage_GetCameraLeft();
    float camera_right = Stage_GetCameraRight();
    float camera_top = Stage_GetCameraTop();
    float camera_bottom = Stage_GetCameraBottom();

    int spot = Egg_ChooseSpot();
    if (spot == EGG_SPOT_UNDER_PLATFORM && !Egg_TryUnderPlatform())
        spot = EGG_SPOT_NORMAL;

    // pick a random point on the stage via raycast down from random point
    while (spot != EGG_SPOT_UNDER_PLATFORM &&
        !Egg_TrySpot(RandomRange(camera_left, camera_right), RandomRange(camera_bottom, camera_top)))
        ;

    Vec3 egg_pos = coll_pos;
    egg_falls_slowly = spot == EGG_SPOT_IN_AIR;
    if (egg_falls_slowly)
    {
        float lowest = coll_pos.Y + EGG_AIR_HEIGHT;
        float highest = camera_top - EGG_AIR_CEILING;
        egg_pos.Y = RandomRange(lowest, highest > lowest ? highest : lowest);
    }

    // random Y velocity on spawn. An egg under a platform would rise through
    // it and land on top, and one in the air just falls.
    int velocity = stc_match->match.timer == MATCH_TIMER_COUNTUP ?
        Options_Main[OPT_VELOCITY].val : TM_EGGS_DEFAULT_VELOCITY;
    if (spot != EGG_SPOT_NORMAL)
        velocity = 0;
    Vec3 rand_velocity = {0, (2 + HSD_Randf()) * velocity, 0};
    SpawnItem item_egg = {
        .it_kind = ITEM_EGG,
        .pos = egg_pos,
        .pos2 = egg_pos,
        .vel = rand_velocity,
    };

    last_coll_pos = coll_pos;

    accumulated_damage = 0;
    return Item_CreateItem2(&item_egg);
}

int Egg_OnTakeDamage(GOBJ *gobj)
{
    // gfx and sfx
    ItemData *egg_data = egg_gobj->userdata;
    accumulated_damage += egg_data->dmg.recent;
    int threshold = stc_match->match.timer == MATCH_TIMER_COUNTUP ?
        Options_Main[OPT_DAMAGETHRESHOLD].val : TM_EGGS_DEFAULT_DAMAGE;
    if (accumulated_damage >= threshold){
        Effect_SpawnSync(1232, gobj, egg_data->pos);
        Item_PlayOnDestroySFXAgain(egg_data, 244, 127, 64);
        
        // manage old and new egg
        Egg_Destroy(gobj);
        egg_gobj = Egg_Spawn();
        egg_counter++;
    }
    return 0;
}

void Event_Init(GOBJ *gobj)
{
    Eggs_LoadSettings();
    /* A retry may reuse the event module. Rebuild gating from the saved mode,
     * not from disabled-row state left by the previous practice session. */
    Options_Main[OPT_FREEPRACTICE].disable = 0;
    for (unsigned i = 0; i < countof(saved_eggs_rows); ++i) Options_Main[saved_eggs_rows[i]].disable = 1;
    for (unsigned i = 0; i < countof(spawn_eggs_rows); ++i) Options_Main[spawn_eggs_rows[i]].disable = 1;
    if (TM_GetSetting(TM_SETTING_EGGS, TM_EGGS_FREE_PRACTICE)) StartFreePractice(0);
    else ChangeHitDisplay(0, 0);
    Eggs_ChangeHitboxTrails(0, 0);
    StageSwap_Init();

    // initialize egg camera subject
    cam = CameraSubject_Alloc();
    cam->boundleft_proj = -10;
    cam->boundright_proj = 10;
    cam->boundtop_proj = 10;
    cam->boundbottom_proj = -10;

    // hud setup
    hud_score_gobj = GObj_Create(0, 0, 0);
    GObj_AddObject(hud_score_gobj, 3, hud_score_jobj);
    GObj_AddGXLink(hud_score_gobj, GXLink_Common, GXLINK_HUD, 80);

    hud_best_gobj = GObj_Create(0, 0, 0);
    GObj_AddObject(hud_best_gobj, 3, hud_best_jobj);
    GObj_AddGXLink(hud_best_gobj, GXLink_Common, GXLINK_HUD, 80);
    canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 81, 19);
    
    // Create text objects
    hud_score_text = Text_CreateText(2, canvas);
    hud_score_text->kerning = 1;
    hud_score_text->use_aspect = 1;
    hud_score_text->viewport_scale.X = 1;
    hud_score_text->viewport_scale.Y = 1;

    hud_best_text = Text_CreateText(2, canvas);
    hud_best_text->kerning = 1;
    hud_best_text->use_aspect = 1;
    hud_best_text->scale.X = 0.35;
    hud_best_text->scale.Y = 0.35;
    hud_best_text->viewport_scale.X = 1;
    hud_best_text->viewport_scale.Y = 1;

    Eggs_PlaceCounter(Options_Main[OPT_COUNTERPOSITION].val);

    // dummy text
    Text_AddSubtext(hud_score_text, 0, 0, "-");
    Text_AddSubtext(hud_best_text, 0, 0, "-");
    
    high_score = Events_GetSavedScore(stc_memcard->EventBackup.event);

    stc_match->end_kind = MATCHENDKIND_NONE;
}

void Eggs_ChangeHitboxTrails(GOBJ *menu_gobj, int value) {
    event_vars->trails->configure(Options_HitboxTrails[OPT_HITBOXTRAILS_ENABLED].val,
                                  Options_HitboxTrails[OPT_HITBOXTRAILS_DECAY].val);
    int vf = TM_GetSetting(TM_SETTING_FLAG, TM_FLAG_TRAILS_VERY_FAST);
    int instant = TM_GetSetting(TM_SETTING_FLAG, TM_FLAG_TRAILS_INSTANT);
    static char *names[] = { TM_GLOBAL_TRAIL_STATE_NAMES };
    Options_HitboxTrails[OPT_HITBOXTRAILS_INFO].name = names[vf | (instant << 1)];
}

void Event_Think(GOBJ *event)
{
    // reset stale moves
    for (int i = 0; i < 6; i++)
    {
        // check if fighter exists
        GOBJ *fighter = Fighter_GetGObj(i);
        if (fighter != 0)
        {
            // reset stale move table
            int *stale_move_table = Fighter_GetStaleMoveTable(i);
            memset(stale_move_table, 0, 0x2C);
        }
    }

    /* The match starts with the challenge's 60 second countdown loaded. Zero it
     * here, after match setup, so Free Practice counts up from 0:00. */
    if (reset_clock)
    {
        stc_match->time_seconds = 0;
        stc_match->time_ms = 0;
        reset_clock = 0;
    }

    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;

    // wait for first frame of player control to spawn first egg
    if (egg_gobj == 0 && hmn_data->flags.input_enable)
    {   
        egg_gobj = Egg_Spawn();
    }

    // when egg exists
    if (egg_gobj != 0) 
    {
        // don't allow holding or nudging
        ItemData *egg_data = egg_gobj->userdata;
        egg_data->can_hold = 0;
        egg_data->can_nudge = 0;
        egg_data->self_vel.X = 0;
        if (egg_falls_slowly && egg_data->self_vel.Y < -EGG_AIR_FALL_SPEED)
            egg_data->self_vel.Y = -EGG_AIR_FALL_SPEED;

        // apply scaling changes
        JOBJ *egg_jobj = Item_GetBoneJOBJ(egg_gobj, 0);
        int scale = stc_match->match.timer == MATCH_TIMER_COUNTUP ? Options_Main[OPT_SCALE].val : 0;
        egg_jobj->scale.X = EggOptions_Size[scale];
        egg_jobj->scale.Y = EggOptions_Size[scale];
        egg_jobj->scale.Z = EggOptions_Size[scale];
        egg_data->scale = EggOptions_Size[scale];

        // show egg hurtbox
        int collision = stc_match->match.timer == MATCH_TIMER_COUNTUP ? Options_Main[OPT_COLLISION].val : 0;
        egg_data->show_hit = collision;
        egg_data->show_model = !collision;

        // set this callback on every frame or else it gets overwritten
        egg_data->it_func->OnTakeDamage = Egg_OnTakeDamage;

        // update camera position
        cam->cam_pos.X = egg_data->pos.X;
        cam->cam_pos.Y = egg_data->pos.Y + 15; // add so it's not awkwardly low
        cam->cam_pos.Z = egg_data->pos.Z;
    }

    // Update egg count, hide when paused
    if (Pause_CheckStatus(1) != 2)
    {
        Text_SetText(hud_score_text, 0, "%d", egg_counter);
        if (stc_match->match.timer == MATCH_TIMER_COUNTUP)
        {
            Text_SetText(hud_best_text, 0, "Free Practice");
            Text_SetColor(hud_best_text, 0, &text_gold);
        }
        else if (high_score > 0)
        {
            Text_SetText(hud_best_text, 0, "Best: %d", high_score);
            Text_SetColor(hud_best_text, 0, &text_white);
        }
        else
        {
            Text_SetText(hud_best_text, 0, "");
        }
    } 
    else
    {
        Text_SetText(hud_score_text, 0, "");
        Text_SetText(hud_best_text, 0, "");
    }

    // After 1 minute and not in free practice
    if (stc_match->time_frames == 3600 && stc_match->match.timer != MATCH_TIMER_COUNTUP)
    {   
        int event_id = stc_memcard->EventBackup.event;
        Events_SetEventAsPlayed(event_id);
        Match_SetEndGraphic(MATCHENDKIND_VSGAME);

        // high score logic
        int high_score = Events_GetSavedScore(event_id);
        if (egg_counter > high_score)
        {
            MatchInfo_0x0010_store(0x9c40); // Makes "new record" audio happen, IDK why
            Match_SetPostMatchSFX(325);
            Events_StoreEventScore(event_id, egg_counter);
        }
        else
        {
            Match_SetPostMatchSFX(324);
        }

        Match_EndImmediate();
    }
}

EventMenu *Event_Menu = &Menu_Main;
