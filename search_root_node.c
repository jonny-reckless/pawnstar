#include "pawnstar.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define ABS(x)   ((x) < 0 ? -(x) : (x))

static volatile bool cancel;
/******************************************************************************
If the worker thread is running, set the cancel flag then wait for it to
finish
*******************************************************************************/
void StopThinkingMoveImmediately()
{
    cancel = true;
}
/******************************************************************************
Entry point for the search to find the computer move.
Returns the best move found, or 0 if there is no move (i.e. game over)
*******************************************************************************/
int SearchRootNode(const Position* srcPosition)
{
    int         i;
    ScoredMove  scoredMoves[MAX_MOVES_PER_POSITION];
    ScoredMove  bestMoveAtEachIteration[MAX_PLY];
    int         moves[MAX_MOVES_PER_POSITION];
    int         principalVariation[MAX_PLY];
    int         moveCount;
    int         depth;
    int         startTime;
    int         stopTime;
    int         bookMove;
    int         bestMove = 0;
    int         msTimeout;
    int         msAllocated;
    int         movesToGo;
    int         alpha;

    if (srcPosition->stateFlags & IS_GAME_OVER)
    {
        return 0;
    }
    if ((bookMove = GetBookMove(srcPosition->hash)) != 0)
    {
        return bookMove;
    } 
    moveCount = GenerateLegalMoves(srcPosition, moves);
    if (moveCount <= 1)
    {
        return moves[0]; /* If there is only 1 legal move available just play it */
    }  
    switch (globals->timeControl.clockType)
    {
    case STANDARD_CHESS_CLOCK:
    default:
        movesToGo   = globals->timeControl.movesPerPeriod - (srcPosition->fullMoveCount % globals->timeControl.movesPerPeriod);
        msAllocated = globals->timeControl.millisecondsRemaining / movesToGo;
        msTimeout   = MAX(100, MIN(msAllocated * 3, globals->timeControl.millisecondsRemaining - 1000));
        break;
    
    case FIXED_DEPTH:
        msTimeout   = 0;
        msAllocated = 0;
        break;
    
    case FIXED_TIME:
        msTimeout   = globals->timeControl.fixedMilliseconds;
        msAllocated = 0;
        break;
    
    case INCREMENTAL_CLOCK:
        msAllocated = globals->timeControl.incrementMilliseconds + (globals->timeControl.millisecondsRemaining / 20);
        msTimeout   = MAX(100, MIN(msAllocated * 3, globals->timeControl.millisecondsRemaining - 1000));
        break;
    }
    InitializeGoodMoveCounts();
    /**************************************************************************
    For first pass move ordering before we do any search, just use the static
    evaluation. Subsequent passes will use the results of the previous
    iteration to sort the moves (the merge sort is stable).
    ***************************************************************************/
    for (i = 0; i != moveCount; ++i)
    {
        Position position[1];
        MakeMove(position, srcPosition, moves[i]);
        scoredMoves[i].move  = moves[i];
        scoredMoves[i].score = EvaluatePosition(position, ALPHA, BETA);
    }
    DEBUG_STATEMENT(DebugXClear());
    startTime = GetMilliseconds();
    cancel = false;
    if (msTimeout)
    {
        SetStopThinkingTimer(msTimeout, &cancel);
    }
    for (depth = 1; depth != MAX_PLY; ++depth)
    {       
        if (globals->timeControl.clockType == FIXED_DEPTH && depth > globals->timeControl.fixedDepth)
        {
            break;
        }
        globals->nodeCount = 0;
        alpha = ALPHA;
        MergeSort(moveCount, scoredMoves);
        for (i = 0; i != moveCount; ++i)
        {
            scoredMoves[i].score = SearchSingleMove(srcPosition, depth, 0, alpha, BETA, scoredMoves[i].move, i, &cancel);
            if (cancel)
            {
                return bestMove;
            }
            if (scoredMoves[i].score > alpha)
            {
                alpha                                = scoredMoves[i].score;
                bestMove                             = scoredMoves[i].move;
                bestMoveAtEachIteration[depth].move  = scoredMoves[i].move;
                bestMoveAtEachIteration[depth].score = scoredMoves[i].score;
                RecordGoodMove(0, scoredMoves[i].move);
                RecordPrincipalVariationMove(srcPosition->hash, scoredMoves[i].move);
                RecordTransposition(srcPosition->hash, depth, scoredMoves[i].score, scoredMoves[i].move, NODE_PV);            
            }
        }        
        stopTime = GetMilliseconds();
        FindPrincipalVariation(srcPosition, principalVariation);
        if (principalVariation[0] == 0)
        {
            principalVariation[0] = bestMove;
            principalVariation[1] = 0;
        }
        if (globals->doShowThinking)
        {
            char moveString[256];
            MoveSequenceToSanString(srcPosition, principalVariation, moveString);
            printf("%2u %5d %4u %8u %s\n", depth, bestMoveAtEachIteration[depth].score, (stopTime - startTime) / 10, globals->nodeCount, moveString);
        }
        if (alpha > WIN_THRESHOLD || 
            alpha < LOSE_THRESHOLD)
        {
            break;
        }
        if (globals->timeControl.clockType == STANDARD_CHESS_CLOCK || 
            globals->timeControl.clockType == INCREMENTAL_CLOCK)
        {
            const int elapsedms = stopTime - startTime;
            bool isBestMoveConsistent = true;
            bool isScoreStable = true;
            for (i = 1; i != depth; ++i)
            {
                if (bestMoveAtEachIteration[i].move != bestMoveAtEachIteration[depth].move)
                {
                    isBestMoveConsistent = false;
                }
                if (ABS(bestMoveAtEachIteration[i].score - bestMoveAtEachIteration[depth].score) > SCORE_INSTABILITY_THRESHOLD)
                {
                    isScoreStable = false;
                }
            }
            if ((isBestMoveConsistent && isScoreStable) && (elapsedms * 9) > msAllocated)
            {
                break;
            }
            if ((isBestMoveConsistent || isScoreStable) && (elapsedms * 3) > msAllocated)
            {
                break;
            }
            if (elapsedms > msAllocated)
            {
                break;
            }
        }             
    }  
    CancelStopThinkingTimer();
    return bestMove;
}
