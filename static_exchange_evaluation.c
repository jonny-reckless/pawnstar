#include "pawnstar.h"

static int EvaluateSwapOff(Position* position, int location, int color, int pieceOnSquare);

extern const bitboard* const ENEMY_PAWN_ATTACKS[2];
/******************************************************************************
Use nominal 1-3-3-5-9 material values for SEE
*******************************************************************************/
static const int PIECE_VALUES[8] = { 0, 100, 300, 300, 500, 900, 10000, 0 };

/******************************************************************************
Determine the SEE (static exchange evaluation) for a move.
Refer to: http://chessprogramming.wikispaces.com/Static+Exchange+Evaluation
*******************************************************************************/
int EvaluateStaticExchange(const Position* srcPosition, int move)
{
    Position position[1];
    MakeMove(position, srcPosition, move);
    if (MOVE_PROMOTED(move))
    {
        return PIECE_VALUES[MOVE_CAPTURED(move)] + PIECE_VALUES[MOVE_PROMOTED(move)] - PIECE_VALUES[PAWN] - 
            EvaluateSwapOff(position, MOVE_TO(move), COLOR_TO_MOVE(position), MOVE_PROMOTED(move));
    }
    return PIECE_VALUES[MOVE_CAPTURED(move)] - 
        EvaluateSwapOff(position, MOVE_TO(move), COLOR_TO_MOVE(position), MOVE_PIECE(move));
}
/******************************************************************************
Recursively determine the exchange swap off value on a specific square.

For optimum speed, we neither copy the position, update the state flags nor 
update the hash during evaluation of the static exchange, as none of these is
required by the function.

NB: This function destroys its position during execution.

I tried replacing this recursive form with its iterative equivalent, but saw no
measurable speed gain, and the iterative form was much less easy to understand
and debug, so the recursive version remains...
*******************************************************************************/
static int EvaluateSwapOff(Position* position, int location, int color, int pieceOnSquare)
{
    int capturingPiece;
    int score;
    const bitboard attackingPieces = position->allPieces[color];
    const bitboard* const interveningSquares = &INTERVENING_SQUARES[location][0];
    bitboard sliders, square;
    /**************************************************************************
    Find the least valuable piece of "color" which directly attacks "location"
    ***************************************************************************/
    bitboard attacker = ENEMY_PAWN_ATTACKS[color][location] & attackingPieces & position->pawns;
    if (attacker)
    {
        attacker &= -attacker; // isolate LSB in case there is more than 1 pawn attacker
        capturingPiece = PAWN;
        goto FoundAttacker;
    }
    attacker = KNIGHT_ATTACKS[location] & attackingPieces & position->knights;
    if (attacker)
    {
        attacker &= -attacker; // isolate LSB in case there is more than 1 knight attacker
        capturingPiece = KNIGHT;
        goto FoundAttacker;
    }
    sliders = BISHOP_ATTACKS[location] & attackingPieces & position->bishops;
    while (sliders)
    {
        const int sliderLocn = FindAndClearLsb(&sliders);
        if (!(interveningSquares[sliderLocn] & position->occupiedSquares))
        {
            capturingPiece = BISHOP;
            attacker = BITBOARD(sliderLocn);
            goto FoundAttacker;
        }
    }
    sliders = ROOK_ATTACKS[location] & attackingPieces & position->rooks;
    while (sliders)
    {
        const int sliderLocn = FindAndClearLsb(&sliders);
        if (!(interveningSquares[sliderLocn] & position->occupiedSquares))
        {
            capturingPiece = ROOK;
            attacker = BITBOARD(sliderLocn);
            goto FoundAttacker;
        }
    }
    sliders = QUEEN_ATTACKS[location] & attackingPieces & position->queens;
    while (sliders)
    {
        const int sliderLocn = FindAndClearLsb(&sliders);
        if (!(interveningSquares[sliderLocn] & position->occupiedSquares))
        {
            capturingPiece = QUEEN;
            attacker = BITBOARD(sliderLocn);
            goto FoundAttacker;
        }
    }
    attacker = KING_ATTACKS[location] & attackingPieces & position->kings;
    if (attacker)
    {
        capturingPiece = KING;
        goto FoundAttacker;
    }

    return 0; // no more attackers to this square

FoundAttacker:
    square = BITBOARD(location);
    position->pieces[pieceOnSquare]     ^= square;
    position->allPieces[ENEMY(color)]   ^= square;
    position->pieces[capturingPiece]    ^= attacker | square;
    position->allPieces[color]          ^= attacker | square;
    position->occupiedSquares            = position->whitePieces | position->blackPieces;
    score = PIECE_VALUES[pieceOnSquare] - EvaluateSwapOff(position, location, ENEMY(color), capturingPiece);
    return score > 0 ? score : 0;
}
