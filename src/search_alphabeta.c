/// @file Alpha-beta search.

#include <assert.h>
#include <stdatomic.h>

#include "chess_clock.h"
#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "search_state.h"
#include "transposition_table.h"

// ---------------------------------------------------------------------------
// Single-move search
// ---------------------------------------------------------------------------

int search_single_move(search_state_t *ss, int depth, int ply, int alpha, int beta, move_t move, variation_t *pv,
                       int move_index)
{
    const position_t *position     = ss_current_position(ss);
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
    default:
        break;
    }

    ss_play_move(ss, move);
    const bool is_checking = position_is_in_check(ss_current_position(ss));
    int        score;
    if (beta > alpha + 1 && move_index > 0 && !was_in_check && !is_checking)
    {
        INCREMENT("pvs attempts");
        score = -search(ss, child_depth, ply + 1, -alpha - 1, -alpha, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -search(ss, child_depth, ply + 1, -beta, -alpha, pv);
        }
    }
    else
    {
        score = -search(ss, child_depth, ply + 1, -beta, -alpha, pv);
    }
    ss_undo_move(ss);
    return score;
}

// ---------------------------------------------------------------------------
// Null-move pruning helper
// ---------------------------------------------------------------------------

// Attempt a null move and return the resulting score (or alpha if conditions aren't met).
// Guards: null-window only, not already a null-move, not in check, not in endgame, and the
// side to move has a material+PST advantage of at least beta+100.  Reduction R=4.
// Returns SEARCH_CANCELLED_SCORE if the search is interrupted during the null search.
static inline int attempt_null_move(search_state_t *ss, int depth, int ply, int alpha, int beta)
{
    const position_t *position = ss_current_position(ss);
    const color_t     color    = position_color_to_move(position);
    if (beta == alpha + 1 && !position_is_null_move(position) && !position_is_in_check(position) &&
        !is_in_endgame(position))
    {
        const int pst_score = color == WHITE ? position->scores[WHITE] - position->scores[BLACK]
                                             : position->scores[BLACK] - position->scores[WHITE];
        if (pst_score >= beta + 100)
        {
            INCREMENT("null move");
            variation_t dummy;
            variation_clear(&dummy);
            ss_make_null_move(ss);
            int score = -search(ss, depth - 4, ply + 1, -beta, -alpha, &dummy);
            ss_undo_move(ss);
            if (ss_is_cancelled(ss))
            {
                return SEARCH_CANCELLED_SCORE;
            }
            if (score >= beta)
            {
                return beta;
            }
            INCREMENT("null move fails");
        }
    }
    return alpha;
}

// ---------------------------------------------------------------------------
// Main alpha-beta search
// ---------------------------------------------------------------------------

