/// @file Tests of static exchange evaluation.

#include <stdio.h>

#include "move.h"
#include "position.h"
#include "square.h"
#include "static_exchange_evaluation.h"

typedef struct
{
    const char *fen_string;
    move_t      move;
    int         see_score;
} see_test_t;

static const see_test_t tests[] = {
    {"1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -",
     /* E1xe5 rook captures pawn */ 0, 100},
    {"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -",
     /* D3xe5 knight captures pawn */ 0, -200},
};

void run_static_exchange_tests(void)
{
    /* Build moves at runtime to avoid static initializer ordering issues. */
    see_test_t t0 = tests[0];
    see_test_t t1 = tests[1];
    t0.move       = move_capture(square_from_string("E1"), square_from_string("E5"), ROOK, PAWN);
    t1.move       = move_capture(square_from_string("D3"), square_from_string("E5"), KNIGHT, PAWN);

    const see_test_t run_tests[2] = {t0, t1};
    bool             is_pass      = true;

    for (int i = 0; i < 2; ++i)
    {
        position_t position = position_from_string(run_tests[i].fen_string);
        char       pos_buf[128];
        char       move_buf[8];
        position_to_string(&position, pos_buf, sizeof(pos_buf));
        move_to_string(run_tests[i].move, move_buf, sizeof(move_buf));
        see_result_t result = evaluate_static_exchange(&position, run_tests[i].move);
        printf("\n%s\nSEE for %s = %d\n", pos_buf, move_buf, result.score);
        is_pass &= (result.score == run_tests[i].see_score);
    }
    printf("\n%s\n", is_pass ? "PASSED" : "FAILED");
    fflush(stdout);
}
