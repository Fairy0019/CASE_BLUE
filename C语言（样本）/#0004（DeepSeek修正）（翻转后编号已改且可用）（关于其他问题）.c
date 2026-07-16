// 虽然棋盘翻转，但编号未变，翻转后让黑棋视角为白棋编号，黑棋用不了，但不影响游戏基本功能

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BOARD_SIZE 8

// 全局变量
int viewMode = 0;   // 0=自动 1=白方在下 2=黑方在下
char board[BOARD_SIZE][BOARD_SIZE];
int whiteToMove = 1;
int whiteCastleKingSide = 1;
int whiteCastleQueenSide = 1;
int blackCastleKingSide = 1;
int blackCastleQueenSide = 1;
int enPassantValid = 0;
int enPassantRow = -1;
int enPassantCol = -1;

// 辅助函数
int isWhite(char piece) { return piece >= 'A' && piece <= 'Z'; }
int isBlack(char piece) { return piece >= 'a' && piece <= 'z'; }
int onBoard(int r, int c) { return r>=0 && r<BOARD_SIZE && c>=0 && c<BOARD_SIZE; }

void initBoard() {
    const char *start[8] = {
        "rnbqkbnr", "pppppppp", "........", "........",
        "........", "........", "PPPPPPPP", "RNBQKBNR"
    };
    for (int r=0; r<BOARD_SIZE; ++r)
        for (int c=0; c<BOARD_SIZE; ++c)
            board[r][c] = start[r][c];
    whiteToMove = 1;
    whiteCastleKingSide = whiteCastleQueenSide = 1;
    blackCastleKingSide = blackCastleQueenSide = 1;
    enPassantValid = 0;
    enPassantRow = enPassantCol = -1;
}

int parseSquare(const char *token, int *row, int *col) {
    if (strlen(token) < 2) return 0;
    char file = token[0], rank = token[1];
    if (file<'a' || file>'h' || rank<'1' || rank>'8') return 0;
    *col = file - 'a';
    *row = 8 - (rank - '0');
    return 1;
}

int isFriendlyPiece(char piece, int white) {
    if (piece=='.') return 0;
    return white ? isWhite(piece) : isBlack(piece);
}
int isEnemyPiece(char piece, int white) {
    if (piece=='.') return 0;
    return white ? isBlack(piece) : isWhite(piece);
}

int clearPath(int sr, int sc, int dr, int dc) {
    int drStep = (dr>sr) ? 1 : (dr<sr) ? -1 : 0;
    int dcStep = (dc>sc) ? 1 : (dc<sc) ? -1 : 0;
    int r = sr+drStep, c = sc+dcStep;
    while (r!=dr || c!=dc) {
        if (board[r][c]!='.') return 0;
        r += drStep; c += dcStep;
    }
    return 1;
}

int canPawnAttack(int sr, int sc, int dr, int dc, int white) {
    int direction = white ? -1 : 1;
    return dr == sr+direction && abs(dc-sc)==1;
}

int canMovePawn(int sr, int sc, int dr, int dc, int white) {
    int direction = white ? -1 : 1;
    char target = board[dr][dc];
    int startRow = white ? 6 : 1;
    if (sc==dc && target=='.') {
        if (dr == sr+direction) return 1;
        if (sr==startRow && dr==sr+2*direction && board[sr+direction][sc]=='.') return 1;
    }
    if (abs(dc-sc)==1 && dr==sr+direction) {
        if (isEnemyPiece(target, white)) return 1;
        if (enPassantValid && dr==enPassantRow && dc==enPassantCol) return 1;
    }
    return 0;
}

int canMoveKnight(int sr, int sc, int dr, int dc) {
    int drAbs = abs(dr-sr), dcAbs = abs(dc-sc);
    return (drAbs==2 && dcAbs==1) || (drAbs==1 && dcAbs==2);
}

int canMoveBishop(int sr, int sc, int dr, int dc) {
    if (abs(dr-sr) != abs(dc-sc)) return 0;
    return clearPath(sr,sc,dr,dc);
}

