/// @file Persistent thread pool for parallel move search.

#include <stdlib.h>

#include "constants.h"
#include "debug_hashtable.h"
#include "move.h"
#include "search.h"
#include "search_state.h"
#include "thread_pool.h"

static int pool_thread_fn(void *arg)
{
    thread_pool_slot_t *slot = arg;
    while (true)
    {
        sem_wait(&slot->work_ready);
        if (atomic_load_explicit(&slot->shutdown, memory_order_relaxed))
            break;

        pool_work_t *w = &slot->work;
        int          score;
        if (ss_is_cancelled(w->ss))
        {
            score = SEARCH_CANCELLED_SCORE;
        }
        else
        {
            variation_t pv;
            variation_clear(&pv);
            score = search_single_move(w->ss, w->depth, w->ply, w->alpha, w->beta, w->move, &pv, 0);
            if (score >= w->beta)
            {
                INCREMENT("parallel cutoffs");
                atomic_store(w->cutoff, true);
            }
        }
        w->result_score = score;
        search_state_free(w->ss);
        sem_post(&slot->work_done);
    }
    return 0;
}

void thread_pool_init(thread_pool_t *self, int n_threads)
{
    self->n_slots    = n_threads;
    self->slots      = malloc((size_t)n_threads * sizeof(thread_pool_slot_t));
    self->free_stack = malloc((size_t)n_threads * sizeof(int));
    self->free_count = n_threads;
    mtx_init(&self->lock, mtx_plain);
    sem_init(&self->available, 0, (unsigned)n_threads);

    for (int i = 0; i < n_threads; ++i)
    {
        self->free_stack[i] = i;
        sem_init(&self->slots[i].work_ready, 0, 0);
        sem_init(&self->slots[i].work_done, 0, 0);
        atomic_init(&self->slots[i].shutdown, false);
        thrd_create(&self->slots[i].thread, pool_thread_fn, &self->slots[i]);
    }
}

void thread_pool_destroy(thread_pool_t *self)
{
    for (int i = 0; i < self->n_slots; ++i)
    {
        atomic_store(&self->slots[i].shutdown, true);
        sem_post(&self->slots[i].work_ready);
    }
    for (int i = 0; i < self->n_slots; ++i)
    {
        thrd_join(self->slots[i].thread, NULL);
        sem_destroy(&self->slots[i].work_ready);
        sem_destroy(&self->slots[i].work_done);
    }
    sem_destroy(&self->available);
    mtx_destroy(&self->lock);
    free(self->slots);
    free(self->free_stack);
}

int thread_pool_try_acquire(thread_pool_t *self)
{
    if (sem_trywait(&self->available) != 0)
        return -1;
    mtx_lock(&self->lock);
    int idx = self->free_stack[--self->free_count];
    mtx_unlock(&self->lock);
    return idx;
}

void thread_pool_dispatch(thread_pool_t *self, int slot_idx, pool_work_t work)
{
    self->slots[slot_idx].work = work;
    sem_post(&self->slots[slot_idx].work_ready);
}

int thread_pool_collect(thread_pool_t *self, int slot_idx)
{
    sem_wait(&self->slots[slot_idx].work_done);
    return self->slots[slot_idx].work.result_score;
}

void thread_pool_release(thread_pool_t *self, int slot_idx)
{
    mtx_lock(&self->lock);
    self->free_stack[self->free_count++] = slot_idx;
    mtx_unlock(&self->lock);
    sem_post(&self->available);
}
