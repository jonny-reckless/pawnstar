/// @file Google Test suite for static exchange evaluation.

#define DEBUGX 0

extern "C"
{
#include "move.h"
#include "piece.h"
#include "position.h"
#include "square.h"
#include "static_exchange_evaluation.h"
}

#include <gtest/gtest.h>

TEST(SEE, RookCapturesPawn)
{
    // Rook on e1 captures a pawn on e5; no other pieces contest e5.
    position_t   pos = position_from_string("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
    move_t       m   = move_capture(square_from_string("e1"), square_from_string("e5"), ROOK, PAWN);
    see_result_t r   = evaluate_static_exchange(&pos, m);
    EXPECT_EQ(r.score, 100);
}

TEST(SEE, KnightCapturesPawnLoses)
{
    // Knight on d3 captures a pawn on e5, but is recaptured by multiple defenders.
    position_t   pos = position_from_string("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
    move_t       m   = move_capture(square_from_string("d3"), square_from_string("e5"), KNIGHT, PAWN);
    see_result_t r   = evaluate_static_exchange(&pos, m);
    EXPECT_EQ(r.score, -200);
}
