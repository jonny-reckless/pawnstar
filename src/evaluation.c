#include "evaluation.h"
#include "attacks.h"
#include "constants.h"
#include "debug_hashtable.h"
#include "game.h"
#include "generated_data.h"
#include "position.h"
#include "square.h"

// clang-format off
static const int PAWN_SQUARE[64] =
{
     0,   0,   0,   0,   0,   0,   0,   0,
    25,  25,  25,  25,  25,  25,  25,  25,
    15,  15,  15,  20,  20,  15,  15,  15,
    10,  10,  10,  15,  15,  10,  10,  10,
     0,   0,   0,  10,  10,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0, -40, -40,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
};

static const int KNIGHT_SQUARE[64] =
{
   -20, -10, -10, -10, -10, -10, -10, -20,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -20, -20, -10, -10, -10, -10, -20, -20,
};

static const int BISHOP_SQUARE[64] =
{
   -10, -10, -10, -10, -10, -10, -10, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,  10,  10,   5,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10, -10, -20, -10, -10, -20, -10, -10,
};

static const int ROOK_SQUARE[64] =
{
     0,   0,   0,   0,   0,   0,   0,   0,
     5,  10,  10,  10,  10,  10,  10,   5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   5,   5,   5,   5,   0,  -5,
};

static const int QUEEN_SQUARE[64] =
{
   -10, -10, -10,  -5,  -5, -10, -10, -10,
   -10,   0,   0,   0,   0,   0,   0, -10,
   -10,   0,   5,   5,   5,   5,   0, -10,
    -5,   0,   5,   5,   5,   5,   0,  -5,
     0,   0,   5,   5,   5,   5,   0,  -5,
   -10,   5,   5,   5,   5,   5,   0, -10,
   -10,   0,   5,   0,   0,   0,   0, -10,
   -10, -10, -10,  -5,  -5, -10, -10, -10,
};

static const int KING_SQUARE_MIDGAME[64] =
{
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -40, -40, -40, -40, -40, -40, -40, -40,
   -20, -20, -20, -20, -20, -20, -20, -20,
    20,  40,  40, -20,   0, -20,  40,  20,
};

static const int KING_SQUARE_ENDGAME[64] =
{
     0,  10,  20,  30,  30,  20,  10,   0,
    10,  20,  30,  40,  40,  30,  20,  10,
    20,  30,  40,  50,  50,  40,  30,  20,
    30,  40,  50,  60,  60,  50,  40,  30,
    30,  40,  50,  60,  60,  50,  40,  30,
    20,  30,  40,  50,  50,  40,  30,  20,
    10,  20,  30,  40,  40,  30,  20,  10,
     0,  10,  20,  30,  30,  20,  10,   0,
};

static const int PASSED_PAWN_SQUARE[64] =
{
     0,   0,   0,   0,   0,   0,   0,   0,
    30,  30,  30,  30,  30,  30,  30,  30,
    30,  30,  30,  30,  30,  30,  30,  30,
    20,  20,  20,  20,  20,  20,  20,  20,
    15,  15,  15,  15,  15,  15,  15,  15,
    10,  10,  10,  10,  10,  10,  10,  10,
     5,   5,   5,   5,   5,   5,   5,   5,
     0,   0,   0,   0,   0,   0,   0,   0,
};

static const int BISHOP_ATTACK_SCORES[14] = {-20, -10, -5, -5, 0, 5, 10, 15, 20, 20, 20, 20, 20, 20};
static const int ROOK_ATTACK_SCORES[15]   = {-10, -10, -5, -5, 0, 5, 10, 15, 20, 20, 20, 20, 20, 20, 20};

static const bitboard_t FILE_BITBOARDS[8] = {
    0x0101010101010101ull, 0x0202020202020202ull, 0x0404040404040404ull, 0x0808080808080808ull,
    0x1010101010101010ull, 0x2020202020202020ull, 0x4040404040404040ull, 0x8080808080808080ull,
};
// clang-format on

