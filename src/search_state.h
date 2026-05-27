#pragma once
/// @file Per-thread search state: position stack, hash stack, and cancellation check.

#include <stdatomic.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "position.h"

#define POS_STACK_MAX  (MAX_PLY + 4)
#define HASH_STACK_MAX (POSITION_STACK_SIZE + MAX_PLY + 4)

/// @brief Compact position record used exclusively for repetition detection.
/// 16 bytes vs 160 bytes for a full position_t — makes thread state creation cheap.
typedef struct
{
    zobrist_t hash;            ///< Zobrist hash of the position.
    uint8_t   half_move_clock; ///< Half-move clock at this position.
} hash_stack_entry_t;

/// @brief Per-thread search state.  The main search thread and each Lazy SMP helper own one.
///
/// pos_stack starts with exactly one entry (the position at thread creation) and grows by one for
/// each move played.  Undo is a free decrement.
///
/// hash_stack holds a compact (hash, half_move_clock) record for every position from game start
/// through the current search node, used exclusively by ss_is_draw_by_repetition.
///
/// game (transposition table, history, time control, cancel flag) is shared via pointer.
typedef struct search_state
{
    position_t         pos_stack[POS_STACK_MAX];   ///< Copy-make stack: 1 entry at creation, grows during search.
    int                pos_len;                    ///< Number of valid entries in pos_stack.
    hash_stack_entry_t hash_stack[HASH_STACK_MAX]; ///< Compact history for repetition detection.
    int                hash_len;                   ///< Number of valid entries in hash_stack.
    int                node_count;                 ///< Nodes counted by this thread.
    game_t            *game;                       ///< Shared: TT, history, time control, cancel flag.
} search_state_t;

/// @brief Heap-allocate and initialise a search_state for one thread (main or helper).
/// pos_stack is seeded with game's current position; hash_stack is built from the full game history.
search_state_t *search_state_from_game(game_t *game);

/// @brief Free a search_state allocated by search_state_from_game.
void search_state_free(search_state_t *ss);

/// @brief True if the 50-move rule draw condition is met.
static inline bool ss_is_draw_by_fifty_moves(const search_state_t *ss)
{
    return ss->pos_stack[ss->pos_len - 1].half_move_clock >= 100;
}

/// @brief True if the current position has occurred twice before (three occurrences total).
bool ss_is_draw_by_repetition(const search_state_t *ss);

/// @brief Return a pointer to the current (deepest) position on the stack.
static inline position_t *ss_current_position(search_state_t *ss)
{
    return &ss->pos_stack[ss->pos_len - 1];
}

/// @brief Apply move: copy current position to next slot, make the move, record in hash_stack.
static inline void ss_play_move(search_state_t *ss, move_t move)
{
    position_make_move(&ss->pos_stack[ss->pos_len], &ss->pos_stack[ss->pos_len - 1], move);
    const position_t *p          = &ss->pos_stack[ss->pos_len];
    ss->hash_stack[ss->hash_len] = (hash_stack_entry_t){p->hash, p->half_move_clock};
    ++ss->pos_len;
    ++ss->hash_len;
}

/// @brief Apply a null move: copy current position to next slot, pass the turn, record in hash_stack.
static inline void ss_make_null_move(search_state_t *ss)
{
    position_make_null_move(&ss->pos_stack[ss->pos_len], &ss->pos_stack[ss->pos_len - 1]);
    const position_t *p          = &ss->pos_stack[ss->pos_len];
    ss->hash_stack[ss->hash_len] = (hash_stack_entry_t){p->hash, p->half_move_clock};
    ++ss->pos_len;
    ++ss->hash_len;
}

/// @brief Undo the last move or null move.
static inline void ss_undo_move(search_state_t *ss)
{
    --ss->pos_len;
    --ss->hash_len;
}

/// @brief True when the search should stop: time expired or UCI stop received.
static inline bool ss_is_cancelled(const search_state_t *ss)
{
    return atomic_load_explicit(&ss->game->is_cancel_pending, memory_order_relaxed);
}
