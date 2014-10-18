#include "pawnstar.h"
#include <ctype.h>
/******************************************************************************
Functions to use an opening book.
Opening book text files are 1 line per line of play, with moves in SAN format.

The book itself is stored as a singly linked list. Book moves are indexed 
according to the Zobrist hash of the position. 

Structure containing a node of the opening book linked list.
*******************************************************************************/
typedef struct BookPosition BookPosition;
struct BookPosition
{
    uint64          hash;       
    int             capacity;   
    int             count;      
    int*            moves;      
    BookPosition*   next;       
};
/******************************************************************************
Head pointer
*******************************************************************************/
static BookPosition* openingBook = NULL;
/******************************************************************************
Create a new book bp on the heap and return a pointer to it
*******************************************************************************/
static BookPosition* NewBookPosition(uint64 hash)
{
    BookPosition* bp    = (BookPosition*)malloc(sizeof(BookPosition));
    bp->hash            = hash;
    bp->capacity        = 1;
    bp->count           = 0;
    bp->moves           = (int*)malloc(sizeof(int));
    bp->next            = NULL;
    return bp;
}
/******************************************************************************
Find a BookPosition from a position hash (NULL if not found)
*******************************************************************************/
static BookPosition* FindBookPosition(uint64 hash)
{
    BookPosition* bp;
    for (bp = openingBook; bp; bp = bp->next)
    {
        if (bp->hash == hash)
        {
            break;
        }
    }
    return bp;
}
/******************************************************************************
Add a new move to the opening book
*******************************************************************************/
static void AddMove(uint64 hash, int move)
{
    BookPosition* bp = FindBookPosition(hash);
    if (!bp)
    {
        bp = NewBookPosition(hash);
        bp->next = openingBook;
        openingBook = bp;
    }
    if (bp->count == bp->capacity)
    {
        bp->capacity <<= 1;
        bp->moves = (int*)realloc(bp->moves, bp->capacity * sizeof(int));
    }
    bp->moves[bp->count++] = move;
}
/******************************************************************************
Select one from the possible book moves for the current position
Returns 0 if there is no book move for this position.
*******************************************************************************/
int GetBookMove(uint64 hash)
{
    const BookPosition* bp = FindBookPosition(hash);
    return bp ? bp->moves[(unsigned)NextRandom() % bp->count] : 0;
}
/******************************************************************************
Free the heap memory used by the opening book
*******************************************************************************/
void FreeOpeningBook()
{
    BookPosition* bp = openingBook;
    BookPosition* next;
    while (bp)
    {
        next = bp->next;
        free(bp->moves);
        free(bp);
        bp = next;
    }
    openingBook = NULL;
}
typedef struct
{
    int move;
    int count;
} BookMoveCount;

/******************************************************************************
Show all possible book moves for this position
*******************************************************************************/
void DisplayAvailableBookMoves(const Position* position)
{
    int i;
    int bookMoves [MAX_MOVES_PER_POSITION] = { 0 };
    int moveCounts[MAX_MOVES_PER_POSITION] = { 0 };
    int nextMove = 0;
    const BookPosition* bp = FindBookPosition(position->hash);
    if (!bp)
    {
        return;
    }
    for (i = 0; i != bp->count; ++i)
    {
        const int* move = FindMove(bookMoves, bp->moves[i]);
        if (!move)
        {
            /* This is the first time we have seen this book move */
            bookMoves [nextMove] = bp->moves[i];
            moveCounts[nextMove] = 1;
            ++nextMove;
        }
        else
        {
            ++moveCounts[move - bookMoves]; /* increment the number of times we saw it */
        }
    }
    printf("move   count\n");
    for (i = 0; i != nextMove; ++i)
    {
        char buffer[8];
        MoveToSanString(position, bookMoves[i], buffer);
        printf("%-8s %3u\n", buffer, moveCounts[i]);
    }
}
/******************************************************************************
Initialize an opening book from a string, with each line containing a book 
line of play. Returns true on success.
*******************************************************************************/
bool InitializeOpeningBookFromString(const char* bookString)
{
    bool result = true;
    char* const book = strdup(bookString);
    char* line;
    char* nextLine = NULL;  
    int lineNumber = 0;
    FreeOpeningBook();
    /* Split the string into lines */
    for (line = book; line; line = nextLine)
    {
        Game game[1];
        char* token;
        char* nextToken = NULL;
        nextLine = strchr(line, '\n');
        if (nextLine)
        {
            *nextLine++ = '\0';
            while (*nextLine == '\n')
            {
                ++nextLine;
            }
        }
        ++lineNumber;
        InitializeGame(game);
        /* Split the line into tokens separated by space(s) */
        for (token = line; token; token = nextToken)
        {
            const uint64 hash = game->position->hash;
            bool isBadMove;
            int move;
            char* c;
            nextToken = strchr(token, ' ');
            if (nextToken)
            {
                *nextToken++ = '\0';
                while (*nextToken == ' ')
                {
                    ++nextToken;
                }
            }     
            if (token[0] == '#')
            {
                break; /* comment - ignore rest of line */
            }
            if (isdigit(token[0]))
            {
                continue; /* move number only - ignore this token */
            }
            if ((c = strchr(token, '?')) != NULL) /* don't make this move! */
            {
                isBadMove = true;
                *c = '\0';
            }
            else
            {
                isBadMove = false;
            }
            move = PlayMoveString(game, token, true);
            if (!move)
            {
                printf("ERROR: illegal move '%s' found in line %u of opening book\n", token, lineNumber);
                result = false;
                break;
            }
            if (!isBadMove)
            {
                AddMove(hash, move);
            }
        }
    }
    free(book);
    return result;
}

/******************************************************************************
Parse the book text file and build the opening book from it
*******************************************************************************/
bool InitializeOpeningBookFromFile(const char filename[])
{
    char* buffer;
    long len;
    bool result;
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        printf("NOTE: unable to open book file '%s'\n", filename);
        return false;
    }
    fseek(file, 0, SEEK_END);
    len = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer = malloc(len + 1);
    fread(buffer, 1, len, file);
    fclose(file);
    buffer[len] = '\0';
    result = InitializeOpeningBookFromString(buffer);
    free(buffer);
    return result;
}
