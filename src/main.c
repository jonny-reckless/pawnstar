/// @file Program entry point.

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "constants.h"
#include "debug_hashtable.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

void process_input(game_t *game, const char *line);

int main(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("   .::.    \n"
           "   _::_    \n"
           " _/____\\_ \n"
           " \\      / \n"
           "  \\____/  \n"
           "  (____)   \n"
           "   |  |    \n"
           "   |__|    \n"
           "  /    \\  \n"
           " (______)  \n"
           "(________) \n"
           "/________\\\n"
           "Pawnstar: a UCI compatible chess engine\n"
           "(C) Jonny Reckless 2009 - 2024\n"
           "Compiled: " __DATE__ " " __TIME__ "\n");

    game_t game;
    game_init(&game);
    if (!opening_book_initialize(&game.book, "pawnstar.book"))
        printf("info string Unable to open book file.\n");

    debug_x_clear();
    printf("ready\n");
    fflush(stdout);

    char line[4096];
    while (fgets(line, sizeof(line), stdin))
        process_input(&game, line);

    game_free(&game);
    return 0;
}
