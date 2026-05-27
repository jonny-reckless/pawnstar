/// @file Per-thread search state constructors and repetition detection.

#include <stdlib.h>
#include <string.h>

#include "debug_hashtable.h"
#include "search_state.h"

search_state_t *search_state_from_game(game_t *game)
{
    search_state_t *ss = malloc(sizeof(search_state_t));
    int             n  = (int)(game->position - game->positions) + 1;
    for (int i = 0; i < n; ++i)
    {
        ss->hash_stack[i] = (hash_stack_entry_t){game->positions[i].hash, game->positions[i].half_move_clock};
    }
    ss->hash_len     = n;
    ss->pos_stack[0] = *game->position;
    ss->pos_len      = 1;
    ss->node_count   = 0;
    ss->game         = game;
    return ss;
}

void search_state_free(search_state_t *ss)
{
    free(ss);
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
