#include "pawnstar.h"
extern const bitboard* const ENEMY_PAWN_ATTACKS[2];
/******************************************************************************
Functions to generate pseudo-legal and strictly-legal moves. Pseudo-legal moves
may leave our king in check; this is tested during search for improved 
efficiency.

Moves are contained within the least significant 24 bits of an integer

  Bits      Interpretation
  
 0 -  5     to (destination square index)
 6 - 11     from (source square index)
12 - 14     moving piece type
15 - 17     captured piece type in the case of capture moves
18 - 20     promoted piece type in the case of pawn promotions
21 - 23     move type

A value of 0 terminates a move list

Given a first pawn promotion move to queen, generate the under promotions to
knight, bishop and rook
*******************************************************************************/
INLINE void GenerateUnderpromotions(int** pmoves)
{
    int* const moves = *pmoves;
    const int move   = *moves;
    moves[1] = move ^ ((ROOK   ^ QUEEN) << 18);
    moves[2] = move ^ ((BISHOP ^ QUEEN) << 18);
    moves[3] = move ^ ((KNIGHT ^ QUEEN) << 18);
    *pmoves += 4;
}
/******************************************************************************
Generate pseudo-legal moves for all pieces
If doAllMoves is false, just do captures and promotions (when not in check)
Returns the end of the buffer (one past the last move generated)
*******************************************************************************/
int* GeneratePseudoLegalMoves(const Position* position, int moves[], bool doAllMoves)
{ 
    bitboard   sources;
    bitboard   dests;
    bitboard   pawns;
    bitboard   promotionsWest;
    bitboard   promotionsEast;
    bitboard   promotions;
    bitboard   capturesWest;
    bitboard   capturesEast;
    bitboard   doublePushes;
    bitboard   singlePushes;
    bitboard   enPassantSources;
    int        pushDelta;
    int        westDelta;
    int        eastDelta;
    uchar      pieceType;
    uchar      boardPieces[64]   = { NO_PIECE };
    const int color              = COLOR_TO_MOVE(position);
    const bitboard vacantSquares = ~position->occupiedSquares;
    /**************************************************************************
    Determine the pieces on the board once, to save continually looking them up
    as we generate moves.
    ***************************************************************************/
    for (pieceType = PAWN; pieceType <= KING; ++pieceType)
    {
        bitboard b = position->pieces[pieceType];
        while (b)
        {
            boardPieces[FindAndClearLsb(&b)] = pieceType;
        }
    }
    /**************************************************************************
    Generate pawn moves
    ***************************************************************************/
    if (color == WHITE)
    {
        pawns            = position->pawns           & position->whitePieces;
        singlePushes     = SHIFT_NORTH(pawns)        & vacantSquares;
        doublePushes     = SHIFT_NORTH(singlePushes) & vacantSquares & RANK_4;
        capturesWest     = SHIFT_NORTHWEST(pawns)    & position->blackPieces;
        capturesEast     = SHIFT_NORTHEAST(pawns)    & position->blackPieces;
        enPassantSources = position->enPassantIndex ? PAWN_ATTACKS_BLACK[position->enPassantIndex] & pawns : NO_SQUARES;
        promotions       = singlePushes & RANK_8;
        promotionsWest   = capturesWest & RANK_8;
        promotionsEast   = capturesEast & RANK_8;   
        capturesWest    ^= promotionsWest;
        capturesEast    ^= promotionsEast;
        singlePushes    ^= promotions;
        pushDelta        = 8;
        westDelta        = 7;
        eastDelta        = 9;
    }
    else
    {
        pawns            = position->pawns           & position->blackPieces;
        singlePushes     = SHIFT_SOUTH(pawns)        & vacantSquares;
        doublePushes     = SHIFT_SOUTH(singlePushes) & vacantSquares & RANK_5;
        capturesWest     = SHIFT_SOUTHWEST(pawns)    & position->whitePieces;
        capturesEast     = SHIFT_SOUTHEAST(pawns)    & position->whitePieces;
        enPassantSources = position->enPassantIndex ? PAWN_ATTACKS_WHITE[position->enPassantIndex] & pawns : NO_SQUARES;
        promotions       = singlePushes & RANK_1;
        promotionsWest   = capturesWest & RANK_1;
        promotionsEast   = capturesEast & RANK_1;   
        capturesWest    ^= promotionsWest;
        capturesEast    ^= promotionsEast;
        singlePushes    ^= promotions;
        pushDelta        = -8;
        westDelta        = -9;
        eastDelta        = -7;
    }
    while (promotionsWest)
    {
        const int to = FindAndClearLsb(&promotionsWest);
        *moves = CONSTRUCT_PROMOTION_MOVE(to - westDelta, to, PAWN_PROMOTION_CAPTURE, boardPieces[to], QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotionsEast)
    {
        const int to = FindAndClearLsb(&promotionsEast);
        *moves = CONSTRUCT_PROMOTION_MOVE(to - eastDelta, to, PAWN_PROMOTION_CAPTURE, boardPieces[to], QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (promotions)
    {
        const int to = FindAndClearLsb(&promotions);
        *moves = CONSTRUCT_PROMOTION_MOVE(to - pushDelta, to, PAWN_PROMOTION_NON_CAPTURE, NO_PIECE, QUEEN);
        GenerateUnderpromotions(&moves);
    }
    while (capturesWest)
    {
        const int to = FindAndClearLsb(&capturesWest);
        *moves++ = CONSTRUCT_MOVE(to - westDelta, to, PAWN, CAPTURE, boardPieces[to]);
    }
    while (capturesEast)
    {
        const int to = FindAndClearLsb(&capturesEast);
        *moves++ = CONSTRUCT_MOVE(to - eastDelta, to, PAWN, CAPTURE, boardPieces[to]);
    }
    while (enPassantSources)
    {
        *moves++ = CONSTRUCT_MOVE(FindAndClearLsb(&enPassantSources), position->enPassantIndex, PAWN, EN_PASSANT_CAPTURE, PAWN);
    }
    if (doAllMoves)
    {
        pushDelta <<= 1;
        while (doublePushes)
        {
            const int to = FindAndClearLsb(&doublePushes);
            *moves++ = CONSTRUCT_MOVE(to - pushDelta, to, PAWN, DOUBLE_PAWN_PUSH, NO_PIECE);
        }
        pushDelta >>= 1;
        while (singlePushes)
        {
            const int to = FindAndClearLsb(&singlePushes);
            *moves++ = CONSTRUCT_MOVE(to - pushDelta, to, PAWN, SINGLE_PAWN_PUSH, NO_PIECE);
        }
    }
    /**************************************************************************
    Generate non pawn piece moves
    ***************************************************************************/
    sources = (position->occupiedSquares ^ position->pawns) & position->allPieces[color];
    dests   = doAllMoves ? ~position->allPieces[color] : position->allPieces[ENEMY(color)];
    while (sources)
    {
        const int from  = FindAndClearLsb(&sources);
        const int piece = boardPieces[from];
        bitboard targets = DetermineAttacksFromSquare(position, from, piece) & dests;
        while (targets)
        {
            const int to = FindAndClearLsb(&targets);
            if (boardPieces[to])
            {
                *moves++ = CONSTRUCT_MOVE(from, to, piece, CAPTURE, boardPieces[to]);
            }
            else
            {
                *moves++ = CONSTRUCT_NON_CAPTURE_MOVE(from, to, piece);
            }
        }
    }
    if (doAllMoves && !(position->stateFlags & IS_CHECK))
    {
        /**********************************************************************
        Generate castling moves
        ***********************************************************************/
        if (color == WHITE && !(position->castleFlags & HAS_WHITE_CASTLED))
        {
            if ((position->castleFlags & MAY_WHITE_K)           &&  // if white retains the right to castle kingside and
                !(position->occupiedSquares & (F1BB | G1BB)) && // f1 and g1 are both vacant and                
                !IsAttacked(position, F1, BLACK)                &&  // f1 is not attacked by black and
                !IsAttacked(position, G1, BLACK))                   // the king's destination is not attacked by black
            {
                *moves++ = CONSTRUCT_MOVE(E1, G1, KING, CASTLING, NO_PIECE);
            }
            if ((position->castleFlags & MAY_WHITE_Q)                   &&
                !(position->occupiedSquares & (B1BB | C1BB | D1BB)) &&                              
                !IsAttacked(position, D1, BLACK)                        &&
                !IsAttacked(position, C1, BLACK))
            {
                *moves++ = CONSTRUCT_MOVE(E1, C1, KING, CASTLING, NO_PIECE);
            }
        }
        else if (color == BLACK && !(position->castleFlags & HAS_BLACK_CASTLED))
        {
            if ((position->castleFlags & MAY_BLACK_K)           &&
                !(position->occupiedSquares & (F8BB | G8BB)) &&                                     
                !IsAttacked(position, F8, WHITE)                &&
                !IsAttacked(position, G8, WHITE))
            {
                *moves++ = CONSTRUCT_MOVE(E8, G8, KING, CASTLING, NO_PIECE);
            }
            if ((position->castleFlags & MAY_BLACK_Q)                   &&
                !(position->occupiedSquares & (B8BB | C8BB | D8BB)) &&                              
                !IsAttacked(position, D8, WHITE)                        &&
                !IsAttacked(position, C8, WHITE))
            {
                *moves++ = CONSTRUCT_MOVE(E8, C8, KING, CASTLING, NO_PIECE);
            }
        }
    }
    *moves = 0;
    return moves;
}
/******************************************************************************
Generate all strictly legal moves for this position
This is relatively slow and is not used at each node of the search, since each 
move has to be tested to see if it leaves the king in check
Returns the number of moves generated
*******************************************************************************/
int GenerateLegalMoves(const Position* position, int moves[])
{
    const int* const initialPtr = moves;
    const int* move;
    int pseudoLegalMoves[MAX_MOVES_PER_POSITION];
    Position dstPosition[1];
    GeneratePseudoLegalMoves(position, pseudoLegalMoves, true);
    for (move = pseudoLegalMoves; *move; ++move)
    {
        MakeMove(dstPosition, position, *move);
        if (!(dstPosition->stateFlags & WAS_MOVE_ILLEGAL))
        {
            *moves++ = *move;
        }
    }
    *moves = 0;
    return (int)(moves - initialPtr);
}
