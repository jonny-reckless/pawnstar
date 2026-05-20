/// @file Functions to search for best move.

#include <stdio.h>

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "search.h"
#include "transposition_table.h"

static inline int max_int(int a, int b)
{
    return a > b ? a : b;
}
static inline int min_int(int a, int b)
{
    return a < b ? a : b;
}
static inline int abs_int(int a)
{
    return a < 0 ? -a : a;
}

/// @brief search the root node and find the best move.
/// @param game The game to search.
/// @return The best move found.
move_t search_root_node(game_t *game)
{
    // Check opening book first.
    move_t book_move = opening_book_get_move(&game->book, position_hash(game_current_position(game)));
    if (!move_equals(book_move, move_none()))
    {
        return book_move;
    }

    move_list_t move_list = position_generate_legal_moves(game_current_position(game));
    if (move_list.size == 0)
    {
        return move_none();
    }
    if (move_list.size == 1)
    {
        return move_list.items[0];
    }

    // Plan time usage.
    int ms_timeout   = 0;
    int ms_allocated = 0;
    switch (game->time_control.clock_type)
    {
    case CLOCK_STANDARD:
    default:
        if (game->time_control.num_moves_remaining != 0)
        {
            ms_allocated = game->time_control.ms_remaining / game->time_control.num_moves_remaining;
        }
        else
        {
            const int num_moves_to_go = max_int(40 - (int)position_move_count(game_current_position(game)), 5);
            ms_allocated              = game->time_control.ms_remaining / num_moves_to_go;
        }
        ms_timeout = max_int(100, min_int(ms_allocated * 2, game->time_control.ms_remaining - 1000));
        game->time_control.hard_stop_ms = chess_clock_elapsed_milliseconds(&game->time_control) + ms_timeout;
        break;

    case CLOCK_FIXED_DEPTH:
        ms_timeout                      = 0;
        ms_allocated                    = 0;
        game->time_control.hard_stop_ms = 0;
        break;

    case CLOCK_FIXED_TIME:
        ms_timeout                      = game->time_control.ms_remaining;
        game->time_control.hard_stop_ms = chess_clock_elapsed_milliseconds(&game->time_control) + ms_timeout;
        ms_allocated                    = 0;
        break;

    case CLOCK_INFINITE:
        game->time_control.hard_stop_ms = 0;
        break;
    }

    int start_ms            = (int)chess_clock_elapsed_milliseconds(&game->time_control);
    game->is_cancel_pending = false;
    debug_x_clear();
    history_table_reset(&game->history_table);
    transposition_table_age(&game->transposition_table);

    variation_list_t principal_variation;
    variation_list_clear(&principal_variation);

    // Collect best moves across iterations for stability checks.
    move_t best_moves[MAX_PLY];
    int    best_moves_n = 0;

    // Initial shallow search to score moves for ordering.
    for (int i = 0; i < move_list.size; ++i)
    {
        variation_list_t pv;
        variation_list_clear(&pv);
        int score = search_single_move(game, START_DEPTH, 0, ALPHA, BETA, move_list.items[i], &pv, i).score;
        move_assign_score(&move_list.items[i], score);
    }
    move_list_stable_sort(&move_list);
    move_t best_move = move_list.items[0];

    for (int depth = START_DEPTH + 1; depth < MAX_PLY; ++depth)
    {
        if (game->time_control.clock_type == CLOCK_FIXED_DEPTH && depth > game->time_control.depth)
        {
            break;
        }

        move_list_stable_sort(&move_list);
        variation_list_t child_pv;
        variation_list_clear(&child_pv);
        int alpha        = ALPHA;
        game->node_count = 0;
        if (best_moves_n < MAX_PLY)
        {
            best_moves[best_moves_n++] = best_move;
        }

        for (int i = 0; i < move_list.size; ++i)
        {
            variation_list_t pv;
            variation_list_clear(&pv);
            int score = search_single_move(game, depth, 0, alpha, BETA, move_list.items[i], &pv, i).score;
            move_assign_score(&move_list.items[i], score);
            if (game->is_cancel_pending)
            {
                return best_move;
            }
            if (score > alpha)
            {
                alpha     = score;
                best_move = move_list.items[i];
                copy_variation(&principal_variation, &pv, best_move);

                // Build PV string and print info.
                char pv_buf[1024];
                int  pv_len = 0;
                char mv_buf[8];
                for (int j = 0; j < principal_variation.size; ++j)
                {
                    move_to_string(principal_variation.items[j], mv_buf, sizeof(mv_buf));
                    int written =
                        snprintf(pv_buf + pv_len, sizeof(pv_buf) - (size_t)pv_len, j == 0 ? "%s" : " %s", mv_buf);
                    if (written > 0)
                    {
                        pv_len += written;
                    }
                }
                printf("info depth %2d score cp %5d time %5lld nodes %8d pv %s\n", depth, alpha,
                       (long long)(chess_clock_elapsed_milliseconds(&game->time_control) - start_ms), game->node_count,
                       pv_buf);
                fflush(stdout);
            }
        }

        int stop_ms = (int)chess_clock_elapsed_milliseconds(&game->time_control);
        if (alpha > WIN_THRESHOLD || alpha < LOSE_THRESHOLD)
        {
            break;
        }

        if (game->time_control.clock_type == CLOCK_STANDARD)
        {
            const int elapsed_ms = stop_ms - start_ms;

            // Check if best move has been consistent across all past iterations.
            bool is_best_move_consistent = true;
            bool is_score_stable         = true;
            for (int k = 0; k < best_moves_n; ++k)
            {
                if (!move_equals(best_moves[k], best_move))
                {
                    is_best_move_consistent = false;
                }
                if (abs_int(move_score(best_moves[k]) - move_score(best_move)) > SCORE_INSTABILITY_THRESHOLD)
                {
                    is_score_stable = false;
                }
            }

            if (is_best_move_consistent && is_score_stable && (elapsed_ms * 4) > ms_allocated)
            {
                printf("info string Best move consistent AND score stable - search terminated.\n");
                fflush(stdout);
                break;
            }
            if ((is_best_move_consistent || is_score_stable) && (elapsed_ms * 2) > ms_allocated)
            {
                printf("info string %s - search terminated.\n",
                       is_best_move_consistent ? "Best move consistent" : "Score stable");
                fflush(stdout);
                break;
            }
            if (elapsed_ms > ms_allocated)
            {
                break;
            }
        }
    }
    return best_move;
}
