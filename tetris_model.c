#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BOARD_WIDTH       10
#define BOARD_HEIGHT      20
#define SEQUENCE_LENGTH   20
#define NUM_PIECE_TYPES   4
#define MAX_ROTATIONS     4

/* 4 tetrominos: I, O, T, L */
typedef enum { PIECE_I = 0, PIECE_O = 1, PIECE_T = 2, PIECE_L = 3 } PieceType;

static const char *PIECE_NAMES[NUM_PIECE_TYPES] = { "I", "O", "T", "L" };
static const char  PIECE_CHARS[NUM_PIECE_TYPES] = { 'I', 'O', 'T', 'L' };

/*
 * Cell offsets [piece][rotation][cell_index][0=row, 1=col]
 * All offsets are relative to the top-left of the bounding box.
 *
 * I   rot0: IIII   rot1: I
 *                        I
 *                        I
 *                        I
 *
 * O   rot0: OO
 *           OO
 *
 * T   rot0: .T.   rot1: T.   rot2: TTT   rot3: .T
 *           TTT         TT         .T.         TT
 *                       T.                     .T
 *
 * L   rot0: L.   rot1: LLL   rot2: LL   rot3: ..L
 *           L.         L..         .L         LLL
 *           LL                     .L
 */
static const int PIECE_CELLS[NUM_PIECE_TYPES][MAX_ROTATIONS][4][2] = {
    /* PIECE_I */
    {
        {{0,0},{0,1},{0,2},{0,3}},
        {{0,0},{1,0},{2,0},{3,0}},
        {{0,0},{0,1},{0,2},{0,3}}, // dup of rot 0
        {{0,0},{1,0},{2,0},{3,0}}, // dup of rot 1
    },
    /* PIECE_O */
    {
        {{0,0},{0,1},{1,0},{1,1}},
        {{0,0},{0,1},{1,0},{1,1}},
        {{0,0},{0,1},{1,0},{1,1}},
        {{0,0},{0,1},{1,0},{1,1}},
    },
    /* PIECE_T */
    {
        {{0,1},{1,0},{1,1},{1,2}},
        {{0,0},{1,0},{1,1},{2,0}},
        {{0,0},{0,1},{0,2},{1,1}},
        {{0,1},{1,0},{1,1},{2,1}},
    },
    /* PIECE_L */
    {
        {{0,0},{1,0},{2,0},{2,1}},
        {{0,0},{0,1},{0,2},{1,0}},
        {{0,0},{0,1},{1,1},{2,1}},
        {{0,2},{1,0},{1,1},{1,2}},
    },
};

static const int PIECE_NUM_ROTATIONS[NUM_PIECE_TYPES] = { 2, 1, 4, 4 };

// sequence
int sequence[SEQUENCE_LENGTH];

static void generate_sequence(void) {
    for (int i = 0; i < SEQUENCE_LENGTH; i++)
        sequence[i] = rand() % NUM_PIECE_TYPES;
}

// board helpers
static int piece_width(int ptype, int rot) {
    int max_col = 0;
    for (int i = 0; i < 4; i++) {
        int c = PIECE_CELLS[ptype][rot][i][1];
        if (c > max_col) max_col = c;
    }
    return max_col + 1;
}

/*
 * Returns the row at which the piece's bounding-box top lands after dropping
 * into the given column, or -1 if the piece cannot be placed at all
 * (column+width overflows or the very first row is already blocked).
 */
static int drop_row(int board[][BOARD_WIDTH], int ptype, int rot, int col) {
    int row;
    for (row = 0; row <= BOARD_HEIGHT; row++) {
        int fits = 1;
        for (int i = 0; i < 4; i++) {
            int pr = PIECE_CELLS[ptype][rot][i][0] + row;
            int pc = PIECE_CELLS[ptype][rot][i][1] + col;
            if (pr >= BOARD_HEIGHT || pc < 0 || pc >= BOARD_WIDTH || board[pr][pc]) {
                fits = 0;
                break;
            }
        }
        if (!fits) break;
    }
    return row - 1; // last valid pos
}

