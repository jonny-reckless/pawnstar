/// @file Alpha-beta search.

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "transposition_table.h"

/// @brief search a single move and return its score and whether it gives check.
single_move_result_t search_single_move(game_t *game, int depth, int ply, int alpha, int beta, move_t move,
                                        variation_t *pv, int move_index)
{
    const position_t *position     = &game->position;
    const bool        was_in_check = position_is_in_check(position);

    int child_depth = depth - 1;
    switch (move_type(move))
    {
    case MOVE_PROMOTION_KNIGHT:
    case MOVE_PROMOTION_BISHOP:
    case MOVE_PROMOTION_ROOK:
    case MOVE_PROMOTION_QUEEN:
        ++child_depth;
        INCREMENT("extensions promotion");
        break;

        // case MOVE_EP_CAPTURE:
        //     ++child_depth;
        //     INCREMENT("extensions ep capture");
        //     break;

    default:
        break;
    }

    game_play_move(game, move);
    const bool is_checking = position_is_in_check(&game->position);
    int        score;
    if (beta > alpha + 1 && move_index > 0 && !was_in_check && !is_checking)
    {
        INCREMENT("pvs attempts");
        score = -search(game, child_depth, ply + 1, -alpha - 1, -alpha, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -search(game, child_depth, ply + 1, -beta, -alpha, pv);
        }
    }
    else
    {
        score = -search(game, child_depth, ply + 1, -beta, -alpha, pv);
    }
    game_undo_move(game, move);
    return (single_move_result_t){score, is_checking};
}

typedef struct
{
    int score;
    int eval_score;
} null_move_result_t;

static inline null_move_result_t attempt_null_move(game_t *game, int depth, int ply, int alpha, int beta)
{
    const position_t *position = &game->position;
    const color_t     color    = position_color_to_move(position);
    if (!position_is_null_move(position) && !position_is_in_check(position) && beta == alpha + 1 &&
        popcount(position_pieces_of_color(position, color)) > 3)
    {
        const int pst_score = color == WHITE ? position->state.scores[WHITE] - position->state.scores[BLACK]
                                             : position->state.scores[BLACK] - position->state.scores[WHITE];
        if (pst_score >= beta + 100)
        {
            INCREMENT("null move");
            variation_t dummy;
            variation_clear(&dummy);
            game_make_null_move(game);
            int score = -search(game, depth - 4, ply + 1, -beta, -alpha, &dummy);
            game_undo_null_move(game);
            if (game->is_cancel_pending)
            {
                return (null_move_result_t){SEARCH_CANCELLED_SCORE, pst_score};
            }
            if (score >= beta)
            {
                return (null_move_result_t){beta, pst_score};
            }
            INCREMENT("null move fails");
            return (null_move_result_t){alpha, pst_score};
        }
    }
    return (null_move_result_t){alpha, ALPHA};
}

