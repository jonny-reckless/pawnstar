#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Functions to support parallel search. SearchTasks are added to a list of
pending tasks, and worker threads (one per processor in the system) consume 
pending tasks. A pending task may be aborted prior to execution, for example if
a beta cutoff renders it redundant.
*******************************************************************************/
static long                 numRunningTasks;
static HANDLE               searchCompleteEvent;
static CONDITION_VARIABLE   newTaskCondVar[1];
static CRITICAL_SECTION     newTaskMutex[1];

static DWORD WINAPI WorkerThreadLoop(LPVOID args);
static void         StartWorkerThreads(int numCPUs);
/******************************************************************************
Initialize the worker threads and synchronization events.
*******************************************************************************/
void InitializeThreads(int numCPUs)
{
    InitializeConditionVariable(newTaskCondVar);
    InitializeCriticalSectionAndSpinCount(newTaskMutex, MUTEX_SPIN_COUNT);
    searchCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    StartWorkerThreads(numCPUs);
}
/******************************************************************************
Wait until all the workers have finished their work.
*******************************************************************************/
void WaitForSearchToComplete(void)
{
    WaitForSingleObject(searchCompleteEvent, INFINITE);
}
/******************************************************************************
Obtain a new search task and return a pointer to it.
If move == 0 then the task will search all nodes, otherwise just search a 
single move.
*******************************************************************************/
SearchTask* NewSearchTask(const Position* srcPosition, 
                          int depth, 
                          int ply, 
                          int alpha, 
                          int beta, 
                          int move, 
                          int moveIndex)
{
    SearchTask* task  = ObtainTaskFromPool();
    task->srcPosition = srcPosition;
    task->depth       = depth;
    task->ply         = ply;
    task->alpha       = alpha;
    task->beta        = beta;
    task->move        = move;
    task->moveIndex   = moveIndex;
    task->cancel      = false;
    task->taskState   = TASK_PENDING;
    task->score       = ILLEGAL_SCORE;
    AddPendingTask(task);
    WakeConditionVariable(newTaskCondVar);
    return task;
}
/******************************************************************************
Called by a search thread whenever it needs to wait for another task to 
complete before it can return. 
Grab the next pending task and execute it.
*******************************************************************************/
void DoSomethingUseful(void)
{
    SearchTask* task;
    EnterCriticalSection(newTaskMutex);
    while ((task = GetNextPendingTask()) == NULL)
    {
        SleepConditionVariableCS(newTaskCondVar, newTaskMutex, INFINITE);
    }
    LeaveCriticalSection(newTaskMutex);
    if (_InterlockedCompareExchange(&task->taskState, TASK_RUNNING, TASK_PENDING) != TASK_PENDING)
    {
        /**********************************************************************
        Task was aborted prior to execution
        ***********************************************************************/
        ReturnTaskToPool(task);
        return;
    }
    _InterlockedIncrement(&numRunningTasks);
    if (task->move)
    {
        task->score = SearchSingleMove(
            task->srcPosition,
            task->depth, 
            task->ply, 
            task->alpha, 
            task->beta, 
            task->move, 
            task->moveIndex, 
            &task->cancel);
    }
    else
    {
        task->score = Search(task->srcPosition, task->depth, task->ply, task->alpha, task->beta, &task->cancel);
    }
    _InterlockedExchange(&task->taskState, TASK_COMPLETED);
    if (_InterlockedDecrement(&numRunningTasks) == 0)
    {
        SetEvent(searchCompleteEvent);
    }
}
/******************************************************************************
Abort a pending (or running) task. Typically invoked because a beta cutoff 
occurred during search which means that this task is now redundant.
*******************************************************************************/
void AbortTask(SearchTask* task)
{
    if (_InterlockedCompareExchange(&task->taskState, TASK_ABORTED, TASK_PENDING) == TASK_PENDING)
    {
        INCREMENT("task aborts pre-emptive");
        return;
    }
    task->cancel = true;
    while (task->taskState != TASK_COMPLETED)
    {
        DoSomethingUseful();
    }
    ReturnTaskToPool(task);
    INCREMENT("task aborts while running");
}
/******************************************************************************
Main search worker thread loop.
*******************************************************************************/
static DWORD WINAPI WorkerThreadLoop(LPVOID args)
{
    UNREFERENCED_PARAMETER(args);
    for ( ; ; )
    {
        DoSomethingUseful();
    }  
}
/******************************************************************************
Start the search worker threads.
*******************************************************************************/
static void StartWorkerThreads(int numCPUs)
{
    while (numCPUs--)
    {
        QueueUserWorkItem((LPTHREAD_START_ROUTINE)WorkerThreadLoop, NULL, WT_EXECUTEDEFAULT);
    }
}