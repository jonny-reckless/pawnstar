#include "pawnstar.h"
/******************************************************************************
Main search routine: fail-hard alpha beta
Refer to:
http://chessprogramming.wikispaces.com/Alpha-Beta
http://chessprogramming.wikispaces.com/Principal+Variation+Search
*******************************************************************************/
int Search(const Position* srcPosition, 
           int depth, 
           int ply, 
           int alpha, 
           int beta, 
           volatile bool* cancel)
{
    int             i;
    Transposition   transposition[1];
    int             move;   
    int             moves[MAX_MOVES_PER_POSITION];
    int             score;
    int             legalMoveCount  = 0;      
    int             bestMove        = 0;
    bool            haveRaisedAlpha = false;
    bool            isTransposition;

    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    ++globals->nodeCount;
    INCREMENT("alpha beta calls");
    INCREMENT_IF(srcPosition->stateFlags & IS_CHECK, "checks");
    /**************************************************************************
    This is a leaf node which requires no further searching if either of the 
    following are true: 

    # it is a drawn position by repetition, material or the 50-move rule
    # we have reached the absolute maximum ply of search
    ***************************************************************************/
    if (IsDrawByRepetition(srcPosition, true) || 
        IsDrawByMaterial(srcPosition)         || 
        IsDrawByFiftyMoves(srcPosition))
    {
        return DRAW_SCORE;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(srcPosition, alpha, beta);
    }
    /**************************************************************************
    Is it appropriate to go to the quiescence search?
    ***************************************************************************/
    if (!(srcPosition->stateFlags & IS_CHECK) && depth <= 0)
    {
        return SearchQuiescent(srcPosition, depth, ply, alpha, beta, cancel);
    }
    /**************************************************************************
    Determine if there is an entry in the transposition table for this 
    position.
    ***************************************************************************/
    isTransposition = FindTransposition(srcPosition->hash, transposition);
    if (isTransposition)
    {
        Position position[1];
        MakeMove(position, srcPosition, transposition->move);
        if (IsDrawByRepetition(position, true))
        {
            /******************************************************************
            We cannot trust this transposition table entry since it resulted 
            in a draw by repetition when played here.
            *******************************************************************/
            INCREMENT("table hit caused draw by repetition");
            isTransposition = false;
        }
        else if (transposition->depth >= depth)
        {
            INCREMENT("table hit");
            switch (transposition->nodeType)
            {
            case NODE_CUT:
                /**************************************************************
                We don't know the exact score of the best move from this 
                position, but we do know it is at least transposition->score
                ***************************************************************/ 
                if (transposition->score >= beta)
                {
                    INCREMENT("table hit beta cutoffs");
                    return beta;
                }
                break;
            
            case NODE_ALL:
                /**************************************************************
                We don't know the exact score of the best move from this 
                position, but we do know it is at most transposition->score
                ***************************************************************/
                if (transposition->score <= alpha)
                {
                    INCREMENT("table hit alpha cutoffs");
                    return alpha;
                }
                break;
            
            case NODE_PV:
                /**************************************************************
                We know the exact score and the best move from this position.
                ***************************************************************/
                INCREMENT("table hit exact score");
                if (transposition->score > alpha && transposition->score < beta && transposition->move)
                {
                    RecordPrincipalVariationMove(srcPosition->hash, transposition->move);
                }
                return transposition->score;
            }            
        }
    } 

    
#if DO_NULL_MOVE_REDUCTION
    /**************************************************************************
    Try null move reduction if ALL of the following are true:

    # we are not in check 
    # this is not the root node
    # the previous move was not a null move    
    # we are not down to king and pawns
    # we have at least 4 pieces remaining
    # the material eval score is high enough for a beta cutoff 
    ***************************************************************************/
    if (!(srcPosition->stateFlags & IS_CHECK)  &&
        ply != 0                               &&
        srcPosition->move)
    {
        const bitboard friendlyPieces = (srcPosition->occupiedSquares ^ srcPosition->kings) & srcPosition->allPieces[COLOR_TO_MOVE(srcPosition)];
        if ((friendlyPieces & ~srcPosition->pawns) && 
            PopCount(friendlyPieces) > 4           && 
            EvaluateMaterial(srcPosition) >= beta)
        {
            Position position[1];
            INCREMENT("null move attempts");
            MakeNullMove(position, srcPosition);
            score = -Search(position, depth - 3, ply, -beta, -beta + 1, cancel);
            if (*cancel)
            {
                return ILLEGAL_SCORE;
            }
            if (score >= beta)
            {   
                return beta;
            }
            INCREMENT("null move fails");
        }
    }
#endif

    /**************************************************************************
    Try any PV table move first
    ***************************************************************************/
    if ((move = GetPrincipalVariationMove(srcPosition->hash)) != 0)
    {
        INCREMENT("pv table move");
        score = SearchSingleMove(srcPosition, depth, ply, alpha, beta, move, 0, cancel);
        if (*cancel)
        {
            return ILLEGAL_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("pv table move caused beta cutoff");
            RecordTransposition(srcPosition->hash, depth, beta, move, NODE_CUT);
            RecordGoodMove(ply, move);
            return beta;
        }
        if (score > alpha)
        {
            INCREMENT("pv table move changed pv");
            haveRaisedAlpha = true;
            bestMove = move;
            alpha = score;
            RecordGoodMove(ply, move);                       
        }
    }
    /**************************************************************************
    Try any transposition table move next
    ***************************************************************************/
    if (isTransposition && transposition->move && transposition->move != move)
    {
        INCREMENT("table move");
        move = transposition->move;
        score = SearchSingleMove(srcPosition, depth, ply, alpha, beta, move, 0, cancel);
        if (*cancel)
        {
            return ILLEGAL_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move caused beta cutoff");
            RecordTransposition(srcPosition->hash, depth, beta, move, NODE_CUT);
            RecordGoodMove(ply, move);
            return beta;
        }
        if (score > alpha)
        {
            INCREMENT("table move changed pv");
            haveRaisedAlpha = true;
            bestMove = move;
            alpha = score;
            RecordGoodMove(ply, move);            
        }
    }
    GeneratePseudoLegalMoves(srcPosition, moves, true);
    SortMoves(moves, ply);
    /**************************************************************************
    Start of main loop
    ***************************************************************************/
    for (i = 0; (move = moves[i]) != 0; ++i)
    {       
        score = SearchSingleMove(srcPosition, depth, ply, alpha, beta, move, legalMoveCount, cancel);
        if (*cancel)
        {
            return ILLEGAL_SCORE;
        }
        if (score == MOVED_INTO_CHECK_SCORE)
        {
            continue;
        }
        ++legalMoveCount;
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            RecordTransposition(srcPosition->hash, depth, beta, move, NODE_CUT);
            RecordGoodMove(ply, move);
            return beta;
        }
        if (score > alpha)
        {
            INCREMENT("pv changed");
            haveRaisedAlpha = true;
            bestMove = move;
            alpha = score;
            RecordGoodMove(ply, move);            
        }
    }
    /**************************************************************************
    End of main loop: we have searched all moves and did not get a cutoff.
    ***************************************************************************/
    if (legalMoveCount == 0)
    {
        /**********************************************************************
        We failed to find any strictly legal moves from this position.
        The position therefore represents either checkmate or stalemate 
        depending on whether we are presently in check.
        If we are in checkmate, add the ply (distance from the root node) to  
        the losing score, so that the search algorithm chooses the shortest  
        path to checkmate when multiple possible checkmates exist in the tree.
        ***********************************************************************/
        if (srcPosition->stateFlags & IS_CHECK)
        {
            INCREMENT("checkmates");
            return CHECKMATED_SCORE + ply;
        }
        INCREMENT("stalemates");
        return DRAW_SCORE;
    }  
    
    if (haveRaisedAlpha)
    {
        /**********************************************************************
        We raised alpha but did not cutoff; this was a PV node
        ***********************************************************************/
        RecordPrincipalVariationMove(srcPosition->hash, bestMove);
        RecordTransposition(srcPosition->hash, depth, alpha, bestMove, NODE_PV);
        RecordGoodMove(ply, bestMove);
    }
    else
    {
        /**********************************************************************
        We tried every move but did not raise alpha; this was an all node
        ***********************************************************************/
        RecordTransposition(srcPosition->hash, depth, alpha, 0, NODE_ALL);
    }
    return alpha;
}