static void place_piece(int board[][BOARD_WIDTH], int ptype, int rot, int col, int row) {
    for (int i = 0; i < 4; i++) {
        int pr = PIECE_CELLS[ptype][rot][i][0] + row;
        int pc = PIECE_CELLS[ptype][rot][i][1] + col;
        board[pr][pc] = ptype + 1; // store 1-based id
    }
}

/* Returns the number of lines cleared. */
static int clear_lines(int board[][BOARD_WIDTH]) {
    int cleared = 0;
    for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < BOARD_WIDTH; c++)
            if (!board[r][c]) { full = 0; break; }
        if (full) {
            cleared++;
            for (int rr = r; rr > 0; rr--)
                memcpy(board[rr], board[rr - 1], BOARD_WIDTH * sizeof(int));
            memset(board[0], 0, BOARD_WIDTH * sizeof(int));
            r++; // recheck the same row after the shift
        }
    }
    return cleared;
}

// evaluation
static int col_height(int board[][BOARD_WIDTH], int col) {
    for (int r = 0; r < BOARD_HEIGHT; r++)
        if (board[r][col]) return BOARD_HEIGHT - r;
    return 0;
}

static int aggregate_height(int board[][BOARD_WIDTH]) {
    int total = 0;
    for (int c = 0; c < BOARD_WIDTH; c++)
        total += col_height(board, c);
    return total;
}

static int count_holes(int board[][BOARD_WIDTH]) {
    int holes = 0;
    for (int c = 0; c < BOARD_WIDTH; c++) {
        int above = 0;
        for (int r = 0; r < BOARD_HEIGHT; r++) {
            if (board[r][c]) above = 1;
            else if (above)  holes++;
        }
    }
    return holes;
}

static int bumpiness(int board[][BOARD_WIDTH]) {
    int bump = 0;
    for (int c = 0; c < BOARD_WIDTH - 1; c++) {
        int diff = col_height(board, c) - col_height(board, c + 1);
        bump += diff < 0 ? -diff : diff;
    }
    return bump;
}

// log helpers
static void write_board(FILE *f, int board[][BOARD_WIDTH]) {
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            int v = board[r][c];
            fputc(v ? PIECE_CHARS[v - 1] : '.', f);
        }
        fputc('\n', f);
    }
}

// stats
typedef struct {
    const char *model_name;
    int turns_played;
    int game_over;
    int total_lines;
    int clears[5];          // count of turns where 0/1/2/3/4 lines were cleared
    int piece_counts[NUM_PIECE_TYPES];
    int peak_agg_height;    // highest aggregate height seen across all turns
    int final_agg_height;
    int final_holes;
    int final_bumpiness;
} Stats;

// simulation
/*
 * model == 0  →  greedy:    maximise lines cleared; break ties by minimising
 *                            aggregate column height.
 * model == 1  →  heuristic: El-Tetris weighted score
 *                            (Dellacherie / Thiery & Scherrer coefficients).
 */
