/// @file Quiescence search.

#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"

/// @brief Alpha-beta quiescence search.
int search_quiescent(game_t *game, int depth, int ply, int alpha, int beta)
{
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return evaluate_position(game, alpha, beta);
    }
    if (position_is_in_check(&game->position))
    {
        INCREMENT("quiescent checks");
        variation_t dummy;
        variation_list_clear(&dummy);
        return search(game, depth, ply, alpha, beta, &dummy);
    }

    int score = evaluate_position(game, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        return score;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    int best_score = score;

    move_list_t move_list = position_generate_legal_captures(&game->position);
    game_score_and_sort_moves(game, &move_list, ply);

    for (int i = 0; i < move_list.size; ++i)
    {
        move_t move = move_list.items[i];

        game_play_move(game, move);
        score = -search_quiescent(game, depth - 1, ply + 1, -beta, -alpha);
        game_undo_move(game, move);
        if (game->is_cancel_pending)
        {
            return SEARCH_CANCELLED_SCORE;
        }

        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            history_table_record_good_move(&game->history_table, ply, move);
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
                history_table_record_good_move(&game->history_table, ply, move);
            }
        }
    }
    return best_score;
}
