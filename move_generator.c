#include "pawnstar.h"
/******************************************************************************
Move generator 
Resumable generator using the switch statement for re-entrancy was inspired 
by Simon Tatham's excellent article "Coroutines in C":
http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
*******************************************************************************/
#define GENERATOR_START     switch(gen->resume_line_num) { case 0: ;
#define YIELD(x)            { gen->resume_line_num = __LINE__; return (x); case __LINE__: ; } 
#define GENERATOR_END       } gen->resume_line_num = 0; 

void InitMoveGen(MoveGenerator* gen, const Position* position, bool do_all_moves)
{
    gen->resume_line_num = 0;
    gen->position        = position;
    gen->do_all_moves    = do_all_moves;
    gen->color           = COLOR_TO_MOVE(position);
    gen->friendly_pieces = position->pieces_of_color[gen->color];
    gen->enemy_pieces    = position->occupied_squares ^ gen->friendly_pieces;
    if (gen->color == WHITE)
    {
        const bitboard pawns          = position->pawns & position->white_pieces;
        gen->pawn_push_delta          = 8;
        gen->pawn_west_delta          = 7;
        gen->pawn_east_delta          = 9;
        gen->pawn_single_pushes       = SHIFT_NORTH(pawns) & ~position->occupied_squares;
        gen->pawn_double_pushes       = SHIFT_NORTH(gen->pawn_single_pushes) & ~position->occupied_squares & RANK_4;
        gen->pawn_captures_en_passant = position->en_passant_index ? PAWN_ATTACKS_BLACK[position->en_passant_index] & pawns : NO_SQUARES;
        gen->promotions_forward       = gen->pawn_single_pushes & RANK_8;
        gen->promotions_west          = SHIFT_NORTHWEST(pawns) & position->black_pieces & RANK_8;
        gen->promotions_east          = SHIFT_NORTHEAST(pawns) & position->black_pieces & RANK_8;     
        gen->seventh_rank             = RANK_7;
    }
    else
    {
        const bitboard pawns          = position->pawns & position->black_pieces;
        gen->pawn_push_delta          = -8;
        gen->pawn_west_delta          = -9;
        gen->pawn_east_delta          = -7;
        gen->pawn_single_pushes       = SHIFT_SOUTH(pawns) & ~position->occupied_squares;
        gen->pawn_double_pushes       = SHIFT_SOUTH(gen->pawn_single_pushes) & ~position->occupied_squares & RANK_5;
        gen->pawn_captures_en_passant = position->en_passant_index ? PAWN_ATTACKS_WHITE[position->en_passant_index] & pawns : NO_SQUARES;
        gen->promotions_forward       = gen->pawn_single_pushes & RANK_1;
        gen->promotions_west          = SHIFT_SOUTHWEST(pawns) & position->white_pieces & RANK_1;
        gen->promotions_east          = SHIFT_SOUTHEAST(pawns) & position->white_pieces & RANK_1;
        gen->seventh_rank             = RANK_2;
    }
    gen->pawn_single_pushes ^= gen->promotions_forward; /* don't do promotional pushes twice */
}
/******************************************************************************
Move generator to yield moves one-by-one in a plausible best first order:

1) Capturing pawn promotions
2) Non capturing pawn promotions
3) Captures by most valuable victim, least valuable attacker
4) En passant captures

If generating all moves:

5) Castling
6) Double pawn pushes
7) Single pawn pushes
8) Non capturing piece moves, in ascending piece value order

NB: Since this is a resumable generator function, there are no locals on 
the stack: all variables are contained within the MoveGenerator structure.
*******************************************************************************/
int NextMove(MoveGenerator* gen)
{
    GENERATOR_START;
    /* promotions */
    while (gen->promotions_west)
    {
        gen->to             = FindAndClearLsb(&gen->promotions_west);
        gen->from           = gen->to - gen->pawn_west_delta;
        gen->captured_piece = PIECE_AT(gen->position, gen->to);
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, QUEEN));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, KNIGHT));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, ROOK));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, BISHOP));
    }
    while (gen->promotions_east)
    {
        gen->to             = FindAndClearLsb(&gen->promotions_east);
        gen->from           = gen->to - gen->pawn_east_delta;
        gen->captured_piece = PIECE_AT(gen->position, gen->to);
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, QUEEN));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, KNIGHT));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, ROOK));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_CAPTURE, gen->captured_piece, BISHOP));
    }
    while (gen->promotions_forward)
    {
        gen->to   = FindAndClearLsb(&gen->promotions_forward);
        gen->from = gen->to - gen->pawn_push_delta;
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_NON_CAPTURE, NO_PIECE, QUEEN));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_NON_CAPTURE, NO_PIECE, KNIGHT));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_NON_CAPTURE, NO_PIECE, ROOK));
        YIELD(CONSTRUCT_PROMOTION_MOVE(gen->from, gen->to, PAWN_PROMOTION_NON_CAPTURE, NO_PIECE, BISHOP));
    }
    /* regular captures MVV / LVA */
    for (gen->captured_piece = QUEEN; gen->captured_piece != NO_PIECE; --gen->captured_piece)
    {
        for (gen->moving_piece = PAWN; gen->moving_piece <= KING; ++gen->moving_piece)
        {
            gen->targets = gen->position->pieces[gen->captured_piece] & gen->enemy_pieces;
            while (gen->targets)
            {
                gen->to      = FindAndClearLsb(&gen->targets);
                gen->sources = AttacksToSquareByType(gen->position, gen->to, gen->color, gen->moving_piece);
                if (gen->moving_piece == PAWN)
                {
                    gen->sources &= ~gen->seventh_rank; /* don't repeat promoting captures: we already generated those */
                }
                while (gen->sources)
                {
                    gen->from = FindAndClearLsb(&gen->sources);
                    YIELD(CONSTRUCT_MOVE(gen->from, gen->to, gen->moving_piece, CAPTURE, gen->captured_piece));
                }
            }
        }
    }
    /* en passant captures */
    while (gen->pawn_captures_en_passant)
    {
        gen->from = FindAndClearLsb(&gen->pawn_captures_en_passant);
        YIELD(CONSTRUCT_MOVE(gen->from, gen->position->en_passant_index, PAWN, EN_PASSANT_CAPTURE, PAWN));
    }
    if (gen->do_all_moves)
    {
        /* castling */
        if (!(gen->position->state_flags & IS_CHECK))
        {
            if (gen->color == WHITE)
            {
                if ((gen->position->castle_flags & MAY_WHITE_K)        && 
                    !(gen->position->occupied_squares & (F1BB | G1BB)) && 
                    !IsAttacked(gen->position, F1, BLACK)              &&
                    !IsAttacked(gen->position, G1, BLACK))
                {
                    YIELD(CONSTRUCT_MOVE(E1, G1, KING, CASTLING, NO_PIECE));
                }
                if ((gen->position->castle_flags & MAY_WHITE_Q)               && 
                    !(gen->position->occupied_squares & (B1BB | C1BB | D1BB)) && 
                    !IsAttacked(gen->position, D1, BLACK)                     &&
                    !IsAttacked(gen->position, C1, BLACK))
                {
                    YIELD(CONSTRUCT_MOVE(E1, C1, KING, CASTLING, NO_PIECE));
                }
            }
            else
            {
                if ((gen->position->castle_flags & MAY_BLACK_K)        && 
                    !(gen->position->occupied_squares & (F8BB | G8BB)) && 
                    !IsAttacked(gen->position, F8, WHITE)              &&
                    !IsAttacked(gen->position, G8, WHITE))
                {
                    YIELD(CONSTRUCT_MOVE(E8, G8, KING, CASTLING, NO_PIECE));
                }
                if ((gen->position->castle_flags & MAY_BLACK_Q)               && 
                    !(gen->position->occupied_squares & (B8BB | C8BB | D8BB)) && 
                    !IsAttacked(gen->position, D8, WHITE)                     &&
                    !IsAttacked(gen->position, C8, WHITE))
                {
                    YIELD(CONSTRUCT_MOVE(E8, C8, KING, CASTLING, NO_PIECE));
                }
            }
        }
        /* pawn non captures */
        while (gen->pawn_double_pushes)
        {
            gen->to = FindAndClearLsb(&gen->pawn_double_pushes);
            YIELD(CONSTRUCT_MOVE(gen->to - gen->pawn_push_delta * 2, gen->to, PAWN, DOUBLE_PAWN_PUSH, NO_PIECE));
        }
        while (gen->pawn_single_pushes)
        {
            gen->to = FindAndClearLsb(&gen->pawn_single_pushes);
            YIELD(CONSTRUCT_NON_CAPTURE_MOVE(gen->to - gen->pawn_push_delta, gen->to, PAWN));
        }
        /* piece non captures */
        for (gen->moving_piece = KNIGHT; gen->moving_piece <= KING; ++gen->moving_piece)
        {
            gen->sources = gen->position->pieces[gen->moving_piece] & gen->friendly_pieces;
            while (gen->sources)
            {
                gen->from    = FindAndClearLsb(&gen->sources);
                gen->targets = AttacksFromSquare(gen->position, gen->from, gen->moving_piece) & ~gen->position->occupied_squares;
                while (gen->targets)
                {
                    gen->to = FindAndClearLsb(&gen->targets);
                    YIELD(CONSTRUCT_NON_CAPTURE_MOVE(gen->from, gen->to, gen->moving_piece));
                }
            }
        }
    }
    YIELD(0); /* terminates move sequence */
    GENERATOR_END;
    return 0;
}