// Node flow:
//  1. Time check every 64 K nodes; draw detection; check extension.
//  2. Transposition table lookup: CUT node → prune if score≥beta; ALL node → prune if score<alpha;
//     PV node → extend depth by 1.
//  3. Depth ≤ 0: fall through to quiescence search.
//  4. Null-move pruning (attempt_null_move).
//  5. TT move searched first (serially, before generating the full move list).
//  6. Full move list generated and sorted; moves searched with LMR where eligible.
//  7. Store result in TT (CUT / ALL / PV node type).
int search(search_state_t *ss, int depth, int ply, int alpha, int beta, variation_t *parent_pv)
{
    INCREMENT("alpha beta calls");
    if ((++ss->node_count & 0xFFFF) == 0 && ss->game->time_control.hard_stop_ms != 0 &&
        chess_clock_elapsed_milliseconds(&ss->game->time_control) >= ss->game->time_control.hard_stop_ms)
    {
        atomic_store(&ss->game->is_cancel_pending, true);
        return SEARCH_CANCELLED_SCORE;
    }

    const position_t *position = ss_current_position(ss);
    if (position_is_draw_by_material(position) || ss_is_draw_by_fifty_moves(ss) || ss_is_draw_by_repetition(ss))
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
        return evaluate_position(position, alpha, beta);
    }

    // Transposition table lookup.
    transposition_t transposition;
    bool has_transposition = transposition_table_find(&ss->game->transposition_table, position->hash, &transposition);
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
        return search_quiescent(ss, depth, ply, alpha, beta);
    }

    int nm_score = attempt_null_move(ss, depth, ply, alpha, beta);
    if (ss_is_cancelled(ss))
    {
        return SEARCH_CANCELLED_SCORE;
    }
    if (nm_score >= beta)
    {
        return nm_score;
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
        int score = search_single_move(ss, depth, ply, alpha, beta, transposition.move, &pv, 0);
        if (ss_is_cancelled(ss))
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            transposition_t rec = {position->hash, transposition.move,         score,
                                   (int16_t)depth, (uint8_t)TRANSPOSITION_CUT, false};
            transposition_table_record(&ss->game->transposition_table, &rec);
            history_table_record_good_move(&ss->game->history_table, ply, transposition.move);
            return score;
        }
        best_score = score;
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha            = score;
            has_raised_alpha = true;
            history_table_record_good_move(&ss->game->history_table, ply, transposition.move);
        }
    }

    move_list_t move_list = position_generate_legal_moves(position);
    if (move_list.size == 0)
    {
        if (position_is_in_check(position))
        {
            INCREMENT("checkmates");
            return CHECKMATED_SCORE + ply;
        }
        INCREMENT("stalemates");
        return DRAW_SCORE;
    }

    game_score_and_sort_moves(ss->game, &move_list, ply);

    for (int i = 0; i < move_list.size; ++i)
    {
        move_t move = move_list.items[i];
        if (has_transposition && move_equals(move, transposition.move))
        {
            continue;
        }

        int lmr_depth = depth;
        if (i > 3 && !position_is_in_check(position) && depth > 2 && beta == alpha + 1 && move_captured(move) == NONE &&
            move_piece(move) != PAWN)
        {
            INCREMENT("late move reduction 1");
            --lmr_depth;
            if (i > 6)
            {
                INCREMENT("late move reduction 2");
                --lmr_depth;
                if (history_table_get_count(&ss->game->history_table, ply, move) == 0)
                {
                    INCREMENT("late move reduction 3");
                    --lmr_depth;
                }
            }
        }

        int score = search_single_move(ss, lmr_depth, ply, alpha, beta, move, &pv, i);
        if (score > alpha && lmr_depth < depth)
        {
            INCREMENT("late move reduction fails");
            score = search_single_move(ss, depth, ply, alpha, beta, move, &pv, i);
        }
        if (ss_is_cancelled(ss))
        {
            return SEARCH_CANCELLED_SCORE;
        }

        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            transposition_t rec = {position->hash, move, score, (int16_t)depth, (uint8_t)TRANSPOSITION_CUT, false};
            transposition_table_record(&ss->game->transposition_table, &rec);
            history_table_record_good_move(&ss->game->history_table, ply, move);
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
                history_table_record_good_move(&ss->game->history_table, ply, move);
                variation_copy(parent_pv, &pv, move);
            }
        }
    }

    if (has_raised_alpha)
    {
        INCREMENT("pv nodes");
        transposition_t rec = {position->hash, best_move, alpha, (int16_t)depth, (uint8_t)TRANSPOSITION_PV, false};
        transposition_table_record(&ss->game->transposition_table, &rec);
        history_table_record_good_move(&ss->game->history_table, ply, best_move);
        variation_copy(parent_pv, &pv, best_move);
    }
    else
    {
        INCREMENT("all nodes");
        transposition_t rec = {position->hash, best_move, best_score, (int16_t)depth, (uint8_t)TRANSPOSITION_ALL,
                               false};
        transposition_table_record(&ss->game->transposition_table, &rec);
    }
    return best_score;
}
