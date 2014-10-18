#include "pawnstar.h"

const bitboard* const ENEMY_PAWN_ATTACKS[2] = { PAWN_ATTACKS_BLACK, PAWN_ATTACKS_WHITE };
const bitboard* const PAWN_ATTACKS[2]       = { PAWN_ATTACKS_WHITE, PAWN_ATTACKS_BLACK };

bitboard BishopAttacks(bitboard occupiedSquares, int location)
{
    return 
        BISHOP_ATTACKS[location]                                                 ^
        FillNorthEast(SHIFT_NORTHEAST(NORTHEAST_OF[location] & occupiedSquares)) ^
        FillSouthEast(SHIFT_SOUTHEAST(SOUTHEAST_OF[location] & occupiedSquares)) ^
        FillSouthWest(SHIFT_SOUTHWEST(SOUTHWEST_OF[location] & occupiedSquares)) ^
        FillNorthWest(SHIFT_NORTHWEST(NORTHWEST_OF[location] & occupiedSquares));
}

bitboard RookAttacks(bitboard occupiedSquares, int location)
{
    return
        ROOK_ATTACKS[location]                                       ^
        FillNorth(SHIFT_NORTH(NORTH_OF[location] & occupiedSquares)) ^
        FillEast (SHIFT_EAST ( EAST_OF[location] & occupiedSquares)) ^
        FillSouth(SHIFT_SOUTH(SOUTH_OF[location] & occupiedSquares)) ^
        FillWest (SHIFT_WEST ( WEST_OF[location] & occupiedSquares));
}

