#include "static_exchange_evaluation.h"
#include "attacks.h"
#include "generated_data.h"
#include "position.h"

#include <string.h>

static const int SEE_VALUES[7] = {0, 100, 300, 300, 500, 900, 10000};

typedef struct
{
    bitboard_t pawns;
    bitboard_t knights;
    bitboard_t bishops;
    bitboard_t rooks;
    bitboard_t queens;
    bitboard_t kings;
    bitboard_t white_pieces;
    bitboard_t black_pieces;
} see_board_t;

static inline bitboard_t *see_board_pieces_of_type(see_board_t *bb, piece_t piece)
{
    switch (piece)
    {
    case PAWN:
        return &bb->pawns;
    case KNIGHT:
        return &bb->knights;
    case BISHOP:
        return &bb->bishops;
    case ROOK:
        return &bb->rooks;
    case QUEEN:
        return &bb->queens;
    default:
        return &bb->kings;
    }
}

static inline bitboard_t *see_board_pieces_of_color(see_board_t *bb, color_t color)
{
    return color == WHITE ? &bb->white_pieces : &bb->black_pieces;
}

static int evaluate_swap_off(see_board_t *bb, square_t location, color_t color, piece_t piece_on_square)
{
    const bitboard_t pawn_attacks   = color == WHITE ? PAWN_ATTACKS_BLACK[location] : PAWN_ATTACKS_WHITE[location];
    const bitboard_t knight_attacks = KNIGHT_ATTACKS[location];
    const bitboard_t king_attacks   = KING_ATTACKS[location];
    const bitboard_t square         = bitboard_from_square(location);
    bitboard_t       occupied       = (bb->white_pieces | bb->black_pieces);
    int              scores[32];
    int              ply;

    for (ply = 0; ply != 32; ++ply)
    {
        piece_t    capturing_piece;
        bitboard_t b_attacks;
        bitboard_t r_attacks;
        bitboard_t attacking = *see_board_pieces_of_color(bb, color);
        bitboard_t attackers = ((pawn_attacks & attacking) & bb->pawns);
        if ((attackers))
        {
            capturing_piece = PAWN;
            goto found;
        }

        attackers = ((knight_attacks & attacking) & bb->knights);
        if ((attackers))
        {
            capturing_piece = KNIGHT;
            goto found;
        }

        b_attacks = bishop_attacks(occupied, location);
        attackers = ((b_attacks & attacking) & bb->bishops);
        if ((attackers))
        {
            capturing_piece = BISHOP;
            goto found;
        }

        r_attacks = rook_attacks(occupied, location);
        attackers = ((r_attacks & attacking) & bb->rooks);
        if ((attackers))
        {
            capturing_piece = ROOK;
            goto found;
        }

        attackers = (((b_attacks | r_attacks) & attacking) & bb->queens);
        if ((attackers))
        {
            capturing_piece = QUEEN;
            goto found;
        }

        attackers = ((king_attacks & attacking) & bb->kings);
        if ((attackers))
        {
            capturing_piece = KING;
            goto found;
        }

        scores[ply] = 0;
        break;

    found:
        attackers = isolate_lsb(attackers);
        *see_board_pieces_of_type(bb, piece_on_square) ^= square;
        *see_board_pieces_of_color(bb, enemy_of(color)) ^= square;
        *see_board_pieces_of_type(bb, capturing_piece) ^= (attackers | square);
        *see_board_pieces_of_color(bb, color) ^= (attackers | square);
        occupied ^= attackers;
        scores[ply]     = SEE_VALUES[piece_on_square];
        piece_on_square = capturing_piece;
        color           = enemy_of(color);
        if (piece_on_square == PAWN && ((square & (RANK1 | RANK8))))
        {
            piece_on_square = QUEEN;
            bb->pawns ^= square;
            bb->queens ^= square;
        }
    }
    for (--ply; ply >= 0; --ply)
    {
        scores[ply] -= scores[ply + 1];
        if (scores[ply] < 0)
        {
            scores[ply] = 0;
        }
    }
    return scores[0];
}

see_result_t evaluate_static_exchange(const position_t *src_position, move_t move)
{
    position_t dst_position;
    position_make_move(&dst_position, src_position, move);
    const bool is_checking  = position_is_in_check(&dst_position);

    see_board_t bb;
    memcpy(&bb, &dst_position, sizeof(see_board_t));
    const piece_t piece    = move_piece(move);
    const piece_t captured = move_captured(move);
    const piece_t promoted = move_promoted(move);
    if (promoted != NONE)
    {
        return (see_result_t){
            SEE_VALUES[captured] + SEE_VALUES[promoted] - SEE_VALUES[PAWN] -
                evaluate_swap_off(&bb, move_to(move), position_color_to_move(&dst_position), move_promoted(move)),
            is_checking};
    }
    return (see_result_t){SEE_VALUES[captured] -
                              evaluate_swap_off(&bb, move_to(move), position_color_to_move(&dst_position), piece),
                          is_checking};
}