/// @brief Alpha-beta main search.
int search(game_t *game, int depth, int ply, int alpha, int beta, variation_t *parent_pv)
{
    INCREMENT("alpha beta calls");
    if ((++game->node_count & 0xFFFF) == 0 && game->time_control.hard_stop_ms != 0 &&
        chess_clock_elapsed_milliseconds(&game->time_control) >= game->time_control.hard_stop_ms)
    {
        game->is_cancel_pending = true;
        return SEARCH_CANCELLED_SCORE;
    }

    const position_t *position = &game->position;
    if (position_is_draw_by_material(position) || game_is_draw_by_fifty_moves(game) || game_is_draw_by_repetition(game))
    {
        return DRAW_SCORE;
    }

    if (position_is_in_check(position))
    {
        INCREMENT("checks");
        ++depth;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return evaluate_position(game, alpha, beta);
    }

    // transposition_t table lookup.
    transposition_t transposition;
    bool has_transposition = transposition_table_find(&game->transposition_table, position->state.hash, &transposition);
    if (has_transposition && transposition.depth >= depth)
    {
        switch ((transposition_node_type_t)transposition.node_type)
        {
        case TRANSPOSITION_CUT:
            INCREMENT("table hit cut node");
            if (transposition.score >= beta)
            {
                INCREMENT("table hit cut node cutoffs");
                return transposition.score;
            }
            break;

        case TRANSPOSITION_ALL:
            INCREMENT("table hit all node");
            if (transposition.score < alpha)
            {
                INCREMENT("table hit all node cutoffs");
                return transposition.score;
            }
            break;

        case TRANSPOSITION_PV:
            INCREMENT("table hit pv node");
            ++depth;
            break;
        }
    }

    if (depth <= 0 && !position_is_in_check(position))
    {
        return search_quiescent(game, depth, ply, alpha, beta);
    }

    null_move_result_t nm = attempt_null_move(game, depth, ply, alpha, beta);
    if (game->is_cancel_pending)
    {
        return SEARCH_CANCELLED_SCORE;
    }
    if (nm.score >= beta)
    {
        return nm.score;
    }

    variation_t pv;
    variation_clear(&pv);
    move_t best_move        = move_none();
    int    best_score       = ALPHA;
    bool   has_raised_alpha = false;

    if (has_transposition && !move_equals(transposition.move, move_none()))
    {
        INCREMENT("table move");
        best_move = transposition.move;
        int score = search_single_move(game, depth, ply, alpha, beta, transposition.move, &pv, 0).score;
        if (game->is_cancel_pending)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            transposition_t rec = {game->position.state.hash,
                                   transposition.move,
                                   score,
                                   (int16_t)depth,
                                   (uint8_t)TRANSPOSITION_CUT,
                                   false};
            transposition_table_record(&game->transposition_table, &rec);
            history_table_record_good_move(&game->history_table, ply, transposition.move);
            return score;
        }
        best_score = score;
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha            = score;
            has_raised_alpha = true;
            history_table_record_good_move(&game->history_table, ply, transposition.move);
        }
    }

    const position_t *pos       = &game->position;
    move_list_t       move_list = position_generate_legal_moves(pos);
    if (move_list.size == 0)
    {
        if (position_is_in_check(pos))
        {
            INCREMENT("checkmates");
            return CHECKMATED_SCORE + ply;
        }
        INCREMENT("stalemates");
        return DRAW_SCORE;
    }

    game_score_and_sort_moves(game, &move_list, ply);

    for (int i = 0; i < move_list.size; ++i)
    {
        move_t move = move_list.items[i];
        if (has_transposition && move_equals(move, transposition.move))
        {
            continue;
        }

        int lmr_depth = depth;
        if (i > 3 && !position_is_in_check(pos) && depth > 2 && beta == alpha + 1 && move_captured(move) == NONE &&
            move_piece(move) != PAWN)
        {
            INCREMENT("late move reduction 1");
            --lmr_depth;
            if (i > 6)
            {
                INCREMENT("late move reduction 2");
                --lmr_depth;
                if (history_table_get_count(&game->history_table, ply, move) == 0)
                {
                    INCREMENT("late move reduction 3");
                    --lmr_depth;
                }
            }
        }

        int score = search_single_move(game, lmr_depth, ply, alpha, beta, move, &pv, i).score;
        if (score > alpha && lmr_depth < depth)
        {
            INCREMENT("late move reduction fails");
            score = search_single_move(game, depth, ply, alpha, beta, move, &pv, i).score;
        }
        if (game->is_cancel_pending)
        {
            return SEARCH_CANCELLED_SCORE;
        }

        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            transposition_t rec = {game->position.state.hash,  move, score, (int16_t)depth,
                                   (uint8_t)TRANSPOSITION_CUT, false};
            transposition_table_record(&game->transposition_table, &rec);
            history_table_record_good_move(&game->history_table, ply, move);
            return score;
        }
        if (score > best_score)
        {
            INCREMENT("best score changed");
            best_score = score;
            best_move  = move;
            if (score > alpha)
            {
                INCREMENT("pv changed");
                alpha            = score;
                has_raised_alpha = true;
                history_table_record_good_move(&game->history_table, ply, move);
            }
        }
    }

    const zobrist_t pos_hash = game->position.state.hash;
    if (has_raised_alpha)
    {
        INCREMENT("pv nodes");
        transposition_t rec = {pos_hash, best_move, alpha, (int16_t)depth, (uint8_t)TRANSPOSITION_PV, false};
        transposition_table_record(&game->transposition_table, &rec);
        history_table_record_good_move(&game->history_table, ply, best_move);
        variation_copy(parent_pv, &pv, best_move);
    }
    else
    {
        INCREMENT("all nodes");
        transposition_t rec = {pos_hash, best_move, best_score, (int16_t)depth, (uint8_t)TRANSPOSITION_ALL, false};
        transposition_table_record(&game->transposition_table, &rec);
    }
    return best_score;
}
