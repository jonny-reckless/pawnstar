/// @file Per-thread search state constructors and repetition detection.

#include <string.h>
#include <threads.h>

#include "debug_hashtable.h"
#include "search_state.h"
#include "slice_allocator.h"

#define SS_POOL_CAPACITY 1000

static slice_allocator_t ss_pool;
static once_flag         ss_pool_once = ONCE_FLAG_INIT;

static void init_ss_pool(void)
{
    slice_allocator_init(&ss_pool, sizeof(search_state_t), SS_POOL_CAPACITY);
}

search_state_t *search_state_from_game(game_t *game)
{
    call_once(&ss_pool_once, init_ss_pool);
    search_state_t *ss = slice_allocator_alloc(&ss_pool);
    int n = (int)(game->position - game->positions) + 1;
    for (int i = 0; i < n; ++i)
    {
        ss->hash_stack[i] = (hash_stack_entry_t){game->positions[i].hash, game->positions[i].half_move_clock};
    }
    ss->hash_len     = n;
    ss->pos_stack[0] = *game->position;
    ss->pos_len      = 1;
    ss->node_count   = 0;
    ss->game         = game;
    ss->was_cutoff   = NULL;
    return ss;
}

search_state_t *search_state_worker(const search_state_t *parent, atomic_bool *cutoff)
{
    search_state_t *ss = slice_allocator_alloc(&ss_pool);
    ss->hash_len = parent->hash_len;
    memcpy(ss->hash_stack, parent->hash_stack, (size_t)parent->hash_len * sizeof(hash_stack_entry_t));
    ss->pos_stack[0] = parent->pos_stack[parent->pos_len - 1];
    ss->pos_len      = 1;
    ss->node_count   = 0;
    ss->game         = parent->game;
    ss->was_cutoff   = cutoff;
    return ss;
}

void search_state_free(search_state_t *ss)
{
    slice_allocator_free(&ss_pool, ss);
}

bool ss_is_draw_by_repetition(const search_state_t *ss)
{
    int n = ss->hash_len;
    if (n < 5)
    {
        return false;
    }
    const zobrist_t h           = ss->hash_stack[n - 1].hash;
    int             repetitions = 2;
    for (int i = n - 5; i >= 0; i -= 2)
    {
        if (ss->hash_stack[i].hash == h)
        {
            if (--repetitions == 0)
            {
                INCREMENT("draws by repetition");
                return true;
            }
        }
        if (ss->hash_stack[i].half_move_clock == 0)
        {
            return false;
        }
    }
    return false;
}