int canMoveRook(int sr, int sc, int dr, int dc) {
    if (sr!=dr && sc!=dc) return 0;
    return clearPath(sr,sc,dr,dc);
}

int canMoveQueen(int sr, int sc, int dr, int dc) {
    return canMoveBishop(sr,sc,dr,dc) || canMoveRook(sr,sc,dr,dc);
}

// 前向声明（因为 squareAttacked 和 isKingInCheck 互相调用，但这里顺序已调好）
int squareAttacked(int row, int col, int byWhite);
int isKingInCheck(int white);

int canCastle(int sr, int sc, int dr, int dc, int white) {
    if (isKingInCheck(white)) return 0;
    if (white) {
        if (sr!=7 || sc!=4) return 0;
        if (dr==7 && dc==6) {
            if (!whiteCastleKingSide) return 0;
            if (board[7][5]!='.' || board[7][6]!='.') return 0;
            if (squareAttacked(7,5,0) || squareAttacked(7,6,0)) return 0;
            if (board[7][7]!='R') return 0;
            return 1;
        }
        if (dr==7 && dc==2) {
            if (!whiteCastleQueenSide) return 0;
            if (board[7][3]!='.' || board[7][2]!='.' || board[7][1]!='.') return 0;
            if (squareAttacked(7,3,0) || squareAttacked(7,2,0)) return 0;
            if (board[7][0]!='R') return 0;
            return 1;
        }
    } else {
        if (sr!=0 || sc!=4) return 0;
        if (dr==0 && dc==6) {
            if (!blackCastleKingSide) return 0;
            if (board[0][5]!='.' || board[0][6]!='.') return 0;
            if (squareAttacked(0,5,1) || squareAttacked(0,6,1)) return 0;
            if (board[0][7]!='r') return 0;
            return 1;
        }
        if (dr==0 && dc==2) {
            if (!blackCastleQueenSide) return 0;
            if (board[0][3]!='.' || board[0][2]!='.' || board[0][1]!='.') return 0;
            if (squareAttacked(0,3,1) || squareAttacked(0,2,1)) return 0;
            if (board[0][0]!='r') return 0;
            return 1;
        }
    }
    return 0;
}

int canMoveKing(int sr, int sc, int dr, int dc) {
    int drAbs = abs(dr-sr), dcAbs = abs(dc-sc);
    if (drAbs==0 && dcAbs==2)
        return canCastle(sr,sc,dr,dc, isWhite(board[sr][sc]));
    return drAbs<=1 && dcAbs<=1;
}

int squareAttacked(int row, int col, int byWhite) {
    for (int r=0; r<BOARD_SIZE; ++r) {
        for (int c=0; c<BOARD_SIZE; ++c) {
            char piece = board[r][c];
            if (!isFriendlyPiece(piece, byWhite)) continue;
            int can = 0;
            char lower = tolower(piece);
            switch (lower) {
                case 'p': can = canPawnAttack(r,c,row,col,byWhite); break;
                case 'n': can = canMoveKnight(r,c,row,col); break;
                case 'b': can = canMoveBishop(r,c,row,col); break;
                case 'r': can = canMoveRook(r,c,row,col); break;
                case 'q': can = canMoveQueen(r,c,row,col); break;
                case 'k': can = canMoveKing(r,c,row,col); break;
            }
            if (can) return 1;
        }
    }
    return 0;
}

int isKingInCheck(int white) {
    int kingRow=-1, kingCol=-1;
    char king = white ? 'K' : 'k';
    for (int r=0; r<BOARD_SIZE; ++r) {
        for (int c=0; c<BOARD_SIZE; ++c) {
            if (board[r][c]==king) { kingRow=r; kingCol=c; break; }
        }
        if (kingRow!=-1) break;
    }
    if (kingRow==-1) return 1;
    return squareAttacked(kingRow, kingCol, !white);
}