static pawn_structure_t determine_pawn_structure(const position_t *pos, color_t color)
{
    const bitboard_t friendly_pawns = (pos->pawns & position_pieces_of_color(pos, color));
    const bitboard_t enemy_pawns    = (pos->pawns ^ friendly_pawns);
    pawn_structure_t ps             = {NO_SQUARES, NO_SQUARES, NO_SQUARES, NO_SQUARES, NO_SQUARES};

    const bitboard_t *passed_pawn_mask    = color == WHITE ? PASSED_PAWN_MASK_WHITE : PASSED_PAWN_MASK_BLACK;
    const bitboard_t *isolated_pawn_mask  = color == WHITE ? ISOLATED_PAWN_MASK_WHITE : ISOLATED_PAWN_MASK_BLACK;
    const bitboard_t *supported_pawn_mask = color == WHITE ? SUPPORTED_PAWN_MASK_WHITE : SUPPORTED_PAWN_MASK_BLACK;
    const bitboard_t *doubled_pawn_mask   = color == WHITE ? DOUBLED_PAWN_MASK_WHITE : DOUBLED_PAWN_MASK_BLACK;

    if (color == WHITE)
    {
        ps.defended_pawns =
            (friendly_pawns & (bitboard_shift_northwest(friendly_pawns) | bitboard_shift_northeast(friendly_pawns)));
    }
    else
    {
        ps.defended_pawns =
            (friendly_pawns & (bitboard_shift_southwest(friendly_pawns) | bitboard_shift_southeast(friendly_pawns)));
    }

    square_t s;
    BB_FOREACH(s, friendly_pawns)
    {
        const bitboard_t b = bitboard_from_square(s);
        if ((!((passed_pawn_mask[s] & enemy_pawns))))
        {
            ps.passed_pawns |= b;
        }
        if (((doubled_pawn_mask[s] & friendly_pawns)))
        {
            ps.doubled_pawns |= b;
        }
        if ((!((isolated_pawn_mask[s] & friendly_pawns))))
        {
            ps.isolated_pawns |= b;
        }
        if ((!((supported_pawn_mask[s] & friendly_pawns))))
        {
            if (color == WHITE)
            {
                const bitboard_t enemy_pawn_attacks =
                    (bitboard_shift_southwest(enemy_pawns) | bitboard_shift_southeast(enemy_pawns));
                const square_t forward_locn = (square_t)(s + 8);
                if (((enemy_pawn_attacks & bitboard_from_square(forward_locn))))
                {
                    ps.backward_pawns |= b;
                }
            }
            else
            {
                const bitboard_t enemy_pawn_attacks =
                    (bitboard_shift_northwest(enemy_pawns) | bitboard_shift_northeast(enemy_pawns));
                const square_t forward_locn = (square_t)(s - 8);
                if (((enemy_pawn_attacks & bitboard_from_square(forward_locn))))
                {
                    ps.backward_pawns |= b;
                }
            }
        }
    }
    ps.passed_pawns &= (~ps.doubled_pawns);
    return ps;
}

static int evaluate_material(const position_t *pos, color_t color)
{
    const bitboard_t friendly_pieces = position_pieces_of_color(pos, color);
    const bitboard_t pawns           = (pos->pawns & friendly_pieces);
    const bitboard_t knights         = (pos->knights & friendly_pieces);
    const bitboard_t bishops         = (pos->bishops & friendly_pieces);
    const bitboard_t rooks           = (pos->rooks & friendly_pieces);
    const bitboard_t queens          = (pos->queens & friendly_pieces);
    // clang-format off
    int score = popcount(pawns)   *  100 +
                popcount(knights) *  400 +
                popcount(bishops) *  400 +
                popcount(rooks)   *  600 +
                popcount(queens)  * 1200;
    // clang-format on
    if (((bishops & WHITE_SQUARES)) && ((bishops & BLACK_SQUARES)))
    {
        score += 50;
    }
    return score;
}

