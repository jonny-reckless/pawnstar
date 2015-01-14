#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>

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

Position::Position()
{
    // VS2012 does not support C++11 delegated construction
    Position start_pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0");
    *this = start_pos;
}

Position& Position::operator=(const Position& that)
{
    if (this != & that)
    {
        memcpy(this, &that, sizeof(Position));
    }
    return *this;
}

Position::Position(const Position& that)
{
    memcpy(this, &that, sizeof(Position));
}

Position::Position(const std::string& fen_string)
{    
    memset(this, 0, sizeof(Position));
    std::istringstream iss(fen_string);
    std::string board_description;
    iss >> board_description;
    int x = 0, y = 7;
    const std::string white = "PNBRQK";
    const std::string black = "pnbrqk";
    for (char c : board_description)
    {
        if (c == '/')
        {
            x = 0;
            --y;
            continue;
        }
        if (c >= '1' && c<= '8')
        {
            x += c - '0';
            continue;
        }
        std::string::size_type index = white.find(c);
        if (index != std::string::npos)
        {
            pieces[index + 1]  |= BITBOARD_XY(x, y);
            white_pieces       |= BITBOARD_XY(x, y);
            piece_on[x + 8 * y] = (uchar)(index + 1);
            ++x;
            continue;
        }
        index = black.find(c);
        if (index != std::string::npos)
        {
            pieces[index + 1]  |= BITBOARD_XY(x, y);
            black_pieces       |= BITBOARD_XY(x, y);
            piece_on[x + 8 * y] = (uchar)(index + 1);
            ++x;
        }
    }
    occupied_squares = white_pieces | black_pieces;
    char side_to_move;
    iss >> side_to_move;
    if (side_to_move == 'b')
    {
        ctx.state_flags |= IS_BLACK_TO_MOVE;
    }
    std::string castling;
    iss >> castling;
    for (char c : castling)
    {
        switch (c)
        {
        default:
            break;
        case 'K':
            ctx.castle_flags |= MAY_WHITE_K;
            break;
        case 'k':
            ctx.castle_flags |= MAY_BLACK_K;
            break;
        case 'Q':
            ctx.castle_flags |= MAY_WHITE_Q;
            break;
        case 'q':
            ctx.castle_flags |= MAY_BLACK_Q;
            break;
        
        }
    }
    std::string ep;
    iss >> ep;
    if (ep.length() == 2)
    {
        ctx.en_passant_square = ep[0] - 'a' + 8 * (ep[1] - '1');
    }
    iss >> ctx.half_move_clock;
    iss >> ctx.move_count;
    ctx.king_location[WHITE] = (uchar)Lsb(kings & white_pieces);
    ctx.king_location[BLACK] = (uchar)Lsb(kings & black_pieces);
}

std::string Position::ToString() const
{
    std::stringstream result;
    int num_empty_squares = 0;
    for (int y = 7; y >= 0; --y)
    {
        for (int x = 0; x < 8; ++x)
        {
            if (piece_on[x + 8 * y] == NO_PIECE)
            {
                ++num_empty_squares;
            }
            else
            {
                if (num_empty_squares)
                {
                    result << num_empty_squares;
                    num_empty_squares = 0;
                }
                char c = ColorAt(x + 8 * y) == WHITE ? " PNBRQK"[piece_on[x + 8 * y]] : " pnbrqk"[piece_on[x + 8 * y]];
                result << c;
            }
        }
        if (num_empty_squares)
        {
            result << num_empty_squares;
            num_empty_squares = 0;
        }
        if (y != 0)
        {
            result << '/';
        }
    }
    if (ctx.state_flags & IS_BLACK_TO_MOVE)
    {
        result << " b ";
    }
    else
    {
        result << " w ";
    }
    if (ctx.castle_flags == 0)
    {
        result << "- ";
    }
    else
    {
        if (ctx.castle_flags & MAY_WHITE_K)
        {
            result << 'K';
        }
        if (ctx.castle_flags & MAY_WHITE_Q)
        {
            result << 'Q';
        }
        if (ctx.castle_flags & MAY_BLACK_K)
        {
            result << 'k';
        }
        if (ctx.castle_flags & MAY_BLACK_Q)
        {
            result << 'q';
        }
        result << ' ';
    }
    if (ctx.en_passant_square == 0)
    {
        result << "- ";
    }
    else
    {
        result << FILE_CHAR(ctx.en_passant_square) << RANK_CHAR(ctx.en_passant_square) << ' ';
    }
    result << ctx.half_move_clock << " mc " << ctx.move_count;
    return result.str();
}


