/// @file Standalone Bratko-Kopec search position tests.
///
/// Each test searches to the minimum depth at which the engine reliably finds
/// the known best move (established by running to depth 12 as a reference).

#include <stdio.h>
#include <string.h>

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "move.h"
#include "search.h"

typedef struct
{
    const char *fen;
    const char *expected_move;
    int         depth;
} bk_case_t;

// clang-format off
static const bk_case_t CASES[] = {
    {"1k1r4/pp1b1R02/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -",                   "d6d1", 10},
    {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - -",                   "f4f5", 10},
    {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -",      "h8g8", 10},
    {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -",        "e5e6", 10},
    {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -",           "c3d5", 10},
    {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -",                     "g5g6", 11},
    {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -",    "h5f6", 10},
    {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -",                      "f4f5", 10},
    {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -",        "f1d3", 10},
    {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -",            "c6e5", 10},
    {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -",       "f2f3", 10},
    {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -",                   "d7f5", 10},
    {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -",              "a1c1", 10},
    {"rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -",        "d1e1", 10},
    {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -",          "g4g7", 10},
    {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -",         "d2e4", 10},
    {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -",  "a8c8", 10},
    {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -",      "f7f5", 10},
    {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -",                   "e8e4", 12},
    {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -",          "c3b5", 12},
    {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -",             "f5h6", 10},
    {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -",    "b7e4", 10},
    {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -",           "c8f5", 10},
    {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -",       "e4f5", 10},
};
// clang-format on

static const int NUM_CASES = (int)(sizeof(CASES) / sizeof(CASES[0]));

int main(void)
{
    int failures = 0;

    for (int i = 0; i < NUM_CASES; ++i)
    {
        const bk_case_t *tc = &CASES[i];

        game_t game;
        game_init(&game);
        game_new_game(&game, tc->fen);
        game.time_control.clock_type = CLOCK_FIXED_DEPTH;
        game.time_control.depth      = tc->depth;

        debug_x_clear();
        move_t best = search_root_node(&game);
        debug_x_write();

        char got[8] = {0};
        if (best != move_none())
            move_to_string(best, got, sizeof(got));

        int pass = (best != move_none()) && (strcmp(got, tc->expected_move) == 0);
        printf("[%s]  pos%02d  got=%-5s  expected=%-5s  %s\n\n", pass ? "PASS" : "FAIL", i + 1, got, tc->expected_move,
               tc->fen);
        fflush(stdout);

        if (!pass)
            failures++;

        game_free(&game);
    }

    printf("\n%d/%d passed\n", NUM_CASES - failures, NUM_CASES);
    return failures > 0 ? 1 : 0;
}