static int evaluate_mobility(const position_t *pos, color_t color, const pawn_structure_t *ps)
{
    const bitboard_t friendly_pieces  = position_pieces_of_color(pos, color);
    const bitboard_t occupied_squares = position_occupied_squares(pos);
    int              score            = 0;
    square_t         s;
    BB_FOREACH(s, (pos->bishops & friendly_pieces))
    {
        bitboard_t attacks = (bishop_attacks(occupied_squares, s) & (~friendly_pieces));
        if (color == WHITE)
        {
            attacks &= (~ps[BLACK].defended_pawns);
        }
        else
        {
            attacks &= (~ps[WHITE].defended_pawns);
        }
        score += BISHOP_ATTACK_SCORES[popcount(attacks)];
    }
    BB_FOREACH(s, (pos->rooks & friendly_pieces))
    {
        const bitboard_t attacks = (rook_attacks(occupied_squares, s) & (~friendly_pieces));
        score += ROOK_ATTACK_SCORES[popcount(attacks)];
    }
    return score;
}

static int evaluate_piece_square(const position_t *pos, color_t color)
{
    const uint8_t    rank_flip       = (uint8_t)(color == WHITE ? RANK_FLIP : 0);
    const bitboard_t friendly_pieces = position_pieces_of_color(pos, color);
    int              score           = 0;
    square_t         s;
    BB_FOREACH(s, (pos->pawns & friendly_pieces))
    {
        score += PAWN_SQUARE[s ^ rank_flip];
    }
    BB_FOREACH(s, (pos->knights & friendly_pieces))
    {
        score += KNIGHT_SQUARE[s ^ rank_flip];
    }
    BB_FOREACH(s, (pos->bishops & friendly_pieces))
    {
        score += BISHOP_SQUARE[s ^ rank_flip];
    }
    BB_FOREACH(s, (pos->rooks & friendly_pieces))
    {
        score += ROOK_SQUARE[s ^ rank_flip];
    }
    BB_FOREACH(s, (pos->queens & friendly_pieces))
    {
        score += QUEEN_SQUARE[s ^ rank_flip];
    }
    return score;
}

static int evaluate_pawn_structure(const pawn_structure_t *ps, color_t color)
{
    const uint8_t rank_flip = (uint8_t)(color == WHITE ? RANK_FLIP : 0);
    int           score     = 0;
    square_t      s;
    BB_FOREACH(s, ps->passed_pawns)
    {
        score += PASSED_PAWN_SQUARE[s ^ rank_flip];
    }
    score += popcount(ps->defended_pawns) * 5;
    score -= popcount(ps->backward_pawns) * 10;
    score -= popcount(ps->doubled_pawns) * 10;
    score -= popcount(ps->isolated_pawns) * 20;
    return score;
}

