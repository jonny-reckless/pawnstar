#include <string>
#include <iostream>

#include "position.h"
#include "move_generator.h"
/******************************************************************************
Structure to hold the counts of different move types
*******************************************************************************/
struct PerftCounts
{
    int legal_moves;
    int captures;
    int ep_captures;
    int castles;
    int promotions;
    int checks;
};
/******************************************************************************
Structure to hold a single perft test
*******************************************************************************/
struct PerftTest
{
    std::string position;
    int         depth;
    PerftCounts counts;
};
/******************************************************************************
Standard test positions for Perft move generation tests
Refer to:
http://chessprogramming.wikispaces.com/Perft
http://chessprogramming.wikispaces.com/Perft+Results
*******************************************************************************/

static const PerftTest PERFT_TESTS[] =
{//    position                                                          depth        nodes   captures      ep   castles promotions   checks
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",                6, { 119060324,   2812008,   5248,        0,        0,   809099 } },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",    5, { 193690690,  35043416,  73365,  4993637,     8392,  3309887 } },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               7, { 178633661,  14519036, 294874,        0,   140024, 12797406 } },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",        6, { 706045033, 210369132,    212, 10882006, 81102984, 26973664 } },
    { "rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R w KQkq -",           3, {     53392,      4381,     75,      969,        0,     2269 } },
    { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -", 5, { 164075551,  19528068,    122,        0,        0,  2998608 } },
};
/*
static const PerftTest PERFT_TESTS[] =
{//    position                                                          depth        nodes   captures      ep   castles promotions   checks
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",                4, { 119060324,   2812008,   5248,        0,        0,   809099 } },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",    4, { 193690690,  35043416,  73365,  4993637,     8392,  3309887 } },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               4, { 178633661,  14519036, 294874,        0,   140024, 12797406 } },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",        4, { 706045033, 210369132,    212, 10882006, 81102984, 26973664 } },
    { "rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R w KQkq -",           4, {     53392,      4381,     75,      969,        0,     2269 } },
    { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -", 4, { 164075551,  19528068,    122,        0,        0,  2998608 } },
};
*/
/******************************************************************************
Categorize a null-terminated set of pseudo-legal moves into strictly-legal
moves of various types, storing the result in counts
*******************************************************************************/
static void CategorizeMoves(Position& position, PerftCounts& counts)
{
    MoveGenerator gen(position, true);
    MoveUndoCtx ctx;
    for (Move move; gen(move); )
    {
        if (position.MakeMove(move, ctx))
        {
            ++counts.legal_moves;
            if (ctx.captured_piece)
            {
                ++counts.captures;
            }
            switch (move.type)
            {
            default:
            case MOVE_REGULAR:
                break;
            case MOVE_CASTLING:
                ++counts.castles;
                break;
            case MOVE_EN_PASSANT_CAPTURE:
                ++counts.ep_captures;
                break;
            case MOVE_PROMOTION_QUEEN:
            case MOVE_PROMOTION_ROOK:
            case MOVE_PROMOTION_BISHOP:
            case MOVE_PROMOTION_KNIGHT:
                ++counts.promotions;
                break;
            }
            if (position.IsInCheck())
            {
                ++counts.checks;
            }
        }
        position.UndoMove(move, ctx);
    }
}
/******************************************************************************
Recursive standard perft test
Refer to:
http://chessprogramming.wikispaces.com/Perft
http://chessprogramming.wikispaces.com/Perft+Results
*******************************************************************************/
static int Perft(Position& position, int depth, PerftCounts& counts)
{
    static int call_count = 0;
    if (!(++call_count & 0x3FFFF))
    {
        printf("\rpositions processed %10d", counts.legal_moves);
    }
    if (depth > 1)
    {
        MoveGenerator gen(position, true);
        MoveUndoCtx ctx;
        for (Move move; gen(move); )
        {
            if (position.MakeMove(move, ctx))
            {
                Perft(position, depth - 1, counts);
            }
            position.UndoMove(move, ctx);
        }
        return counts.legal_moves;
    }
    CategorizeMoves(position, counts);
    return counts.legal_moves;
}
/******************************************************************************
Run perft test on the standard test positions
*******************************************************************************/
#include <Windows.h>
void RunPerftTests()
{
    bool pass = true;
    int total_nodes = 0;
    int first_start = GetTickCount();
    int stop = 0;
    for (const PerftTest& test : PERFT_TESTS)
    {
        std::cout << test.position << std::endl;
        Position position(test.position);
        PerftCounts counts = { 0 };
        total_nodes += test.counts.legal_moves;
        int start = GetTickCount();
        Perft(position, test.depth, counts);
        stop = GetTickCount();
        if (stop == start)
        {
            stop = start + 1; // avoid divide by zero error in positions per second for short tests
        }
        printf("\rdepth                                   %10u\n", test.depth);
        printf(  "positions                               %10u\n", counts.legal_moves);
        printf(  "captures                                %10u\n", counts.captures);
        printf(  "ep captures                             %10u\n", counts.ep_captures);
        printf(  "castles                                 %10u\n", counts.castles);
        printf(  "promotions                              %10u\n", counts.promotions);
        printf(  "checks                                  %10u\n", counts.checks);
        printf(  "elapsed milliseconds                    %10u\n", stop - start);
        printf(  "positions per second                    %10u\n", counts.legal_moves / (stop - start) * 1000);
        if (!!memcmp(&counts, &test.counts, sizeof(PerftCounts)))
        {
            printf("************* ERROR on this position *************\n");
            pass = false;
        }
    }
    printf("total elapsed milliseconds              %10u\n", stop - first_start);
    printf("mean positions per second               %10u\n", total_nodes / (stop - first_start) * 1000);
    if (pass)
    {
        printf("******************* PERFT PASS *******************\n");
    }
    else
    {
        printf("******************* PERFT FAIL *******************\n");
    }
}