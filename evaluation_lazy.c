#include "pawnstar.h"

static const int SCORE_MATERIAL_VALUES[7] = {
      0,    // not used
    100,    // pawn
    325,    // knight
    325,    // bishop
    500,    // rook
    900,    // queen
      0,    // king (N/A)
};

/******************************************************************************
Material only evaluation
*******************************************************************************/
int EvaluateMaterial(const Position* position)
{
    int score = 0;
    int piece;
    for (piece = PAWN; piece <= QUEEN; ++piece)
    {
        const bitboard b = position->pieces[piece];
        score += SCORE_MATERIAL_VALUES[piece] * (PopCount(b & position->whitePieces) - PopCount(b & position->blackPieces));
    }
    return position->stateFlags & IS_BLACK_TO_MOVE ? -score : score;
}