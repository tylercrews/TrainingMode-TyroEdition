#include "events.h"
#include "stage_points.h"
#include "stage_swap.h"

/* Ledgegrab Speedrun: count the frames from your first move to the goal ledge.
 * Results are session-only. Character and stage change the times too much for
 * a saved record to mean anything, and this .dat reloads with each match, so
 * Change Stage and leaving the event both clear them. They stay in statics:
 * a savestate load rewinds the event's userdata. */

// A result stays up this long before the restart.
#define LINGER_FRAMES 60
// Still going after 30 seconds is a DNF. Also the most a run can count.
#define DNF_FRAMES (30 * 60)
// At Ledge stands this far inland of the edge, so nobody starts out teetering.
#define LEDGE_INSET 5

// How many frames off the best a run can be and still be green, then yellow.
// Any further off is red.
#define GREEN_RANGE 4
#define YELLOW_RANGE 10

// The menu choices survive Change Stage's restart; the results don't.
#define LEDGEGRAB_CARRY_TAG 0x4C677253 /* "LgrS" */

enum menu_options { OPT_GOAL, OPT_START, OPT_FACING };
enum start_position {
    START_LEDGE, START_FAR_LEDGE, START_UNDER_CLOSE, START_UNDER_FAR,
    START_CENTER, START_TOP, START_CLOSE, START_FAR, START_COUNT
};
enum facing { FACING_TOWARD, FACING_AWAY, FACING_COUNT };
enum hud_row { ROW_COUNTER, ROW_BEST, ROW_PREVIOUS, ROW_COUNT };

// Left and right share a record: they're the same run, mirrored.
typedef struct LedgegrabRecord {
    u16 best, previous; // 0 until there is one. Previous can be DNF_FRAMES.
} LedgegrabRecord;
static LedgegrabRecord records[START_COUNT][FACING_COUNT];

typedef struct LedgegrabCarry {
    int goal, start, facing;
} LedgegrabCarry;

static void ChangeRun(GOBJ *menu, int value);
static void ChangeStart(GOBJ *menu, int value);
static void Exit(GOBJ *menu);

static const char *goal_names[] = {"Left", "Right"};
static const char *facing_names[FACING_COUNT] = {"Toward Ledge", "Away From Ledge"};
static const char *start_names[START_COUNT] = {
    "At Ledge", "At Far Ledge", "Under Close Plat", "Under Far Plat",
    "Center Stage", "Top Plat", "Close Plat", "Far Plat"};
static const char *start_names_unavailable[START_COUNT] = {
    "At Ledge", "At Far Ledge", "Under Close Plat (N/A)", "Under Far Plat (N/A)",
    "Center Stage", "Top Plat (N/A)", "Close Plat (N/A)", "Far Plat (N/A)"};
static const char *start_display[START_COUNT];
static int start_available[START_COUNT];
static int previous_start;

static EventOption options[] = {
    {.kind = OPTKIND_STRING, .name = "Goal Ledge", .value_num = countof(goal_names), .values = goal_names,
     .OnChange = ChangeRun,
     .desc = {"The ledge to reach. Starts are named from it:",
              "Close is on its side of the stage, Far the other.",
              "D-Pad down also switches it and restarts."}},
    {.kind = OPTKIND_STRING, .name = "Start Position", .value_num = START_COUNT, .values = start_display,
     .OnChange = ChangeStart,
     .desc = {"Where you start. Each start and facing keeps",
              "its own Session Best and Previous time.",
              "Platforms this stage doesn't have are skipped.",
              "D-Pad left/right also cycles these and restarts."}},
    {.kind = OPTKIND_STRING, .name = "Facing", .value_num = FACING_COUNT, .values = facing_names,
     .OnChange = ChangeRun,
     .desc = {"Start facing toward or away from the goal ledge.",
              "D-Pad up also switches it and restarts."}},
    {.kind = OPTKIND_MENU, .name = "Change Stage", .menu = &StageSwap_Menu,
     .desc = {"Restart on another tournament-legal stage.",
              "This clears every Session Best and Previous time."}},
    {.kind = OPTKIND_INFO, .name = "Help",
     .desc = {"Grab the goal ledge in as few frames as you can.",
              "The count starts when you move. 30 seconds is a DNF.",
              "D-Pad left/right: previous or next start position.",
              "D-Pad down: switch ledge. D-Pad up: switch facing."}},
    {.kind = OPTKIND_FUNC, .name = "Exit", .OnSelect = Exit,
     .desc = {"Return to the Event Select Screen."}},
};
static EventMenu menu = {.name = "Ledgegrab Speedrun", .option_num = countof(options), .options = options, .on_exit = Exit};
EventMenu *Event_Menu = &menu;

