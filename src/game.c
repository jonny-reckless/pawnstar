/// @file game_t state management.

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "debug_hashtable.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "search_state.h"
#include "transposition_table.h"

/// @brief Centipawn values for each piece type indexed by piece_t (NONE through KING).
static const int PIECE_VALUES[7] = {0, 100, 300, 300, 500, 900, 10000};

// ---------------------------------------------------------------------------
// Worker thread trampoline
// ---------------------------------------------------------------------------

static void search_worker(game_t *game);

static int search_thread_entry(void *arg)
{
    search_worker((game_t *)arg);
    return 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void game_init(game_t *self)
{
    transposition_table_init(&self->transposition_table, (size_t)HASHTABLE_MEGABYTES);
    history_table_init(&self->history_table);
    opening_book_init(&self->book);
    self->worker_running = false;
    int num_cpus         = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus < 1)
    {
        num_cpus = 1;
    }
    thread_pool_init(&self->thread_pool, num_cpus);
    slice_allocator_init(&self->ss_pool, sizeof(search_state_t), NUM_ALLOCATOR_SLICES);
    game_new_game_default(self);
}

void game_free(game_t *self)
{
    game_stop_thinking(self);
    transposition_table_free(&self->transposition_table);
    history_table_free(&self->history_table);
    opening_book_free(&self->book);
    thread_pool_destroy(&self->thread_pool);
    slice_allocator_destroy(&self->ss_pool);
}

void game_new_game(game_t *self, const char *fen_string)
{
    chess_clock_init(&self->time_control);
    self->node_count = 0;
    atomic_store(&self->is_cancel_pending, false);
    self->position     = &self->positions[0];
    self->positions[0] = position_from_string(fen_string);
}

void game_new_game_default(game_t *self)
{
    game_new_game(self, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void game_play_move(game_t *self, move_t move)
{
    position_make_move(&self->position[1], &self->position[0], move);
    ++self->position;
}

void game_make_null_move(game_t *self)
{
    position_make_null_move(&self->position[1], &self->position[0]);
    ++self->position;
}

void game_undo_move(game_t *self)
{
    --self->position;
}

void game_score_and_sort_moves(game_t *self, move_list_t *moves, int ply)
{
    for (int i = 0; i < moves->size; ++i)
    {
        move_t *m     = &moves->items[i];
        int     score = PIECE_VALUES[(int)move_captured(*m)] * 1000 - PIECE_VALUES[(int)move_piece(*m)] * 100 +
                        (int)history_table_get_count(&self->history_table, ply, *m);
        move_assign_score(m, score);
    }
    move_list_sort(moves);
}

bool game_is_draw_by_fifty_moves(const game_t *self)
{
    if (self->position->half_move_clock >= 100)
    {
        INCREMENT("draws by 50 moves");
        return true;
    }
    return false;
}

bool game_is_draw_by_repetition(const game_t *self)
{
    int             repetitions = 2;
    const zobrist_t hash        = self->position->hash;
    for (const position_t *p = self->position - 2; p >= self->positions; p -= 2)
    {
        if (p->hash == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition");
            return true;
        }
        if (p->half_move_clock == 0)
        {
            return false;
        }
    }
    return false;
}

move_t game_play_move_from_string(game_t *self, const char *move_str)
{
    move_list_t move_list = position_generate_legal_moves(self->position);
    char        buf[8];
    for (int i = 0; i < move_list.size; ++i)
    {
        move_to_string(move_list.items[i], buf, sizeof(buf));
        if (strcmp(buf, move_str) == 0)
        {
            game_play_move(self, move_list.items[i]);
            return move_list.items[i];
        }
    }
    return move_none();
}

bool game_is_game_over(game_t *self)
{
    const position_t *pos = self->position;
    if (position_is_checkmate(pos))
    {
        printf("%s\n", position_color_to_move(pos) == BLACK ? "1-0 {white mates}" : "0-1 {black mates}");
        return true;
    }
    if (position_is_stalemate(pos))
    {
        printf("1/2-1/2 {stalemate}\n");
        return true;
    }
    if (game_is_draw_by_repetition(self))
    {
        printf("1/2-1/2 {draw by repetition}\n");
        return true;
    }
    if (position_is_draw_by_material(pos))
    {
        printf("1/2-1/2 {draw by insufficient material}\n");
        return true;
    }
    if (game_is_draw_by_fifty_moves(self))
    {
        printf("1/2-1/2 {draw by 50 reversible moves}\n");
        return true;
    }
    return false;
}

static void search_worker(game_t *game)
{
    move_t move = search_root_node(game);
    if (!move_equals(move, move_none()))
    {
        game_play_move(game, move);
        char buf[8];
        move_to_string(move, buf, sizeof(buf));
        printf("bestmove %s\n", buf);
        fflush(stdout);
        game_is_game_over(game);
    }
}

void game_stop_thinking(game_t *self)
{
    atomic_store(&self->is_cancel_pending, true);
    if (self->worker_running)
    {
        thrd_join(self->worker_thread, NULL);
        self->worker_running = false;
    }
}

void game_start_thinking(game_t *self)
{
    game_stop_thinking(self);
    atomic_store(&self->is_cancel_pending, false);
    self->worker_running = true;
    if (thrd_create(&self->worker_thread, search_thread_entry, self) != thrd_success)
    {
        self->worker_running = false;
    }
}