int canMovePiece(int sr, int sc, int dr, int dc, int white) {
    if (!onBoard(sr,sc) || !onBoard(dr,dc)) return 0;
    char piece = board[sr][sc];
    if (piece=='.' || !isFriendlyPiece(piece,white)) return 0;
    if (isFriendlyPiece(board[dr][dc], white)) return 0;
    char lower = tolower(piece);
    switch (lower) {
        case 'p': return canMovePawn(sr,sc,dr,dc,white);
        case 'n': return canMoveKnight(sr,sc,dr,dc);
        case 'b': return canMoveBishop(sr,sc,dr,dc);
        case 'r': return canMoveRook(sr,sc,dr,dc);
        case 'q': return canMoveQueen(sr,sc,dr,dc);
        case 'k': return canMoveKing(sr,sc,dr,dc);
        default: return 0;
    }
}

int executeMove(int sr, int sc, int dr, int dc, int white) {
    if (!canMovePiece(sr,sc,dr,dc,white)) return 0;
    char piece = board[sr][sc];
    char target = board[dr][dc];
    char backup[BOARD_SIZE][BOARD_SIZE];
    int bWCK=whiteCastleKingSide, bWCQ=whiteCastleQueenSide;
    int bBCK=blackCastleKingSide, bBCQ=blackCastleQueenSide;
    int bEPV=enPassantValid, bEPR=enPassantRow, bEPC=enPassantCol;
    memcpy(backup, board, sizeof(board));

    if (tolower(piece)=='k' && abs(dc-sc)==2) {
        if (white) {
            if (dc==6) { board[7][5]='R'; board[7][7]='.'; }
            else if (dc==2) { board[7][3]='R'; board[7][0]='.'; }
            whiteCastleKingSide=whiteCastleQueenSide=0;
        } else {
            if (dc==6) { board[0][5]='r'; board[0][7]='.'; }
            else if (dc==2) { board[0][3]='r'; board[0][0]='.'; }
            blackCastleKingSide=blackCastleQueenSide=0;
        }
    }
    if (tolower(piece)=='r') {
        if (white) {
            if (sr==7 && sc==0) whiteCastleQueenSide=0;
            if (sr==7 && sc==7) whiteCastleKingSide=0;
        } else {
            if (sr==0 && sc==0) blackCastleQueenSide=0;
            if (sr==0 && sc==7) blackCastleKingSide=0;
        }
    }
    if (tolower(target)=='r') {
        if (target=='R') {
            if (dr==7 && dc==0) whiteCastleQueenSide=0;
            if (dr==7 && dc==7) whiteCastleKingSide=0;
        } else {
            if (dr==0 && dc==0) blackCastleQueenSide=0;
            if (dr==0 && dc==7) blackCastleKingSide=0;
        }
    }

    board[dr][dc] = piece;
    board[sr][sc] = '.';

    // 吃过路兵
    if (tolower(piece)=='p' && abs(dr-sr)==1 && abs(dc-sc)==1 && target=='.'
        && enPassantValid && dr==enPassantRow && dc==enPassantCol) {
        int captureRow = white ? dr+1 : dr-1;
        board[captureRow][dc] = '.';
    }

    if (tolower(piece)=='p' && abs(dr-sr)==2) {
        enPassantValid = 1;
        enPassantRow = (sr+dr)/2;
        enPassantCol = sc;
    } else {
        enPassantValid = 0;
    }

    if (isKingInCheck(white)) {
        memcpy(board, backup, sizeof(board));
        whiteCastleKingSide=bWCK; whiteCastleQueenSide=bWCQ;
        blackCastleKingSide=bBCK; blackCastleQueenSide=bBCQ;
        enPassantValid=bEPV; enPassantRow=bEPR; enPassantCol=bEPC;
        return 0;
    }

    if (tolower(piece)=='p' && (dr==0 || dr==7))
        board[dr][dc] = white ? 'Q' : 'q';
    return 1;
}

