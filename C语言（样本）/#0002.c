#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BOARD_SIZE 8

char board[BOARD_SIZE][BOARD_SIZE];
int whiteToMove = 1;
int whiteCastleKingSide = 1;
int whiteCastleQueenSide = 1;
int blackCastleKingSide = 1;
int blackCastleQueenSide = 1;
int enPassantValid = 0;
int enPassantRow = -1;
int enPassantCol = -1;

void initBoard() {
    const char *start[8] = {
        "rnbqkbnr",
        "pppppppp",
        "........",
        "........",
        "........",
        "........",
        "PPPPPPPP",
        "RNBQKBNR"
    };

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            board[r][c] = start[r][c];
        }
    }
    whiteToMove = 1;
    whiteCastleKingSide = 1;
    whiteCastleQueenSide = 1;
    blackCastleKingSide = 1;
    blackCastleQueenSide = 1;
    enPassantValid = 0;
    enPassantRow = -1;
    enPassantCol = -1;
}

// void printBoard() {
//     printf("    a   b   c   d   e   f   g   h\n");
//     printf("  +---+---+---+---+---+---+---+---+\n");
//     for (int r = 0; r < BOARD_SIZE; ++r) {
//         printf("%d |", 8 - r);
//         for (int c = 0; c < BOARD_SIZE; ++c) {
//             printf(" %c |", board[r][c]);
//         }
//         printf(" %d\n", 8 - r);
//         printf("  +---+---+---+---+---+---+---+---+\n");
//     }
//     printf("    a   b   c   d   e   f   g   h\n");
// }（原本没有棋盘翻转功能）

void printBoard()
{
    printf("    a   b   c   d   e   f   g   h\n");
    printf("  +---+---+---+---+---+---+---+---+\n");

    if (whiteToMove)
    {
        // 白方走棋：正常顺序（行 0 到 7，对应行号 8 → 1）
        for (int r = 0; r < BOARD_SIZE; ++r)
        {
            printf("%d |", 8 - r);
            for (int c = 0; c < BOARD_SIZE; ++c)
            {
                printf(" %c |", board[r][c]);
            }
            printf(" %d\n", 8 - r);
            printf("  +---+---+---+---+---+---+---+---+\n");
        }
    }
    else
    {
        // 黑方走棋：翻转显示（行 7 到 0，行号变为 1 → 8）
        for (int r = BOARD_SIZE - 1; r >= 0; --r)
        {
            printf("%d |", r + 1); // r=7 → 8, r=0 → 1
            for (int c = 0; c < BOARD_SIZE; ++c)
            {
                printf(" %c |", board[r][c]);
            }
            printf(" %d\n", r + 1);
            printf("  +---+---+---+---+---+---+---+---+\n");
        }
    }

    printf("    a   b   c   d   e   f   g   h\n");
}


int isWhite(char piece) {
    return piece >= 'A' && piece <= 'Z';
}

int isBlack(char piece) {
    return piece >= 'a' && piece <= 'z';
}

int onBoard(int r, int c) {
    return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE;
}

