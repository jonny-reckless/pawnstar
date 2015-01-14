#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "bitboard_constants.h"
/******************************************************************************
Function like macros
*******************************************************************************/
#define FILE_OF(locn)               ((locn) & 7)
#define RANK_OF(locn)               ((locn) >> 3)
#define FILE_CHAR(locn)             ('a' + ((locn) & 7))
#define RANK_CHAR(locn)             ('1' + ((locn) >> 3))
#define BITBOARD(locn)              (1ull << (locn))
#define BITBOARD_XY(x,y)            (1ull << ((x) + 8 * (y)))
#define FILE_BITBOARD(locn)         (FILE_A << ((locn) & 0x07))
#define RANK_BITBOARD(locn)         (RANK_1 << ((locn) & 0x38))
#define SHIFT_NORTH(b)              ((b) << 8)
#define SHIFT_NORTHEAST(b)          (((b) & MASK_EAST_1) << 9)
#define SHIFT_EAST(b)               (((b) & MASK_EAST_1) << 1)
#define SHIFT_SOUTHEAST(b)          (((b) & MASK_EAST_1) >> 7)
#define SHIFT_SOUTH(b)              ((b) >> 8)
#define SHIFT_SOUTHWEST(b)          (((b) & MASK_WEST_1) >> 9)
#define SHIFT_WEST(b)               (((b) & MASK_WEST_1) >> 1)
#define SHIFT_NORTHWEST(b)          (((b) & MASK_WEST_1) << 7)
#define ENEMY(color)                (!(color))
#define HAS_SINGLE_BIT_SET(x)       ((x) && !((x) & ((x) - 1)))
#define ERROR(x)                    { printf(x) ; exit(1); }