static struct run {
    bool moving;                // Past the first move, so the count is running.
    int frames;                 // DNF_FRAMES for a DNF.
    int linger;                 // Counts down to the restart once the run is over.
    LedgegrabRecord *finished;  // Where the Previous time goes at the restart.
} run = { 0 };
static bool restart_pending = false;
static bool restarting = false;

static LedgegrabRecord *CurrentRecord(void) {
    return &records[options[OPT_START].val][options[OPT_FACING].val];
}

static float Outward(void) { return options[OPT_GOAL].val ? 1 : -1; }

// Readout ---------------------------------------------------------------

static GXColor White = {0xff, 0xff, 0xff, 0xff};
static GXColor Cyan = {0x00, 0xff, 0xff, 0xff};
static GXColor Green = {0x00, 0xff, 0x00, 0xff};
static GXColor Yellow = {0xff, 0xe0, 0x00, 0xff};
static GXColor Red = {0xff, 0x00, 0x00, 0xff};
static GXColor Gold = {0xff, 0xe1, 0x41, 0xff}; // Under Fire's count.
static GXColor Outline = {0x00, 0x00, 0x00, 0xff};

// A black outline of four, then the colored text on top, as in Under Fire.
#define ROW_SUBTEXTS 5
#define ROW_FOREGROUND (ROW_SUBTEXTS - 1)

static Text *hud_text;
static int row_shown[ROW_COUNT] = {-1, -1, -1};

static void CreateHUD(void) {
    int canvas = Text_CreateCanvas(2, 0, 0, 0, 0, GXLINK_HUD, 81, 19);
    Text *text = hud_text = Text_CreateText(2, canvas);
    if (!text)
        return;
    text->kerning = text->align = 1;
    text->trans = (Vec3){320, 55, 0};
    text->viewport_scale.X = text->viewport_scale.Y = 1;
    text->scale.X = text->scale.Y = .8f;
    static const Vec2 offsets[ROW_SUBTEXTS] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}, {0, 0}};
    static const float row_y[ROW_COUNT] = {0, 34, 62};
    for (int row = 0; row < ROW_COUNT; ++row) {
        for (int i = 0; i < ROW_SUBTEXTS; ++i) {
            int subtext = Text_AddSubtext(text, offsets[i].X, row_y[row] + offsets[i].Y, "");
            Text_SetColor(text, subtext, i < ROW_FOREGROUND ? &Outline : row == ROW_COUNTER ? &White : &Gold);
            if (row != ROW_COUNTER)
                Text_SetScale(text, subtext, .5625f, .5625f); // .8 * .5625 = Under Fire's .45.
        }
    }
}

static void SetCounterColor(GXColor *color) {
    if (hud_text)
        Text_SetColor(hud_text, ROW_FOREGROUND, color);
}

static void SetRow(int row, const char *label, int frames) {
    if (frames == row_shown[row])
        return;
    row_shown[row] = frames;
    for (int i = 0; i < ROW_SUBTEXTS; ++i) {
        int subtext = row * ROW_SUBTEXTS + i;
        if (frames == DNF_FRAMES)
            Text_SetText(hud_text, subtext, "%s: DNF", label);
        else if (frames || row == ROW_COUNTER)
            Text_SetText(hud_text, subtext, "%s: %d", label, frames);
        else
            Text_SetText(hud_text, subtext, "%s: -", label);
    }
}

static GXColor *ResultColor(int frames, int best) {
    if (frames >= DNF_FRAMES)
        return &Red;
    if (!best || frames <= best)
        return &Cyan;
    int off = frames - best;
    return off <= GREEN_RANGE ? &Green : off <= YELLOW_RANGE ? &Yellow : &Red;
}

// Ledges ----------------------------------------------------------------