int parseSquare(const char *token, int *row, int *col) {
    if (strlen(token) < 2) return 0;
    char file = token[0];
    char rank = token[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return 0;
    *col = file - 'a';
    *row = 8 - (rank - '0');
    return 1;
}

int isFriendlyPiece(char piece, int white) {
    if (piece == '.') return 0;
    return white ? isWhite(piece) : isBlack(piece);
}

int isEnemyPiece(char piece, int white) {
    if (piece == '.') return 0;
    return white ? isBlack(piece) : isWhite(piece);
}

int clearPath(int sr, int sc, int dr, int dc) {
    int drStep = (dr > sr) ? 1 : (dr < sr) ? -1 : 0;
    int dcStep = (dc > sc) ? 1 : (dc < sc) ? -1 : 0;
    int r = sr + drStep;
    int c = sc + dcStep;
    while (r != dr || c != dc) {
        if (board[r][c] != '.') return 0;
        r += drStep;
        c += dcStep;
    }
    return 1;
}

int canPawnAttack(int sr, int sc, int dr, int dc, int white) {
    int direction = white ? -1 : 1;
    return dr == sr + direction && abs(dc - sc) == 1;
}

int canMovePawn(int sr, int sc, int dr, int dc, int white) {
    int direction = white ? -1 : 1;
    char target = board[dr][dc];
    int startRow = white ? 6 : 1;
    if (sc == dc && target == '.') {
        if (dr == sr + direction) return 1;
        if (sr == startRow && dr == sr + 2 * direction && board[sr + direction][sc] == '.') return 1;
    }
    if (abs(dc - sc) == 1 && dr == sr + direction) {
        if (isEnemyPiece(target, white)) return 1;
        if (enPassantValid && dr == enPassantRow && dc == enPassantCol) return 1;
    }
    return 0;
}

int canMoveKnight(int sr, int sc, int dr, int dc) {
    int drAbs = abs(dr - sr);
    int dcAbs = abs(dc - sc);
    return (drAbs == 2 && dcAbs == 1) || (drAbs == 1 && dcAbs == 2);
}

int canMoveBishop(int sr, int sc, int dr, int dc) {
    if (abs(dr - sr) != abs(dc - sc)) return 0;
    return clearPath(sr, sc, dr, dc);
}

int canMoveRook(int sr, int sc, int dr, int dc) {
    if (sr != dr && sc != dc) return 0;
    return clearPath(sr, sc, dr, dc);
}

int canCastle(int sr, int sc, int dr, int dc, int white) {
    if (isKingInCheck(white)) return 0;
    if (white) {
        if (sr != 7 || sc != 4) return 0;
        if (dr == 7 && dc == 6) {
            if (!whiteCastleKingSide) return 0;
            if (board[7][5] != '.' || board[7][6] != '.') return 0;
            if (squareAttacked(7, 5, 0) || squareAttacked(7, 6, 0)) return 0;
            if (board[7][7] != 'R') return 0;
            return 1;
        }
        if (dr == 7 && dc == 2) {
            if (!whiteCastleQueenSide) return 0;
            if (board[7][3] != '.' || board[7][2] != '.' || board[7][1] != '.') return 0;
            if (squareAttacked(7, 3, 0) || squareAttacked(7, 2, 0)) return 0;
            if (board[7][0] != 'R') return 0;
            return 1;
        }
    } else {
        if (sr != 0 || sc != 4) return 0;
        if (dr == 0 && dc == 6) {
            if (!blackCastleKingSide) return 0;
            if (board[0][5] != '.' || board[0][6] != '.') return 0;
            if (squareAttacked(0, 5, 1) || squareAttacked(0, 6, 1)) return 0;
            if (board[0][7] != 'r') return 0;
            return 1;
        }
        if (dr == 0 && dc == 2) {
            if (!blackCastleQueenSide) return 0;
            if (board[0][3] != '.' || board[0][2] != '.' || board[0][1] != '.') return 0;
            if (squareAttacked(0, 3, 1) || squareAttacked(0, 2, 1)) return 0;
            if (board[0][0] != 'r') return 0;
            return 1;
        }
    }
    return 0;
}

int canMoveQueen(int sr, int sc, int dr, int dc) {
    return canMoveBishop(sr, sc, dr, dc) || canMoveRook(sr, sc, dr, dc);
}

int canMoveKing(int sr, int sc, int dr, int dc) {
    int drAbs = abs(dr - sr);
    int dcAbs = abs(dc - sc);
    if (drAbs == 0 && dcAbs == 2) {
        return canCastle(sr, sc, dr, dc, isWhite(board[sr][sc]));
    }
    return drAbs <= 1 && dcAbs <= 1;
}

int squareAttacked(int row, int col, int byWhite) {
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            char piece = board[r][c];
            if (!isFriendlyPiece(piece, byWhite)) continue;
            int canMove = 0;
            char lower = tolower(piece);
            switch (lower) {
                case 'p': canMove = canPawnAttack(r, c, row, col, byWhite); break;
                case 'n': canMove = canMoveKnight(r, c, row, col); break;
                case 'b': canMove = canMoveBishop(r, c, row, col); break;
                case 'r': canMove = canMoveRook(r, c, row, col); break;
                case 'q': canMove = canMoveQueen(r, c, row, col); break;
                case 'k': canMove = canMoveKing(r, c, row, col); break;
            }
            if (canMove) return 1;
        }
    }
    return 0;
}

