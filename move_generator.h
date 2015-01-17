#pragma once

#include "bitboard_constants.h"
#include "macros.h"
#include "types.h"
#include "inline_functions.h"
#include "generator.h"
#include "position.h"

#pragma warning(push)
#pragma warning(disable:4127)

$generator(MoveGenerator)
{
    const Position& position;
    int             color;
    bool            do_all_moves;
    bitboard        promotions_west;
    bitboard        promotions_east;
    bitboard        promotions_forward;
    bitboard        pawn_captures_en_passant;
    bitboard        pawn_single_pushes;
    bitboard        pawn_double_pushes;
    bitboard        friendly_pieces;
    bitboard        enemy_pieces;
    bitboard        targets;
    bitboard        sources;
    bitboard        attacks_to;
    bitboard        seventh_rank;
    signed char     pawn_push_delta;
    signed char     pawn_west_delta;
    signed char     pawn_east_delta;
    uchar           captured_piece;
    uchar           moving_piece;
    uchar           promoted_piece;
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
            pawn_captures_en_passant = position.ctx.en_passant_square ? PAWN_ATTACKS_BLACK[position.ctx.en_passant_square] & pawns : NO_SQUARES;
            promotions_forward       = pawn_single_pushes & RANK_8;
            promotions_west          = SHIFT_NORTHWEST(pawns) & position.black_pieces & RANK_8;
            promotions_east          = SHIFT_NORTHEAST(pawns) & position.black_pieces & RANK_8;     
            seventh_rank             = RANK_7;
        }
        else
        {
            const bitboard pawns     = position.pawns & position.black_pieces;
            pawn_push_delta          = -8;
            pawn_west_delta          = -9;
            pawn_east_delta          = -7;
            pawn_single_pushes       = SHIFT_SOUTH(pawns) & ~position.occupied_squares;
            pawn_double_pushes       = SHIFT_SOUTH(pawn_single_pushes) & ~position.occupied_squares & RANK_5;
            pawn_captures_en_passant = position.ctx.en_passant_square ? PAWN_ATTACKS_WHITE[position.ctx.en_passant_square] & pawns : NO_SQUARES;
            promotions_forward       = pawn_single_pushes & RANK_1;
            promotions_west          = SHIFT_SOUTHWEST(pawns) & position.white_pieces & RANK_1;
            promotions_east          = SHIFT_SOUTHEAST(pawns) & position.white_pieces & RANK_1;
            seventh_rank             = RANK_2;
        }
        pawn_single_pushes ^= promotions_forward;
    }

    $emit(Move)

    // generate moves in a plausible "best first" sequence
    // pawn promotions
    if (promotions_west | promotions_east | promotions_forward)
    {
        for (promoted_piece = QUEEN; promoted_piece >= KNIGHT; --promoted_piece)
        {
            targets = promotions_west;
            while (targets)
            {
                to = (uchar)FindAndClearLsb(&targets);
                from = to - pawn_west_delta;
                $yield(Move(from, to, promoted_piece));
            }
            targets = promotions_east;
            while (targets)
            {
                to = (uchar)FindAndClearLsb(&targets);
                from = to - pawn_east_delta;
                $yield(Move(from, to, promoted_piece));
            }
            targets = promotions_forward;
            while (targets)
            {
                to = (uchar)FindAndClearLsb(&targets);
                from = to - pawn_push_delta;
                $yield(Move(from, to, promoted_piece));
            }
        }
    }
    // captures MVV / LVA
    for (captured_piece = QUEEN; captured_piece != NO_PIECE; --captured_piece)
    {
        targets = position.pieces[captured_piece] & enemy_pieces;
        while (targets)
        {
            to = (uchar)FindAndClearLsb(&targets);
            attacks_to = position.AttacksToSquare(to, color);
            sources = attacks_to & position.pawns & ~seventh_rank;
            while (sources)
            {
                from = (uchar)FindAndClearLsb(&sources);
                $yield(Move(from, to));
            }
            for (moving_piece = KNIGHT; moving_piece <= KING; ++moving_piece)
            {
                sources = attacks_to & position.pieces[moving_piece];
                while (sources)
                {
                    from = (uchar)FindAndClearLsb(&sources);
                    $yield(Move(from, to));
                }
            }
        }      
    }
    // en passant captures
    while (pawn_captures_en_passant)
    {
        from = (uchar)FindAndClearLsb(&pawn_captures_en_passant);
        $yield(Move(from, position.ctx.en_passant_square, MOVE_EN_PASSANT_CAPTURE));
    }
    if (do_all_moves)
    {
        // castling
        if (!position.IsInCheck())
        {
            if (color == WHITE)
            {
                if ((position.ctx.castle_flags & MAY_WHITE_K) && !(position.occupied_squares & (F1BB | G1BB)) && !position.IsAttacked(F1, BLACK) && !position.IsAttacked(G1, BLACK))
                {
                    $yield(Move(E1, G1, MOVE_CASTLING));
                }
                if ((position.ctx.castle_flags & MAY_WHITE_Q) && !(position.occupied_squares & (B1BB | C1BB | D1BB)) && !position.IsAttacked(D1, BLACK) && !position.IsAttacked(C1, BLACK))
                {
                    $yield(Move(E1, C1, MOVE_CASTLING));
                }
            }
            else
            {
                if ((position.ctx.castle_flags & MAY_BLACK_K) && !(position.occupied_squares & (F8BB | G8BB)) && !position.IsAttacked(F8, WHITE) && !position.IsAttacked(G8, WHITE))
                {
                    $yield(Move(E8, G8, MOVE_CASTLING));
                }
                if ((position.ctx.castle_flags & MAY_BLACK_Q) && !(position.occupied_squares & (B8BB | C8BB | D8BB)) && !position.IsAttacked(D8, WHITE) && !position.IsAttacked(C8, WHITE))
                {
                    $yield(Move(E8, C8, MOVE_CASTLING));
                }
            }
        }
        // pawn non captures
        while (pawn_double_pushes)
        {
            to = (uchar)FindAndClearLsb(&pawn_double_pushes);
            $yield(Move(to - pawn_push_delta * 2, to));
        }
        while (pawn_single_pushes)
        {
            to = (uchar)FindAndClearLsb(&pawn_single_pushes);
            $yield(Move(to - pawn_push_delta, to));
        }
        // piece non captures
        for (moving_piece = KNIGHT; moving_piece <= KING; ++moving_piece)
        {
            sources = position.pieces[moving_piece] & friendly_pieces;
            while (sources)
            {
                from = (uchar)FindAndClearLsb(&sources);
                targets = position.AttacksFromSquare(from) & ~position.occupied_squares;
                while (targets)
                {
                    to = (uchar)FindAndClearLsb(&targets);
                    $yield(Move(from, to));
                }
            }
        }
    }
    $stop;

private:
    MoveGenerator& operator=(const MoveGenerator& that);
};

#pragma warning(pop)