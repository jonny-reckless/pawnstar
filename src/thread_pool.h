#pragma once
/// @file Persistent thread pool for parallel move search.
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

#include "move.h"

/// Forward declaration — thread_pool.h must not include search_state.h (circular).
typedef struct search_state search_state_t;

/// @brief Work item for one parallel move search.
/// Filled by the dispatcher before thread_pool_dispatch(); read by the pool thread.
/// result_score is written by the thread before it posts work_done.
typedef struct
{
    search_state_t *ss;           ///< Worker-owned search state; freed by the pool thread after search.
    move_t          move;         ///< Move to search.
    int             depth;        ///< Remaining depth (same as the parent's depth).
    int             ply;          ///< Ply from the root (same as the parent's ply).
    int             alpha;        ///< Alpha at the time of dispatch (parent's current alpha).
    int             beta;         ///< Beta at the time of dispatch (parent's beta).
    int             result_score; ///< Written by the pool thread before signalling done.
    atomic_bool    *was_cutoff;   ///< Per-batch flag; the thread stores true when score ≥ beta.
} pool_work_t;

/// @brief Function executed by a pool thread to process one work item.
/// Returns the result score, which the pool thread stores in work->result_score.
typedef int (*pool_fn_t)(pool_work_t *work);

/// @brief One slot in the pool: a persistent thread with its own work/done semaphores.
/// The thread loops: wait on work_ready → call fn(work) → post work_done.
typedef struct
{
    pool_work_t work;       ///< work function args.
    pool_fn_t   fn;         ///< Function to call; set by thread_pool_dispatch.
    sem_t       work_ready; ///< Dispatcher posts to wake the thread.
    sem_t       work_done;  ///< Thread posts when result_score is ready.
    atomic_bool shutdown;   ///< Set by thread_pool_destroy(); checked after work_ready fires.
    thrd_t      thread;
} thread_pool_slot_t;

/// @brief Fixed-size pool of persistent worker threads.
typedef struct
{
    thread_pool_slot_t *slots;
    int                 n_slots;
    int                *free_stack; ///< Indices of currently idle slots.
    int                 free_count;
    mtx_t               lock;      ///< Protects free_stack and free_count.
    sem_t               available; ///< Counts idle slots; used for non-blocking trywait.
} thread_pool_t;

/// @brief Spawn @p n_threads persistent worker threads and initialize synchronization objects.
void thread_pool_init(thread_pool_t *self, int n_threads);

/// @brief Signal all threads to exit and join them.
/// Must only be called when no work is in flight (i.e., all dispatched slots have been collected
/// and released).
void thread_pool_destroy(thread_pool_t *self);

/// @brief Non-blocking: claim an idle slot. Returns the slot index, or -1 if none are available.
int thread_pool_try_acquire(thread_pool_t *self);

/// @brief Set the work item and function on @p slot_idx and wake its thread.
void thread_pool_dispatch(thread_pool_t *self, int slot_idx, pool_work_t work, pool_fn_t fn);

/// @brief Block until @p slot_idx's thread finishes and return the result score.
int thread_pool_collect(thread_pool_t *self, int slot_idx);

/// @brief Return @p slot_idx to the idle pool. Must be called after thread_pool_collect.
void thread_pool_release(thread_pool_t *self, int slot_idx);
