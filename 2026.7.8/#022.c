#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_USERNAME 20
#define MAX_PASSWORD 20
#define BOARD_SIZE 8
#define MAX_MOVES 200
#define XOR_KEY 0x2A

typedef struct
{
    char board[BOARD_SIZE][BOARD_SIZE];
    int whiteToMove;
    int whiteCastleKingSide, whiteCastleQueenSide;
    int blackCastleKingSide, blackCastleQueenSide;
    int enPassantValid;
    int enPassantRow, enPassantCol;
    int moveCount;
    char currentGameMoves[MAX_MOVES][10];
    char uid[64];        // 当前对局的唯一标识
} GameState;

typedef struct
{
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char uid[64];
} User;

typedef struct
{
    char moves[MAX_MOVES][10];
    int moveCount;
    char uid[64];        // 对局UID
    int finished;        // 1:已完成(有[GAME_END]), 0:未完成
    char result[50];     // 结果文本
} UnfinishedGame;        // 这里实际存储“未完成或已完成的对局信息”，改名但保持原结构名

GameState game;
int viewMode = 1;          // 1:白下 2:黑下
User currentUser = {0};

// Function prototypes
void initBoard(void);
void printBoard(void);
int parseMoveString(const char *moveStr, char *src, char *dst, char *promote);
int parseSquare(const char *s, int *r, int *c);
int executeMove(int sr, int sc, int dr, int dc, int white, int silentPromo);
int loadGameFromMoves(char moves[][10], int moveCnt);
void continueGame(void);
void manageUnfinishedGames(void);
void uidToTimeString(const char *uid, char *out, size_t size);

    // ---------- 加密 ----------
    void encryptToHex(const char *plain, char *hexOut)
{
    int len = strlen(plain);
    for (int i = 0; i < len; i++)
    {
        unsigned char c = plain[i] ^ XOR_KEY;
        sprintf(hexOut + 2 * i, "%02X", c);
    }
    hexOut[2 * len] = '\0';
}

void decryptFromHex(const char *hex, char *plainOut)
{
    int len = strlen(hex) / 2;
    for (int i = 0; i < len; i++)
    {
        unsigned int byte;
        sscanf(hex + 2 * i, "%02X", &byte);
        plainOut[i] = byte ^ XOR_KEY;
    }
    plainOut[len] = '\0';
}

