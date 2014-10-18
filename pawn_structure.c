#include "pawnstar.h"
/******************************************************************************
Identifies the pawn-related features of the current position
*******************************************************************************/
void DeterminePawnStructure(const Position* position, PawnStructure ps[2])
{
    const bitboard whitePawns = position->pawns & position->whitePieces;
    const bitboard blackPawns = position->pawns & position->blackPieces;

    const bitboard whiteAttacks = SHIFT_NORTHWEST(whitePawns) | SHIFT_NORTHEAST(whitePawns);
    const bitboard blackAttacks = SHIFT_SOUTHWEST(blackPawns) | SHIFT_SOUTHEAST(blackPawns);

    const bitboard whitePotentialAttacks = FillNorth(whiteAttacks);
    const bitboard blackPotentialAttacks = FillSouth(blackAttacks);
    
    const bitboard whitePawnFiles = FillNorthAndSouth(whitePawns);
    const bitboard blackPawnFiles = FillNorthAndSouth(blackPawns);
    
    const bitboard whiteDoubledPawns = whitePawns & FillSouth(SHIFT_SOUTH(whitePawns));
    const bitboard blackDoubledPawns = blackPawns & FillNorth(SHIFT_NORTH(blackPawns));
    
    const bitboard whitePotentialPushes = FillNorth(SHIFT_NORTH(whitePawns));
    const bitboard blackPotentialPushes = FillSouth(SHIFT_SOUTH(blackPawns));

    ps[WHITE].pawnAttacks = whiteAttacks;
    ps[BLACK].pawnAttacks = blackAttacks;

    ps[WHITE].doubledPawns = whiteDoubledPawns;
    ps[BLACK].doubledPawns = blackDoubledPawns;

    ps[WHITE].defendedPawns = whitePawns & whiteAttacks;
    ps[BLACK].defendedPawns = blackPawns & blackAttacks;

    ps[WHITE].pawnHoles = ~(RANK_1 | RANK_2 | whitePotentialAttacks);
    ps[BLACK].pawnHoles = ~(RANK_7 | RANK_8 | blackPotentialAttacks);

    ps[WHITE].isolatedPawns = whitePawns & ~(SHIFT_WEST(whitePawnFiles) | SHIFT_EAST(whitePawnFiles));
    ps[BLACK].isolatedPawns = blackPawns & ~(SHIFT_WEST(blackPawnFiles) | SHIFT_EAST(blackPawnFiles));

    ps[WHITE].blockedPawns = whitePawns & blackPotentialPushes & ~blackPotentialAttacks;
    ps[BLACK].blockedPawns = blackPawns & whitePotentialPushes & ~whitePotentialAttacks;

    ps[WHITE].passedPawns = whitePawns & ~(whiteDoubledPawns | blackPotentialAttacks | blackPotentialPushes);
    ps[BLACK].passedPawns = blackPawns & ~(blackDoubledPawns | whitePotentialAttacks | whitePotentialPushes);

    ps[WHITE].outposts = ps[BLACK].pawnHoles & whiteAttacks;
    ps[BLACK].outposts = ps[WHITE].pawnHoles & blackAttacks;
}