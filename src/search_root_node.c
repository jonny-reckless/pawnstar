/// @file Iterative-deepening root search with Lazy SMP helper threads.

#include <stdio.h>
#include <string.h>
#include <threads.h>

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "search.h"
#include "search_state.h"
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

// ---------------------------------------------------------------------------
// Lazy SMP helper thread (root search)
// ---------------------------------------------------------------------------

/// @brief Entry point for standard Lazy SMP helper threads.
/// Runs iterative deepening from START_DEPTH until the search is cancelled.
/// Shares the transposition table with the main thread; contributes TT entries
/// that improve the main thread's move ordering and pruning.
static int lazy_helper_fn(void *arg)
{
    game_t         *game = arg;
    search_state_t *ss   = search_state_from_game(game);
    variation_t     pv;
    for (int depth = START_DEPTH; !ss_is_cancelled(ss); ++depth)
    {
        variation_clear(&pv);
        search(ss, depth, 0, ALPHA, BETA, &pv);
    }
    search_state_free(ss);
    return 0;
}

// ---------------------------------------------------------------------------
// PV-leaf helper thread
// ---------------------------------------------------------------------------

/// @brief Shared state between the main thread and the PV-leaf helper.
/// The main thread writes @c pv then bumps @c generation with memory_order_release
/// after each completed depth.  The helper reads @c generation with
/// memory_order_acquire, then snapshots @c pv.
typedef struct
{
    game_t     *game;
    atomic_int  generation; ///< 0 = no PV yet; incremented after each depth.
    variation_t pv;         ///< Current best PV; written before bumping generation.
} pv_helper_arg_t;