int hasLegalMove(int white) {
    char backup[BOARD_SIZE][BOARD_SIZE];
    int bWCK=whiteCastleKingSide, bWCQ=whiteCastleQueenSide;
    int bBCK=blackCastleKingSide, bBCQ=blackCastleQueenSide;
    int bEPV=enPassantValid, bEPR=enPassantRow, bEPC=enPassantCol;
    for (int sr=0; sr<BOARD_SIZE; ++sr)
        for (int sc=0; sc<BOARD_SIZE; ++sc)
            if (isFriendlyPiece(board[sr][sc], white))
                for (int dr=0; dr<BOARD_SIZE; ++dr)
                    for (int dc=0; dc<BOARD_SIZE; ++dc)
                        if (canMovePiece(sr,sc,dr,dc,white)) {
                            memcpy(backup, board, sizeof(board));
                            int moved = executeMove(sr,sc,dr,dc,white);
                            memcpy(board, backup, sizeof(board));
                            whiteCastleKingSide=bWCK; whiteCastleQueenSide=bWCQ;
                            blackCastleKingSide=bBCK; blackCastleQueenSide=bBCQ;
                            enPassantValid=bEPV; enPassantRow=bEPR; enPassantCol=bEPC;
                            if (moved) return 1;
                        }
    return 0;
}

void printBoard() {
    printf("    a   b   c   d   e   f   g   h\n");
    printf("  +---+---+---+---+---+---+---+---+\n");
    int flip = 0;
    if (viewMode==0) flip = (whiteToMove==0);
    else if (viewMode==1) flip = 0;
    else flip = 1;
    if (!flip) {
        for (int r=0; r<BOARD_SIZE; ++r) {
            printf("%d |", 8-r);
            for (int c=0; c<BOARD_SIZE; ++c) printf(" %c |", board[r][c]);
            printf(" %d\n", 8-r);
            printf("  +---+---+---+---+---+---+---+---+\n");
        }
    } else {
        for (int r=BOARD_SIZE-1; r>=0; --r) {
            printf("%d |", r+1);
            for (int c=0; c<BOARD_SIZE; ++c) printf(" %c |", board[r][c]);
            printf(" %d\n", r+1);
            printf("  +---+---+---+---+---+---+---+---+\n");
        }
    }
    printf("    a   b   c   d   e   f   g   h\n");
}

void printInstructions() {
    printf("国际象棋游戏规则：\n");
    printf("- 每方交替移动一枚棋子。\n");
    printf("- 兵直走一格，首步可走两格；斜吃敌方棋子。\n");
    printf("- 马走日字，越过棋子。\n");
    printf("- 象走斜线，车走直线，后可走直线和斜线。\n");
    printf("- 王可走一格，不能将自己置于将军。\n");
    printf("- 王可进行王车易位：白方 e1->g1/h1 或 e1->c1，黑方 e8->g8/h8。\n");
    printf("- 吃过路兵：如果敌方兵首走两格通过你的兵，下一步可沿对角线吃掉它。\n");
    printf("- 兵升变到对方底线自动升变为后。\n");
    printf("- 失去所有合法着法且王在将军判负；无将军但无合法着法判为僵局。\n");
    printf("输入格式示例：e2 e4 或 e7e5。输入 quit 结束游戏。\n\n");
}

void printMenu() {
    printf("=== 国际象棋游戏菜单 ===\n");
    printf("1. 新游戏\n");
    printf("2. 游戏规则\n");
    printf("3. 切换棋盘翻转模式\n");
    printf("4. 退出\n");
    printf("请选择：");
}

void toggleViewMode() {
    printf("\n当前棋盘视角模式：");
    if (viewMode==0) printf("自动（根据走棋方翻转）\n");
    else if (viewMode==1) printf("始终白方在下（不翻转）\n");
    else printf("始终黑方在下（强制翻转）\n");
    printf("请选择新模式：\n  0 - 自动\n  1 - 始终白方在下\n  2 - 始终黑方在下\n输入数字：");
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin)) {
        int newMode;
        if (sscanf(buf, "%d", &newMode)==1 && newMode>=0 && newMode<=2) {
            viewMode = newMode;
            printf("视角模式已切换。\n");
        } else printf("无效输入，保持原模式。\n");
    }
}

