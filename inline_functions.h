#pragma once

#include "types.h"
#include "bitboard_constants.h"
#include "generated_data.h"

#define INLINE static __forceinline 
/******************************************************************************
Find the index of the least significant bit set in a bitboard, also known as
bit scan forward, or trailing zero count - bitboard must be non zero.
*******************************************************************************/
#if _M_X64
INLINE int Lsb(bitboard x)
{
    unsigned long index;
    _BitScanForward64(&index, x);
    return (int)index;
}
#else
INLINE int Lsb(bitboard x)
{
    unsigned long index;
    if ((unsigned long)x)
    {
        _BitScanForward(&index, (unsigned long)x);
        return (int)index;
    }
    _BitScanForward(&index, (unsigned long)(x >> 32));
    return (int)(index + 32);
}
#endif
/******************************************************************************
Population count (Hamming weight)
*******************************************************************************/
INLINE int PopCnt(bitboard x)
{
#if _M_X64
    return (int)__popcnt64(x);
#else
    return __popcnt((int)x) + __popcnt((int)(x >> 32));
#endif
}
/******************************************************************************
Find the index of and clear the least significant bit set in a bitboard
*******************************************************************************/
INLINE int FindAndClearLsb(bitboard* x)
{
    const int result = Lsb(*x);
#if _M_X64
    _bittestandreset64((__int64*)x, result);
#else
    *x &= *x - 1;
#endif
    return result;
}
/******************************************************************************
Bitboard vector fill functions:
These functions shift in the specified direction by 1, 2 and 4 squares 
successively, masking off the appropriate edge files to avoid wraparound in the
larger shifts, and ORing in the results, which "fills" the bitboard to the edge
of the board in the specified direction in 3 successive shift-or operations.
These functions are used to determine sliding piece attacks in the presence of 
blocking pieces. NB: masking is not required when shifting north and south, 
since the files remain unchanged in these directions.
*******************************************************************************/
INLINE bitboard FillNorth(bitboard b)
{
    b |= b <<  8;
    b |= b << 16;
    b |= b << 32;
    return b;
}
INLINE bitboard FillNortheast(bitboard b)
{
    b |= (b & MASK_EAST_1) <<  9;
    b |= (b & MASK_EAST_2) << 18;
    b |= (b & MASK_EAST_4) << 36;
    return b;
}
INLINE bitboard FillEast(bitboard b)
{
    b |= (b & MASK_EAST_1) << 1;
    b |= (b & MASK_EAST_2) << 2;
    b |= (b & MASK_EAST_4) << 4;
    return b;
}
INLINE bitboard FillSoutheast(bitboard b)
{
    b |= (b & MASK_EAST_1) >>  7;
    b |= (b & MASK_EAST_2) >> 14;
    b |= (b & MASK_EAST_4) >> 28;
    return b;
}
INLINE bitboard FillSouth(bitboard b)
{
    b |= b >>  8;
    b |= b >> 16;
    b |= b >> 32;
    return b;
}
INLINE bitboard FillSouthwest(bitboard b)
{
    b |= (b & MASK_WEST_1) >>  9;
    b |= (b & MASK_WEST_2) >> 18;
    b |= (b & MASK_WEST_4) >> 36;
    return b;
}
INLINE bitboard FillWest(bitboard b)
{
    b |= (b & MASK_WEST_1) >> 1;
    b |= (b & MASK_WEST_2) >> 2;
    b |= (b & MASK_WEST_4) >> 4;
    return b;
}
INLINE bitboard FillNorthwest(bitboard b)
{
    b |= (b & MASK_WEST_1) <<  7;
    b |= (b & MASK_WEST_2) << 14;
    b |= (b & MASK_WEST_4) << 28;
    return b;
}
INLINE bitboard FillNorthAndSouth(bitboard b)
{
    b |= (b <<  8) | (b >>  8);
    b |= (b << 16) | (b >> 16);
    b |= (b << 32) | (b >> 32);
    return b;
}

INLINE bitboard BishopAttacks(bitboard occupied_squares, int location)
{
    return 
        BISHOP_ATTACKS[location]                                                  ^
        FillNortheast(SHIFT_NORTHEAST(NORTHEAST_OF[location] & occupied_squares)) ^
        FillSoutheast(SHIFT_SOUTHEAST(SOUTHEAST_OF[location] & occupied_squares)) ^
        FillSouthwest(SHIFT_SOUTHWEST(SOUTHWEST_OF[location] & occupied_squares)) ^
        FillNorthwest(SHIFT_NORTHWEST(NORTHWEST_OF[location] & occupied_squares));
}

INLINE bitboard RookAttacks(bitboard occupied_squares, int location)
{
    return
        ROOK_ATTACKS[location]                                        ^
        FillNorth(SHIFT_NORTH(NORTH_OF[location] & occupied_squares)) ^
        FillEast (SHIFT_EAST ( EAST_OF[location] & occupied_squares)) ^
        FillSouth(SHIFT_SOUTH(SOUTH_OF[location] & occupied_squares)) ^
        FillWest (SHIFT_WEST ( WEST_OF[location] & occupied_squares));
}

INLINE bitboard QueenAttacks(bitboard occupied_squares, int location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}