// ---------- 辅助 ----------
int isWhite(char p) { return p >= 'A' && p <= 'Z'; }
int isBlack(char p) { return p >= 'a' && p <= 'z'; }
int onBoard(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
void trimNL(char *s) { s[strcspn(s, "\n")] = 0; }

void getUserFilePath(const char *prefix, char *out, size_t size)
{
    const char *id = currentUser.uid[0] ? currentUser.uid : currentUser.username;
    snprintf(out, size, "%s_%s.txt", prefix, id);
}

FILE *openUserFile(const char *prefix, const char *mode)
{
    char name[64];
    getUserFilePath(prefix, name, sizeof(name));
    return fopen(name, mode);
}

// 生成唯一ID：时间戳_自增序号
void generate_uid(char *buf, size_t size)
{
    static int counter = 0;
    time_t t = time(NULL);
    if (counter > 9999) counter = 0;
    snprintf(buf, size, "%ld_%d", (long)t, counter++);
}

// 完成对局并更新统计（不写games文件，只更新stats，因为调用时games已经写好了结果标记）
void finishGameStatsOnly(const char *result, int win, int loss, int draw, int quit)
{
    char fname[64];
    getUserFilePath("stats", fname, sizeof(fname));
    int ow = 0, ol = 0, od = 0, oq = 0;
    FILE *f = fopen(fname, "r");
    if (f)
    {
        fscanf(f, "赢:%d 输:%d 平:%d 中途退出:%d", &ow, &ol, &od, &oq);
        fclose(f);
    }
    ow += win;
    ol += loss;
    od += draw;
    oq += quit;
    f = fopen(fname, "w");
    if (f)
    {
        fprintf(f, "赢:%d 输:%d 平:%d 中途退出:%d\n", ow, ol, od, oq);
        fclose(f);
    }
}

// 原finishGame改为只更新stats，不再写games（因为games中的结果标记由调用者负责）
void finishGame(const char *result, int win, int loss, int draw, int quit)
{
    finishGameStatsOnly(result, win, loss, draw, quit);
}

// ---------- 用户管理 ----------
int userExists(const char *name)
{
    FILE *fp = fopen("users.txt", "r");
    if (!fp) return 0;
    char line[100], u[20];
    while (fgets(line, sizeof(line), fp))
    {
        sscanf(line, "%s", u);
        if (strcmp(u, name) == 0)
        {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

int validUsername(const char *name)
{
    for (int i = 0; name[i]; i++)
        if (!isalnum(name[i]))
            return 0;
    return 1;
}

void registerUser()
{
    char u[20], p1[20], p2[20];
    printf("\n===== 注册 =====\n用户名: ");
    fgets(u, 20, stdin);
    trimNL(u);
    if (!validUsername(u))
    {
        printf("用户名只能包含字母或数字！\n");
        return;
    }
    if (userExists(u))
    {
        printf("用户名已存在！\n");
        return;
    }
    printf("密码: ");
    fgets(p1, 20, stdin);
    trimNL(p1);
    printf("确认密码: ");
    fgets(p2, 20, stdin);
    trimNL(p2);
    if (strcmp(p1, p2))
    {
        printf("密码不一致！\n");
        return;
    }
    char uid[64];
    generate_uid(uid, sizeof(uid));
    FILE *fp = fopen("users.txt", "a");
    if (fp)
    {
        char hex[2 * MAX_PASSWORD + 1];
        encryptToHex(p1, hex);
        fprintf(fp, "%s %s %s\n", u, hex, uid);
        fclose(fp);
        printf("注册成功！UID=%s\n", uid);
    }
    else
        printf("无法保存用户！\n");
}

int login()
{
    char u[20], p[20];
    printf("\n===== 登录 =====\n用户名: ");
    fgets(u, 20, stdin);
    trimNL(u);
    printf("密码: ");
    fgets(p, 20, stdin);
    trimNL(p);
    FILE *fp = fopen("users.txt", "r");
    if (!fp)
    {
        printf("用户文件不存在，请先注册。\n");
        return 0;
    }
    char line[200], un[20], pw[20], uid[64];
    while (fgets(line, sizeof(line), fp))
    {
        int tokens = sscanf(line, "%19s %19s %63s", un, pw, uid);
        char dec[MAX_PASSWORD];
        decryptFromHex(pw, dec);
        if (strcmp(un, u) == 0 && strcmp(dec, p) == 0)
        {
            strcpy(currentUser.username, u);
            strcpy(currentUser.password, p);
            currentUser.uid[0] = '\0';
            if (tokens == 3)
                strcpy(currentUser.uid, uid);
            fclose(fp);
            printf("登录成功！欢迎 %s\n", u);
            return 1;
        }
    }
    fclose(fp);
    printf("用户名或密码错误！\n");
    return 0;
}

void changePassword()
{
    if (!currentUser.username[0])
    {
        printf("请先登录！\n");
        return;
    }
    char old[20], new1[20], new2[20];
    printf("\n===== 修改密码 =====\n原密码: ");
    fgets(old, 20, stdin);
    trimNL(old);
    if (strcmp(old, currentUser.password))
    {
        printf("原密码错误！\n");
        return;
    }
    printf("新密码: ");
    fgets(new1, 20, stdin);
    trimNL(new1);
    printf("确认新密码: ");
    fgets(new2, 20, stdin);
    trimNL(new2);
    if (strcmp(new1, new2))
    {
        printf("两次新密码不一致！\n");
        return;
    }
    FILE *fp = fopen("users.txt", "r"), *tmp = fopen("users.tmp", "w");
    if (!fp || !tmp)
    {
        printf("系统错误！\n");
        if (fp) fclose(fp);
        if (tmp) fclose(tmp);
        return;
    }
    char line[200], un[20], pw[20], uid[64];
    int updated = 0;
    while (fgets(line, sizeof(line), fp))
    {
        int tokens = sscanf(line, "%19s %19s %63s", un, pw, uid);
        if (strcmp(un, currentUser.username) == 0)
        {
            char hex[2 * MAX_PASSWORD + 1];
            encryptToHex(new1, hex);
            if (currentUser.uid[0])
                fprintf(tmp, "%s %s %s\n", currentUser.username, hex, currentUser.uid);
            else if (tokens == 3)
                fprintf(tmp, "%s %s %s\n", currentUser.username, hex, uid);
            else
                fprintf(tmp, "%s %s\n", currentUser.username, hex);
            strcpy(currentUser.password, new1);
            updated = 1;
        }
        else
            fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove("users.txt");
    rename("users.tmp", "users.txt");
    printf(updated ? "密码修改成功！\n" : "修改失败！\n");
}

void logout()
{
    if (!currentUser.username[0])
        printf("尚未登录。\n");
    else
    {
        printf("用户 %s 已退出。\n", currentUser.username);
        currentUser.username[0] = 0;
        currentUser.uid[0] = 0;
    }
}

void showStats()
{
    if (!currentUser.username[0])
    {
        printf("请先登录！\n");
        return;
    }
    char fname[64];
    getUserFilePath("stats", fname, sizeof(fname));
    FILE *fp = fopen(fname, "r");
    if (!fp)
    {
        printf("暂无战绩记录。\n");
        return;
    }
    char line[100];
    if (fgets(line, sizeof(line), fp))
        printf("\n=== %s 的战绩 ===\n%s", currentUser.username, line);
    else
        printf("暂无战绩。\n");
    fclose(fp);
}

// ---------- 棋谱与未完成对局 ----------
void saveCurrentMove(const char *moveStr, int stepNo)
{
    FILE *fp = openUserFile("games", "a");
    if (!fp) return;
    // 注意：开始标记已在newGame中写入，这里不再写
    fprintf(fp, "%d. %s\n", stepNo, moveStr);
    fclose(fp);
}

// 扫描所有对局（包括已完成和未完成），返回数量
int scanAllGames(UnfinishedGame *games, int maxGames)
{
    FILE *fp = openUserFile("games", "r");
    if (!fp) return 0;
    char line[256];
    int inGame = 0, count = 0, curMoveCnt = 0, finished = 0;
    char curMoves[MAX_MOVES][10];
    char curUid[64] = "";
    char curResult[50] = "";
    while (fgets(line, sizeof(line), fp))
    {
        trimNL(line);
        if (strncmp(line, "[GAME_START", 11) == 0)
        {
            if ((inGame || finished) && curUid[0] && count < maxGames)
            {
                UnfinishedGame *ug = &games[count++];
                ug->moveCount = curMoveCnt;
                for (int i = 0; i < curMoveCnt; i++)
                    strcpy(ug->moves[i], curMoves[i]);
                strcpy(ug->uid, curUid);
                ug->finished = finished;
                strcpy(ug->result, curResult);
            }
            inGame = 1;
            curMoveCnt = 0;
            finished = 0;
            curResult[0] = 0;
            // 提取UID
            char *p = strstr(line, "UID=");
            if (p)
            {
                p += 4;
                int i = 0;
                while (*p && !isspace(*p) && i < 63)
                    curUid[i++] = *p++;
                curUid[i] = 0;
            }
            else
            {
                curUid[0] = 0; // 旧格式无UID
            }
        }
        else if (strstr(line, "[GAME_END]") && (inGame || curUid[0]))
        {
            finished = 1;
            if (curUid[0] && count < maxGames)
            {
                UnfinishedGame *ug = &games[count++];
                ug->moveCount = curMoveCnt;
                for (int i = 0; i < curMoveCnt; i++)
                    strcpy(ug->moves[i], curMoves[i]);
                strcpy(ug->uid, curUid);
                ug->finished = finished;
                strcpy(ug->result, curResult);
            }
            inGame = 0;
            curUid[0] = 0;
            curResult[0] = 0;
            curMoveCnt = 0;
            finished = 0;
        }
        else if (strstr(line, "[RESULT]") && inGame)
        {
            sscanf(line, "[RESULT] %[^\n]", curResult);
        }
        else if (inGame && line[0] >= '0' && line[0] <= '9')
        {
            char move[10];
            if (sscanf(line, "%*d. %s", move) == 1 && curMoveCnt < MAX_MOVES)
                strcpy(curMoves[curMoveCnt++], move);
        }
    }
    if (inGame && curUid[0] && count < maxGames)
    {
        UnfinishedGame *ug = &games[count++];
        ug->moveCount = curMoveCnt;
        for (int i = 0; i < curMoveCnt; i++)
            strcpy(ug->moves[i], curMoves[i]);
        strcpy(ug->uid, curUid);
        ug->finished = 0;
        strcpy(ug->result, "");
    }
    fclose(fp);
    return count;
}

// 通过UID删除对局（物理删除，并更新战绩为退出）
void removeGameByUID(const char *uid, int moveCount)
{
    char filename[64];
    getUserFilePath("games", filename, sizeof(filename));
    FILE *fp = fopen(filename, "r");
    if (!fp) return;
    FILE *tmp = fopen("games.tmp", "w");
    if (!tmp)
    {
        fclose(fp);
        return;
    }

    char line[256];
    int skip = 0;
    int found = 0;
    while (fgets(line, sizeof(line), fp))
    {
        if (!skip && strncmp(line, "[GAME_START", 11) == 0)
        {
            char tmpUid[64] = "";
            char *p = strstr(line, "UID=");
            if (p)
            {
                p += 4;
                int i = 0;
                while (*p && !isspace(*p) && i < 63)
                    tmpUid[i++] = *p++;
                tmpUid[i] = 0;
            }
            if (strcmp(tmpUid, uid) == 0)
            {
                skip = 1; // 跳过这个对局
                found = 1;
                continue;
            }
        }
        if (!skip)
            fputs(line, tmp);
        if (skip && strstr(line, "[GAME_END]"))
            skip = 0;
    }
    fclose(fp);
    fclose(tmp);
    remove(filename);
    rename("games.tmp", filename);

    // 更新战绩：中途退出
    if (found)
    {
        int quitSide = (moveCount % 2 == 0) ? 0 : 1; // 0:白退出,1:黑退出，但只记录退出次数
        finishGameStatsOnly("中途退出", 0, 0, 0, 1);
        printf("已删除该对局，并记录为中途退出。\n");
    }
}

// 扫描未完成对局（兼容旧接口，内部调用scanAllGames过滤finished==0）
int scanUnfinishedGames(UnfinishedGame *games)
{
    UnfinishedGame all[10];
    int cnt = scanAllGames(all, 10);
    int j = 0;
    for (int i = 0; i < cnt; i++)
    {
        if (!all[i].finished)
        {
            games[j++] = all[i];
        }
    }
    return j;
}

void removeUnfinishedGameAtIndex(int index)
{
    UnfinishedGame games[10];
    int cnt = scanUnfinishedGames(games);
    if (index < 0 || index >= cnt) return;
    // 通过UID删除
    removeGameByUID(games[index].uid, games[index].moveCount);
}

void checkAndResumeUnfinishedGame()
{
    UnfinishedGame games[10];
    int cnt = scanUnfinishedGames(games);
    if (cnt == 0) return;
    if (cnt > 3)
    {
        printf("发现 %d 个未完成对局,保留最近3个,其余自动标记为退出。\n", cnt);
        while (cnt > 3)
        {
            removeUnfinishedGameAtIndex(0);
            cnt = scanUnfinishedGames(games);
        }
    }
    cnt = scanUnfinishedGames(games);
    if (cnt == 0) return;
    printf("\n========== 恢复系统 ==========\n发现 %d 个未完成对局：\n", cnt);
    for (int i = 0; i < cnt; i++)
        printf("%d. 已走 %d 步\n", i + 1, games[i].moveCount);
    printf("选择对局 (1~%d) 或 0 退出: ", cnt);
    char buf[10];
    fgets(buf, 10, stdin);
    int ch = atoi(buf);
    if (ch < 1 || ch > cnt)
    {
        printf("退出恢复。\n");
        return;
    }
    UnfinishedGame *ug = &games[ch - 1];
    printf("1-继续  2-放弃并记录退出  3-忽略 : ");
    fgets(buf, 10, stdin);
    int act = atoi(buf);
    if (act == 1)
    {
        if (loadGameFromMoves(ug->moves, ug->moveCount))
        {
            game.moveCount = ug->moveCount;
            for (int i = 0; i < game.moveCount; i++)
                strcpy(game.currentGameMoves[i], ug->moves[i]);
            game.whiteToMove = (game.moveCount % 2 == 0);
            strcpy(game.uid, ug->uid);
            printf("恢复成功！继续游戏...\n");
            continueGame();
        }
        else
            printf("恢复失败。\n");
    }
    else if (act == 2)
    {
        removeUnfinishedGameAtIndex(ch - 1);
    }
    else
        printf("忽略该对局。\n");
}

int canStartNewGame(void)
{
    UnfinishedGame games[10];
    int cnt;
    while (1)
    {
        cnt = scanUnfinishedGames(games);
        if (cnt < 3)
            return 1;
        printf("\n⚠️ 已有 %d 个未完成对局（最多保留3个）。必须先处理它们才能开始新游戏。\n", cnt);
        printf("请选择：\n  1 - 查看并处理未完成对局\n  2 - 放弃开始新游戏（返回主菜单）\n请输入：");
        char buf[10];
        fgets(buf, 10, stdin);
        int opt = atoi(buf);
        if (opt == 1)
        {
            manageUnfinishedGames();
        }
        else
        {
            printf("已取消开始新游戏。\n");
            return 0;
        }
    }
}

void manageUnfinishedGames()
{
    UnfinishedGame games[10];
    int cnt = scanUnfinishedGames(games);
    if (cnt == 0)
    {
        printf("没有未完成对局。\n");
        return;
    }
    while (1)
    {
        printf("\n未完成对局列表：\n");
        for (int i = 0; i < cnt; i++)
            printf("%d. 步数 %d\n", i + 1, games[i].moveCount);
        printf("选择 (1~%d) 或 0 返回: ", cnt);
        char buf[10];
        fgets(buf, 10, stdin);
        int ch = atoi(buf);
        if (ch == 0) break;
        if (ch < 1 || ch > cnt)
        {
            printf("无效输入。\n");
            continue;
        }
        UnfinishedGame *ug = &games[ch - 1];
        printf("1-继续  2-放弃并记录退出  3-忽略 : ");
        fgets(buf, 10, stdin);
        int act = atoi(buf);
        if (act == 1)
        {
            if (loadGameFromMoves(ug->moves, ug->moveCount))
            {
                game.moveCount = ug->moveCount;
                for (int i = 0; i < game.moveCount; i++)
                    strcpy(game.currentGameMoves[i], ug->moves[i]);
                game.whiteToMove = (game.moveCount % 2 == 0);
                strcpy(game.uid, ug->uid);
                continueGame();
                printf("游戏结束，返回主菜单。\n");
                return;
            }
            else
                printf("恢复失败。\n");
        }
        else if (act == 2)
        {
            removeUnfinishedGameAtIndex(ch - 1);
            cnt = scanUnfinishedGames(games);
            if (cnt == 0)
            {
                printf("没有未完成对局了。\n");
                return;
            }
        }
        else
        {
            printf("忽略。\n");
            break;
        }
    }
}

void listGames()
{
    UnfinishedGame all[20];
    int cnt = scanAllGames(all, 20);
    if (cnt == 0)
    {
        printf("无对局记录。\n");
        return;
    }
    printf("\n=== 所有对局 ===\n");
    for (int i = 0; i < cnt; i++)
    {
        char timeStr[64];
        uidToTimeString(all[i].uid, timeStr, sizeof(timeStr));

        printf("%2d.%s\n", i + 1, timeStr);

        if (all[i].finished)
            printf("   %s\n", all[i].result);
        else
            printf("   未完成\n");

        printf("   %d步\n", all[i].moveCount);

        printf("----------------------\n");
    }
}

void uidToTimeString(const char *uid, char *out, size_t size)
{
    long timestamp = 0;
    sscanf(uid, "%ld", &timestamp);

    time_t t = (time_t)timestamp;
    struct tm *tm_info = localtime(&t);

    strftime(out, size, "%Y-%m-%d %H:%M", tm_info);
}

void replayGame(int idx)
{
    UnfinishedGame all[20];
    int cnt = scanAllGames(all, 20);
    if (idx < 1 || idx > cnt)
    {
        printf("序号无效。\n");
        return;
    }
    UnfinishedGame *ug = &all[idx-1];
    if (!ug->finished)
    {
        printf("该对局尚未结束，不能回放。\n");
        return;
    }
    printf("\n回放第%d局 (结果: %s)\n按回车下一步，q退出\n", idx, ug->result);
    GameState saved = game;
    initBoard();
    int moveCnt = ug->moveCount;
    for (int i = 0; i < moveCnt; i++)
    {
        char src[8], dst[8], promo;
        parseMoveString(ug->moves[i], src, dst, &promo);
        int sr, sc, dr, dc;
        parseSquare(src, &sr, &sc);
        parseSquare(dst, &dr, &dc);
        executeMove(sr, sc, dr, dc, (i % 2 == 0), 1);
        if (promo)
            game.board[dr][dc] = promo;
        printBoard();
        printf("第%d步: %s\n", i+1, ug->moves[i]);
        if (i == moveCnt-1) break;
        char inp[10];
        fgets(inp, 10, stdin);
        if (inp[0] == 'q' || inp[0] == 'Q')
        {
            printf("中断回放。\n");
            break;
        }
    }
    game = saved;
}

// ---------- 棋盘与移动规则 ----------
void initBoard()
{
    const char *start[8] = {"rnbqkbnr", "pppppppp", "........", "........", "........", "........", "PPPPPPPP", "RNBQKBNR"};
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            game.board[r][c] = start[r][c];
    game.whiteToMove = 1;
    game.whiteCastleKingSide = game.whiteCastleQueenSide = 1;
    game.blackCastleKingSide = game.blackCastleQueenSide = 1;
    game.enPassantValid = 0;
    game.enPassantRow = game.enPassantCol = -1;
    game.moveCount = 0;
    game.uid[0] = 0;
}

int parseSquare(const char *s, int *r, int *c)
{
    if (strlen(s) < 2) return 0;
    char f = s[0], rank = s[1];
    if (f < 'a' || f > 'h' || rank < '1' || rank > '8') return 0;
    *c = f - 'a';
    *r = 8 - (rank - '0');
    return 1;
}

int isFriendly(char p, int white) { return p != '.' && (white ? isWhite(p) : isBlack(p)); }
int isEnemy(char p, int white) { return p != '.' && (white ? isBlack(p) : isWhite(p)); }

int clearPath(int sr, int sc, int dr, int dc)
{
    int drs = (dr > sr) ? 1 : (dr < sr) ? -1 : 0;
    int dcs = (dc > sc) ? 1 : (dc < sc) ? -1 : 0;
    int r = sr + drs, c = sc + dcs;
    while (r != dr || c != dc)
    {
        if (game.board[r][c] != '.') return 0;
        r += drs;
        c += dcs;
    }
    return 1;
}

int canPawnAttack(int sr, int sc, int dr, int dc, int white)
{
    int dir = white ? -1 : 1;
    return dr == sr + dir && abs(dc - sc) == 1;
}

int canMovePawn(int sr, int sc, int dr, int dc, int white)
{
    int dir = white ? -1 : 1;
    char t = game.board[dr][dc];
    int startRow = white ? 6 : 1;
    if (sc == dc && t == '.' && dr == sr + dir) return 1;
    if (sc == dc && t == '.' && sr == startRow && dr == sr + 2 * dir && game.board[sr + dir][sc] == '.') return 1;
    if (abs(dc - sc) == 1 && dr == sr + dir && isEnemy(t, white)) return 1;
    if (abs(dc - sc) == 1 && dr == sr + dir && t == '.' &&
        game.enPassantValid && dr == game.enPassantRow && dc == game.enPassantCol) return 1;
    return 0;
}

int canMoveKnight(int sr, int sc, int dr, int dc)
{
    int drs = abs(dr - sr), dcs = abs(dc - sc);
    return (drs == 2 && dcs == 1) || (drs == 1 && dcs == 2);
}

int canMoveBishop(int sr, int sc, int dr, int dc)
{
    if (abs(dr - sr) != abs(dc - sc)) return 0;
    return clearPath(sr, sc, dr, dc);
}

int canMoveRook(int sr, int sc, int dr, int dc)
{
    if (sr != dr && sc != dc) return 0;
    return clearPath(sr, sc, dr, dc);
}

int canMoveQueen(int sr, int sc, int dr, int dc)
{
    return canMoveBishop(sr, sc, dr, dc) || canMoveRook(sr, sc, dr, dc);
}

int squareAttacked(int row, int col, int byWhite);
int canKingAttack(int sr, int sc, int dr, int dc);
int isKingInCheck(int white);

int canCastle(int sr, int sc, int dr, int dc, int white)
{
    if (isKingInCheck(white)) return 0;
    if (white)
    {
        if (sr != 7 || sc != 4) return 0;
        if (dr == 7 && dc == 6)
        {
            if (!game.whiteCastleKingSide) return 0;
            if (game.board[7][5] != '.' || game.board[7][6] != '.') return 0;
            if (squareAttacked(7, 5, 0) || squareAttacked(7, 6, 0)) return 0;
            return game.board[7][7] == 'R';
        }
        if (dr == 7 && dc == 2)
        {
            if (!game.whiteCastleQueenSide) return 0;
            if (game.board[7][3] != '.' || game.board[7][2] != '.' || game.board[7][1] != '.') return 0;
            if (squareAttacked(7, 3, 0) || squareAttacked(7, 2, 0)) return 0;
            return game.board[7][0] == 'R';
        }
    }
    else
    {
        if (sr != 0 || sc != 4) return 0;
        if (dr == 0 && dc == 6)
        {
            if (!game.blackCastleKingSide) return 0;
            if (game.board[0][5] != '.' || game.board[0][6] != '.') return 0;
            if (squareAttacked(0, 5, 1) || squareAttacked(0, 6, 1)) return 0;
            return game.board[0][7] == 'r';
        }
        if (dr == 0 && dc == 2)
        {
            if (!game.blackCastleQueenSide) return 0;
            if (game.board[0][3] != '.' || game.board[0][2] != '.' || game.board[0][1] != '.') return 0;
            if (squareAttacked(0, 3, 1) || squareAttacked(0, 2, 1)) return 0;
            return game.board[0][0] == 'r';
        }
    }
    return 0;
}

int canMoveKing(int sr, int sc, int dr, int dc)
{
    int drs = abs(dr - sr), dcs = abs(dc - sc);
    if (drs == 0 && dcs == 2)
        return canCastle(sr, sc, dr, dc, isWhite(game.board[sr][sc]));
    return drs <= 1 && dcs <= 1;
}

int canKingAttack(int sr, int sc, int dr, int dc)
{
    int drs = abs(dr - sr), dcs = abs(dc - sc);
    return drs <= 1 && dcs <= 1;
}

int squareAttacked(int row, int col, int byWhite)
{
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            char p = game.board[r][c];
            if (!isFriendly(p, byWhite)) continue;
            char low = tolower(p);
            int ok = 0;
            switch (low)
            {
            case 'p': ok = canPawnAttack(r, c, row, col, byWhite); break;
            case 'n': ok = canMoveKnight(r, c, row, col); break;
            case 'b': ok = canMoveBishop(r, c, row, col); break;
            case 'r': ok = canMoveRook(r, c, row, col); break;
            case 'q': ok = canMoveQueen(r, c, row, col); break;
            case 'k': ok = canKingAttack(r, c, row, col); break;
            }
            if (ok) return 1;
        }
    }
    return 0;
}

int isKingInCheck(int white)
{
    char king = white ? 'K' : 'k';
    int kr = -1, kc = -1;
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (game.board[r][c] == king)
            {
                kr = r;
                kc = c;
                break;
            }
        }
        if (kr >= 0) break;
    }
    if (kr < 0 || kc < 0) return 0;
    return squareAttacked(kr, kc, !white);
}

int canMovePiece(int sr, int sc, int dr, int dc, int white)
{
    if (!onBoard(sr, sc) || !onBoard(dr, dc)) return 0;
    char p = game.board[sr][sc];
    if (p == '.' || !isFriendly(p, white)) return 0;
    if (isFriendly(game.board[dr][dc], white)) return 0;
    char low = tolower(p);
    switch (low)
    {
    case 'p': return canMovePawn(sr, sc, dr, dc, white);
    case 'n': return canMoveKnight(sr, sc, dr, dc);
    case 'b': return canMoveBishop(sr, sc, dr, dc);
    case 'r': return canMoveRook(sr, sc, dr, dc);
    case 'q': return canMoveQueen(sr, sc, dr, dc);
    case 'k': return canMoveKing(sr, sc, dr, dc);
    default: return 0;
    }
}

int executeMove(int sr, int sc, int dr, int dc, int white, int silentPromo)
{
    if (!canMovePiece(sr, sc, dr, dc, white)) return 0;
    char piece = game.board[sr][sc];
    char target = game.board[dr][dc];
    GameState backup = game;

    if (tolower(piece) == 'k' && abs(dc - sc) == 2)
    {
        if (white)
        {
            if (dc == 6) { game.board[7][5] = 'R'; game.board[7][7] = '.'; }
            else if (dc == 2) { game.board[7][3] = 'R'; game.board[7][0] = '.'; }
            game.whiteCastleKingSide = game.whiteCastleQueenSide = 0;
        }
        else
        {
            if (dc == 6) { game.board[0][5] = 'r'; game.board[0][7] = '.'; }
            else if (dc == 2) { game.board[0][3] = 'r'; game.board[0][0] = '.'; }
            game.blackCastleKingSide = game.blackCastleQueenSide = 0;
        }
    }

    if (tolower(piece) == 'k')
    {
        if (white) game.whiteCastleKingSide = game.whiteCastleQueenSide = 0;
        else game.blackCastleKingSide = game.blackCastleQueenSide = 0;
    }
    if (tolower(piece) == 'r')
    {
        if (white)
        {
            if (sr == 7 && sc == 0) game.whiteCastleQueenSide = 0;
            if (sr == 7 && sc == 7) game.whiteCastleKingSide = 0;
        }
        else
        {
            if (sr == 0 && sc == 0) game.blackCastleQueenSide = 0;
            if (sr == 0 && sc == 7) game.blackCastleKingSide = 0;
        }
    }
    if (tolower(target) == 'r')
    {
        if (target == 'R')
        {
            if (dr == 7 && dc == 0) game.whiteCastleQueenSide = 0;
            if (dr == 7 && dc == 7) game.whiteCastleKingSide = 0;
        }
        else
        {
            if (dr == 0 && dc == 0) game.blackCastleQueenSide = 0;
            if (dr == 0 && dc == 7) game.blackCastleKingSide = 0;
        }
    }

    game.board[dr][dc] = piece;
    game.board[sr][sc] = '.';

    if (tolower(piece) == 'p' && dr == backup.enPassantRow && dc == backup.enPassantCol && backup.enPassantValid && target == '.')
    {
        int capRow = white ? dr + 1 : dr - 1;
        game.board[capRow][dc] = '.';
    }

    game.enPassantValid = 0;
    if (tolower(piece) == 'p' && abs(dr - sr) == 2)
    {
        game.enPassantValid = 1;
        game.enPassantRow = (sr + dr) / 2;
        game.enPassantCol = sc;
    }

    if (isKingInCheck(white))
    {
        game = backup;
        return 0;
    }

    if (tolower(piece) == 'p' && (dr == 0 || dr == 7))
    {
        char promo = silentPromo ? (white ? 'Q' : 'q') : 0;
        if (!silentPromo)
        {
            printf("兵升变！选择 Q(后) R(车) N(马) B(象): ");
            char inp[8];
            while (1)
            {
                fgets(inp, 8, stdin);
                if (inp[0] == '\n') continue;
                char c = toupper(inp[0]);
                if (c == 'Q' || c == 'R' || c == 'N' || c == 'B')
                {
                    promo = white ? c : tolower(c);
                    break;
                }
                printf("无效，请输入 Q,R,N,B : ");
            }
        }
        game.board[dr][dc] = promo;
    }
    return 1;
}

int hasLegalMove(int white)
{
    GameState bak = game;
    for (int sr = 0; sr < 8; sr++)
        for (int sc = 0; sc < 8; sc++)
            if (isFriendly(game.board[sr][sc], white))
                for (int dr = 0; dr < 8; dr++)
                    for (int dc = 0; dc < 8; dc++)
                        if (canMovePiece(sr, sc, dr, dc, white))
                        {
                            if (executeMove(sr, sc, dr, dc, white, 1))
                            {
                                game = bak;
                                return 1;
                            }
                            game = bak;
                        }
    return 0;
}

void printBoard()
{
    if (viewMode == 1)
        printf("    a   b   c   d   e   f   g   h\n  +---+---+---+---+---+---+---+---+\n");
    else
        printf("    h   g   f   e   d   c   b   a\n  +---+---+---+---+---+---+---+---+\n");
    int order[8];
    for (int i = 0; i < 8; i++) order[i] = (viewMode == 1) ? i : 7 - i;
    for (int i = 0; i < 8; i++)
    {
        int r = order[i];
        printf("%d |", (viewMode == 1) ? 8 - r : i + 1);
        for (int c = 0; c < 8; c++)
        {
            int cc = (viewMode == 1) ? c : 7 - c;
            printf(" %c |", game.board[r][cc]);
        }
        printf(" %d\n  +---+---+---+---+---+---+---+---+\n", (viewMode == 1) ? 8 - r : i + 1);
    }
    if (viewMode == 1)
        printf("    a   b   c   d   e   f   g   h\n");
    else
        printf("    h   g   f   e   d   c   b   a\n");
}

void printInstructions()
{
    printf("国际象棋规则：王车易位、吃过路兵、兵升变。\n输入格式: e2 e4 或 e7e8Q, quit退出。\n\n");
}

void toggleViewMode()
{
    printf("当前视角: %s\n", viewMode == 1 ? "白在下" : "黑在下");
    printf("1-白在下 2-黑在下: ");
    char buf[10];
    fgets(buf, 10, stdin);
    int v = atoi(buf);
    if (v == 1 || v == 2) viewMode = v;
    else printf("无效，保持原样。\n");
}

// ---------- 棋谱解析与加载 ----------
int parseMoveString(const char *moveStr, char *src, char *dst, char *promote)
{
    if (!moveStr) return 0;
    int len = strlen(moveStr);
    if (len < 4 || len > 5) return 0;
    src[0] = moveStr[0]; src[1] = moveStr[1]; src[2] = '\0';
    dst[0] = moveStr[2]; dst[1] = moveStr[3]; dst[2] = '\0';
    *promote = (len == 5) ? moveStr[4] : 0;
    return 1;
}

int loadGameFromMoves(char moves[][10], int moveCnt)
{
    initBoard();
    game.whiteCastleKingSide = game.whiteCastleQueenSide = 1;
    game.blackCastleKingSide = game.blackCastleQueenSide = 1;
    game.enPassantValid = 0;
    game.whiteToMove = 1;
    for (int i = 0; i < moveCnt; i++)
    {
        char src[8], dst[8], promo;
        if (!parseMoveString(moves[i], src, dst, &promo)) return 0;
        int sr, sc, dr, dc;
        if (!parseSquare(src, &sr, &sc) || !parseSquare(dst, &dr, &dc)) return 0;
        if (game.board[sr][sc] == '.') return 0;
        if (!executeMove(sr, sc, dr, dc, (i % 2 == 0), 1)) return 0;
        if (promo) game.board[dr][dc] = promo;
    }
    return 1;
}

void continueGame()
{
    char line[64];
    while (1)
    {
        printBoard();
        printf("%s 走棋: ", game.whiteToMove ? "白方" : "黑方");
        if (!fgets(line, 64, stdin)) break;
        trimNL(line);
        char src[8] = "", dst[8] = "";
        char promoChar = 0;
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
        {
            const char *res = game.whiteToMove ? "白方中途退出" : "黑方中途退出";
            printf("游戏结束（%s）\n", res);
            if (game.moveCount)
            {
                FILE *fp = openUserFile("games", "a");
                if (fp)
                {
                    fprintf(fp, "[RESULT] %s\n[GAME_END]\n", res);
                    fclose(fp);
                }
                finishGame(res, 0, 0, 0, 1);
            }
            return;
        }
        char token1[16] = "", token2[16] = "";
        int tokenCount = sscanf(line, "%15s %15s", token1, token2);
        if (tokenCount == 1)
        {
            int len = strlen(token1);
            if (len == 4 || len == 5)
            {
                src[0] = token1[0]; src[1] = token1[1]; src[2] = '\0';
                dst[0] = token1[2]; dst[1] = token1[3]; dst[2] = '\0';
                if (len == 5)
                    promoChar = token1[4];
            }
            else
            {
                printf("坐标无效。\n");
                continue;
            }
        }
        else if (tokenCount == 2)
        {
            if (strlen(token1) != 2 || (strlen(token2) != 2 && strlen(token2) != 3))
            {
                printf("坐标无效。\n");
                continue;
            }
            strcpy(src, token1);
            dst[0] = token2[0]; dst[1] = token2[1]; dst[2] = '\0';
            if (strlen(token2) == 3)
                promoChar = token2[2];
        }
        else
        {
            continue;
        }
        if (promoChar)
        {
            char c = toupper(promoChar);
            if (c != 'Q' && c != 'R' && c != 'N' && c != 'B')
            {
                printf("升变棋子无效。请选择 Q/R/N/B。\n");
                continue;
            }
            promoChar = c;
        }
        int rs, cs, rd, cd;
        if (!parseSquare(src, &rs, &cs) || !parseSquare(dst, &rd, &cd))
        {
            printf("坐标无效。\n");
            continue;
        }
        int sr = rs, sc = cs, dr = rd, dc = cd;
        if (!executeMove(sr, sc, dr, dc, game.whiteToMove, promoChar ? 1 : 0))
        {
            printf("非法移动！\n");
            continue;
        }
        if (promoChar && tolower(game.board[dr][dc]) == 'q')
        {
            game.board[dr][dc] = game.whiteToMove ? promoChar : tolower(promoChar);
        }
        char moveStr[10];
        sprintf(moveStr, "%c%c%c%c", 'a' + cs, '8' - rs, 'a' + cd, '8' - rd);
        char nowPiece = game.board[dr][dc];
        if (tolower(nowPiece) != 'p' && (dr == 0 || dr == 7))
        {
            char tmp[10];
            sprintf(tmp, "%s%c", moveStr, nowPiece);
            strcpy(moveStr, tmp);
        }
        saveCurrentMove(moveStr, game.moveCount + 1);
        if (game.moveCount < MAX_MOVES)
            strcpy(game.currentGameMoves[game.moveCount], moveStr);
        game.moveCount++;

        if (isKingInCheck(!game.whiteToMove))
        {
            if (!hasLegalMove(!game.whiteToMove))
            {
                printBoard();
                printf("将死！%s 胜利。\n", game.whiteToMove ? "白方" : "黑方");
                const char *res = game.whiteToMove ? "白方赢" : "黑方赢";
                FILE *fp = openUserFile("games", "a");
                if (fp)
                {
                    fprintf(fp, "[RESULT] %s\n[GAME_END]\n", res);
                    fclose(fp);
                }
                if (game.whiteToMove)
                    finishGame(res, 1, 0, 0, 0);
                else
                    finishGame(res, 0, 1, 0, 0);
                return;
            }
            printf("将军！\n");
        }
        else if (!hasLegalMove(!game.whiteToMove))
        {
            printBoard();
            printf("僵局，平局。\n");
            FILE *fp = openUserFile("games", "a");
            if (fp)
            {
                fprintf(fp, "[RESULT] 平局\n[GAME_END]\n");
                fclose(fp);
            }
            finishGame("平局", 0, 0, 1, 0);
            return;
        }
        game.whiteToMove = !game.whiteToMove;
    }
}

void newGame()
{
    if (!canStartNewGame()) return;
    initBoard();
    game.moveCount = 0;
    generate_uid(game.uid, sizeof(game.uid));
    printf("\n新游戏开始！UID=%s\n", game.uid);
    printInstructions();
    FILE *fp = openUserFile("games", "a");
    if (fp)
    {
        fprintf(fp, "[GAME_START UID=%s]\n", game.uid);
        fclose(fp);
    }
    continueGame();
}

int main()
{
    printf("===== 国际象棋（账户+恢复+翻转+密码加密）=====\n");
    char line[64];
    while (1)
    {
        printf("\n");
        if (!currentUser.username[0])
        {
            printf("=== 未登录 ===\n1.登录 2.注册 3.退出\n选择: ");
        }
        else
        {
            printf("=== 用户: %s ===\n1.改密码 2.退出登录 3.新游戏 4.规则 5.翻转模式 6.战绩 7.历史对局 8.未完成对局 9.退出\n选择: ", currentUser.username);
        }
        if (!fgets(line, 64, stdin)) break;
        int ch = atoi(line);
        if (!currentUser.username[0])
        {
            if (ch == 1)
            {
                if (login())
                    checkAndResumeUnfinishedGame();
            }
            else if (ch == 2)
                registerUser();
            else if (ch == 3)
            {
                printf("再见！\n");
                return 0;
            }
            else
                printf("无效选择。\n");
        }
        else
        {
            switch (ch)
            {
            case 1: changePassword(); break;
            case 2: logout(); break;
            case 3: newGame(); break;
            case 4: printInstructions(); break;
            case 5: toggleViewMode(); break;
            case 6: showStats(); break;
            case 7:
                listGames();
                printf("输入回放序号 (0返回): ");
                {
                    char idxBuf[16];
                    if (fgets(idxBuf, sizeof(idxBuf), stdin))
                    {
                        int idx = atoi(idxBuf);
                        if (idx > 0)
                            replayGame(idx);
                    }
                }
                break;
            case 8: manageUnfinishedGames(); break;
            case 9:
                printf("再见！\n");
                return 0;
            default:
                printf("无效选择。\n");
            }
        }
    }

    return 0;
}
