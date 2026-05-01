#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BOARD_WIDTH  10
#define BOARD_HEIGHT 20
#define MAX_LINE     256

// ansi color for pieces
static const char *piece_color(char ch) {
    switch (ch) {
        case 'I': return "\033[96m";
        case 'O': return "\033[93m";
        case 'T': return "\033[95m";
        case 'L': return "\033[91m";
        default:  return "\033[0m";
    }
}

static void render(const char *model, int turn, const char *piece,
                   int rotation, int col, int lines_cleared,
                   char board[BOARD_HEIGHT][BOARD_WIDTH + 1]) {
    // move cursor to top-left and clear screen
    printf("\033[2J\033[H");

    // board
    printf("\033[1m+----------+\033[0m\n");
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        printf("\033[1m|\033[0m");
        for (int c = 0; c < BOARD_WIDTH; c++) {
            char ch = board[r][c];
            if (ch == '.' || ch == '\0') {
                printf("\033[90m.\033[0m");
            } else {
                printf("%s%c\033[0m", piece_color(ch), ch);
            }
        }
        printf("\033[1m|\033[0m\n");
    }
    printf("\033[1m+----------+\033[0m\n");

    fflush(stdout);
}

// log parser
/* States for which board section we are currently reading. */
typedef enum { READING_NONE, READING_PLACED, READING_BOARD } ReadState;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <log_file>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }

    char line[MAX_LINE];
    char model[32]  = "unknown";
    char piece[16]  = "";
    int  turn       = 0;
    int  rotation   = 0;
    int  col        = 0;
    int  lines_cl   = 0;
    int  board_row  = 0;
    ReadState state = READING_NONE;

    char placed[BOARD_HEIGHT][BOARD_WIDTH + 1];
    char board[BOARD_HEIGHT][BOARD_WIDTH + 1];
    memset(placed, 0, sizeof(placed));
    memset(board,  0, sizeof(board));

    while (fgets(line, sizeof(line), f)) {
        // rm trailing newline
        int len = (int)strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';

        if (strncmp(line, "MODEL ", 6) == 0) {
            strncpy(model, line + 6, sizeof(model) - 1);
            model[sizeof(model) - 1] = '\0';

        } else if (strncmp(line, "TURN ", 5) == 0) {
            turn      = atoi(line + 5);
            board_row = 0;
            state     = READING_NONE;

        } else if (strncmp(line, "PIECE ", 6) == 0) {
            strncpy(piece, line + 6, sizeof(piece) - 1);
            piece[sizeof(piece) - 1] = '\0';

        } else if (strncmp(line, "ROTATION ", 9) == 0) {
            rotation = atoi(line + 9);

        } else if (strncmp(line, "COLUMN ", 7) == 0) {
            col = atoi(line + 7);

        } else if (strncmp(line, "LINES_CLEARED ", 14) == 0) {
            lines_cl = atoi(line + 14);

        } else if (strcmp(line, "PLACED") == 0) {
            state     = READING_PLACED;
            board_row = 0;

        } else if (strcmp(line, "BOARD") == 0) {
            state     = READING_BOARD;
            board_row = 0;

        } else if (strncmp(line, "GAME_OVER", 9) == 0) {
            printf("\033[2J\033[H");
            printf("\033[1;31m=== GAME OVER (model: %s, turn: %d) ===\033[0m\n",
                   model, turn);
            fflush(stdout);
            break;

        } else if (strncmp(line, "TOTAL_LINES", 11) == 0) {
            int total = atoi(line + 12);
            printf("\033[1;32m=== End of replay | Model: %s | Total lines: %d ===\033[0m\n",
                   model, total);
            fflush(stdout);
            break;

        } else if (state != READING_NONE && board_row < BOARD_HEIGHT && len == BOARD_WIDTH) {
            // board row data
            char (*target)[BOARD_WIDTH + 1] = (state == READING_PLACED) ? placed : board;
            strncpy(target[board_row], line, BOARD_WIDTH);
            target[board_row][BOARD_WIDTH] = '\0';
            board_row++;

            if (board_row == BOARD_HEIGHT) {
                if (state == READING_PLACED) {
                    // show before clearing
                    if (lines_cl > 0) {
                        render(model, turn, piece, rotation, col, 0, placed);
                        usleep(500000); // .5s
                    }
                } else {
                    // final after clearing
                    render(model, turn, piece, rotation, col, lines_cl, board);
                    usleep(500000); // .5s
                }
                state = READING_NONE;
            }
        }
    }

    fclose(f);
    return 0;
}
