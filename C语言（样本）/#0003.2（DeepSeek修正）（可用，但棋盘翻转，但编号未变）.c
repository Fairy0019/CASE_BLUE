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

void playGame() {
    initBoard();
    printf("\n新游戏开始！\n");
    printInstructions();
    char line[64];
    while (1) {
        printBoard();
        printf("%s 走棋：", whiteToMove ? "白方" : "黑方");
        if (!fgets(line, sizeof(line), stdin)) break;
        char src[8]={0}, dst[8]={0};
        if (sscanf(line, "%7s %7s", src, dst) < 1) continue;
        if (strcmp(src,"quit")==0 || strcmp(src,"exit")==0) {
            printf("游戏结束。\n");
            return;
        }
        if (strlen(src)==4 && dst[0]=='\0') {
            dst[0]=src[2]; dst[1]=src[3]; dst[2]='\0';
            src[2]='\0';
        }
        int sr,sc,dr,dc;
        if (!parseSquare(src,&sr,&sc) || !parseSquare(dst,&dr,&dc)) {
            printf("无效输入，请使用 a1 到 h8 的坐标。\n");
            continue;
        }
        if (!executeMove(sr,sc,dr,dc,whiteToMove)) {
            printf("非法移动。请重新输入。\n");
            continue;
        }
        if (isKingInCheck(!whiteToMove)) {
            if (!hasLegalMove(!whiteToMove)) {
                printBoard();
                printf("将死！%s 胜利。\n", whiteToMove ? "白方" : "黑方");
                return;
            }
            printf("将军！\n");
        } else if (!hasLegalMove(!whiteToMove)) {
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