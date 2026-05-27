#pragma once
/// @file Program configuration constants.
#include <stdint.h>

typedef uint64_t zobrist_t; ///< Zobrist hash type.

#define MAX_MOVES_PER_POSITION 256 ///< maximum possible number of moves for a chess position
#define MAX_PLY                64  ///< terminate the search at this ply no matter what
#ifndef HASHTABLE_MEGABYTES
#define HASHTABLE_MEGABYTES 64 ///< default transposition table size in MB
#endif
#define BETA                        11000       ///< greater than any possible evaluation score
#define ALPHA                       (-11000)    ///< smaller than any possible evaluation score
#define CHECKMATED_SCORE            (-10000)    ///< score for losing the game
#define WIN_THRESHOLD               9000        ///< score threshold for winning at any ply
#define LOSE_THRESHOLD              (-9000)     ///< score threshold for losing at any ply
#define DRAW_SCORE                  0           ///< score for achieving a draw
#define RANK_FLIP                   0x38        ///< used as an XOR mask to flip the board horizontally
#define SEARCH_CANCELLED_SCORE      (-12345678) ///< illegal value returned when search is cancelled
#define SCORE_INSTABILITY_THRESHOLD 50          ///< window of score variability
#define START_DEPTH                 3           ///< depth to do full width search
#define MEGABYTE                    (1 << 20)   ///< One mebibyte (2^20 bytes).
