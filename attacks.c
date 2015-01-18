#include "pawnstar.h"

const bitboard* const ENEMY_PAWN_ATTACKS[2] = { PAWN_ATTACKS_BLACK, PAWN_ATTACKS_WHITE };
const bitboard* const PAWN_ATTACKS[2]       = { PAWN_ATTACKS_WHITE, PAWN_ATTACKS_BLACK };

bitboard BishopAttacks(bitboard occupied_squares, int location)
{
    return 
        BISHOP_ATTACKS[location]                                                 ^
        FillNorthEast(SHIFT_NORTHEAST(NORTHEAST_OF[location] & occupied_squares)) ^
        FillSouthEast(SHIFT_SOUTHEAST(SOUTHEAST_OF[location] & occupied_squares)) ^
        FillSouthWest(SHIFT_SOUTHWEST(SOUTHWEST_OF[location] & occupied_squares)) ^
        FillNorthWest(SHIFT_NORTHWEST(NORTHWEST_OF[location] & occupied_squares));
}

bitboard RookAttacks(bitboard occupied_squares, int location)
{
    return
        ROOK_ATTACKS[location]                                       ^
        FillNorth(SHIFT_NORTH(NORTH_OF[location] & occupied_squares)) ^
        FillEast (SHIFT_EAST ( EAST_OF[location] & occupied_squares)) ^
        FillSouth(SHIFT_SOUTH(SOUTH_OF[location] & occupied_squares)) ^
        FillWest (SHIFT_WEST ( WEST_OF[location] & occupied_squares));
}

bitboard QueenAttacks(bitboard occupied_squares, int location)
{
    return BishopAttacks(occupied_squares, location) | RookAttacks(occupied_squares, location);
}
/******************************************************************************
Determine if location is attacked by color
*******************************************************************************/
bool IsAttacked(const Position* position, int location, int color)
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    const bitboard attacking_pieces = position->pieces_of_color[color];
    bitboard sliding_attackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by direct lookup since blockers 
    do not affect their attack set.
    ***************************************************************************/
    if (attacking_pieces & 
        ((ENEMY_PAWN_ATTACKS[color][location] & position->pawns  )  | 
        (            KNIGHT_ATTACKS[location] & position->knights)  | 
        (              KING_ATTACKS[location] & position->kings)))
    {
        return true;
    }
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = (position->rooks | position->queens) & ROOK_ATTACKS[location] & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & position->occupied_squares))
        {
            return true;
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    sliding_attackers = (position->bishops | position->queens) & BISHOP_ATTACKS[location] & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & position->occupied_squares))
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
bitboard AttacksFromSquare(const Position* position, int location, int piece)
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
        return BishopAttacks(position->occupied_squares, location);
    case ROOK:
        return RookAttacks(position->occupied_squares, location);
    case QUEEN:
        return QueenAttacks(position->occupied_squares, location);
    case KING:
        return KING_ATTACKS[location];
    }
    return NO_SQUARES;
}
/******************************************************************************
Generate a bitboard of all pieces which directly attack location.
*******************************************************************************/
bitboard AttacksToSquareByColor(const Position* position, int location, int color)
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];  
    const bitboard attacking_pieces           = position->pieces_of_color[color];
    bitboard sliding_attackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by lookup since blockers do not 
    affect their attacks
    ***************************************************************************/
    bitboard attackers = attacking_pieces & (
        (ENEMY_PAWN_ATTACKS[color][location] & position->pawns  ) |
        (           KNIGHT_ATTACKS[location] & position->knights) |
        (             KING_ATTACKS[location] & position->kings  ));
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = ROOK_ATTACKS[location] & (position->rooks | position->queens) & attacking_pieces;
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    sliding_attackers = BISHOP_ATTACKS[location] & (position->bishops | position->queens) & attacking_pieces;
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    return attackers;
}

