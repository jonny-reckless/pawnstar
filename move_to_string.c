#include "pawnstar.h"
/******************************************************************************
Convert a move into Xboard alphanumeric string format
*******************************************************************************/
void MoveToString(int move, char moveString[])
{
    sprintf(moveString, "%c%c%c%c",
            FILE_CHAR(MOVE_FROM(move)),
            RANK_CHAR(MOVE_FROM(move)),
            FILE_CHAR(MOVE_TO  (move)),
            RANK_CHAR(MOVE_TO  (move)));
    if (MOVE_PROMOTED(move))
    {
        sprintf(moveString + 4, "%c", "  nbrq"[MOVE_PROMOTED(move)]);
    }
}
/******************************************************************************
Convert a move into standard algebraic notation
Refer to:
http://en.wikipedia.org/wiki/Portable_Game_Notation#Movetext
*******************************************************************************/
char* MoveToSanString(const Position* position, int theMove, char moveString[])
{
    int legalMoves[MAX_MOVES_PER_POSITION];
    Position dstPosition[1];
    char disambiguation[4];
    char fromSquare[4];
    char toSquare[4];
    const int* move;
    bool isSourceAmbiguous = false;
    bool isFileUnique      = true;
    bool isRankUnique      = true;
    sprintf(fromSquare, "%c%c", FILE_CHAR(MOVE_FROM(theMove)), RANK_CHAR(MOVE_FROM(theMove)));
    sprintf(toSquare,   "%c%c", FILE_CHAR(MOVE_TO  (theMove)), RANK_CHAR(MOVE_TO  (theMove)));    
    /**************************************************************************
    Determine if there is more than one piece of the same type which is capable 
    of moving to the target square, and will require further disambiguation
    ***************************************************************************/
    GenerateLegalMoves(position, legalMoves);
    for (move = legalMoves; *move; ++move)
    {
        if (MOVE_PIECE(*move) == MOVE_PIECE(theMove) && 
            MOVE_TO   (*move) == MOVE_TO   (theMove) &&
            MOVE_FROM (*move) != MOVE_FROM (theMove))
        {
            isSourceAmbiguous = true;
            if (FILE_OF(MOVE_FROM(*move)) == FILE_OF(MOVE_FROM(theMove)))
            {
                isFileUnique = false;
            }
            if (RANK_OF(MOVE_FROM(*move)) == RANK_OF(MOVE_FROM(theMove)))
            {
                isRankUnique = false;
            }   
        }
    }
    /**************************************************************************
    Determine how to disambiguate source square based on the uniqueness of the 
    source rank and file
    ***************************************************************************/
    if (isSourceAmbiguous)
    {
        if (isFileUnique)
        {
            sprintf(disambiguation, "%c", fromSquare[0]); 
        }
        else if (isRankUnique)
        {
            sprintf(disambiguation, "%c", fromSquare[1]);
        }
        else
        {
            strcpy(disambiguation, fromSquare);
        }
    }
    else
    {
        disambiguation[0] = '\0';
    }
    /**************************************************************************
    Now generate the SAN string based on the move type
    ***************************************************************************/
    switch (MOVE_TYPE(theMove))
    {
    case NON_CAPTURE:
        moveString += sprintf(moveString, "%c%s%s", "  NBRQK"[MOVE_PIECE(theMove)], disambiguation, toSquare);
        break;
    case SINGLE_PAWN_PUSH:
    case DOUBLE_PAWN_PUSH:
        moveString += sprintf(moveString, "%s", toSquare); 
        break;
    case CASTLING:
        switch (MOVE_TO(theMove))
        {
        case G1:
        case G8:
            moveString += sprintf(moveString, "O-O");
            break;
        case C1:
        case C8:
            moveString += sprintf(moveString, "O-O-O");
            break;
        }
        break;
    case EN_PASSANT_CAPTURE:
        moveString += sprintf(moveString, "%cx%se.p.", fromSquare[0], toSquare);
        break;
    case CAPTURE:
        if (MOVE_PIECE(theMove) == PAWN)
        {
            moveString += sprintf(moveString, "%cx%s", fromSquare[0], toSquare);
        }
        else
        {
            moveString += sprintf(moveString, "%c%sx%s", "  NBRQK"[MOVE_PIECE(theMove)], disambiguation, toSquare);
        }
        break;
    case PAWN_PROMOTION_NON_CAPTURE:
        moveString += sprintf(moveString, "%s=%c", toSquare, "  NBRQ"[MOVE_PROMOTED(theMove)]);
        break;
    case PAWN_PROMOTION_CAPTURE:
        moveString += sprintf(moveString, "%cx%s=%c", fromSquare[0], toSquare, "  NBRQ"[MOVE_PROMOTED(theMove)]);
        break;
    }
    /**************************************************************************
    Determine if this move results in check or checkmate
    ***************************************************************************/
    MakeMove(dstPosition, position, theMove);
    if (IsCheckmate(dstPosition))
    {
        moveString += sprintf(moveString, "#");
    }
    else if (dstPosition->stateFlags & IS_CHECK)
    {
        moveString += sprintf(moveString, "+");
    }
    return moveString;
}
/******************************************************************************
Convert a sequence of moves into SAN representation with each move separated by
a space
*******************************************************************************/
void MoveSequenceToSanString(const Position* position, const int moves[], char moveString[])
{
    bool isFirstMove = true;
    Position srcPosition[1];
    Position dstPosition[1];
    const int* move;
    *srcPosition = *position;
    for (move = moves; *move; ++move)
    {
        if (!isFirstMove)
        {
            *moveString++ = ' ';
        }
        moveString = MoveToSanString(srcPosition, *move, moveString);
        MakeMove(dstPosition, srcPosition, *move);
        *srcPosition = *dstPosition;
        isFirstMove = false;
    }
}
static const char* const STRINGS_TO_REMOVE[] = {
    "+",
    "#",
    "ep",
    "e.p.",
    "!",
    "?",
    NULL,
};
/******************************************************************************
Compare two move strings for equality. Sometimes UIs don't bother to put the 
"e.p.", "+" or "#" at the end of the move string; we still want these to 
compare as equal.
*******************************************************************************/
bool AreMoveStringsEqual(char* str1, char* str2)
{
    const char* const * x;
    if (!str1 || !str2 || !strlen(str1) || !strlen(str2))
    {
        return false;
    }
    for (x = STRINGS_TO_REMOVE; *x; ++x)
    {
        char* y;
        if ((y = strstr(str1, *x)) != NULL)
        {
            *y = '\0';
        }
        if ((y = strstr(str2, *x)) != NULL)
        {
            *y = '\0';
        }
    }
    return !strcmp(str1, str2);
}