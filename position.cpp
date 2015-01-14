#include "position.h"
#include "macros.h"
#include "generated_data.h"
#include "inline_functions.h"

static const bitboard* const ENEMY_PAWN_ATTACKS[2] = { PAWN_ATTACKS_BLACK, PAWN_ATTACKS_WHITE };
static const bitboard* const PAWN_ATTACKS[2]       = { PAWN_ATTACKS_WHITE, PAWN_ATTACKS_BLACK };

/******************************************************************************
Castling rights masks are ANDed with the castling flags for the move source and
destination squares to determine new castling rights following a move

Only moves involving squares a1, e1, h1, a8, e8 or h8 affect castling rights
*******************************************************************************/
static const uchar CASTLING_RIGHTS_MASKS[64] =
{
    0xFD, 0xFF, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF, 0xFE, // 1
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 3
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 4
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 7
    0xF7, 0xFF, 0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFB, // 8
};//  a     b     c     d     e     f     g     h
/******************************************************************************
The following arrays are indexed by the king's destination square for castling 
moves and indicate the corresponding rook's move squares
*******************************************************************************/
static const uchar ROOK_FROM_LOCATION[64] = 
{
     0,  0, A1,  0,  0,  0, H1,  0, // 1
     0,  0,  0,  0,  0,  0,  0,  0, // 2
     0,  0,  0,  0,  0,  0,  0,  0, // 3
     0,  0,  0,  0,  0,  0,  0,  0, // 4
     0,  0,  0,  0,  0,  0,  0,  0, // 5
     0,  0,  0,  0,  0,  0,  0,  0, // 6
     0,  0,  0,  0,  0,  0,  0,  0, // 7
     0,  0, A8,  0,  0,  0, H8,  0, // 8
};// a   b   c   d   e   f   g   h

static const uchar ROOK_TO_LOCATION[64] = 
{
     0,  0, D1,  0,  0,  0, F1,  0, // 1
     0,  0,  0,  0,  0,  0,  0,  0, // 2
     0,  0,  0,  0,  0,  0,  0,  0, // 3
     0,  0,  0,  0,  0,  0,  0,  0, // 4
     0,  0,  0,  0,  0,  0,  0,  0, // 5
     0,  0,  0,  0,  0,  0,  0,  0, // 6
     0,  0,  0,  0,  0,  0,  0,  0, // 7
     0,  0, D8,  0,  0,  0, F8,  0, // 8
};// a   b   c   d   e   f   g   h


