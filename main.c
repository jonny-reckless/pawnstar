#include "pawnstar.h"

Context globals[1];

static void InitializeGlobals(void)
{
    globals->timeControl.clockType              = STANDARD_CHESS_CLOCK;
    globals->timeControl.baseMilliseconds       = 300000;
    globals->timeControl.fixedDepth             = 7;
    globals->timeControl.fixedMilliseconds      = 5000;
    globals->timeControl.incrementMilliseconds  = 5000;
    globals->timeControl.millisecondsPerPeriod  = 300000;
    globals->timeControl.millisecondsRemaining  = 300000;
    globals->timeControl.movesPerPeriod         = 40;
    globals->nodeCount                          = 0;
    globals->engineColor                        = NEITHER_COLOR;
    globals->doShowThinking                     = true;
}

extern const CommandHandler handlers[];

int main()
{       
    printf(
#if 0
        "                       .::.                            \n"
        "                       _::_                            \n"
        "                     _/____\\_                         \n"
        "                     \\      /                         \n"
        "                      \\____/                          \n"
        "                      (____)                           \n"
        "                       |  |                            \n"
        "                       |__|                            \n"
        "                      /    \\                          \n"
        "                     (______)                          \n"
        "                    (________)                         \n"
        "                    /________\\                      \n\n"
#endif
        "Pawnstar: A Winboard and Xboard compatible chess engine\n"
        "(C) Jonny Reckless 2009 - 2014                         \n"
        "Compiled: " __DATE__ " " __TIME__                     "\n"
        );
    InitializeGlobals();
#if !DO_EVALUATION_FULL
    InitializePieceSquareTable();
#endif
    InitializeTranspositionTable(HASHTABLE_MEGABYTES);
    InitializeGoodMoveCounts();
    InitializeTaskList();
    InitializePrincipalVariationTable();
    InitializeThreads(1);
    if (!InitializeOpeningBookFromFile("pawnstar.book"))
    {
        printf("NOTE: using built in opening book\n");
        InitializeOpeningBookFromString(OPENING_BOOK_MOVES);
    }
    InitializeGame(globals->game);
    DEBUG_STATEMENT(DebugXClear());
    for ( ; ; )
    {
        char nullchar[1];
        char lineBuffer[STRING_BUF_LEN];
        char* command;
        char* argument;
        char* nl;
        const CommandHandler* commandHandler;
        fgets(lineBuffer, sizeof(lineBuffer), stdin);
        nl = strchr(lineBuffer, '\n');
        if (nl)
        {
            *nl = '\0';
        }
        command = strtok(lineBuffer, " ");
        if (!command)
        {
            continue;
        }
        argument = strtok(NULL, "");
        if (!argument)
        {
            nullchar[0] = '\0';
            argument = nullchar;
        }
        for (commandHandler = handlers; commandHandler->name != NULL; ++commandHandler)
        {
            if (!strcmp(commandHandler->name, command))
            {
                commandHandler->function(argument);
                break;
            }
        }
    }
}
