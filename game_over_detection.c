#include "pawnstar.h"
/******************************************************************************
Determine if the current position represents a draw by repetition of position
3 times (i.e. this position has occurred in the history twice previously)

Since a draw by repetition must occur with the same side to move, we only need 
to consider positions which are 2n plies away from the present position

Since pawn moves and capture moves are irreversible, we only need to consider 
the last half-move-clock positions (as we can only reach the same position by
a sequence of reversible moves)

We start looking 4 plies back from the current position, since 2 moves 
per side is the minimum required to obtain the same position (where each side
makes then subsequently unmakes a reversible move)

We use position Zobrist hash values to determine position equality - there is a
tiny chance of hash collision causing an error - I have never seen it happen 
in practice
*******************************************************************************/
bool IsDrawByRepetition(const Position* position, bool isSearch)
{
    const uint64 hash = position->hash;
    const int reversibleMoveCount = position->reversibleMoveCount;
    int i;
    int repetitions = isSearch ? 1 : 2;
    if (reversibleMoveCount < 4)
    {
        return false;
    }
    position = position->previous->previous->previous->previous;
    for (i = 4; i <= reversibleMoveCount; i += 2)
    {
        if (position->hash == hash && --repetitions == 0)
        {
            INCREMENT("draws by repetition");
            return true;
        }
        position = position->previous->previous;
    }
    return false;
}
/******************************************************************************
Determine if the current position represents a draw by the 50 move rule
(50 consecutive reversible moves by each player is a drawn game)
*******************************************************************************/
bool IsDrawByFiftyMoves(const Position* position)
{
    if (position->reversibleMoveCount >= 100)
    {
        INCREMENT("draws by 50 moves");
        return true;
    }
    return false;
}
/******************************************************************************
Determine if the current position represents a draw by insufficient material

according to FIDE rules the drawn combinations are:

a) king vs king
b) king and knight vs king
c) king and bishop vs king
d) king and bishop vs king and bishop, with bishops on the same color square
*******************************************************************************/
bool IsDrawByMaterial(const Position* position)
{
    switch (PopCount(position->occupiedSquares))
    {
    case 0:
    case 1:
        printf("ERROR: too few pieces for play\n");
    case 2:
        /**********************************************************************
        king vs king
        ***********************************************************************/
        INCREMENT("draws by material (2)");
        return true;
    case 3:
        /**********************************************************************
        king and bishop vs king
        king and knight vs king
        ***********************************************************************/
        if (position->bishops | position->knights)
        {
            INCREMENT("draws by material (3)");
            return true;
        }
        return false;       
    case 4:
        /**********************************************************************
        king and bishop vs king and bishop with bishops on same color square
        ***********************************************************************/
        {
            const bitboard whiteBishops = position->bishops & position->whitePieces;
            const bitboard blackBishops = position->bishops ^ whiteBishops;
            if (whiteBishops && blackBishops && (!(whiteBishops & WHITE_SQUARES) == !(blackBishops & WHITE_SQUARES)))
            {
                INCREMENT("draws by material (4)");
                return true;
            }
            return false;
        }
    default:
        return false;
    }
}
/******************************************************************************
Determine if the current position represents stalemate
*******************************************************************************/
bool IsStalemate(const Position* position)
{
    int moves[MAX_MOVES_PER_POSITION];
    return 
        !(position->stateFlags & IS_CHECK) && 
        GenerateLegalMoves(position, moves) == 0;
}
/******************************************************************************
Determine if the current position represents checkmate
*******************************************************************************/
bool IsCheckmate(const Position* position)
{
    int moves[MAX_MOVES_PER_POSITION];
    return 
        (position->stateFlags & IS_CHECK) && 
        GenerateLegalMoves(position, moves) == 0;
}
/******************************************************************************
Determine if the current position state is a legal chess position:
a) each side shall have strictly 1 king
b) kings shall not be adjacent
c) the side not on move shall not be in check
*******************************************************************************/
bool IsPositionLegal(const Position* position)
{
    const int color        = COLOR_TO_MOVE(position);
    const bitboard whiteKing = position->kings & position->whitePieces;
    const bitboard blackKing = position->kings & position->blackPieces;
    return
        HAS_SINGLE_BIT_SET(whiteKing)                 &&
        HAS_SINGLE_BIT_SET(blackKing)                 &&
        whiteKing != blackKing                        &&
        !(KING_ATTACKS[Lsb(whiteKing)] & blackKing)   &&
        !IsAttacked(position, position->kingLocation[ENEMY(color)], color);
}
/******************************************************************************
If the game is over, display the xboard style result string
*******************************************************************************/
void DisplayResultIfGameOver(const Position* position)
{
    switch (position->stateFlags & IS_GAME_OVER)
    {
    default:
        break;
    case IS_CHECKMATE:
        printf((position->stateFlags & IS_BLACK_TO_MOVE) ? "1-0 {white mates}\n" : "0-1 {black mates}\n");
        break;
    case IS_STALEMATE:
        printf("1/2-1/2 {stalemate}\n");
        break;
    case IS_DRAW_REPETITION:
        printf("1/2-1/2 {draw by repetition}\n");
        break;
    case IS_DRAW_MATERIAL:
        printf("1/2-1/2 {draw by insufficient material}\n");
        break;
    case IS_DRAW_50_MOVES:
        printf("1/2-1/2 {draw by 50 reversible moves}\n");
        break;
    }
}