bool Position::MakeMove(Move move, PositionFlags& flags)
{
    const int color     = COLOR_TO_MOVE(this);
    const bitboard from = BITBOARD(move.from);
    const bitboard to   = BITBOARD(move.to);
    const uchar piece   = this->piece_on[move.from];

    flags = this->flags;
    this->flags.en_passant_square = 0; 
    
    switch (piece)
    {
    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
    regular_move:
        flags.captured_piece         = this->piece_on[move.to];        
        this->piece_on[move.to]      = piece;
        this->piece_on[move.from]    = NO_PIECE;
        this->pieces_of_color[color] ^= from | to;
        this->pieces[piece]          ^= from | to;
        if (flags.captured_piece)
        {
            this->pieces_of_color[ENEMY(color)] ^= to;
            this->pieces[flags.captured_piece]  ^= to;
        }
        break;

    case PAWN:
        if (move.type == MOVE_REGULAR)
        {
            if (((move.from - move.to) & 0x0F) == 0) // double pawn push
            {
                this->flags.en_passant_square = (move.from + move.to) >> 1;
            }
            goto regular_move;
        }
        if (move.type == MOVE_EN_PASSANT_CAPTURE)
        {
            const uchar en_passant_location     = (move.from & 0x38) | (move.to & 0x07);
            flags.captured_piece                = PAWN;
            this->piece_on[move.from]           = NO_PIECE;           
            this->piece_on[en_passant_location] = NO_PIECE;
            this->piece_on[move.to]             = PAWN;
            this->pieces_of_color[color]        ^= from | to;
            this->pieces_of_color[ENEMY(color)] ^= BITBOARD(en_passant_location);
            this->pawns                         ^= from | to | BITBOARD(en_passant_location);
        }
        else
        {
            // pawn promotion
            const uchar promoted_piece   = move.type;
            flags.captured_piece         = this->piece_on[move.to];
            this->piece_on[move.from]    = NO_PIECE;
            this->piece_on[move.to]      = promoted_piece;
            this->pieces_of_color[color] ^= from | to;
            this->pawns                  ^= from;
            this->pieces[promoted_piece] ^= to;
            if (flags.captured_piece)
            {
                this->pieces_of_color[ENEMY(color)] ^= to;
                this->pieces[flags.captured_piece]  ^= to;
            }
        }
        break;

    case KING:
        this->flags.king_location[color] = move.to;
        if (move.type == MOVE_REGULAR)
        {
            goto regular_move;
        }
        else
        {
            /* castling move */
            const uchar rook_from     = ROOK_FROM_LOCATION[move.to];
            const uchar rook_to       = ROOK_TO_LOCATION  [move.to];
            this->piece_on[move.from] = NO_PIECE;
            this->piece_on[rook_from] = NO_PIECE;
            this->piece_on[move.to]   = KING;
            this->piece_on[rook_to]   = ROOK;
            this->kings ^= from | to;
            this->rooks ^= BITBOARD(rook_from) | BITBOARD(rook_to);
            this->pieces_of_color[color] ^= from | to | BITBOARD(rook_from) | BITBOARD(rook_to);
        }
        break;
    }
    this->occupied_squares   = this->white_pieces | this->black_pieces;
    this->flags.castle_flags &= CASTLING_RIGHTS_MASKS[move.from] & CASTLING_RIGHTS_MASKS[move.to];
    this->flags.state_flags  ^= IS_BLACK_TO_MOVE;
    this->flags.state_flags  &= ~(IS_CHECK | MOVED_INTO_CHECK);
    if (IsAttacked(this->flags.king_location[color], ENEMY(color)))
    {
        this->flags.state_flags |= MOVED_INTO_CHECK;
    }
    if (IsAttacked(this->flags.king_location[ENEMY(color)], color))
    {
        this->flags.state_flags |= IS_CHECK;
    }
    if (piece == PAWN || flags.captured_piece)
    {
        this->flags.half_move_clock = 0;
    }
    else
    {
        ++this->flags.half_move_clock;
    }
    if (color == BLACK)
    {
        ++this->flags.move_count;
    }
    return !(this->flags.state_flags & MOVED_INTO_CHECK);
}