bool Position::MakeMove(Move move, MoveUndoCtx& undo_ctx)
{
    const int color       = ColorToMove();
    const bitboard from   = BITBOARD(move.from);
    const bitboard to     = BITBOARD(move.to);
    const uchar piece     = piece_on[move.from];
    undo_ctx              = ctx;
    ctx.en_passant_square = 0; 
    
    switch (piece)
    {
    case KNIGHT:
    case BISHOP:
    case ROOK:
    case QUEEN:
    regular_move:
        undo_ctx.captured_piece = piece_on[move.to];        
        piece_on[move.to]       = piece;
        piece_on[move.from]     = NO_PIECE;
        pieces_of_color[color]  ^= from | to;
        pieces[piece]           ^= from | to;
        if (undo_ctx.captured_piece)
        {
            pieces_of_color[ENEMY(color)]   ^= to;
            pieces[undo_ctx.captured_piece] ^= to;
        }
        break;

    case PAWN:
        if (move.type == MOVE_REGULAR)
        {
            if (((move.from - move.to) & 0x0F) == 0) // double pawn push
            {
                ctx.en_passant_square = (move.from + move.to) >> 1;
            }
            goto regular_move;
        }
        if (move.type == MOVE_EN_PASSANT_CAPTURE)
        {
            const uchar en_passant_location = (move.from & 0x38) | (move.to & 0x07);
            undo_ctx.captured_piece         = PAWN;
            piece_on[move.from]             = NO_PIECE;           
            piece_on[en_passant_location]   = NO_PIECE;
            piece_on[move.to]               = PAWN;
            pieces_of_color[color]          ^= from | to;
            pieces_of_color[ENEMY(color)]   ^= BITBOARD(en_passant_location);
            pawns                           ^= from | to | BITBOARD(en_passant_location);
        }
        else
        {
            // pawn promotion
            const uchar promoted_piece   = move.type;
            undo_ctx.captured_piece      = piece_on[move.to];
            this->piece_on[move.from]    = NO_PIECE;
            this->piece_on[move.to]      = promoted_piece;
            this->pieces_of_color[color] ^= from | to;
            this->pawns                  ^= from;
            this->pieces[promoted_piece] ^= to;
            if (undo_ctx.captured_piece)
            {
                this->pieces_of_color[ENEMY(color)]   ^= to;
                this->pieces[undo_ctx.captured_piece] ^= to;
            }
        }
        break;

    case KING:
        ctx.king_location[color] = move.to;
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

    occupied_squares = white_pieces | black_pieces;
    ctx.castle_flags &= CASTLING_RIGHTS_MASKS[move.from] & CASTLING_RIGHTS_MASKS[move.to];
    ctx.state_flags  ^= IS_BLACK_TO_MOVE;
    ctx.state_flags  &= ~(IS_CHECK | MOVED_INTO_CHECK);
    if (IsAttacked(ctx.king_location[color], ENEMY(color)))
    {
        ctx.state_flags |= MOVED_INTO_CHECK;
    }
    if (IsAttacked(ctx.king_location[ENEMY(color)], color))
    {
        ctx.state_flags |= IS_CHECK;
    }
    if (piece == PAWN || undo_ctx.captured_piece)
    {
        ctx.half_move_clock = 0;
    }
    else
    {
        ++ctx.half_move_clock;
    }
    if (color == BLACK)
    {
        ++ctx.move_count;
    }
    return !(ctx.state_flags & MOVED_INTO_CHECK);
}

void Position::UndoMove(Move move, const MoveUndoCtx& undo_ctx)
{
    const int color     = ENEMY(ColorToMove());
    const bitboard from = BITBOARD(move.from);
    const bitboard to   = BITBOARD(move.to);
    const uchar piece   = piece_on[move.to]; // except in the case of pawn promotions

    switch (move.type)
    {
    case MOVE_REGULAR:
        piece_on[move.to]      = NO_PIECE;
        piece_on[move.from]    = piece;
        pieces[piece]          ^= from | to;
        pieces_of_color[color] ^= from | to;
        if (undo_ctx.captured_piece)
        {
            piece_on[move.to]               = undo_ctx.captured_piece;
            pieces[undo_ctx.captured_piece] ^= to;
            pieces_of_color[ENEMY(color)]   ^= to;
        }
        break;

    case MOVE_PROMOTION_KNIGHT:
    case MOVE_PROMOTION_BISHOP:
    case MOVE_PROMOTION_ROOK:
    case MOVE_PROMOTION_QUEEN:
        piece_on[move.to]      = NO_PIECE;
        piece_on[move.from]    = PAWN;
        pieces[piece]          ^= to;
        pawns                  ^= from;
        pieces_of_color[color] ^= from | to;
        if (undo_ctx.captured_piece)
        {
            piece_on[move.to]               = undo_ctx.captured_piece;
            pieces[undo_ctx.captured_piece] ^= to;
            pieces_of_color[ENEMY(color)]   ^= to;
        }
        break;

    case MOVE_EN_PASSANT_CAPTURE:
        {
            const uchar en_passant_location = (move.from & 0x38) | (move.to & 0x07);
            piece_on[move.to]               = NO_PIECE;
            piece_on[move.from]             = PAWN;
            piece_on[en_passant_location]   = PAWN;
            pieces_of_color[color]          ^= from | to;
            pieces_of_color[ENEMY(color)]   ^= BITBOARD(en_passant_location);
            pawns                           ^= from | to | BITBOARD(en_passant_location);          
            break;
        }       
    
    case MOVE_CASTLING:
        {
            const uchar rook_from = ROOK_FROM_LOCATION[move.to];
            const uchar rook_to   = ROOK_TO_LOCATION  [move.to];
            piece_on[move.from]   = KING;
            piece_on[rook_from]   = ROOK;
            piece_on[move.to]     = NO_PIECE;
            piece_on[rook_to]     = NO_PIECE;
            kings ^= from | to;
            rooks ^= BITBOARD(rook_from) | BITBOARD(rook_to);
            pieces_of_color[color] ^= from | to | BITBOARD(rook_from) | BITBOARD(rook_to);
            break;
        }
    }
    ctx = undo_ctx;
}

bool Position::IsAttacked(int location, int color) const
{
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    const bitboard attacking_pieces = pieces_of_color[color];
    /**************************************************************************
    Pawn, knight and king attacks can be done by direct lookup since blockers 
    do not affect their attack set.
    ***************************************************************************/
    if (attacking_pieces & 
        ((ENEMY_PAWN_ATTACKS[color][location] & pawns  )   | 
        (            KNIGHT_ATTACKS[location] & knights)   | 
        (              KING_ATTACKS[location] & kings)))
    {
        return true;
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    bitboard sliding_attackers = (bishops | queens) & BISHOP_ATTACKS[location] & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & occupied_squares))
        {
            return true;
        }
    }
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    sliding_attackers = (rooks | queens) & ROOK_ATTACKS[location] & attacking_pieces;
    while (sliding_attackers)
    {
        if (!(intervening_squares[FindAndClearLsb(&sliding_attackers)] & occupied_squares))
        {
            return true;
        }
    }
    return false;
}

