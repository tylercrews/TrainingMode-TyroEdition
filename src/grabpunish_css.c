#include "events.h"

/* Character select for 2v1 Grab Punish Practice: the VS select, always in
 * Teams mode, with exactly one CPU on your team (your teammate) and every
 * other CPU (the enemy) on another team. The team buttons still pick the
 * colors:
 * - Your color: your teammate comes along.
 * - Your teammate's color: you come along.
 * - Another CPU onto your color: it becomes your teammate, and the old
 *   teammate moves to another team.
 * The select opens on the teams of the last match (saved in EventInit's
 * GrabPunish_PrepareMatch), apart from the normal saved costume.
 * A team change does what the native team button does (80261B6C): set the
 * door's team and its match player's team, then redraw the portrait. The
 * door's color follows its team every frame. */

static s8 prev_team[4]; // Each door's team last frame; -1 when closed.
static s8 mate;         // Your teammate's door; -1 until there's a CPU.
static u8 your_team, enemy_team; // From the save.
static bool team_loaded;         // Your door has its saved team.

static bool IsOpen(MnSlChrDoor *door) { return door->p_kind == PKIND_HMN || door->p_kind == PKIND_CPU; }
// The enemy's saved team, unless that's your team now.
static int EnemyTeam(int team) { return enemy_team != team ? enemy_team : (team + 1) % 3; }

static void SetTeam(int port, int team) {
    MnSlChrDoor *door = &stc_css_data->doors[port];
    if (door->team == team)
        return;
    door->team = team;
    (*stc_css_minorscene)->vs_data.match_init.playerData[port].team = team;
    CSS_UpdateCSP(port);
}

static void Teams_Think(GOBJ *gobj) {
    VSMinorData *scene = *stc_css_minorscene;
    MatchInit *match = &scene->vs_data.match_init;
    MnSlChrDoor *doors = stc_css_data->doors;

    // Undo the Teams / Free-for-all toggle, as the toggle itself would.
    if (!match->is_teams) {
        match->is_teams = 1;
        CSS_SetModeTexture(scene->css_kind);
    }

    int hmn = -1;
    for (int i = 0; i < 4 && hmn < 0; i++)
        if (doors[i].p_kind == PKIND_HMN)
            hmn = i;
    if (hmn >= 0) {
        if (!team_loaded) {
            team_loaded = true;
            SetTeam(hmn, your_team);
        }
        int team = doors[hmn].team;
        if (mate >= 0 && doors[mate].p_kind != PKIND_CPU)
            mate = -1;
        for (int i = 0; i < 4; i++) {
            if (i == mate || doors[i].p_kind != PKIND_CPU || prev_team[i] < 0)
                continue;
            if (doors[i].team != prev_team[i] && doors[i].team == team)
                mate = i;
        }
        if (mate >= 0 && prev_team[mate] >= 0 && doors[mate].team != prev_team[mate]
            && doors[mate].team != team) {
            team = doors[mate].team;
            SetTeam(hmn, team);
        }
        // The first CPU (one already on your team, if any) is your teammate.
        for (int i = 0; i < 4 && mate < 0; i++)
            if (doors[i].p_kind == PKIND_CPU && doors[i].team == team)
                mate = i;
        for (int i = 0; i < 4 && mate < 0; i++)
            if (doors[i].p_kind == PKIND_CPU)
                mate = i;
        if (mate >= 0)
            SetTeam(mate, team);
        // A newly opened CPU takes the enemy's team too.
        for (int i = 0; i < 4; i++)
            if (i != mate && doors[i].p_kind == PKIND_CPU && (doors[i].team == team || prev_team[i] < 0))
                SetTeam(i, EnemyTeam(team));
    }

    for (int i = 0; i < 4; i++)
        prev_team[i] = IsOpen(&doors[i]) ? doors[i].team : -1;
}

void OnCSSLoad(HSD_Archive *archive) {
    // Before the CSS sets its mode texture, so it opens in Teams.
    (*stc_css_minorscene)->vs_data.match_init.is_teams = 1;
    for (int i = 0; i < 4; i++)
        prev_team[i] = -1;
    mate = -1;
    your_team = TM_GetSetting(TM_SETTING_GRAB_PUNISH, TM_GP_YOUR_TEAM);
    enemy_team = TM_GetSetting(TM_SETTING_GRAB_PUNISH, TM_GP_ENEMY_TEAM);
    team_loaded = false;
    // Priority 2: after the CSS's own input handling, like Toggle CSS Icon.
    GOBJ *gobj = GObj_Create(4, 5, 0);
    GObj_AddProc(gobj, Teams_Think, 2);
}