bitboard AttacksToSquareByType(const Position* position, int location, int color, int piece)
{
    const bitboard attacking_pieces = position->pieces_of_color[color];
    bitboard attackers;
    bitboard sliding_attackers;
    const bitboard* intervening_squares;   
    switch (piece)
    {
    default:
        return NO_SQUARES;

    case PAWN:
        return ENEMY_PAWN_ATTACKS[color][location] & position->pawns & attacking_pieces;

    case KNIGHT:
        return KNIGHT_ATTACKS[location] & position->knights & attacking_pieces;
    
    case BISHOP:
        intervening_squares = &INTERVENING_SQUARES[location][0]; 
        sliding_attackers   = BISHOP_ATTACKS[location] & position->bishops & attacking_pieces;
        attackers           = NO_SQUARES;
        while (sliding_attackers)
        {
            const int locn = FindAndClearLsb(&sliding_attackers);
            if (!(intervening_squares[locn] & position->occupied_squares))
            {
                attackers |= BITBOARD(locn);
            }
        }
        return attackers;
    
    case ROOK:
        intervening_squares = &INTERVENING_SQUARES[location][0]; 
        sliding_attackers   = ROOK_ATTACKS[location] & position->rooks & attacking_pieces;
        attackers           = NO_SQUARES;
        while (sliding_attackers)
        {
            const int locn = FindAndClearLsb(&sliding_attackers);
            if (!(intervening_squares[locn] & position->occupied_squares))
            {
                attackers |= BITBOARD(locn);
            }
        }
        return attackers;

    case QUEEN:
        intervening_squares = &INTERVENING_SQUARES[location][0]; 
        sliding_attackers   = QUEEN_ATTACKS[location] & position->queens & attacking_pieces;
        attackers           = NO_SQUARES;
        while (sliding_attackers)
        {
            const int locn = FindAndClearLsb(&sliding_attackers);
            if (!(intervening_squares[locn] & position->occupied_squares))
            {
                attackers |= BITBOARD(locn);
            }
        }
        return attackers;

    case KING:
        return KING_ATTACKS[location] & position->kings & attacking_pieces;
    }
}
/******************************************************************************
Generate a bitboard pieces of one color which directly attack location.
*******************************************************************************/
bitboard AttacksToSquare(const Position* position, int location)
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    bitboard sliding_attackers;
    /**************************************************************************
    Pawn, knight and king attacks can be done by lookup since blockers do not 
    affect their attacks
    ***************************************************************************/
    bitboard attackers =
        (PAWN_ATTACKS_WHITE[location] & position->pawns & position->black_pieces) |
        (PAWN_ATTACKS_BLACK[location] & position->pawns & position->white_pieces) |
        (    KNIGHT_ATTACKS[location] & position->knights)                       |
        (      KING_ATTACKS[location] & position->kings);
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = ROOK_ATTACKS[location] & (position->rooks | position->queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    sliding_attackers = BISHOP_ATTACKS[location] & (position->bishops | position->queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & position->occupied_squares))
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
    const bitboard occupied_squares = position->occupied_squares;
    memset(position->attacks_from, 0, sizeof(position->attacks_from));
    memset(position->attacks_to,   0, sizeof(position->attacks_to));
    pieces = position->pawns & position->white_pieces;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacks_from[location] = PAWN_ATTACKS_WHITE[location];
    }
    pieces = position->pawns & position->black_pieces;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacks_from[location] = PAWN_ATTACKS_BLACK[location];
    }
    pieces = position->knights;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacks_from[location] = KNIGHT_ATTACKS[location];
    }
    pieces = position->bishops;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacks_from[location] = BishopAttacks(occupied_squares, location);
    }
    pieces = position->rooks;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacks_from[location] = RookAttacks(occupied_squares, location);
    }
    pieces = position->queens;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacks_from[location] = QueenAttacks(occupied_squares, location);
    }
    pieces = position->kings;
    while (pieces)
    {
        const int location = FindAndClearLsb(&pieces);
        position->attacks_from[location] = KING_ATTACKS[location];
    }
    /**************************************************************************
    Determine the attacks-to-square
    ***************************************************************************/
    pieces = occupied_squares;
    while (pieces)
    {
        const bitboard source = pieces & -pieces;     
        bitboard attacks = position->attacks_from[Lsb(source)];
        while (attacks)
        {
            position->attacks_to[FindAndClearLsb(&attacks)] |= source;
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
INLINE bitboard UpdateSingleDirectionAttacks(bitboard attacks, bitboard occupied_squares, int location, uchar direction)
{
    switch (direction)
    {
    case DIR_NORTH:
        attacks |= NORTH_OF[location];
        attacks ^= FillNorth(SHIFT_NORTH(NORTH_OF[location] & occupied_squares));
        break;
    case DIR_NORTHEAST:
        attacks |= NORTHEAST_OF[location];
        attacks ^= FillNorthEast(SHIFT_NORTHEAST(NORTHEAST_OF[location] & occupied_squares));
        break;
    case DIR_EAST:
        attacks |= EAST_OF[location];
        attacks ^= FillEast(SHIFT_EAST(EAST_OF[location] & occupied_squares));
        break;
    case DIR_SOUTHEAST:
        attacks |= SOUTHEAST_OF[location];
        attacks ^= FillSouthEast(SHIFT_SOUTHEAST(SOUTHEAST_OF[location] & occupied_squares));
        break;
    case DIR_SOUTH:
        attacks |= SOUTH_OF[location];
        attacks ^= FillSouth(SHIFT_SOUTH(SOUTH_OF[location] & occupied_squares));
        break;
    case DIR_SOUTHWEST:
        attacks |= SOUTHWEST_OF[location];
        attacks ^= FillSouthWest(SHIFT_SOUTHWEST(SOUTHWEST_OF[location] & occupied_squares));
        break;
    case DIR_WEST:
        attacks |= WEST_OF[location];
        attacks ^= FillWest(SHIFT_WEST(WEST_OF[location] & occupied_squares));
        break;
    case DIR_NORTHWEST:
        attacks |= NORTHWEST_OF[location];
        attacks ^= FillNorthWest(SHIFT_NORTHWEST(NORTHWEST_OF[location] & occupied_squares));
        break;
    }
    return attacks;
}
/******************************************************************************
Update the new attacks to array, given the changes in the squares attacked by
array at location from prev_attacks_from to new_attacks_from
*******************************************************************************/
INLINE void UpdateAttacksTo(Position* position, int from)
{ 
    const bitboard source = BITBOARD(from);
    const bitboard old_attacks_from = position->previous->attacks_from[from];
    const bitboard new_attacks_from = position->attacks_from[from];
    /**************************************************************************
    Remove attacks to squares which are no longer attacked
    ***************************************************************************/
    bitboard attacks = old_attacks_from & ~new_attacks_from;
    while (attacks)
    {
        const int to = FindAndClearLsb(&attacks);
        position->attacks_to[to] &= ~source;
    }
    /**************************************************************************
    Add attacks to squares which were previously not attacked
    ***************************************************************************/
    attacks = new_attacks_from & ~old_attacks_from;
    while (attacks)
    {
        const int to = FindAndClearLsb(&attacks);
        position->attacks_to[to] |= source;
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
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    const uchar* const direction_from = &DIRECTIONS[location][0];
    const bitboard occupied_squares = position->occupied_squares;
    bitboard sliding_pieces = 
        (BISHOP_ATTACKS[location] & (position->bishops | position->queens)) |
        (  ROOK_ATTACKS[location] & (position->rooks   | position->queens));
    while (sliding_pieces)
    {
        const int slider_location = FindAndClearLsb(&sliding_pieces);       
        if (!(intervening_squares[slider_location] & occupied_squares)) /* if this piece formerly attacked location */
        {
            position->attacks_from[slider_location] = 
                UpdateSingleDirectionAttacks(position->attacks_from[slider_location], 
                occupied_squares, 
                slider_location, 
                direction_from[slider_location]);
            UpdateAttacksTo(position, slider_location);
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
    int rook_src, rook_dst, en_passant_capture_locn;
    const bitboard occupied_squares = position->occupied_squares;   
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
            rook_src = H1;
            rook_dst = F1;
            break;
        case C1:
            rook_src = A1;
            rook_dst = D1;
            break;
        case G8:
            rook_src = H8;
            rook_dst = F8;
            break;
        case C8:
            rook_src = A8;
            rook_dst = D8;
            break;
        default:
            printf("ERROR: illegal castling move in update attack map\n");
            return;
        }
        position->attacks_from[from]    = NO_SQUARES;       
        position->attacks_from[to]      = KING_ATTACKS[to];
        position->attacks_from[rook_src] = NO_SQUARES;
        position->attacks_from[rook_dst] = RookAttacks(occupied_squares, rook_dst);
        UpdateAttacksTo(position, rook_src);
        UpdateAttacksTo(position, rook_dst);
        break;

    case EN_PASSANT_CAPTURE:
        en_passant_capture_locn = (from & 0x38) | (to & 0x07); /* e.p. capture location is source rank, destination file */
        position->attacks_from[en_passant_capture_locn] = NO_SQUARES;
        position->attacks_from[from] = NO_SQUARES;
        position->attacks_from[to]   = PAWN_ATTACKS[color][to];
        UpdateSlidingAttacks(position, en_passant_capture_locn); /* e.p. pawn might have been blocking x-ray attacks by a sliding piece */
        UpdateAttacksTo(position, en_passant_capture_locn);
        break;

    case PAWN_PROMOTION_CAPTURE:
    case PAWN_PROMOTION_NON_CAPTURE:
        piece = MOVE_PROMOTED(position->move); /* piece on destination square is no longer a pawn */
        /* FALLTHROUGH */
    default:
        position->attacks_from[from] = NO_SQUARES;
        switch (piece)
        {
        case PAWN:
            position->attacks_from[to] = PAWN_ATTACKS[color][to];
            break;
        case KNIGHT:
            position->attacks_from[to] = KNIGHT_ATTACKS[to];
            break;
        case BISHOP:
            position->attacks_from[to] = BishopAttacks(occupied_squares, to);
            break;
        case ROOK:
            position->attacks_from[to] = RookAttacks(occupied_squares, to);
            break;
        case QUEEN:
            position->attacks_from[to] = QueenAttacks(occupied_squares, to);
            break;
        case KING:
            position->attacks_from[to] = KING_ATTACKS[to];
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