static int evaluate_king(const position_t *pos, color_t color)
{
    const uint8_t    rank_flip       = (uint8_t)(color == WHITE ? RANK_FLIP : 0);
    const bitboard_t friendly_pieces = position_pieces_of_color(pos, color);
    const bitboard_t enemy_pieces    = position_pieces_of_color(pos, enemy_of(color));
    const bitboard_t friendly_pawns  = (friendly_pieces & pos->pawns);
    const bitboard_t enemy_pawns     = (enemy_pieces & pos->pawns);
    const bool       is_endgame      = (!(pos->queens)) && popcount((position_occupied_squares(pos) ^ pos->pawns)) < 8;

    const square_t king_loc = pos->king_location[color];
    const int      piece_square_score =
        is_endgame ? KING_SQUARE_ENDGAME[king_loc ^ rank_flip] : KING_SQUARE_MIDGAME[king_loc ^ rank_flip];

    int safety_score = 0;
    if (!is_endgame)
    {
        const bitboard_t pawn_shelter_1 =
            color == WHITE ? KING_PAWN_SHELTER_WHITE[king_loc] : KING_PAWN_SHELTER_BLACK[king_loc];
        bitboard_t pawn_shelter_2, pawn_shelter_3;
        if (color == WHITE)
        {
            pawn_shelter_2 = bitboard_shift_north(pawn_shelter_1);
            pawn_shelter_3 = bitboard_shift_north(pawn_shelter_2);
        }
        else
        {
            pawn_shelter_2 = bitboard_shift_south(pawn_shelter_1);
            pawn_shelter_3 = bitboard_shift_south(pawn_shelter_2);
        }
        // clang-format off
        safety_score =
            15 * popcount((friendly_pawns & pawn_shelter_1)) +
            10 * popcount((friendly_pawns & pawn_shelter_2)) +
             5 * popcount((friendly_pawns & pawn_shelter_3)) -
            15 * popcount((enemy_pawns & pawn_shelter_1)) -
            10 * popcount((enemy_pawns & pawn_shelter_2)) -
             5 * popcount((enemy_pawns & pawn_shelter_3));
        // clang-format on

        const bitboard_t king_file = FILE_BITBOARDS[square_file(king_loc)];
        if ((!((king_file & friendly_pawns))))
        {
            safety_score -= 15;
        }
        bitboard_t adjacent_file = bitboard_shift_west(king_file);
        if ((adjacent_file) && (!((adjacent_file & friendly_pawns))))
        {
            safety_score -= 10;
        }
        adjacent_file = bitboard_shift_east(king_file);
        if ((adjacent_file) && (!((adjacent_file & friendly_pawns))))
        {
            safety_score -= 10;
        }
    }
    const bitboard_t adjacent1 = KING_ATTACKS[king_loc];
    const bitboard_t adjacent2 = (KING_ATTACKS2[king_loc] ^ adjacent1);
    const bitboard_t non_pawns = (friendly_pieces ^ friendly_pawns);
    safety_score += 10 * popcount((adjacent1 & non_pawns)) + 5 * popcount((adjacent2 & non_pawns));
    return piece_square_score + safety_score;
}

int evaluate_position(const game_t *game, int alpha, int beta)
{
    INCREMENT("eval calls");
    const position_t *position = game_current_position_const(game);
    if (position_is_draw_by_material(position) || game_is_draw_by_fifty_moves(game) || game_is_draw_by_repetition(game))
    {
        return DRAW_SCORE;
    }
    int scores[2];
    scores[WHITE] = evaluate_material(position, WHITE);
    scores[BLACK] = evaluate_material(position, BLACK);
    int score =
        position_color_to_move(position) == WHITE ? scores[WHITE] - scores[BLACK] : scores[BLACK] - scores[WHITE];
    if (score > beta + 200 || score < alpha - 200)
    {
        INCREMENT("eval material cutoff");
        return score;
    }
    scores[WHITE] += evaluate_piece_square(position, WHITE);
    scores[BLACK] += evaluate_piece_square(position, BLACK);
    pawn_structure_t ps[2];
    ps[WHITE] = determine_pawn_structure(position, WHITE);
    ps[BLACK] = determine_pawn_structure(position, BLACK);
    scores[WHITE] += evaluate_pawn_structure(&ps[WHITE], WHITE);
    scores[BLACK] += evaluate_pawn_structure(&ps[BLACK], BLACK);
    scores[WHITE] += evaluate_mobility(position, WHITE, ps);
    scores[BLACK] += evaluate_mobility(position, BLACK, ps);
    scores[WHITE] += evaluate_king(position, WHITE);
    scores[BLACK] += evaluate_king(position, BLACK);
    score = position_color_to_move(position) == WHITE ? scores[WHITE] - scores[BLACK] : scores[BLACK] - scores[WHITE];
    return (score / 5) * 5;
}
