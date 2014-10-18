#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Functions to manage a list of pending search tasks and a list of free search 
tasks, which is used to avoid the overhead of frequent memory allocation; an 
expensive operation during search.

With the exception of one-time called FreeAllSearchTasks and InitializeTaskList,
all functions are thread-safe as they are called from each of the search worker 
threads.
*******************************************************************************/
static volatile int    pendingTaskCount;
static SearchTask*      pendingTasksHead;
static SearchTask*      pendingTasksTail;
static SearchTask*      freeTasks;
static CRITICAL_SECTION pendingMutex[1];
static CRITICAL_SECTION freeMutex[1];
/******************************************************************************
Initialize the lists (set up the mutices)
*******************************************************************************/
void InitializeTaskList(void)
{
    InitializeCriticalSectionAndSpinCount(pendingMutex, MUTEX_SPIN_COUNT);
    InitializeCriticalSectionAndSpinCount(freeMutex,    MUTEX_SPIN_COUNT);
}
/******************************************************************************
Free all heap memory used by the lists
NOT THREAD SAFE
*******************************************************************************/
void FreeAllSearchTasks(void)
{
    SearchTask* task = freeTasks;
    SearchTask* next;
    while (task)
    {
        next = task->next;
        free(task);
        task = next;
    }    
    task = pendingTasksHead;
    while (task)
    {
        next = task->next;
        free(task);
        task = next;
    }
    freeTasks        = NULL;
    pendingTasksHead = NULL;
    pendingTasksTail = NULL;
    pendingTaskCount = 0;
}
/******************************************************************************
Get a SearchTask from the free list, allocating extra tasks if required (this
should happen rarely in practice so is not a big performance hit)
*******************************************************************************/
SearchTask* ObtainTaskFromPool(void)
{
    SearchTask* result;
    EnterCriticalSection(freeMutex);
    result = freeTasks;
    if (!result)
    {
        int i;
        INCREMENT("task allocations");
        result = calloc(NUM_TASKS_TO_ALLOCATE_AT_ONCE, sizeof(SearchTask));
        for (i = 0; i != NUM_TASKS_TO_ALLOCATE_AT_ONCE - 1; ++i)
        {
            result[i].next = &result[i + 1];
        }
        result[NUM_TASKS_TO_ALLOCATE_AT_ONCE - 1].next = NULL;
    }
    freeTasks = result->next;
    MemoryBarrier();
    LeaveCriticalSection(freeMutex);
    return result;
}
/******************************************************************************
Return a SearchTask to the free list
*******************************************************************************/
void ReturnTaskToPool(SearchTask* task)
{
    EnterCriticalSection(freeMutex);
    task->next = freeTasks;
    freeTasks = task;
    MemoryBarrier();
    LeaveCriticalSection(freeMutex);
}
/******************************************************************************
Push a pending task to the head of the pending list
*******************************************************************************/
void AddPendingTask(SearchTask* task)
{
    EnterCriticalSection(pendingMutex);
    if (!pendingTaskCount)
    {
        task->prev = NULL;
        task->next = NULL;
        pendingTasksHead = task;
        pendingTasksTail = task;
        pendingTaskCount = 1;
    }
    else
    {
        task->prev = NULL;
        task->next = pendingTasksHead;
        pendingTasksHead->prev = task;
        pendingTasksHead = task;
        ++pendingTaskCount;
    }
    MemoryBarrier();
    LeaveCriticalSection(pendingMutex);
}
/******************************************************************************
Pop a pending task from the tail of the pending list
*******************************************************************************/
SearchTask* GetNextPendingTask(void)
{
    SearchTask* result;
    EnterCriticalSection(pendingMutex);
    switch (pendingTaskCount)
    {
    case 0:
        result = NULL;
        break;
    case 1:
        result = pendingTasksTail;
        pendingTasksTail = NULL;
        pendingTasksHead = NULL;
        pendingTaskCount = 0;
        break;
    default:
        result = pendingTasksTail;
        pendingTasksTail = pendingTasksTail->prev;
        pendingTasksTail->next = NULL;
        --pendingTaskCount;
    }
    MemoryBarrier();
    LeaveCriticalSection(pendingMutex);
    return result;
}
/******************************************************************************
Returns the number of currently pending tasks
*******************************************************************************/
int GetNumberOfPendingTasks(void)
{
    return pendingTaskCount;
}