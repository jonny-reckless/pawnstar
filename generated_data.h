#pragma once

#include "types.h"

extern const bitboard NORTH_OF[64];
extern const bitboard NORTHEAST_OF[64];
extern const bitboard EAST_OF[64];
extern const bitboard SOUTHEAST_OF[64];
extern const bitboard SOUTH_OF[64];
extern const bitboard SOUTHWEST_OF[64];
extern const bitboard WEST_OF[64];
extern const bitboard NORTHWEST_OF[64];
extern const bitboard PAWN_ATTACKS_WHITE[64];
extern const bitboard PAWN_ATTACKS_BLACK[64];
extern const bitboard KNIGHT_ATTACKS[64];
extern const bitboard BISHOP_ATTACKS[64];
extern const bitboard ROOK_ATTACKS[64];
extern const bitboard QUEEN_ATTACKS[64];
extern const bitboard KING_ATTACKS[64];
extern const bitboard INTERVENING_SQUARES[64][64];
extern const int64    PIECE_SQUARE_HASHES[2][8][64];
extern const int64    CASTLING_RIGHTS_HASHES[16];
extern const int64    EN_PASSANT_HASHES[8];
