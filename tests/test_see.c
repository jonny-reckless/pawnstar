/// @file Standalone static exchange evaluation tests.

#include <stdio.h>

#include "move.h"
#include "piece.h"
#include "position.h"
#include "square.h"
#include "static_exchange_evaluation.h"

typedef struct
{
    const char *fen;
    const char *from;
    const char *to;
    piece_t     piece;
    piece_t     captured;
    int         expected;
    const char *label;
} see_case_t;

static const see_case_t CASES[] = {
    {"1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -", "e1", "e5", ROOK, PAWN, 100, "rook captures pawn, no defenders"},
    {"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "d3", "e5", KNIGHT, PAWN, -200,
     "knight captures pawn, loses to defenders"},
};

static const int NUM_CASES = (int)(sizeof(CASES) / sizeof(CASES[0]));

int main(void)
{
    int failures = 0;

    for (int i = 0; i < NUM_CASES; ++i)
    {
        const see_case_t *tc  = &CASES[i];
        position_t        pos = position_from_string(tc->fen);
        move_t m = move_capture(square_from_string(tc->from), square_from_string(tc->to), tc->piece, tc->captured);
        see_result_t result = evaluate_static_exchange(&pos, m);
        int          pass   = (result.score == tc->expected);
        printf("[%s]  %s  score=%d  expected=%d\n", pass ? "PASS" : "FAIL", tc->label, result.score, tc->expected);
        if (!pass)
        {
            failures++;
        }
    }

    printf("\n%d/%d passed\n", NUM_CASES - failures, NUM_CASES);
    return failures > 0 ? 1 : 0;
}
