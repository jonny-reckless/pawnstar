/// @file Quiescence search.

#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "search_state.h"

int search_quiescent(search_state_t *ss, int depth, int ply, int alpha, int beta)
{
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return evaluate_position(ss_current_position(ss), alpha, beta);
    }
    if (position_is_in_check(ss_current_position(ss)))
    {
        INCREMENT("quiescent checks");
        variation_t dummy;
        variation_clear(&dummy);
        return search(ss, depth, ply, alpha, beta, &dummy);
    }

    int score = evaluate_position(ss_current_position(ss), alpha, beta);
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

    move_list_t move_list = position_generate_legal_captures(ss_current_position(ss));
    game_score_and_sort_moves(ss->game, &move_list, ply);

    for (int i = 0; i < move_list.size; ++i)
    {
        move_t move = move_list.items[i];

        ss_play_move(ss, move);
        score = -search_quiescent(ss, depth - 1, ply + 1, -beta, -alpha);
        ss_undo_move(ss);
        if (ss_is_cancelled(ss))
        {
            return SEARCH_CANCELLED_SCORE;
        }

        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
            }
        }
    }
    return best_score;
}