// void playGame() {
//     initBoard();
//     printf("\n新游戏开始！\n");
//     printInstructions();
//     char line[64];
//     while (1) {
//         printBoard();
//         printf("%s 走棋：", whiteToMove ? "白方" : "黑方");
//         if (!fgets(line, sizeof(line), stdin)) break;
//         char src[8]={0}, dst[8]={0};
//         if (sscanf(line, "%7s %7s", src, dst) < 1) continue;
//         if (strcmp(src,"quit")==0 || strcmp(src,"exit")==0) {
//             printf("游戏结束。\n");
//             return;
//         }
//         if (strlen(src)==4 && dst[0]=='\0') {
//             dst[0]=src[2]; dst[1]=src[3]; dst[2]='\0';
//             src[2]='\0';
//         }
//         int sr,sc,dr,dc;
//         if (!parseSquare(src,&sr,&sc) || !parseSquare(dst,&dr,&dc)) {
//             printf("无效输入，请使用 a1 到 h8 的坐标。\n");
//             continue;
//         }
//         if (!executeMove(sr,sc,dr,dc,whiteToMove)) {
//             printf("非法移动。请重新输入。\n");
//             continue;
//         }
//         if (isKingInCheck(!whiteToMove)) {
//             if (!hasLegalMove(!whiteToMove)) {
//                 printBoard();
//                 printf("将死！%s 胜利。\n", whiteToMove ? "白方" : "黑方");
//                 return;
//             }
//             printf("将军！\n");
//         } else if (!hasLegalMove(!whiteToMove)) {
//             printBoard();
//             printf("僵局，平局。\n");
//             return;
//         }
//         whiteToMove = !whiteToMove;
//     }
//     printf("游戏结束。\n");
// }（修改前）

void playGame()
{
    initBoard();
    printf("\n新游戏开始！\n");
    printInstructions();
    char line[64];
    while (1)
    {
        printBoard();
        printf("%s 走棋：", whiteToMove ? "白方" : "黑方");
        if (!fgets(line, sizeof(line), stdin))
            break;
        char src[8] = {0}, dst[8] = {0};
        if (sscanf(line, "%7s %7s", src, dst) < 1)
            continue;
        if (strcmp(src, "quit") == 0 || strcmp(src, "exit") == 0)
        {
            printf("游戏结束。\n");
            return;
        }
        if (strlen(src) == 4 && dst[0] == '\0')
        {
            dst[0] = src[2];
            dst[1] = src[3];
            dst[2] = '\0';
            src[2] = '\0';
        }
        int sr, sc, dr, dc;
        if (!parseSquare(src, &sr, &sc) || !parseSquare(dst, &dr, &dc))
        {
            printf("无效输入，请使用 a1 到 h8 的坐标。\n");
            continue;
        }

        // 计算当前是否翻转显示（与 printBoard 逻辑一致）
        int flip = 0;
        if (viewMode == 0)
            flip = (whiteToMove == 0);
        else if (viewMode == 1)
            flip = 0;
        else
            flip = 1;

        if (flip)
        {
            sr = 7 - sr;
            dr = 7 - dr;
        }

        if (!executeMove(sr, sc, dr, dc, whiteToMove))
        {
            printf("非法移动。请重新输入。\n");
            continue;
        }
        if (isKingInCheck(!whiteToMove))
        {
            if (!hasLegalMove(!whiteToMove))
            {
                printBoard();
                printf("将死！%s 胜利。\n", whiteToMove ? "白方" : "黑方");
                return;
            }
            printf("将军！\n");
        }
        else if (!hasLegalMove(!whiteToMove))
        {
            printBoard();
            printf("僵局，平局。\n");
            return;
        }
        whiteToMove = !whiteToMove;
    }
    printf("游戏结束。\n");
}

int main() {
    printf("简单国际象棋游戏（C 语言实现）\n");
    char line[64];
    while (1) {
        printMenu();
        if (!fgets(line, sizeof(line), stdin)) break;
        if (line[0]=='1') playGame();
        else if (line[0]=='2') printInstructions();
        else if (line[0]=='3') toggleViewMode();
        else if (line[0]=='4') { printf("退出程序。\n"); break; }
        else printf("无效选择，请输入 1、2、3 或 4。\n");
    }
    return 0;
}