static void simulate(int model, const char *log_file, Stats *st) {
    int board[BOARD_HEIGHT][BOARD_WIDTH];
    memset(board, 0, sizeof(board));
    memset(st, 0, sizeof(*st));
    st->model_name = (model == 0) ? "greedy" : "heuristic";

    FILE *f = fopen(log_file, "w");
    if (!f) { perror("fopen"); return; }

    fprintf(f, "MODEL %s\n\n", st->model_name);

    for (int turn = 0; turn < SEQUENCE_LENGTH; turn++) {
        int ptype     = sequence[turn];
        int best_rot  = -1;
        int best_col  = -1;
        double best_score = -1e18;

        for (int rot = 0; rot < PIECE_NUM_ROTATIONS[ptype]; rot++) {
            int pw      = piece_width(ptype, rot);
            int max_col = BOARD_WIDTH - pw;

            for (int col = 0; col <= max_col; col++) {
                int row = drop_row(board, ptype, rot, col);
                if (row < 0) continue;

                int tmp[BOARD_HEIGHT][BOARD_WIDTH];
                memcpy(tmp, board, sizeof(board));
                place_piece(tmp, ptype, rot, col, row);
                int lines = clear_lines(tmp);

                double score;
                if (model == 0) {
                    int ah = aggregate_height(tmp);
                    score  = lines * 10000.0 - ah;
                } else {
                    int ah    = aggregate_height(tmp);
                    int holes = count_holes(tmp);
                    int bump  = bumpiness(tmp);
                    score = -0.510066 * ah
                           + 0.760666 * lines
                           - 0.356630 * holes
                           - 0.184483 * bump;
                }

                if (score > best_score) {
                    best_score = score;
                    best_rot   = rot;
                    best_col   = col;
                }
            }
        }

        if (best_rot == -1) {
            fprintf(f, "TURN %d\nGAME_OVER\n\n", turn + 1);
            printf("[%s] Game over at turn %d\n", st->model_name, turn + 1);
            st->game_over = 1;
            break;
        }

        int row = drop_row(board, ptype, best_rot, best_col);
        place_piece(board, ptype, best_rot, best_col, row);

        int pre[BOARD_HEIGHT][BOARD_WIDTH];
        memcpy(pre, board, sizeof(board));

        int lines = clear_lines(board);

        // accummulate stats
        st->turns_played++;
        st->total_lines += lines;
        st->clears[lines < 5 ? lines : 4]++;
        st->piece_counts[ptype]++;
        int ah = aggregate_height(board);
        if (ah > st->peak_agg_height) st->peak_agg_height = ah;

        fprintf(f, "TURN %d\n",          turn + 1);
        fprintf(f, "PIECE %s\n",         PIECE_NAMES[ptype]);
        fprintf(f, "ROTATION %d\n",      best_rot);
        fprintf(f, "COLUMN %d\n",        best_col);
        fprintf(f, "LINES_CLEARED %d\n", lines);
        fprintf(f, "PLACED\n");
        write_board(f, pre);
        fprintf(f, "BOARD\n");
        write_board(f, board);
        fprintf(f, "\n");

        printf("[%s] Turn %2d: piece=%-1s rot=%d col=%2d lines=%d\n",
               st->model_name, turn + 1, PIECE_NAMES[ptype], best_rot, best_col, lines);
    }

    // final board metrics
    st->final_agg_height = aggregate_height(board);
    st->final_holes      = count_holes(board);
    st->final_bumpiness  = bumpiness(board);

    fprintf(f, "TOTAL_LINES_CLEARED %d\n", st->total_lines);
    fclose(f);

    printf("[%s] Done — total lines cleared: %d  →  %s\n\n",
           st->model_name, st->total_lines, log_file);
}

// analysis
static void print_analysis(const Stats *g, const Stats *h) {
    printf("\nGREEDY vs HEURISTIC\n");

    printf("turns: \t\t%d \t%d\n", g->turns_played, h->turns_played);
    printf("lines: \t\t%d \t%d\n", g->total_lines, h->total_lines);
    printf("holes: \t\t%d \t%d\n", g->final_holes, h->final_holes);
    printf("height: \t%d \t%d\n", g->final_agg_height, h->final_agg_height);
    printf("bump: \t\t%d \t%d\n", g->final_bumpiness, h->final_bumpiness);

    printf("\nclears:\n");
    for (int i = 0; i <= 4; i++) {
        printf("%d: \t%d \t%d\n", i, g->clears[i], h->clears[i]);
    }

    printf("\nwinner: %s\n",
        (g->total_lines > h->total_lines) ? "GREEDY" :
        (h->total_lines > g->total_lines) ? "HEURISTIC" : "TIE");
}

int main(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned)(ts.tv_nsec ^ ((unsigned long)ts.tv_sec << 20) ^ (unsigned)getpid()));
    generate_sequence();

    printf("Sequence (%d pieces): ", SEQUENCE_LENGTH);
    for (int i = 0; i < SEQUENCE_LENGTH; i++)
        printf("%s ", PIECE_NAMES[sequence[i]]);
    printf("\n\n");

    Stats greedy_stats, heuristic_stats;
    simulate(0, "greedy_log.txt",    &greedy_stats);
    simulate(1, "heuristic_log.txt", &heuristic_stats);

    print_analysis(&greedy_stats, &heuristic_stats);

    return 0;
}
