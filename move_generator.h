#pragma once

#include "types.h"
#include "position.h"
#include "generator.h"
#include "inline_functions.h"

$generator(MoveGenerator)
{
    const Position& position;
    int             color;
    bool            do_all_moves;
    bitboard        promotions_west;
    bitboard        promotions_east;
    bitboard        promotions_forward;
    bitboard        pawn_captures_west;
    bitboard        pawn_captures_east;
    bitboard        pawn_captures_en_passant;
    bitboard        pawn_single_pushes;
    bitboard        pawn_double_pushes;
    bitboard        friendly_pieces;
    bitboard        enemy_pieces;
    bitboard        targets;
    bitboard        sources;
    bitboard        attacks_by_type;
    bitboard        attacks_from[64];
    signed char     pawn_push_delta;
    signed char     pawn_west_delta;
    signed char     pawn_east_delta;
    uchar           captured_piece;
    uchar           capturing_piece;
    uchar           from;
    uchar           to;

    MoveGenerator(const Position& position, bool do_all_moves) : position(position), do_all_moves(do_all_moves)
    {
        color               = position.ColorToMove();
        friendly_pieces     = position.pieces_of_color[color];
        enemy_pieces        = position.occupied_squares ^ friendly_pieces;
        if (color == WHITE)
        {
            const bitboard pawns     = position.pawns & position.white_pieces;
            pawn_push_delta          = 8;
            pawn_west_delta          = 7;
            pawn_east_delta          = 9;
            pawn_single_pushes       = SHIFT_NORTH(pawns) & ~position.occupied_squares;
            pawn_double_pushes       = SHIFT_NORTH(pawn_single_pushes) & ~position.occupied_squares & RANK_4;
            pawn_captures_west       = SHIFT_NORTHWEST(pawns) & position.black_pieces;
            pawn_captures_east       = SHIFT_NORTHEAST(pawns) & position.black_pieces;
            pawn_captures_en_passant = position.flags.en_passant_square ? PAWN_ATTACKS_BLACK[position.flags.en_passant_square] & pawns : NO_SQUARES;
            promotions_forward       = pawn_single_pushes & RANK_8;
            promotions_west          = pawn_captures_west & RANK_8;
            promotions_east          = pawn_captures_east & RANK_8;       
        }
        else
        {
            const bitboard pawns     = position.pawns & position.black_pieces;
            pawn_push_delta          = -8;
            pawn_west_delta          = -9;
            pawn_east_delta          = -7;
            pawn_single_pushes       = SHIFT_SOUTH(pawns) & ~position.occupied_squares;
            pawn_double_pushes       = SHIFT_SOUTH(pawn_single_pushes) & ~position.occupied_squares & RANK_5;
            pawn_captures_west       = SHIFT_SOUTHWEST(pawns) & position.white_pieces;
            pawn_captures_east       = SHIFT_SOUTHEAST(pawns) & position.white_pieces;
            pawn_captures_en_passant = position.flags.en_passant_square ? PAWN_ATTACKS_WHITE[position.flags.en_passant_square] & pawns : NO_SQUARES;
            promotions_forward       = pawn_single_pushes & RANK_1;
            promotions_west          = pawn_captures_west & RANK_1;
            promotions_east          = pawn_captures_east & RANK_1;
        }
        pawn_single_pushes ^= promotions_forward;
        pawn_captures_west ^= promotions_west;
        pawn_captures_east ^= promotions_east;
    }


    $emit(Move)

    // pawn promotions
    while (promotions_west)
    {
        to = FindAndClearLsb(&promotions_west);
        from = to - pawn_west_delta;
        $yield(Move(from, to, MOVE_PROMOTION_QUEEN);
        $yield(Move(from, to, MOVE_PROMOTION_ROOK);
        $yield(Move(from, to, MOVE_PROMOTION_BISHOP);
        $yield(Move(from, to, MOVE_PROMOTION_KNIGHT);
    }
    while (promotions_east)
    {
        to = FindAndClearLsb(&promotions_east);
        from = to - pawn_east_delta;
        $yield(Move(from, to, MOVE_PROMOTION_QUEEN);
        $yield(Move(from, to, MOVE_PROMOTION_ROOK);
        $yield(Move(from, to, MOVE_PROMOTION_BISHOP);
        $yield(Move(from, to, MOVE_PROMOTION_KNIGHT);
    }
    while (promotions_forward)
    {
        to = FindAndClearLsb(&promotions_forward);
        from = to - pawn_push_delta;
        $yield(Move(from, to, MOVE_PROMOTION_QUEEN);
        $yield(Move(from, to, MOVE_PROMOTION_ROOK);
        $yield(Move(from, to, MOVE_PROMOTION_BISHOP);
        $yield(Move(from, to, MOVE_PROMOTION_KNIGHT);
    }
    // regular pawn captures
    for (captured_piece = QUEEN; captured_piece != NO_PIECE; --captured_piece)
    {
        targets = pawn_captures_west & position.pieces[captured_piece];
        while (targets)
        {
            to = FindAndClearLsb(&targets);
            from = to - pawn_west_delta;
            pawn_captures_west ^= BITBOARD(to);
            $yield(Move(from, to));
        }
        targets = pawn_captures_east & position.pieces[captured_piece_type];
        while (targets)
        {
            to = FindAndClearLsb(&targets);
            from = to - pawn_east_delta;
            pawn_captures_east ^= BITBOARD(to);
            $yield(Move(from, to));
        }
    }
    // en passant captures
    while (pawn_captures_en_passant)
    {
        from = FindAndClearLsb(&pawn_captures_en_passant);
        $yield(Move(from, position.flags.en_passant_square, MOVE_EN_PASSANT_CAPTURE));
    }
    // piece captures
    for (capturing_piece = KNIGHT; capturing_piece <= KING; ++capturing_piece)
    {
        sources = position.pieces[capturing_piece] & friendly_pieces;
        attacks_by_type = NO_SQUARES;
        while (sources)
        {
            from = FindAndClearLsb(&sources);
            attacks_from[from] = position.AttacksFromSquare(from);
            attacks_by_type |= attacks_from[from];
        }
        for (captured_piece = QUEEN; captured_piece != NO_PIECE; --captured_piece)
        {
            targets = position.pieces[captured_piece] & enemy_pieces & attacks_by_type;
            while (targets)
            {
                to = FindAndClearLsb(&targets);
                sources = position.pieces[capturing_piece] & friendly_pieces;
                while (sources)
                {
                    from = FindAndClearLsb(&sources);
                    if (attacks_from[from] & BITBOARD(to))
                    {
                        $yield(Move(from, to));
                    }
                }
            }
        }
    }
    if (do_all_moves)
    {
        if (!positon.IsInCheck())
        {
            if (color == WHITE)
            {
                if ((position.flags.castle_flags & MAY_WHITE_K) && !(position.occupied_squares & (F1BB | G1BB)) && !position.IsAttacked(F1, BLACK) && !position.IsAttacked(G1, BLACK))
                {
                    $yield(Move(E1, G1, MOVE_CASTLING));
                }
                if ((position.flags.castle_flags & MAY_WHITE_Q) && !(position.occupied_squares & (B1BB | C1BB | D1BB)) && !position.IsAttacked(D1, BLACK) && !position.IsAttacked(C1, BLACK))
                {
                    $yield(Move(E1, C1, MOVE_CASTLING));
                }
            }
            else
            {
            }
        }
    }






    $stop;
};