bitboard Position::AttacksFromSquare(int location) const
{
    switch (piece_on[location])
    {
    default:
    case NO_PIECE:
        return NO_SQUARES;
    case PAWN:
        return white_pieces & BITBOARD(location) ? PAWN_ATTACKS_WHITE[location] : PAWN_ATTACKS_BLACK[location];
    case KNIGHT:
        return KNIGHT_ATTACKS[location];
    case BISHOP:
        return BishopAttacks(occupied_squares, location);
    case ROOK:
        return   RookAttacks(occupied_squares, location);
    case QUEEN:
        return  QueenAttacks(occupied_squares, location);
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
        (PAWN_ATTACKS_WHITE[location] & pawns & black_pieces) |
        (PAWN_ATTACKS_BLACK[location] & pawns & white_pieces) |
        (    KNIGHT_ATTACKS[location] & knights)              |
        (      KING_ATTACKS[location] & kings);
    /**************************************************************************
    Rook and queen horizontal and vertical sliding attacks
    ***************************************************************************/
    bitboard sliding_attackers = ROOK_ATTACKS[location] & (rooks | queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    /**************************************************************************
    Bishop and queen diagonal and antidiagonal sliding attacks
    ***************************************************************************/
    sliding_attackers = BISHOP_ATTACKS[location] & (bishops | queens);
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    return attackers;
}

bitboard Position::AttacksToSquare(int location, int color) const
{
    const bitboard attacking_pieces = pieces_of_color[color];
    const bitboard* const intervening_squares = &INTERVENING_SQUARES[location][0];
    bitboard attackers = attacking_pieces & 
        ((ENEMY_PAWN_ATTACKS[color][location] & pawns)   |
        (            KNIGHT_ATTACKS[location] & knights) |
        (              KING_ATTACKS[location] & kings));
    bitboard sliding_attackers = ROOK_ATTACKS[location] & (rooks | queens) & attacking_pieces;
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    sliding_attackers = BISHOP_ATTACKS[location] & (bishops | queens) & attacking_pieces;
    while (sliding_attackers)
    {
        const int locn = FindAndClearLsb(&sliding_attackers);
        if (!(intervening_squares[locn] & occupied_squares))
        {
            attackers |= BITBOARD(locn);
        }
    }
    return attackers;
}