// All in stage units. The black edge sits about this far outside each marker.
#define MARKER_EDGE 1
// The wrong ledge's X: center to the end of a stroke along each axis, and half
// a stroke's thickness.
#define X_ARM 10
#define X_STROKE 2.5f
// The goal ledge's !, raised to clear a fighter standing at the ledge. The dot
// is square; the bar narrows toward it.
#define BANG_RAISE 20
#define BANG_DOT 3
#define BANG_GAP 2
#define BANG_LENGTH 17
#define BANG_NARROW 1.75f
#define BANG_WIDE 3.5f
#define BANG_TOP (BANG_RAISE + BANG_DOT * 2 + BANG_GAP + BANG_LENGTH)
// Two strokes, or a dot and a bar, for each marker.
#define MARKER_QUADS 4
// Room the camera keeps around each ledge, and above the !.
#define LEDGE_CAMERA_MARGIN 15

static Vec2 ledges[2]; // Left, then right.
static bool ledges_found = false;

/* A lone fighter's camera zooms in, which would leave the markers off-screen
 * from across the stage, so each ledge gets a camera subject too. The match
 * frees them. */
static void FindLedges(void) {
    float edges[2];
    GroundEdges(&edges[0], &edges[1]);
    for (int i = 0; i < 2; ++i) {
        Vec3 point;
        int line;
        GroundPoint(edges[i], &point, &line);
        ledges[i] = (Vec2){point.X, point.Y};

        CmSubject *cam = CameraSubject_Alloc();
        cam->kind = 0;
        cam->cam_pos = point;
        cam->boundleft_proj = cam->boundleft_curr = -LEDGE_CAMERA_MARGIN;
        cam->boundright_proj = cam->boundright_curr = LEDGE_CAMERA_MARGIN;
        cam->boundtop_proj = cam->boundtop_curr = BANG_TOP + LEDGE_CAMERA_MARGIN;
        cam->boundbottom_proj = cam->boundbottom_curr = -LEDGE_CAMERA_MARGIN;
    }
    ledges_found = true;
}

// The diagonal through at: flip 1 rises to the right, -1 to the left.
static void AddStroke(Vec2 at, float flip, float arm, float half, GXColor color) {
    GFX_AddVtx(at.X + flip * (-arm + half), at.Y - arm - half, 0, color);
    GFX_AddVtx(at.X + flip * (arm + half), at.Y + arm - half, 0, color);
    GFX_AddVtx(at.X + flip * (arm - half), at.Y + arm + half, 0, color);
    GFX_AddVtx(at.X + flip * (-arm - half), at.Y - arm + half, 0, color);
}

static void AddBar(float x, float bottom, float top, float half_bottom, float half_top, GXColor color) {
    GFX_AddVtx(x - half_bottom, bottom, 0, color);
    GFX_AddVtx(x + half_bottom, bottom, 0, color);
    GFX_AddVtx(x + half_top, top, 0, color);
    GFX_AddVtx(x - half_top, top, 0, color);
}

// Both markers, grown by edge on every side.
static void AddMarkers(float edge, GXColor x_color, GXColor bang_color) {
    int goal = options[OPT_GOAL].val;
    Vec2 wrong = ledges[!goal], target = ledges[goal];
    AddStroke(wrong, 1, X_ARM + edge, X_STROKE + edge, x_color);
    AddStroke(wrong, -1, X_ARM + edge, X_STROKE + edge, x_color);

    float dot = target.Y + BANG_RAISE, bar = dot + BANG_DOT * 2 + BANG_GAP;
    AddBar(target.X, dot - edge, dot + BANG_DOT * 2 + edge, BANG_DOT + edge, BANG_DOT + edge, bang_color);
    AddBar(target.X, bar - edge, bar + BANG_LENGTH + edge, BANG_NARROW + edge, BANG_WIDE + edge, bang_color);
}

/* With no depth test the stage can't hide them, and they stack in draw order:
 * the black edges first, the colors on top. */
static void DrawMarkers(GOBJ *gobj, int pass) {
    if (pass != 2 || !ledges_found)
        return;
    event_vars->GFX_Start(MARKER_QUADS * 2 * 4, (GFX_Params) { .shape = GX_QUADS, .no_depth = true });
    AddMarkers(MARKER_EDGE, Outline, Outline);
    AddMarkers(0, Red, Green);
}

// Starts ----------------------------------------------------------------

