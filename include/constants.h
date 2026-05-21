#pragma once
/// @file Program configuration constants.
#include <stdint.h>

typedef uint64_t zobrist_t; ///< Zobrist hash type.

static const int HASHTABLE_MEGABYTES = 64;                ///< default transposition table size in MB
#define MAX_MOVES_PER_POSITION 256                        ///< maximum possible number of moves for a chess position
static const int BETA             = 11000;                ///< greater than any possible evaluation score
static const int ALPHA            = -11000;               ///< smaller than any possible evaluation score
static const int CHECKMATED_SCORE = -10000;               ///< score for losing the game
static const int WIN_THRESHOLD    = 9000;                 ///< score threshold for winning at any ply
static const int LOSE_THRESHOLD   = -9000;                ///< score threshold for losing at any ply
static const int DRAW_SCORE       = 0;                    ///< score for achieving a draw
#define MAX_PLY 64                                        ///< terminate the search at this ply no matter what
static const int RANK_FLIP                   = 0x38;      ///< used as an XOR mask to flip the board horizontally
static const int SEARCH_CANCELLED_SCORE      = -12345678; ///< illegal value returned when search is cancelled
static const int SCORE_INSTABILITY_THRESHOLD = 50;        ///< window of score variability
static const int START_DEPTH                 = 3;         ///< depth to do full width search
static const int MEGABYTE                    = 1 << 20;   ///< One mebibyte (2^20 bytes).
