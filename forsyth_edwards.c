#include "pawnstar.h"
#include <ctype.h>

#define IS_IN_CHECK(position, color) (IsAttacked(position, position->kingLocation[color], ENEMY(color)))
/******************************************************************************
Used so that during draw by repetition detection, positions with no predecessor 
can be sent into harmless loop rather than trying to dereference a null pointer 
*******************************************************************************/
static Position nullPosition[1];
static bool IsNumeric(const char buffer[]);
/******************************************************************************
Compute the Zobrist hash of a position
*******************************************************************************/
uint64 ComputeHash(const Position* position)
{
    int piece;
    bitboard b;
    uint64 hash = position->stateFlags & IS_BLACK_TO_MOVE ? ~0ull : 0ull;
    hash ^= CASTLING_RIGHTS_HASHES[position->castleFlags & ALL_RIGHTS];
    if (position->enPassantIndex)
    {
        hash ^= EN_PASSANT_HASHES[FILE_OF(position->enPassantIndex)];
    }
    for (piece = PAWN; piece <= KING; ++piece)
    {
        b = position->pieces[piece] & position->whitePieces;
        while (b)
        {
            hash ^= PIECE_SQUARE_HASHES[WHITE][piece][FindAndClearLsb(&b)];
        }
        b = position->pieces[piece] & position->blackPieces;
        while (b)
        {
            hash ^= PIECE_SQUARE_HASHES[BLACK][piece][FindAndClearLsb(&b)];
        }
    }
    return hash;
}
/******************************************************************************
Set the state of the board to that specified in a FEN string
refer to http://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
returns true on success
returns false if an illegal FEN string was provided
*******************************************************************************/
bool PositionFromString(const char fenString[], Position* position)
{
    char buffer[STRING_BUF_LEN];
    size_t i;
    int x = 0, y = 7;
    const char* const whitePieces = "PNBRQK";
    const char* const blackPieces = "pnbrqk";
    const char* const epFiles     = "abcdefgh";
    const char* const epRanks     = "36";
    char* token;
    const char* index;
    if (strlen(fenString) >= STRING_BUF_LEN)
    {
        printf("ERROR: FEN string too long\n");
        return false;
    }
    strcpy(buffer, fenString);
    i = strlen(buffer);
    if (i && buffer[i - 1] == '\n')
    {
        buffer[i - 1] = '\0';
    }
    token = strtok(buffer, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not contain board definition\n");
        return false;
    }
    memset(nullPosition, 0, sizeof(Position));
    memset(position, 0, sizeof(Position));
    nullPosition->previous = nullPosition; // prevents null pointer dereferencing in draw by repetition detection
    position->previous     = nullPosition;
    /**************************************************************************
    Pieces on the board
    ***************************************************************************/
    for ( ; *token; ++token)
    {
        if (*token == '/')
        {
            if (x != 8)
            {
                printf("ERROR: FEN string contains too few files\n");
                return false;
            }
            x = 0;
            if (--y == -1)
            {
                printf("ERROR: FEN string contains too many ranks\n");
                return false;
            }
            continue;
        }
        if (*token >= '1' && *token <= '8')
        {
            x += *token - '0';
            continue;
        }
        if (x >= 8)
        {
            printf("ERROR: FEN string contains too many files\n");
            return false;
        }
        index = strchr(whitePieces, *token);
        if (index)
        {
            const int piece = (int)(index - whitePieces) + 1;
            position->pieces[piece] |= BITBOARD_XY(x, y);
            position->whitePieces   |= BITBOARD_XY(x, y);
            ++x;
            continue;
        }
        index = strchr(blackPieces, *token);
        if (index)
        {
            const int piece = (int)(index - blackPieces) + 1;
            position->pieces[piece] |= BITBOARD_XY(x, y);
            position->blackPieces   |= BITBOARD_XY(x, y);
            ++x;
        }
        else
        {
            printf("ERROR: unrecognized character '%c' in FEN string\n", *token);
            return false;
        }
    }
    if (y)
    {
        printf("ERROR: FEN string contains too few ranks\n");
        return false;
    }
    position->occupiedSquares = position->whitePieces | position->blackPieces;
    /**************************************************************************
    Side to move
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not specify side to move\n");
        return false;
    }
    if (!strcmp(token, "b"))
    {
        position->stateFlags |= IS_BLACK_TO_MOVE;
    }
    else if (!!strcmp(token, "w"))
    {
        printf("ERROR: FEN string contains illegal side to move '%s'\n", token);
        return false;
    }
    /**************************************************************************
    Castling rights
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not specify castling rights\n");
        return false;
    }
    if (!strcmp("-", token))
    {
        position->castleFlags = 0;
    }
    else
    {
        const char* c;
        for (c = token; *c; ++c)
        {
            switch (*c)
            {
            case 'K':
                position->castleFlags |= MAY_WHITE_K;
                break;
            case 'Q':
                position->castleFlags |= MAY_WHITE_Q;
                break;
            case 'k':
                position->castleFlags |= MAY_BLACK_K;
                break;
            case 'q':
                position->castleFlags |= MAY_BLACK_Q;
                break;
            default:
                printf("ERROR: FEN string specifies illegal castling right '%c'\n", *c);
                return false;
            }
        }
    }
    /**************************************************************************
    En passant capture square
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (!token)
    {
        printf("ERROR: FEN string does not specify en passant capture availability\n");
        return false;
    }
    if (!strcmp(token, "-"))
    {
        position->enPassantIndex = 0;
    }
    else
    {
        if (strlen(token) != 2 || !strchr(epFiles, token[0]) || !strchr(epRanks, token[1]))
        {
            printf("ERROR: FEN string specifies illegal en passant square '%s'\n", token);
            return false;
        }
        position->enPassantIndex = token[0] - 'a' + 8 * (token[1] - '1');
    }
    /**************************************************************************
    Half move clock - optional
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (token)
    {
        if (!IsNumeric(token))
        {
            printf("ERROR: FEN string specifies illegal half move clock '%s'\n", token);
            return false;
        }
        position->reversibleMoveCount = (uchar)atoi(token);
    }
    /**************************************************************************
    Full move number - optional
    ***************************************************************************/
    token = strtok(NULL, " ");
    if (token)
    {
        if (!IsNumeric(token))
        {
            printf("ERROR: FEN string specifies illegal full move number '%s'\n", token);
            return false;
        }
        position->fullMoveCount = (uchar)atoi(token) - 1;
    }
    position->kingLocation[WHITE] = (uchar)Lsb(position->kings & position->whitePieces);
    position->kingLocation[BLACK] = (uchar)Lsb(position->kings & position->blackPieces);
    position->hash = ComputeHash(position);
    if (!IsPositionLegal(position))
    {
        printf("ERROR: FEN string specifies an illegal chess position\n");
        return false;
    }
    /**************************************************************************
    Is the position in check?
    ***************************************************************************/
    if (IS_IN_CHECK(position, COLOR_TO_MOVE(position)))
    {
        position->stateFlags |= IS_CHECK;
    }
    /**************************************************************************
    Check to see if this position represents checkmate, stalemate or draw
    by insufficient material and set flags accordingly
    ***************************************************************************/
    if (IsCheckmate(position))
    {
        printf("NOTE: FEN string specifies a checkmate position\n");
        position->stateFlags |= IS_CHECKMATE;
    }
    else if (IsStalemate(position))
    {
        printf("NOTE: FEN string specifies a stalemate position\n");
        position->stateFlags |= IS_STALEMATE;
    }
    else if (IsDrawByMaterial(position))
    {
        printf("NOTE: FEN string specifies a draw by insufficient material position\n");
        position->stateFlags |= IS_DRAW_MATERIAL;
    }
    return true;
}
/******************************************************************************
Generate the FEN string for the current position
*******************************************************************************/
void PositionToString(const Position* position, char fenString[])
{
    const char* const initialPtr = fenString;
    int x, y;
    /**************************************************************************
    Pieces on the board
    ***************************************************************************/
    for (y = 7; y >= 0; --y)
    {
        for (x = 0; x < 8; ++x)
        {
            if (!(position->occupiedSquares & BITBOARD_XY(x, y)))
            {
                if (initialPtr == fenString || !(fenString[-1] >= '1' && fenString[-1] <= '8'))
                {
                    *fenString++ = '1';
                }
                else
                {
                    ++fenString[-1];
                }
            }
            else
            {
                const char piece = (position->whitePieces & BITBOARD_XY(x, y)) ?
                                   " PNBRQK"[PIECE_AT(position, x + 8 * y)] : " pnbrqk"[PIECE_AT(position, x + 8 * y)];
                *fenString++ = piece;
            }
        }
        if (y)
        {
            *fenString++ = '/';
        }
    }
    /**************************************************************************
    Side to move
    ***************************************************************************/
    fenString += sprintf(fenString, (position->stateFlags & IS_BLACK_TO_MOVE) ? " b " : " w ");
    /**************************************************************************
    Castling rights
    ***************************************************************************/
    if (!(position->castleFlags & ALL_RIGHTS))
    {
        *fenString++ = '-';
    }
    else
    {
        if (position->castleFlags & MAY_WHITE_K)
        {
            *fenString++ = 'K';
        }
        if (position->castleFlags & MAY_WHITE_Q)
        {
            *fenString++ = 'Q';
        }
        if (position->castleFlags & MAY_BLACK_K)
        {
            *fenString++ = 'k';
        }
        if (position->castleFlags & MAY_BLACK_Q)
        {
            *fenString++ = 'q';
        }
    }
    *fenString++ = ' ';
    /**************************************************************************
    En passant capture availability
    ***************************************************************************/
    if (!position->enPassantIndex)
    {
        fenString += sprintf(fenString, "- ");
    }
    else
    {
        fenString += sprintf(fenString,
                             "%c%c ",
                             FILE_CHAR(position->enPassantIndex),
                             RANK_CHAR(position->enPassantIndex));
    }
    /**************************************************************************
    Half move clock
    ***************************************************************************/
    fenString += sprintf(fenString, "%u ", position->reversibleMoveCount);
    /**************************************************************************
    Full move number
    ***************************************************************************/
    fenString += sprintf(fenString, "%u", position->fullMoveCount + 1);
}
/******************************************************************************
Set up the position for the start of a game
*******************************************************************************/
void NewGame(Position* position)
{
    PositionFromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", position);
}
/******************************************************************************
Set up the game for the start of a new game
*******************************************************************************/
void InitializeGame(Game* game)
{
    game->position = game->stack;
    NewGame(game->position);
}
/******************************************************************************
Does a C string contain a positive integral numeric value?
*******************************************************************************/
static bool IsNumeric(const char buffer[])
{
    if (!buffer || !strlen(buffer))
    {
        return false;
    }
    while (*buffer)
    {
        if (!isdigit(*buffer++))
        {
            return false;
        }
    }
    return true;
}