/* Every start is relative to the goal ledge, so changing the goal mirrors it. */
static int ResolveStart(int choice, Vec3 *position, int *line) {
    float outward = Outward();
    if (choice == START_LEDGE || choice == START_FAR_LEDGE) {
        float left, right;
        GroundEdges(&left, &right);
        int on_right = (choice == START_LEDGE) == (outward > 0);
        GroundPoint(on_right ? right - LEDGE_INSET : left + LEDGE_INSET, position, line);
        return 1;
    }
    if (choice == START_CENTER) {
        GroundPoint(0, position, line);
        return 1;
    }
    int close = choice == START_UNDER_CLOSE || choice == START_CLOSE;
    int side = choice == START_TOP ? 0 : close ? outward : -outward;
    Vec3 platform;
    int platform_line;
    if (!FindPlatform(side, &platform, &platform_line))
        return 0;
    if (choice == START_UNDER_CLOSE || choice == START_UNDER_FAR)
        return FindSolidGround(platform.X, position, line) && position->Y < platform.Y - 4;
    *position = platform;
    *line = platform_line;
    return 1;
}

static void RefreshStarts(void) {
    for (int i = 0; i < START_COUNT; ++i) {
        Vec3 position;
        int line;
        start_available[i] = ResolveStart(i, &position, &line);
        start_display[i] = start_available[i] ? start_names[i] : start_names_unavailable[i];
    }
}

// Nana follows where Popo was a few frames ago; point that history at him.
static void SeedFollower(GOBJ *sub, GOBJ *leader) {
    FighterData *main_data = leader->userdata;
    FighterData *sub_data = sub->userdata;
    for (u32 i = 0; i < countof(sub_data->cpu.leader_log); ++i) {
        CPULeaderLog *log = &sub_data->cpu.leader_log[i];
        log->pos = main_data->phys.pos;
        log->facing_direction = main_data->facing_direction;
    }
}

static void PlaceFighter(GOBJ *fighter, Vec3 ground, int line, float facing) {
    FighterData *ft = fighter->userdata;
    // Rebirth also clears a previous KO.
    Fighter_EnterSleep(fighter, ft->flags.ms);
    Fighter_EnterRebirth(fighter);
    GroundAt(fighter, ground, line);
    ft->facing_direction = ft->facing_direction_prev = facing;
    Fighter_EnterWait(fighter);
}

static void UpdateCameraBox(GOBJ *fighter) {
    Fighter_UpdateCameraBox(fighter);

    FighterData *data = fighter->userdata;
    CmSubject *subject = data->camera_subject;
    subject->boundleft_curr = subject->boundleft_proj;
    subject->boundright_curr = subject->boundright_proj;

    Match_CorrectCamera();
}

// Run -------------------------------------------------------------------

static void Reset(void) {
    if (run.finished)
        run.finished->previous = run.frames;
    run = (struct run) { 0 };
    restart_pending = false;
    SetCounterColor(&White);

    event_vars->Savestate_Load_v1(event_vars->savestate, Savestate_Silent);

    /* Fountain's platforms keep moving, so check again. Falling back here keeps
     * the run's result under the start it really used. */
    RefreshStarts();
    int choice = options[OPT_START].val;
    if (!start_available[choice])
        choice = START_CENTER;
    options[OPT_START].val = options[OPT_START].val_prev = previous_start = choice;

    Vec3 ground;
    int line;
    ResolveStart(choice, &ground, &line);
    float facing = options[OPT_FACING].val == FACING_TOWARD ? Outward() : -Outward();

    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    PlaceFighter(hmn, ground, line, facing);
    GOBJ *hmn_sub = Fighter_GetSubcharGObj(0, 1);
    if (hmn_sub && hmn_data->flags.has_follower) {
        PlaceFighter(hmn_sub, ground, line, facing);
        SeedFollower(hmn_sub, hmn);
    }
    UpdateCameraBox(hmn);
}

// Ends the run: DNF_FRAMES for a DNF. The result lingers, then Reset follows.
static void Finish(int frames) {
    LedgegrabRecord *record = CurrentRecord();
    SetCounterColor(ResultColor(frames, record->best));
    if (frames < DNF_FRAMES && (!record->best || frames < record->best))
        record->best = frames;
    run.frames = frames;
    run.finished = record;
    run.linger = LINGER_FRAMES;
}

static bool OnGoalLedge(FighterData *ft) {
    return (ft->state_id == ASID_CLIFFCATCH || ft->state_id == ASID_CLIFFWAIT)
        && ft->phys.pos.X * Outward() > 0;
}

// Menu ------------------------------------------------------------------

/* The menu runs paused, so these leave the restart for the next frame. */
static void ChangeRun(GOBJ *menu, int value) { restart_pending = true; }