int isKingInCheck(int white) {
    int kingRow = -1, kingCol = -1;
    char king = white ? 'K' : 'k';
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (board[r][c] == king) {
                kingRow = r;
                kingCol = c;
                break;
            }
        }
        if (kingRow != -1) break;
    }
    if (kingRow == -1) return 1;
    return squareAttacked(kingRow, kingCol, !white);
}

int canMovePiece(int sr, int sc, int dr, int dc, int white) {
    if (!onBoard(sr, sc) || !onBoard(dr, dc)) return 0;
    char piece = board[sr][sc];
    if (piece == '.' || !isFriendlyPiece(piece, white)) return 0;
    if (isFriendlyPiece(board[dr][dc], white)) return 0;

    char lower = tolower(piece);
    int valid = 0;
    switch (lower) {
        case 'p': valid = canMovePawn(sr, sc, dr, dc, white); break;
        case 'n': valid = canMoveKnight(sr, sc, dr, dc); break;
        case 'b': valid = canMoveBishop(sr, sc, dr, dc); break;
        case 'r': valid = canMoveRook(sr, sc, dr, dc); break;
        case 'q': valid = canMoveQueen(sr, sc, dr, dc); break;
        case 'k': valid = canMoveKing(sr, sc, dr, dc); break;
        default: valid = 0; break;
    }
    return valid;
}

int executeMove(int sr, int sc, int dr, int dc, int white) {
    if (!canMovePiece(sr, sc, dr, dc, white)) return 0;
    char piece = board[sr][sc];
    char target = board[dr][dc];

    char backup[BOARD_SIZE][BOARD_SIZE];
    int backupWhiteCastleKS = whiteCastleKingSide;
    int backupWhiteCastleQS = whiteCastleQueenSide;
    int backupBlackCastleKS = blackCastleKingSide;
    int backupBlackCastleQS = blackCastleQueenSide;
    int backupEnPassantValid = enPassantValid;
    int backupEnPassantRow = enPassantRow;
    int backupEnPassantCol = enPassantCol;

    memcpy(backup, board, sizeof(board));

    if (tolower(piece) == 'k' && abs(dc - sc) == 2) {
        if (white) {
            if (dc == 6) {
                board[7][5] = 'R';
                board[7][7] = '.';
            } else if (dc == 2) {
                board[7][3] = 'R';
                board[7][0] = '.';
            }
            whiteCastleKingSide = 0;
            whiteCastleQueenSide = 0;
        } else {
            if (dc == 6) {
                board[0][5] = 'r';
                board[0][7] = '.';
            } else if (dc == 2) {
                board[0][3] = 'r';
                board[0][0] = '.';
            }
            blackCastleKingSide = 0;
            blackCastleQueenSide = 0;
        }
    }

    if (tolower(piece) == 'r') {
        if (white) {
            if (sr == 7 && sc == 0) whiteCastleQueenSide = 0;
            if (sr == 7 && sc == 7) whiteCastleKingSide = 0;
        } else {
            if (sr == 0 && sc == 0) blackCastleQueenSide = 0;
            if (sr == 0 && sc == 7) blackCastleKingSide = 0;
        }
    }

    if (tolower(target) == 'r') {
        if (target == 'R') {
            if (dr == 7 && dc == 0) whiteCastleQueenSide = 0;
            if (dr == 7 && dc == 7) whiteCastleKingSide = 0;
        } else {
            if (dr == 0 && dc == 0) blackCastleQueenSide = 0;
            if (dr == 0 && dc == 7) blackCastleKingSide = 0;
        }
    }

    board[dr][dc] = piece;
    board[sr][sc] = '.';

    if (tolower(piece) == 'p' && abs(dr - sr) == 1 && abs(dc - sc) == 1 && target == '.' && enPassantValid && dr == enPassantRow && dc == enPassantCol) {
        int captureRow = white ? dr + 1 : dr - 1;
        board[captureRow][dc] = '.';
    }

    if (tolower(piece) == 'p' && abs(dr - sr) == 2) {
        enPassantValid = 1;
        enPassantRow = (sr + dr) / 2;
        enPassantCol = sc;
    } else {
        enPassantValid = 0;
    }

    if (isKingInCheck(white)) {
        memcpy(board, backup, sizeof(board));
        whiteCastleKingSide = backupWhiteCastleKS;
        whiteCastleQueenSide = backupWhiteCastleQS;
        blackCastleKingSide = backupBlackCastleKS;
        blackCastleQueenSide = backupBlackCastleQS;
        enPassantValid = backupEnPassantValid;
        enPassantRow = backupEnPassantRow;
        enPassantCol = backupEnPassantCol;
        return 0;
    }

    if (tolower(piece) == 'p' && (dr == 0 || dr == 7)) {
        board[dr][dc] = white ? 'Q' : 'q';
    }
    return 1;
}