/// @brief Entry point for the PV-leaf helper thread.
/// Waits for the main thread to publish a PV, plays out all PV moves on a fresh
/// search_state_t to advance to the leaf position, then runs iterative deepening
/// from there.  When a newer PV is published the current search is abandoned and
/// the helper restarts from the new leaf.  Exits when is_cancel_pending is set.
static int lazy_pv_helper_fn(void *arg)
{
    pv_helper_arg_t *pv_arg  = arg;
    game_t          *game    = pv_arg->game;
    search_state_t  *ss      = NULL;
    int              last_gen = 0;

    while (!atomic_load_explicit(&game->is_cancel_pending, memory_order_relaxed))
    {
        int gen = atomic_load_explicit(&pv_arg->generation, memory_order_acquire);
        if (gen == last_gen)
        {
            thrd_yield();
            continue;
        }
        last_gen = gen;

        // Snapshot the PV published by the main thread.
        variation_t pv;
        memcpy(&pv, &pv_arg->pv, sizeof(variation_t));

        // Re-initialise search state from the game root.
        if (ss != NULL)
        {
            search_state_free(ss);
        }
        ss = search_state_from_game(game);

        // Advance to the PV leaf by playing out every move in the PV.
        int pv_ply = 0;
        for (int i = 0; i < pv.size && !ss_is_cancelled(ss); ++i)
        {
            ss_play_move(ss, pv.items[i]);
            ++pv_ply;
        }

        // Search from the leaf at increasing depths.
        // pv_ply is passed as the ply argument so that mate-distance scores are
        // measured from the game root rather than the leaf.
        for (int depth = 1; !ss_is_cancelled(ss); ++depth)
        {
            if (atomic_load_explicit(&pv_arg->generation, memory_order_relaxed) != last_gen)
            {
                break; // newer PV available — restart from the updated leaf
            }
            variation_t child_pv;
            variation_clear(&child_pv);
            search(ss, depth, pv_ply, ALPHA, BETA, &child_pv);
        }
    }

    if (ss != NULL)
    {
        search_state_free(ss);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Root search
// ---------------------------------------------------------------------------

#define MAX_HELPERS 64

move_t search_root_node(game_t *game)
{
    // Check opening book first.
    move_t book_move = opening_book_get_move(&game->book, game->position->hash);
    if (!move_equals(book_move, move_none()))
    {
        return book_move;
    }

    move_list_t move_list = position_generate_legal_moves(game->position);
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
            const int num_moves_to_go = max_int(40 - (int)(game->position->move_counter), 5);
            ms_allocated              = game->time_control.ms_remaining / num_moves_to_go;
        }
        ms_timeout = max_int(100, min_int(ms_allocated * 2, game->time_control.ms_remaining - 1000));
        game->time_control.hard_stop_ms = chess_clock_elapsed_milliseconds(&game->time_control) + ms_timeout;
        break;
    case CLOCK_FIXED_DEPTH:
        game->time_control.hard_stop_ms = 0;
        break;
    case CLOCK_FIXED_TIME:
        ms_timeout                      = game->time_control.ms_remaining;
        game->time_control.hard_stop_ms = chess_clock_elapsed_milliseconds(&game->time_control) + ms_timeout;
        break;
    case CLOCK_INFINITE:
        game->time_control.hard_stop_ms = 0;
        break;
    }

    int start_ms = (int)chess_clock_elapsed_milliseconds(&game->time_control);
    atomic_store(&game->is_cancel_pending, false);
    debug_x_clear();
    history_table_reset(&game->history_table);
    transposition_table_age(&game->transposition_table);

    // Allocate one helper slot for PV-leaf searching; the rest do root-level Lazy SMP.
    int n_helpers = game->n_helpers < MAX_HELPERS ? game->n_helpers : MAX_HELPERS;

    pv_helper_arg_t pv_arg;
    pv_arg.game = game;
    atomic_init(&pv_arg.generation, 0);
    variation_clear(&pv_arg.pv);

    thrd_t pv_helper;
    int    n_pv_helpers = 0;
    if (n_helpers > 0)
    {
        thrd_create(&pv_helper, lazy_pv_helper_fn, &pv_arg);
        n_pv_helpers = 1;
        n_helpers--;
    }

    thrd_t helpers[MAX_HELPERS];
    for (int i = 0; i < n_helpers; ++i)
    {
        thrd_create(&helpers[i], lazy_helper_fn, game);
    }

    search_state_t *ss = search_state_from_game(game);

    variation_t principal_variation;
    variation_clear(&principal_variation);

    move_t best_moves[MAX_PLY];
    int    best_moves_n = 0;

    // Initial shallow pass to score moves for ordering.
    for (int i = 0; i < move_list.size; ++i)
    {
        variation_t pv;
        variation_clear(&pv);
        int score = search_single_move(ss, START_DEPTH, 0, ALPHA, BETA, move_list.items[i], &pv, i);
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
        int alpha      = ALPHA;
        ss->node_count = 0;
        if (best_moves_n < MAX_PLY)
        {
            best_moves[best_moves_n++] = best_move;
        }

        for (int i = 0; i < move_list.size; ++i)
        {
            variation_t pv;
            variation_clear(&pv);
            int score = search_single_move(ss, depth, 0, alpha, BETA, move_list.items[i], &pv, i);
            move_assign_score(&move_list.items[i], score);
            if (atomic_load_explicit(&game->is_cancel_pending, memory_order_relaxed))
            {
                goto done;
            }
            if (score > alpha)
            {
                alpha     = score;
                best_move = move_list.items[i];
                variation_copy(&principal_variation, &pv, best_move);

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
                       (long long)(chess_clock_elapsed_milliseconds(&game->time_control) - start_ms), ss->node_count,
                       pv_buf);
                fflush(stdout);
            }
        }

        // Publish the completed-depth PV to the PV-leaf helper thread.
        if (n_pv_helpers > 0 && principal_variation.size > 0)
        {
            memcpy(&pv_arg.pv, &principal_variation, sizeof(variation_t));
            atomic_fetch_add_explicit(&pv_arg.generation, 1, memory_order_release);
        }

        int stop_ms = (int)chess_clock_elapsed_milliseconds(&game->time_control);
        if (alpha > WIN_THRESHOLD || alpha < LOSE_THRESHOLD)
        {
            break;
        }

        if (game->time_control.clock_type == CLOCK_STANDARD)
        {
            const int elapsed_ms = stop_ms - start_ms;

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

done:
    // Signal helpers to stop and wait for them to exit.
    atomic_store(&game->is_cancel_pending, true);
    if (n_pv_helpers > 0)
    {
        thrd_join(pv_helper, NULL);
    }
    for (int i = 0; i < n_helpers; ++i)
    {
        thrd_join(helpers[i], NULL);
    }

    search_state_free(ss);
    return best_move;
}
