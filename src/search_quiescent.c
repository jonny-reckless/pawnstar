/// @file Quiescence search.

#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "transposition_table.h"

/// @brief Alpha-beta quiescence search.
int search_quiescent(game_t *game, int depth, int ply, int alpha, int beta)
{
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return evaluate_position(game, alpha, beta);
    }
    if (position_is_in_check(game_current_position(game)))
    {
        INCREMENT("quiescent checks");
        variation_list_t dummy;
        variation_list_clear(&dummy);
        return search(game, depth, ply, alpha, beta, &dummy);
    }

    const zobrist_t hash = position_hash(game_current_position(game));
    transposition_t transposition;
    bool            has_transposition = transposition_table_find(&game->quiescent_table, hash, &transposition);
    if (has_transposition && transposition.score >= beta)
    {
        INCREMENT("quiescent table beta cutoffs");
        return transposition.score;
    }

    int score = evaluate_position(game, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        transposition_t rec = {hash, move_none(), score, (int16_t)depth, (uint8_t)TRANSPOSITION_CUT, false};
        transposition_table_record(&game->quiescent_table, &rec);
        return score;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    int best_score = score;

    if (has_transposition && !move_equals(transposition.move, move_none()))
    {
        INCREMENT("quiescent table move");
        game_play_move(game, transposition.move);
        score = -search_quiescent(game, depth - 1, ply + 1, -beta, -alpha);
        game_undo_move(game);
        if (game->is_cancel_pending)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent table move beta cutoffs");
            history_table_record_good_move(&game->history_table, ply, transposition.move);
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent table move raise alpha");
                history_table_record_good_move(&game->history_table, ply, transposition.move);
            }
        }
    }

    move_list_t move_list = position_generate_legal_captures(game_current_position(game));
    game_score_and_sort_moves(game, &move_list, ply);

    for (int i = 0; i < move_list.size; ++i)
    {
        move_t move = move_list.items[i];
        if (has_transposition && move_equals(move, transposition.move))
        {
            continue;
        }

        game_play_move(game, move);
        score = -search_quiescent(game, depth - 1, ply + 1, -beta, -alpha);
        game_undo_move(game);
        if (game->is_cancel_pending)
        {
            return SEARCH_CANCELLED_SCORE;
        }

        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            history_table_record_good_move(&game->history_table, ply, move);
            transposition_t rec = {hash, move, score, (int16_t)depth, (uint8_t)TRANSPOSITION_CUT, false};
            transposition_table_record(&game->quiescent_table, &rec);
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
                transposition_t rec = {hash, move, score, (int16_t)depth, (uint8_t)TRANSPOSITION_CUT, false};
                transposition_table_record(&game->quiescent_table, &rec);
            }
        }
    }
    return best_score;
}