int hasLegalMove(int white) {
    char backup[BOARD_SIZE][BOARD_SIZE];
    int backupWhiteCastleKS = whiteCastleKingSide;
    int backupWhiteCastleQS = whiteCastleQueenSide;
    int backupBlackCastleKS = blackCastleKingSide;
    int backupBlackCastleQS = blackCastleQueenSide;
    int backupEnPassantValid = enPassantValid;
    int backupEnPassantRow = enPassantRow;
    int backupEnPassantCol = enPassantCol;

    for (int sr = 0; sr < BOARD_SIZE; ++sr) {
        for (int sc = 0; sc < BOARD_SIZE; ++sc) {
            if (!isFriendlyPiece(board[sr][sc], white)) continue;
            for (int dr = 0; dr < BOARD_SIZE; ++dr) {
                for (int dc = 0; dc < BOARD_SIZE; ++dc) {
                    if (!canMovePiece(sr, sc, dr, dc, white)) continue;
                    memcpy(backup, board, sizeof(board));
                    int moved = executeMove(sr, sc, dr, dc, white);
                    memcpy(board, backup, sizeof(board));
                    whiteCastleKingSide = backupWhiteCastleKS;
                    whiteCastleQueenSide = backupWhiteCastleQS;
                    blackCastleKingSide = backupBlackCastleKS;
                    blackCastleQueenSide = backupBlackCastleQS;
                    enPassantValid = backupEnPassantValid;
                    enPassantRow = backupEnPassantRow;
                    enPassantCol = backupEnPassantCol;
                    if (moved) return 1;
                }
            }
        }
    }
    return 0;
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
    printf("3. 退出\n");
    printf("请选择：");
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

        char src[8] = {0}, dst[8] = {0};
        if (sscanf(line, "%7s %7s", src, dst) < 1) continue;
        if (strcmp(src, "quit") == 0 || strcmp(src, "exit") == 0) {
            printf("游戏结束。\n");
            break;
        }

        if (strlen(src) == 4 && dst[0] == '\0') {
            dst[0] = src[2];
            dst[1] = src[3];
            dst[2] = '\0';
            src[2] = '\0';
        }

        int sr, sc, dr, dc;
        if (!parseSquare(src, &sr, &sc) || !parseSquare(dst, &dr, &dc)) {
            printf("无效输入，请使用 a1 到 h8 的坐标。\n");
            continue;
        }

        if (!executeMove(sr, sc, dr, dc, whiteToMove)) {
            printf("非法移动。请重新输入。\n");
            continue;
        }

        if (isKingInCheck(!whiteToMove)) {
            if (!hasLegalMove(!whiteToMove)) {
                printBoard();
                printf("将死！%s 胜利。\n", whiteToMove ? "白方" : "黑方");
                break;
            }
            printf("将军！\n");
        } else if (!hasLegalMove(!whiteToMove)) {
            printBoard();
            printf("僵局，平局。\n");
            break;
        }

        whiteToMove = !whiteToMove;
    }
}

int main() {
    printf("简单国际象棋游戏（C 语言实现）\n");
    char line[64];
    while (1) {
        printMenu();
        if (!fgets(line, sizeof(line), stdin)) break;
        if (line[0] == '1') {
            playGame();
            break;
        } else if (line[0] == '2') {
            printInstructions();
        } else if (line[0] == '3') {
            printf("退出程序。\n");
            break;
        } else {
            printf("无效选择，请输入 1、2 或 3。\n");
        }
    }
    return 0;
}
