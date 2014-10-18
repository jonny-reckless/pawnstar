#include "pawnstar.h"
/******************************************************************************
Determine any pinned pieces and their corresponding allowed target squares
NB: the contents of pins->allowedSquares[x] for non-pinned pieces is garbage
(whatever happened to be there before we were called)
*******************************************************************************/
void DeterminePins(const Position* position, Pins* pins)
{
    int color;
    pins->pinnedPieces = NO_SQUARES;
    for (color = WHITE; color <= BLACK; ++color)
    {
        const int kingLocation = position->kingLocation[color];
        const bitboard* const interveningSquaresArr = &INTERVENING_SQUARES[kingLocation][0];
        const bitboard friendlyPieces = position->allPieces[color];
        bitboard enemySlidingPieces = 
            ((BISHOP_ATTACKS[kingLocation] & (position->bishops | position->queens)) | 
            (   ROOK_ATTACKS[kingLocation] & (position->rooks   | position->queens))) & ~friendlyPieces;
        while (enemySlidingPieces)
        {
            const int sliderLocation                = FindAndClearLsb(&enemySlidingPieces);
            const bitboard interveningSquares       = interveningSquaresArr[sliderLocation];
            const bitboard interveningPieces        = interveningSquares & position->occupiedSquares;
            const bitboard interveningFriendlyPiece = interveningPieces & friendlyPieces;           
            if (interveningFriendlyPiece && HAS_SINGLE_BIT_SET(interveningPieces))
            {
                pins->pinnedPieces |= interveningFriendlyPiece;
                pins->allowedSquares[Lsb(interveningFriendlyPiece)] = interveningSquares | BITBOARD(sliderLocation);
            }
        }
    }
}