bitboard QueenAttacks(bitboard occupiedSquares, int location)
{
    return BishopAttacks(occupiedSquares, location) | RookAttacks(occupiedSquares, location);
}
/******************************************************************************
Determine if location is attacked by color
*******************************************************************************/
bool IsAttacked(const Position* position, int location, int color)
{
    const bitboard* const interveningSquares = &INTERVENING_SQUARES[location][0];
    const bitboard attackingPieces = position->allPieces[color];
    bitboard slidingAttackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by direct lookup since blockers 
    do not affect their attack set.
    ***************************************************************************/
    if (attackingPieces & 
        ((ENEMY_PAWN_ATTACKS[color][location] & position->pawns  )   | 
        (             KNIGHT_ATTACKS[location] & position->knights)   | 
        (               KING_ATTACKS[location] & position->kings)))
    {
        return true;
    }
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    slidingAttackers = (position->rooks | position->queens) & ROOK_ATTACKS[location] & attackingPieces;
    while (slidingAttackers)
    {
        if (!(interveningSquares[FindAndClearLsb(&slidingAttackers)] & position->occupiedSquares))
        {
            return true;
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    slidingAttackers = (position->bishops | position->queens) & BISHOP_ATTACKS[location] & attackingPieces;
    while (slidingAttackers)
    {
        if (!(interveningSquares[FindAndClearLsb(&slidingAttackers)] & position->occupiedSquares))
        {
            return true;
        }
    }
    return false;
}
/******************************************************************************
Determine the set of squares attacked by the piece (if any) standing on 
location
*******************************************************************************/
bitboard DetermineAttacksFromSquare(const Position* position, int location, int piece)
{
    switch (piece)
    {
    case NO_PIECE:
        return NO_SQUARES;
    case PAWN:
        return COLOR_AT(position, location) == WHITE ? PAWN_ATTACKS_WHITE[location] : PAWN_ATTACKS_BLACK[location];
    case KNIGHT:
        return KNIGHT_ATTACKS[location];
    case BISHOP:
        return BishopAttacks(position->occupiedSquares, location);
    case ROOK:
        return RookAttacks(position->occupiedSquares, location);
    case QUEEN:
        return QueenAttacks(position->occupiedSquares, location);
    case KING:
        return KING_ATTACKS[location];
    }
    return NO_SQUARES;
}
/******************************************************************************
Generate a bitboard of all pieces which directly attack location.
*******************************************************************************/
bitboard DetermineAttacksToSquare(const Position* position, int location)
{
    const bitboard* const interveningSquares = &INTERVENING_SQUARES[location][0];
    bitboard slidingAttackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by lookup since blockers do not 
    affect their attacks
    ***************************************************************************/
    bitboard attackers =
        (PAWN_ATTACKS_WHITE[location] & position->pawns & position->blackPieces) |
        (PAWN_ATTACKS_BLACK[location] & position->pawns & position->whitePieces) |
        (    KNIGHT_ATTACKS[location] & position->knights)                       |
        (      KING_ATTACKS[location] & position->kings);
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    slidingAttackers = ROOK_ATTACKS[location] & (position->rooks | position->queens);
    while (slidingAttackers)
    {
        const int locn = FindAndClearLsb(&slidingAttackers);
        if (!(interveningSquares[locn] & position->occupiedSquares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    slidingAttackers = BISHOP_ATTACKS[location] & (position->bishops | position->queens);
    while (slidingAttackers)
    {
        const int locn = FindAndClearLsb(&slidingAttackers);
        if (!(interveningSquares[locn] & position->occupiedSquares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    return attackers;
}
#if 0
/*
THESE FUNCTIONS HAVE BEEN DEPRECATED AS HAVING THE ATTACKS TO AND ATTACKS FROM
ARRAYS AS PART OF THE POSITION STRUCTURE LIMITS MOVE SPEED DUE TO THE MEMORY
COPY OVERHEAD (AN ADDITIONAL 1024 BYTES PER MOVE). WE NOW GENERATE ATTACKS ON
THE FLY AS REQUIRED. THIS IS ACTUALLY QUICKER THAN INCREMENTALLY MAINTAINING 
THEM.
*/
/******************************************************************************
Generate the attack map of which pieces attack which squares, and which squares 
are attacked by which pieces.
*******************************************************************************/
void DetermineAttackMap(Position* position)
{
    bitboard pieces;
    const bitboard occupiedSquares = position->occupiedSquares;
    memset(position->attacksFrom, 0, sizeof(position->attacksFrom));
    memset(position->attacksTo,   0, sizeof(position->attacksTo));
    pieces = position->pawns & position->whitePieces;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacksFrom[location] = PAWN_ATTACKS_WHITE[location];
    }
    pieces = position->pawns & position->blackPieces;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacksFrom[location] = PAWN_ATTACKS_BLACK[location];
    }
    pieces = position->knights;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacksFrom[location] = KNIGHT_ATTACKS[location];
    }
    pieces = position->bishops;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacksFrom[location] = BishopAttacks(occupiedSquares, location);
    }
    pieces = position->rooks;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacksFrom[location] = RookAttacks(occupiedSquares, location);
    }
    pieces = position->queens;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacksFrom[location] = QueenAttacks(occupiedSquares, location);
    }
    pieces = position->kings;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacksFrom[location] = KING_ATTACKS[location];
    }
    /**************************************************************************
    Determine the attacks-to-square
    ***************************************************************************/
    pieces = occupiedSquares;
    while (pieces)
    {
        const bitboard source = pieces & -pieces;     
        bitboard attacks = position->attacksFrom[Lsb(source)];
        while (attacks)
        {
            position->attacksTo[FindAndClearLsb(&attacks)] |= source;
        }
        pieces &= pieces - 1;
    }
}
/******************************************************************************
Update sliding piece attacks for a single square in a single direction based on 
the possible new blocking piece, i.e. recompute the sliding piece attack in 
direction "direction" from square "location" and leave all other directions of 
sliding piece attacks unmodified.
*******************************************************************************/
INLINE bitboard UpdateSingleDirectionAttacks(bitboard attacks, bitboard occupiedSquares, int location, uchar direction)
{
    switch (direction)
    {
    case DIR_NORTH:
        attacks |= NORTH_OF[location];
        attacks ^= FillNorth(SHIFT_NORTH(NORTH_OF[location] & occupiedSquares));
        break;
    case DIR_NORTHEAST:
        attacks |= NORTHEAST_OF[location];
        attacks ^= FillNorthEast(SHIFT_NORTHEAST(NORTHEAST_OF[location] & occupiedSquares));
        break;
    case DIR_EAST:
        attacks |= EAST_OF[location];
        attacks ^= FillEast(SHIFT_EAST(EAST_OF[location] & occupiedSquares));
        break;
    case DIR_SOUTHEAST:
        attacks |= SOUTHEAST_OF[location];
        attacks ^= FillSouthEast(SHIFT_SOUTHEAST(SOUTHEAST_OF[location] & occupiedSquares));
        break;
    case DIR_SOUTH:
        attacks |= SOUTH_OF[location];
        attacks ^= FillSouth(SHIFT_SOUTH(SOUTH_OF[location] & occupiedSquares));
        break;
    case DIR_SOUTHWEST:
        attacks |= SOUTHWEST_OF[location];
        attacks ^= FillSouthWest(SHIFT_SOUTHWEST(SOUTHWEST_OF[location] & occupiedSquares));
        break;
    case DIR_WEST:
        attacks |= WEST_OF[location];
        attacks ^= FillWest(SHIFT_WEST(WEST_OF[location] & occupiedSquares));
        break;
    case DIR_NORTHWEST:
        attacks |= NORTHWEST_OF[location];
        attacks ^= FillNorthWest(SHIFT_NORTHWEST(NORTHWEST_OF[location] & occupiedSquares));
        break;
    }
    return attacks;
}
/******************************************************************************
Update the new attacks to array, given the changes in the squares attacked by
array at location from prevAttacksFrom to newAttacksFrom
*******************************************************************************/
INLINE void UpdateAttacksTo(Position* position, int from)
{ 
    const bitboard source = BITBOARD(from);
    const bitboard oldAttacksFrom = position->previous->attacksFrom[from];
    const bitboard newAttacksFrom = position->attacksFrom[from];
    /**************************************************************************
    Remove attacks to squares which are no longer attacked
    ***************************************************************************/
    bitboard attacks = oldAttacksFrom & ~newAttacksFrom;
    while (attacks)
    {
        const int to = FindAndClearLsb(&attacks);
        position->attacksTo[to] &= ~source;
    }
    /**************************************************************************
    Add attacks to squares which were previously not attacked
    ***************************************************************************/
    attacks = newAttacksFrom & ~oldAttacksFrom;
    while (attacks)
    {
        const int to = FindAndClearLsb(&attacks);
        position->attacksTo[to] |= source;
    }
}
/******************************************************************************
Update sliding piece attacks which might have changed as a result of making a
move to or from square "location". 

Both the source and destination square of the move might affect the blocker of 
previous sliding piece attacks, and so any potential sliding piece affected by 
this move must have its attack set updated. This function updates any sliding 
piece "X-ray" attacks which are affected by a move involving location.
*******************************************************************************/
INLINE void UpdateSlidingAttacks(Position* position, int location)
{
    const bitboard* const interveningSquares = &INTERVENING_SQUARES[location][0];
    const uchar* const directionFrom = &DIRECTIONS[location][0];
    const bitboard occupiedSquares = position->occupiedSquares;
    bitboard slidingPieces = 
        (BISHOP_ATTACKS[location] & (position->bishops | position->queens)) |
        (  ROOK_ATTACKS[location] & (position->rooks   | position->queens));
    while (slidingPieces)
    {
        const int sliderLocation = FindAndClearLsb(&slidingPieces);       
        if (!(interveningSquares[sliderLocation] & occupiedSquares)) /* if this piece formerly attacked location */
        {
            position->attacksFrom[sliderLocation] = 
                UpdateSingleDirectionAttacks(position->attacksFrom[sliderLocation], 
                occupiedSquares, 
                sliderLocation, 
                directionFrom[sliderLocation]);
            UpdateAttacksTo(position, sliderLocation);
        }
    }    
}
/******************************************************************************
Incrementally update the attack map based on the move which led to this
position. This is much cheaper than recomputing the entire attack map for every
node in the search tree.
*******************************************************************************/
void UpdateAttackMap(Position* position)
{  
    int rookSrc, rookDst, enPassantCaptureLocn;
    const bitboard occupiedSquares = position->occupiedSquares;   
    const int color   = ENEMY(COLOR_TO_MOVE(position)); /* who made the last move */
    const int from     = MOVE_FROM(position->move);
    const int to       = MOVE_TO(position->move);
    int piece          = MOVE_PIECE(position->move);
    if (!position->move)
    {
        return; /* prev move was a null move so no changes to attack map */
    }
    switch (MOVE_TYPE(position->move))
    {
    case CASTLING:
        switch (to)
        {
        case G1:
            rookSrc = H1;
            rookDst = F1;
            break;
        case C1:
            rookSrc = A1;
            rookDst = D1;
            break;
        case G8:
            rookSrc = H8;
            rookDst = F8;
            break;
        case C8:
            rookSrc = A8;
            rookDst = D8;
            break;
        default:
            printf("ERROR: illegal castling move in update attack map\n");
            return;
        }
        position->attacksFrom[from]    = NO_SQUARES;       
        position->attacksFrom[to]      = KING_ATTACKS[to];
        position->attacksFrom[rookSrc] = NO_SQUARES;
        position->attacksFrom[rookDst] = RookAttacks(occupiedSquares, rookDst);
        UpdateAttacksTo(position, rookSrc);
        UpdateAttacksTo(position, rookDst);
        break;

    case EN_PASSANT_CAPTURE:
        enPassantCaptureLocn = (from & 0x38) | (to & 0x07); /* e.p. capture location is source rank, destination file */
        position->attacksFrom[enPassantCaptureLocn] = NO_SQUARES;
        position->attacksFrom[from] = NO_SQUARES;
        position->attacksFrom[to]   = PAWN_ATTACKS[color][to];
        UpdateSlidingAttacks(position, enPassantCaptureLocn); /* e.p. pawn might have been blocking x-ray attacks by a sliding piece */
        UpdateAttacksTo(position, enPassantCaptureLocn);
        break;

    case PAWN_PROMOTION_CAPTURE:
    case PAWN_PROMOTION_NON_CAPTURE:
        piece = MOVE_PROMOTED(position->move); /* piece on destination square is no longer a pawn */
        /* FALLTHROUGH */
    default:
        position->attacksFrom[from] = NO_SQUARES;
        switch (piece)
        {
        case PAWN:
            position->attacksFrom[to] = PAWN_ATTACKS[color][to];
            break;
        case KNIGHT:
            position->attacksFrom[to] = KNIGHT_ATTACKS[to];
            break;
        case BISHOP:
            position->attacksFrom[to] = BishopAttacks(occupiedSquares, to);
            break;
        case ROOK:
            position->attacksFrom[to] = RookAttacks(occupiedSquares, to);
            break;
        case QUEEN:
            position->attacksFrom[to] = QueenAttacks(occupiedSquares, to);
            break;
        case KING:
            position->attacksFrom[to] = KING_ATTACKS[to];
        }
        break;      
    }
    /**************************************************************************
    Finally update any sliding piece x-ray attacks which might have become 
    blocked or unblocked as a result of the move, and amend the attacks to
    square array.
    ***************************************************************************/
    UpdateSlidingAttacks(position, from);
    UpdateSlidingAttacks(position, to);
    UpdateAttacksTo(position, from);
    UpdateAttacksTo(position, to);
}
#endif
