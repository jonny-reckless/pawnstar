/// @file UCI command handlers.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"

typedef void (*handler_fn_t)(game_t *game, char **args, int argc);

typedef struct
{
    handler_fn_t function;
    const char  *name;
    const char  *description;
} handler_t;

// Forward declaration for help handler (needs handlers array).
static void handle_help(game_t *game, char **args, int argc);

static void handle_bookmoves(game_t *game, char **args, int argc)
{
    (void)args;
    (void)argc;
    opening_book_display_available_moves(&game->book, game->position);
}

static void handle_freebook(game_t *game, char **args, int argc)
{
    (void)args;
    (void)argc;
    opening_book_free(&game->book);
}

static void handle_eval(game_t *game, char **args, int argc)
{
    (void)args;
    (void)argc;
    printf("evaluation %d\n", evaluate_position(game->position, ALPHA, BETA));
    fflush(stdout);
}

static void handle_dbg(game_t *game, char **args, int argc)
{
    (void)game;
    (void)args;
    (void)argc;
    debug_x_write();
}

static void handle_dbgclear(game_t *game, char **args, int argc)
{
    (void)game;
    (void)args;
    (void)argc;
    debug_x_clear();
}

static void handle_getboard(game_t *game, char **args, int argc)
{
    (void)args;
    (void)argc;
    char buf[128];
    position_to_string(game->position, buf, sizeof(buf));
    printf("%s\n", buf);
    fflush(stdout);
}

static void handle_uci(game_t *game, char **args, int argc)
{
    (void)game;
    (void)args;
    (void)argc;
    printf("id name Pawnstar\n");
    printf("id author Jonny Reckless\n");
    printf("uciok\n");
    fflush(stdout);
}

static void handle_ucinewgame(game_t *game, char **args, int argc)
{
    (void)args;
    (void)argc;
    game_stop_thinking(game);
    game_new_game_default(game);
}

static void handle_go(game_t *game, char **args, int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        if ((strcmp(args[i], "wtime") == 0 && position_color_to_move(game->position) == WHITE) ||
            (strcmp(args[i], "btime") == 0 && position_color_to_move(game->position) == BLACK))
        {
            game->time_control.clock_type   = CLOCK_STANDARD;
            game->time_control.ms_remaining = atoi(args[++i]);
            continue;
        }
        if (strcmp(args[i], "depth") == 0)
        {
            game->time_control.clock_type = CLOCK_FIXED_DEPTH;
            game->time_control.depth      = atoi(args[++i]);
            continue;
        }
        if (strcmp(args[i], "movetime") == 0)
        {
            game->time_control.clock_type   = CLOCK_FIXED_TIME;
            game->time_control.ms_remaining = atoi(args[++i]);
            continue;
        }
        if (strcmp(args[i], "infinite") == 0)
        {
            game->time_control.clock_type   = CLOCK_INFINITE;
            game->time_control.ms_remaining = 0;
        }
        if (strcmp(args[i], "movestogo") == 0)
        {
            game->time_control.num_moves_remaining = atoi(args[++i]);
        }
    }
    game_start_thinking(game);
}

static void handle_stop(game_t *game, char **args, int argc)
{
    (void)args;
    (void)argc;
    game_stop_thinking(game);
}

static void handle_quit(game_t *game, char **args, int argc)
{
    (void)args;
    (void)argc;
    game_stop_thinking(game);
    exit(0);
}

static void handle_isready(game_t *game, char **args, int argc)
{
    (void)game;
    (void)args;
    (void)argc;
    printf("readyok\n");
    fflush(stdout);
}

static void handle_position(game_t *game, char **args, int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(args[i], "startpos") == 0)
        {
            game_new_game_default(game);
            continue;
        }
        if (strcmp(args[i], "fen") == 0 && i + 1 < argc)
        {
            // FEN may span up to 6 tokens; collect them.
            // Find "moves" keyword to know where FEN ends.
            char fen_buf[256] = {0};
            int  j            = i + 1;
            for (; j < argc && strcmp(args[j], "moves") != 0; ++j)
            {
                if (fen_buf[0])
                {
                    strncat(fen_buf, " ", sizeof(fen_buf) - strlen(fen_buf) - 1);
                }
                strncat(fen_buf, args[j], sizeof(fen_buf) - strlen(fen_buf) - 1);
            }
            game_new_game(game, fen_buf);
            i = j - 1;
            continue;
        }
        if (strcmp(args[i], "moves") == 0)
        {
            continue;
        }
        game_play_move_from_string(game, args[i]);
    }
}

#define COMMAND(name) handle_##name, #name

// clang-format off
static const handler_t handlers[] =
{
    { COMMAND(bookmoves),  "Display available book moves for current position" },
    { COMMAND(dbg),        "Display diagnostic counts" },
    { COMMAND(dbgclear),   "Reset diagnostic counts" },
    { COMMAND(eval),       "Display the current static evaluation" },
    { COMMAND(freebook),   "Delete the opening book from memory" },
    { COMMAND(getboard),   "Display the FEN of the current position" },
    { COMMAND(go),         "search the current position" },
    { COMMAND(help),       "Display a summary of commands" },
    { COMMAND(isready),    "Respond with readyok" },
    { COMMAND(position),   "Set the position and series of moves" },
    { COMMAND(quit),       "Exit the program" },
    { COMMAND(stop),       "Stop searching and return best move found" },
    { COMMAND(uci),        "Enter UCI protocol" },
    { COMMAND(ucinewgame), "UCI mode start new game" },
};
// clang-format on

static const int NUM_HANDLERS = (int)(sizeof(handlers) / sizeof(handlers[0]));

static void handle_help(game_t *game, char **args, int argc)
{
    (void)game;
    (void)args;
    (void)argc;
    printf("Available commands:\n");
    for (int i = 0; i < NUM_HANDLERS; ++i)
    {
        printf("%-12s %s\n", handlers[i].name, handlers[i].description);
    }
    fflush(stdout);
}

void process_input(game_t *game, const char *line)
{
    char buf[4096];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *argv[512];
    int   argc = 0;
    char *save_ptr;
    for (char *token = strtok_r(buf, " \t\r\n", &save_ptr); token; token = strtok_r(NULL, " \t\r\n", &save_ptr))
    {
        argv[argc++] = token;
    }
    if (argc == 0)
    {
        return;
    }
    for (int i = 0; i < NUM_HANDLERS; ++i)
    {
        if (strcmp(argv[0], handlers[i].name) == 0)
        {
            handlers[i].function(game, argv, argc);
            break;
        }
    }
}