/******************************************************************************
Search a single move and return its score, or MOVED_INTO_CHECK_SCORE if the 
move was not legal.
*******************************************************************************/
int SearchSingleMove(const Position* srcPosition, 
                     int depth, 
                     int ply, 
                     int alpha, 
                     int beta, 
                     int move, 
                     int moveIndex, 
                     volatile bool* cancel)
{
    static const int SEVENTH_RANK[2] = { 6, 1 };
    Position position[1];
    int childDepth;
    int score;
    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    MakeMove(position, srcPosition, move);
    if (position->stateFlags & WAS_MOVE_ILLEGAL)
    {
        return MOVED_INTO_CHECK_SCORE;
    }
    /******************************************************************
    Extend the search depth if ANY of the following are true:
    # We are in check
    # This is a pawn promotion
    # This is a pawn push to the 7th rank
    
    Reduce the search depth if ALL of the following are true:
    # We are not in check
    # Search depth is greater than or equal to 3
    # This move does not give check
    # The move was not from the PV or TT
    # This move does not have a positive static exchange evaluation
    *******************************************************************/
    if (srcPosition->stateFlags & IS_CHECK)
    {
        INCREMENT("extensions check");
        childDepth = depth;
    }
    else if (MOVE_PROMOTED(move))
    {
        INCREMENT("extensions promotion");
        childDepth = depth;
    }
    else if (MOVE_PIECE(move) == PAWN && 
             RANK_OF(MOVE_TO(move)) == SEVENTH_RANK[COLOR_TO_MOVE(srcPosition)])
    {
        INCREMENT("extensions push to 7th");
        childDepth = depth;
    }
#if DO_LATE_MOVE_REDUCTION
    else if (depth < 3                         || 
             moveIndex == 0                    ||
             (position->stateFlags & IS_CHECK) ||
             srcPosition->move == 0            ||
             EvaluateStaticExchange(srcPosition, move) >= 0)
    {
        INCREMENT("extensions none (normal search depth)");
        childDepth = depth - 1;
    }
    else
    {
        INCREMENT("extensions reduce LMR");
        childDepth = depth - 2;
    }
#else
    else
    {
        INCREMENT("extensions none (normal search depth)");
        childDepth = depth - 1;
    }
#endif
    if (moveIndex != 0 && !(srcPosition->stateFlags & IS_CHECK))
    {
        INCREMENT("pvs attempts");
        score = -Search(position, childDepth, ply + 1, -alpha - 1, -alpha, cancel);
        INCREMENT_IF(score >= beta, "pvs cutoffs");
        if (score > alpha && score < beta)
        {
            INCREMENT("pvs fails");
            score = -Search(position, childDepth, ply + 1, -beta, -alpha, cancel);
        }
    }
    else
    {
        score = -Search(position, childDepth, ply + 1, -beta, -alpha, cancel);
    }
#if DO_LATE_MOVE_REDUCTION
    if (childDepth == depth - 2 && score > alpha && score < beta)
    {
        /**********************************************************************
        A late move which was depth reduced turned out to be a good move after 
        all. Redo the search with regular depth.
        ***********************************************************************/
        INCREMENT("lmr fail");
        score = -Search(position, depth - 1, ply + 1, -beta, -alpha, cancel);
    }
#endif
    return score;
}