static void ChangeStart(GOBJ *menu, int value) {
    RefreshStarts();
    // Keep moving in the selector's direction past missing platforms.
    int direction = (value - previous_start + START_COUNT) % START_COUNT == 1 ? 1 : -1;
    for (int i = 0; i < START_COUNT && !start_available[value]; ++i)
        value = (value + direction + START_COUNT) % START_COUNT;
    options[OPT_START].val = previous_start = value;
    restart_pending = true;
}

static void Exit(GOBJ *menu) {
    Memcard_SaveIfChanged();
    stc_match->state = 3;
    Match_EndVS();
}

static void StageSwap_BeforeRestart(void) {
    LedgegrabCarry *carry = event_vars->carry_store(LEDGEGRAB_CARRY_TAG, sizeof(LedgegrabCarry));
    if (!carry)
        return;
    carry->goal = options[OPT_GOAL].val;
    carry->start = options[OPT_START].val;
    carry->facing = options[OPT_FACING].val;
}

// D-pad -----------------------------------------------------------------

// Unlike the menu's selector, the D-pad wraps around.
static void CycleStart(int direction) {
    ChangeStart(0, (options[OPT_START].val + direction + START_COUNT) % START_COUNT);
}

static void ToggleOption(int row) {
    options[row].val = options[row].val_prev = !options[row].val;
    restart_pending = true;
}

static void ReadDPad(FighterData *ft) {
    int down = ft->input.down;
    // Up is also the taunt. If the fighter reads this after the restart, a taunt
    // out of the new Wait would start the count.
    ft->input.down &= ~HSD_BUTTON_DPAD_UP;
    if (down & HSD_BUTTON_DPAD_LEFT)
        CycleStart(-1);
    else if (down & HSD_BUTTON_DPAD_RIGHT)
        CycleStart(1);
    else if (down & HSD_BUTTON_DPAD_DOWN)
        ToggleOption(OPT_GOAL);
    else if (down & HSD_BUTTON_DPAD_UP)
        ToggleOption(OPT_FACING);
}

// Event -----------------------------------------------------------------

void Event_Init(GOBJ *gobj) {
    const LedgegrabCarry *carry = event_vars->carry_take(LEDGEGRAB_CARRY_TAG, sizeof(LedgegrabCarry));
    if (carry) {
        options[OPT_GOAL].val = options[OPT_GOAL].val_prev = carry->goal;
        options[OPT_START].val = options[OPT_START].val_prev = carry->start;
        options[OPT_FACING].val = options[OPT_FACING].val_prev = carry->facing;
    }
    previous_start = options[OPT_START].val;
    for (int i = 0; i < START_COUNT; ++i)
        start_display[i] = start_names[i];
    StageSwap_Init();
    CreateHUD();
    // The in-game effects link draws after the stage and fighters.
    GObj_AddGXLink(GObj_Create(0, 7, 0), DrawMarkers, MATCHGX_EFFECT, 255);
}

void Event_Think(GOBJ *gobj) {
    if (restarting || Pause_CheckStatus(1) == 2)
        return;
    if (event_vars->game_timer == 1) {
        // Only the six legal stages: anything else restarts on Battlefield.
        if (!LegalStage()) {
            restarting = true;
            event_vars->restart_on_stage(GRKINDEXT_BATTLE);
            return;
        }
        event_vars->Savestate_Save_v1(event_vars->savestate, Savestate_Silent);
        FindLedges();
        Reset();
    }

    FighterData *hmn_data = Fighter_GetGObj(0)->userdata;

    ReadDPad(hmn_data);
    if (hmn_data->flags.dead) {
        if (!run.linger)
            Finish(DNF_FRAMES);
        restart_pending = true;
    }

    if (run.linger && --run.linger == 0)
        restart_pending = true;
    if (restart_pending) {
        Reset();
        return;
    }
    if (run.linger)
        return;

    if (hmn_data->state_id != ASID_WAIT)
        run.moving = true;
    if (!run.moving)
        return;
    if (OnGoalLedge(hmn_data))
        Finish(run.frames);
    else if (++run.frames >= DNF_FRAMES)
        Finish(DNF_FRAMES);
}

void Event_Update(void) {
    if (!hud_text)
        return;
    hud_text->hidden = ((MenuData *)event_vars->menu_gobj->userdata)->mode == MenuMode_Paused;
    LedgegrabRecord *record = CurrentRecord();
    SetRow(ROW_COUNTER, "Frames", run.frames);
    SetRow(ROW_BEST, "Session Best", record->best);
    SetRow(ROW_PREVIOUS, "Previous", record->previous);
}
