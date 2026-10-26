#include "server.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

void SanitizeUsername(const char *in, char *out, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < max_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '_' || c == '-') out[j++] = (char)tolower(c);
    }
    out[j] = '\0';
}

void SanitizePassword(const char *in, char *out, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < max_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '_' || c == '-') out[j++] = (char)c;
    }
    out[j] = '\0';
    if (j == 0) strcpy(out, "1234");
}

void SaveAccountJSON(
    const char *clean_user,
    const char *clean_pass,
    uint8_t plane_class,
    uint8_t skin_id,
    uint8_t unlocked_skins,
    int xp,
    int gems,
    int elo
) {
    if (clean_user[0] == '\0') return;
    mkdir("FA_Server", 0755);
    mkdir("FA_Server/accounts", 0755);
    char path[128];
    snprintf(path, sizeof(path), "FA_Server/accounts/%s.json", clean_user);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n"
               "  \"username\": \"%s\",\n"
               "  \"password\": \"%s\",\n"
               "  \"plane_class\": %u,\n"
               "  \"skin_id\": %u,\n"
               "  \"unlocked_skins\": %u,\n"
               "  \"xp\": %d,\n"
               "  \"gems\": %d,\n"
               "  \"elo\": %d\n"
               "}\n",
            clean_user, clean_pass, (unsigned int)plane_class, (unsigned int)skin_id,
            (unsigned int)(unlocked_skins | 1u), xp, gems, elo);
    fclose(f);
}

bool AuthenticateAccountJSON(
    const char *raw_user,
    const char *raw_pass,
    bool allow_create_if_missing,
    char *clean_out,
    char *clean_pass_out,
    uint8_t *out_class,
    uint8_t *out_skin,
    uint8_t *out_unlocked_skins,
    int *out_xp,
    int *out_gems,
    int *out_elo
) {
    SanitizeUsername(raw_user, clean_out, MAX_USERNAME_LEN);
    SanitizePassword(raw_pass, clean_pass_out, MAX_PASSWORD_LEN);
    *out_class = 0;
    *out_skin = 0;
    *out_unlocked_skins = 1;
    *out_xp = 0;
    *out_gems = 0;
    *out_elo = 1000;

    if (clean_out[0] == '\0') return false;

    mkdir("FA_Server", 0755);
    mkdir("FA_Server/accounts", 0755);

    char path[128];
    snprintf(path, sizeof(path), "FA_Server/accounts/%s.json", clean_out);

    FILE *f = fopen(path, "r");
    if (!f) {
        if (!allow_create_if_missing) return false;
        SaveAccountJSON(clean_out, clean_pass_out, 0, 0, 1, 0, 0, 1000);
        printf("[Compte JSON] Nouveau compte cree : %s (1000 ELO)\n", path);
        return true;
    }

    char savedPass[MAX_PASSWORD_LEN] = "";
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned int uval = 0; int val = 0; char strVal[64] = "";
        if (sscanf(line, " \"password\": \"%15[^\"]\"", strVal) == 1) strncpy(savedPass, strVal, MAX_PASSWORD_LEN - 1);
        else if (sscanf(line, " \"plane_class\": %u", &uval) == 1) { if (uval <= 2) *out_class = (uint8_t)uval; }
        else if (sscanf(line, " \"skin_id\": %u", &uval) == 1) { if (uval <= 4) *out_skin = (uint8_t)uval; }
        else if (sscanf(line, " \"unlocked_skins\": %u", &uval) == 1) { *out_unlocked_skins = (uint8_t)(uval | 1u); }
        else if (sscanf(line, " \"xp\": %d", &val) == 1) *out_xp = val;
        else if (sscanf(line, " \"gems\": %d", &val) == 1) *out_gems = val;
        else if (sscanf(line, " \"elo\": %d", &val) == 1) *out_elo = val;
    }
    fclose(f);

    if (*out_elo < 100) *out_elo = 1000;
    if (savedPass[0] == '\0') {
        SaveAccountJSON(clean_out, clean_pass_out, *out_class, *out_skin, *out_unlocked_skins, *out_xp, *out_gems, *out_elo);
        return true;
    }
    if (strcmp(savedPass, clean_pass_out) != 0) return false;
    return true;
}

void RewardMatchPlayersJSON(GameRoom *room) {
    if (room->rewards_distributed) return;
    room->rewards_distributed = true;

    int winner = -1;
    if (room->team_buildings_destroyed[0] > room->team_buildings_destroyed[1]) winner = 0;
    else if (room->team_buildings_destroyed[1] > room->team_buildings_destroyed[0]) winner = 1;
    else if (room->team_city_damage[0] > room->team_city_damage[1]) winner = 0;
    else if (room->team_city_damage[1] > room->team_city_damage[0]) winner = 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!room->clients[i].connected) continue;
        char cleanUser[MAX_USERNAME_LEN];
        SanitizeUsername(room->clients[i].state.username, cleanUser, MAX_USERNAME_LEN);
        if (cleanUser[0] == '\0') continue;

        char path[128];
        snprintf(path, sizeof(path), "FA_Server/accounts/%s.json", cleanUser);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char savedPass[MAX_PASSWORD_LEN] = "1234";
        uint8_t cls = 0, skin = 0, unlocked = 1;
        int xp = 0, gems = 0, elo = 1000;
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            unsigned int uval = 0; int val = 0; char strVal[64] = "";
            if (sscanf(line, " \"password\": \"%15[^\"]\"", strVal) == 1) strncpy(savedPass, strVal, MAX_PASSWORD_LEN - 1);
            else if (sscanf(line, " \"plane_class\": %u", &uval) == 1) cls = (uint8_t)uval;
            else if (sscanf(line, " \"skin_id\": %u", &uval) == 1) skin = (uint8_t)uval;
            else if (sscanf(line, " \"unlocked_skins\": %u", &uval) == 1) unlocked = (uint8_t)(uval | 1u);
            else if (sscanf(line, " \"xp\": %d", &val) == 1) xp = val;
            else if (sscanf(line, " \"gems\": %d", &val) == 1) gems = val;
            else if (sscanf(line, " \"elo\": %d", &val) == 1) elo = val;
        }
        fclose(f);

        uint8_t team = (i < 5) ? 0 : 1;
        int gainedXp   = 50 + (int)room->team_buildings_destroyed[team] * 15 + ((winner == (int)team) ? 100 : 0);
        int gainedGems = (winner == (int)team) ? 25 : 10;
        int deltaElo   = (winner == -1) ? 5 : ((winner == (int)team) ? 25 : -15);
        int newElo     = (elo + deltaElo < 100) ? 100 : (elo + deltaElo);

        SaveAccountJSON(cleanUser, savedPass, cls, skin, unlocked, xp + gainedXp, gems + gainedGems, newElo);
        printf("[Ranked JSON] %s : +%d XP, +%d Gems, ELO %d -> %d (%s)\n",
               cleanUser, gainedXp, gainedGems, elo, newElo, GetRankName(newElo));
    }
}