void Position::UndoMove(Move move, const PositionFlags& flags)
{
    const int color     = COLOR_NOT_TO_MOVE(this);
    const bitboard from = BITBOARD(move.from);
    const bitboard to   = BITBOARD(move.to);
    const uchar piece   = this->piece_on[move.to]; /* NB except in the case of pawn promotions */

    switch (move.type)
    {
    case MOVE_REGULAR:
        this->piece_on[move.to]      = NO_PIECE;
        this->piece_on[move.from]    = piece;
        this->pieces[piece]          ^= from | to;
        this->pieces_of_color[color] ^= from | to;
        if (flags.captured_piece)
        {
            this->piece_on[move.to]             = flags.captured_piece;
            this->pieces[flags.captured_piece]  ^= to;
            this->pieces_of_color[ENEMY(color)] ^= to;
        }
        break;

    case MOVE_PROMOTION_KNIGHT:
    case MOVE_PROMOTION_BISHOP:
    case MOVE_PROMOTION_ROOK:
    case MOVE_PROMOTION_QUEEN:
        this->piece_on[move.to]      = NO_PIECE;
        this->piece_on[move.from]    = PAWN;
        this->pieces[piece]          ^= to;
        this->pawns                  ^= from;
        this->pieces_of_color[color] ^= from | to;
        if (flags.captured_piece)
        {
            this->piece_on[move.to]             = flags.captured_piece;
            this->pieces[flags.captured_piece]  ^= to;
            this->pieces_of_color[ENEMY(color)] ^= to;
        }
        break;

    case MOVE_EN_PASSANT_CAPTURE:
        {
            const uchar en_passant_location     = (move.from & 0x38) | (move.to & 0x07);
            this->piece_on[move.to]             = NO_PIECE;
            this->piece_on[move.from]           = PAWN;
            this->piece_on[en_passant_location] = PAWN;
            this->pieces_of_color[color]        ^= from | to;
            this->pieces_of_color[ENEMY(color)] ^= BITBOARD(en_passant_location);
            this->pawns                         ^= from | to | BITBOARD(en_passant_location);          
            break;
        }       
    
    case MOVE_CASTLING:
        {
            const uchar rook_from = ROOK_FROM_LOCATION[move.to];
            const uchar rook_to   = ROOK_TO_LOCATION  [move.to];
            this->piece_on[move.from] = KING;
            this->piece_on[rook_from] = ROOK;
            this->piece_on[move.to]   = NO_PIECE;
            this->piece_on[rook_to]   = NO_PIECE;
            this->kings ^= from | to;
            this->rooks ^= BITBOARD(rook_from) | BITBOARD(rook_to);
            this->pieces_of_color[color] ^= from | to | BITBOARD(rook_from) | BITBOARD(rook_to);
            break;
        }
    }
    this->flags = flags;
}

bool Position::IsAttacked(int location, int color) const
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    const bitboard attacking_pieces = this->pieces_of_color[color];
    /**************************************************************************
    Pawn, knight and king attacks can be done by direct lookup since blockers 
    do not affect their attack set.
    ***************************************************************************/
    if (attacking_pieces & 
        ((ENEMY_PAWN_ATTACKS[color][location] & this->pawns  )   | 
        (            KNIGHT_ATTACKS[location] & this->knights)   | 
        (              KING_ATTACKS[location] & this->kings)))
    {
        return true;
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    bitboard sliding_attackers = (this->bishops | this->queens) & BISHOP_ATTACKS[location] & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & this->occupied_squares))
        {
            return true;
        }
    }
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = (this->rooks | this->queens) & ROOK_ATTACKS[location] & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & this->occupied_squares))
        {
            return true;
        }
    }
    return false;
}

bitboard Position::AttacksFromSquare(int location) const
{
    switch (this->piece_on[location])
    {
    default:
    case NO_PIECE:
        return NO_SQUARES;
    case PAWN:
        return this->white_pieces & BITBOARD(location) ? PAWN_ATTACKS_WHITE[location] : PAWN_ATTACKS_BLACK[location];
    case KNIGHT:
        return KNIGHT_ATTACKS[location];
    case BISHOP:
        return BishopAttacks(this->occupied_squares, location);
    case ROOK:
        return RookAttacks(this->occupied_squares, location);
    case QUEEN:
        return QueenAttacks(this->occupied_squares, location);
    case KING:
        return KING_ATTACKS[location];
    }
}

bitboard Position::AttacksToSquare(int location) const
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    /**************************************************************************
    Pawn, knight and king attacks can be done by lookup since blockers do not 
    affect their attacks
    ***************************************************************************/
    bitboard attackers =
        (PAWN_ATTACKS_WHITE[location] & this->pawns & this->black_pieces) |
        (PAWN_ATTACKS_BLACK[location] & this->pawns & this->white_pieces) |
        (    KNIGHT_ATTACKS[location] & this->knights)                        |
        (      KING_ATTACKS[location] & this->kings);
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    bitboard sliding_attackers = ROOK_ATTACKS[location] & (this->rooks | this->queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & this->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    sliding_attackers = BISHOP_ATTACKS[location] & (this->bishops | this->queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & this->occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    return attackers;
}