// 上一个问题：棋盘翻转后，玩家输入的坐标仍然是以白棋视角为基础的，这会导致黑棋玩家在翻转模式下输入错误的坐标。
// 解决方法是在解析输入坐标时，根据当前视角模式进行转换，使得无论翻不翻转，玩家输入的坐标都能正确对应到棋盘上的位置。

// 其他问题：
// no.1将军时不能进行非法移动：在将军状态下，玩家只能进行能够解除将军的合法移动。玩家应在执行移动前检查是否处于将军状态（可添加“将军！”文字提示），并允许玩家选择不解除将军的移动。（让对方将杀王棋）
// no.2没有悔棋功能：可以通过保存棋盘状态的历史记录来实现悔棋功能，允许玩家回退到之前的状态。
// no.3没有保存/加载功能：可以实现将棋盘状态保存到文件中，以及从文件加载棋盘状态的功能，方便玩家继续之前的游戏。
// no.4没有AI对手：可以实现一个简单的AI对手，使用基本的评估函数和搜索算法来选择移动。
// 另：可以考虑将棋子改成中文字符，增加游戏的本地化和趣味性。
// 同样，pe2e4/pe2 e4用兵(卒)e2e4/兵（卒）e2 e4输入也可以，增加输入的灵活性和趣味性。
// 没有认输选项：可以添加一个认输选项，允许玩家在觉得自己处于不利局面时选择认输，结束游戏并宣布对方获胜。
// 没有平局提议：可以添加一个平局提议功能，允许玩家在觉得局面僵持不下时向对方提议平局，对方可以接受或拒绝。
// 没有自动保存功能：可以实现自动保存游戏进度的功能，以防止意外关闭程序导致游戏数据丢失，允许玩家在重新打开程序时继续之前的游戏。
// 没有游戏回放功能：可以添加一个游戏回放功能，允许玩家在游戏结束后回顾整个对局的过程，分析每一步的得失，提升棋艺水平。
// 在对局中不可更改设置：可以允许玩家在对局中更改一些设置，如切换视角模式、调整棋盘样式等，增加游戏的灵活性和个性化体验。

// 以下暂不考虑：
// no.5没有时间控制：可以添加计时器功能，限制每方的思考时间，增加游戏的紧张感和挑战性。
// no.6没有特殊规则处理：可以完善对特殊规则的处理，如三次重复局面、五十步规则等，以使游戏更加完整和符合国际象棋的正式规则。
// no.7没有图形界面：可以开发一个图形用户界面，使游戏更加直观和易于操作，提升用户体验。
// no.8没有网络对战功能：可以实现网络对战功能，允许玩家与远程的朋友进行游戏，增加游戏的社交性和互动性。
// no.9没有多语言支持：可以添加多语言支持，使得不同语言的玩家都能享受游戏，扩大游戏的受众范围。
// no.10没有游戏结束判定：可以完善游戏结束的判定逻辑，确保在将死、僵局等情况下正确结束游戏，并显示相应的结果。
// no.11没有用户界面优化：可以优化用户界面，如增加颜色、改进输入提示等，使游戏更加美观和易用。
// no.12没有错误处理：可以添加更多的错误处理机制，如输入验证、异常处理等，提高程序的健壮性和用户体验。
// no.13没有游戏统计功能：可以添加游戏统计功能，记录玩家的胜率、对局数等数据，增加游戏的趣味性和挑战性。
// no.14没有教程或引导：可以添加一个教程模式，指导新手玩家学习国际象棋的基本规则和策略，帮助他们更好地上手游戏。
// no.15没有多种游戏模式：可以添加不同的游戏模式，如快速对局、经典对局等，满足不同玩家的需求和喜好。
// no.16没有自定义设置：可以添加自定义设置选项，允许玩家调整游戏的难度、棋盘样式等，提升游戏的个性化体验。
// no.17简单的结算动画：可以添加一些简单的动画效果，如棋子移动的动画、将军时的闪烁效果等，使游戏更加生动有趣。
// 没有多种棋盘样式：可以添加不同的棋盘样式和棋子设计，允许玩家根据自己的喜好选择，增加游戏的个性化和趣味性。