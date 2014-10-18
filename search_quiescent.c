#include "pawnstar.h"
/******************************************************************************
Fail-hard alpha-beta quiescence search
Refer to: http://chessprogramming.wikispaces.com/Quiescence+Search
*******************************************************************************/
int SearchQuiescent(const Position* srcPosition, 
                    int depth,
                    int ply, 
                    int alpha, 
                    int beta, 
                    volatile bool* cancel)
{
    int     i;   
    int     moves[MAX_MOVES_PER_POSITION];
    int     move;
    int     score;
    bool    isFirstMove = true;
    
    INCREMENT("quiescent calls");
    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(srcPosition, alpha, beta);
    }
    if (srcPosition->stateFlags & IS_CHECK)
    {
        INCREMENT("quiescent checks");
        /**********************************************************************
        Standing pat is not allowed in check - we must do a full search
        ***********************************************************************/
        return Search(srcPosition, depth, ply, alpha, beta, cancel);
    }
    score = EvaluatePosition(srcPosition, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        return beta;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    GeneratePseudoLegalMoves(srcPosition, moves, false);    
    SortMoves(moves, ply);
    /**************************************************************************
    Main loop
    ***************************************************************************/
    for (i = 0; (move = moves[i]) != 0; ++i)
    {
        Position position[1];
#if DO_QUIESCENCE_STATIC_EXCHANGE_EVAL
        if (EvaluateStaticExchange(srcPosition, move) < 0)
        {
            INCREMENT("quiescent SEE skips");
            continue;
        }
#endif
        MakeMove(position, srcPosition, move);
        if (position->stateFlags & WAS_MOVE_ILLEGAL)
        {
            continue;
        }
        if (isFirstMove)
        {
            score = -SearchQuiescent(position, depth - 1, ply + 1, -beta, -alpha, cancel);
            isFirstMove = false;
        }
        else
        {
            INCREMENT("quiescent pvs attempts");
            score = -SearchQuiescent(position, depth - 1, ply + 1, -alpha - 1, -alpha, cancel);
            if (score > alpha && score < beta)
            {
                INCREMENT("quiescent pvs fails");
                score = -SearchQuiescent(position, depth - 1, ply + 1, -beta, -alpha, cancel);
            }
        }
        if (*cancel)
        {
            return ILLEGAL_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;            
            INCREMENT("quiescent pv changed");
        }
    }
    return alpha;
}